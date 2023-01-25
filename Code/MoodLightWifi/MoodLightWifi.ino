#include <Adafruit_NeoPixel.h>

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <ArduinoJson.h>

// Hardware defs
#define PIN_BUTTON 0
#define PIN_LED 2
#define LED_COUNT 17
#define SEGMENT_COUNT ((int)(LED_COUNT / 2))

// Timing Defs
#define INPUT_TIME 300

// Wifi declarations
const char *ssid = "RavLamp";
const char *password = "hbdravraj1"; //Needs to be at least 8 characters

IPAddress local_ip(192, 168, 1, 1);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

ESP8266WebServer server(80);

// LED and hard input declarations
Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

uint32_t state = 0;
const int stateCount = 5;

uint32_t currentColor;
int brightness = 250;
uint32_t animStep = 0;
unsigned long ledUpdateTime = 15;
unsigned long timerLEDUpdate = 0;
unsigned long timerInputUpdate = 0;

int prevButtonState;

void setup()
{
  // Wifi Setup
  Serial.begin(115200);
  Serial.println("Booting");

  WiFi.mode(WIFI_AP);
  Serial.print("Setting soft-AP config...");
  Serial.println(WiFi.softAPConfig(local_ip, gateway, subnet) ? "Ready" : "Failed");
  Serial.print("Setting soft-AP...");
  Serial.println(WiFi.softAP(ssid, password) ? "Ready" : "Failed");
  Serial.print("Soft-AP IP...");
  Serial.println(WiFi.softAPIP());

  delay(100);
  SPIFFS.begin();

  if (!MDNS.begin("ravlampio", WiFi.softAPIP()))
  {
    Serial.println("MDNS responder did not setup");
    while (1)
    {
      delay(1000);
    }
  }
  else
  {
    Serial.println("MDNS setup is successful!");
  }

  server.on("/getcurrent", []()
            {
              StaticJsonDocument<120> doc;
              doc["r"] = currentColor >> 16;
              doc["g"] = (currentColor & 0xFFFF) >> 8;
              doc["b"] = (currentColor & 0xFF);
              doc["bright"] = strip.getBrightness();
              doc["mode"] = state;
              char dataResponse[120];
              serializeJson(doc, dataResponse, 120);
              server.send(200, "application/json", dataResponse);
              Serial.print("Sent: ");
              Serial.println(dataResponse);
            });

  // Request to update the lamp
  server.on("/update", []()
            {
              //    server.send(200, "text/plain", "got the message");
              String data = server.arg("plain");
              StaticJsonDocument<100> doc;
              DeserializationError err = deserializeJson(doc, data);
              if (err)
              {
                Serial.print("Deserialization failed");
                Serial.println(err.c_str());
              }
              String r = doc["r"];
              String g = doc["g"];
              String b = doc["b"];
              String bright = doc["bright"];
              String modeid = doc["mode"];
              Serial.print("Red: ");
              Serial.println(r);
              Serial.print("Green: ");
              Serial.println(g);
              Serial.print("Blue: ");
              Serial.println(b);
              Serial.print("Bright: ");
              Serial.println(bright);
              Serial.print("Mode: ");
              Serial.println(modeid);
              server.send(200, "application/json", data);
              brightness = bright.toInt();
              currentColor = strip.Color(r.toInt(), g.toInt(), b.toInt());
              state = modeid.toInt();
              SPIFFSwrite();
              initModeChange();
            });
  server.onNotFound([]()
                    {
                      if (!handleFileRead(server.uri()))
                      {
                        server.send(404, "text/plain", "404: Not Found");
                      }
                    });

  server.begin();
  MDNS.addService("http", "tcp", 80);

  // LED Setup
  strip.begin();
  strip.show();
  strip.setBrightness(brightness);
  pinMode(PIN_BUTTON, INPUT);
  prevButtonState = digitalRead(PIN_BUTTON);
  delay(100);
  timerLEDUpdate = millis();
  timerInputUpdate = millis();

  // Load color from spiffs
  File colorFile = SPIFFS.open("/defaultcolor.txt", "r");
  if (!colorFile)
  {
    Serial.println("Color File Load Failed");
    currentColor = strip.Color(255, 255, 255);
  }
  else
  {
    uint8_t r = colorFile.parseInt();
    uint8_t g = colorFile.parseInt();
    uint8_t b = colorFile.parseInt();
    brightness = colorFile.parseInt();
    state = colorFile.parseInt();
    currentColor = strip.Color(r, g, b);
  }
  colorFile.close();
  initModeChange();
  Serial.print(F("Current Color: "));
  Serial.println(currentColor);
}

void loop()
{
  // Wifi Handler
  MDNS.update();
  server.handleClient();

  // Input read cycle
  if (millis() - timerInputUpdate > INPUT_TIME)
  {
    if (digitalRead(PIN_BUTTON) == LOW && prevButtonState == HIGH)
    {
      state = (state + 1) % stateCount;
      brightness = 250;
      initModeChange();
      SPIFFSwrite();
    }
    prevButtonState = digitalRead(PIN_BUTTON);
  }

  // Mode handler
  switch (state)
  {
  case 0:
    writeStrip(strip.Color(255, 255, 255));
    break;
  case 1:
    breathing(&animStep, currentColor);
    break;
  case 2:
    rainbow(&animStep);
    break;
  case 3:
    writeStrip(currentColor);
    break;
  default:
    break;
  }

  // Strip update cycle
  if (millis() - timerLEDUpdate > ledUpdateTime)
  {
    strip.show();
    animStep++;
    timerLEDUpdate = millis();
  }
} // End void loop

// Wifi Methods
String getContentType(String filename)
{ // convert the file extension to the MIME type
  if (filename.endsWith(".html"))
    return "text/html";
  else if (filename.endsWith(".css"))
    return "text/css";
  else if (filename.endsWith(".js"))
    return "application/javascript";
  return "text/plain";
}

bool handleFileRead(String path)
{ // send the right file to the client (if it exists)
  Serial.println("handleFileRead: " + path);
  if (path.endsWith("/"))
    path += "index.html";                    // If a folder is requested, send the index file
  String contentType = getContentType(path); // Get the MIME type
  path.replace("/css", "");
  path.replace("/js", "");
  path.replace(".css", ".css.gz");
  path.replace(".js", ".js.gz");
  path.replace(".html", ".gz");
  Serial.println("editied path: " + path);
  Serial.println("Content Type: " + contentType);
  if (SPIFFS.exists(path))
  {                                                     // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    size_t sent = server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  Serial.println("\tFile Not Found");
  return false; // If the file doesn't exist, return false
}

void SPIFFSwrite()
{
  File colorFile = SPIFFS.open("/defaultcolor.txt", "w");
  if (!colorFile)
  {
    Serial.println("Color File Load Failed");
    return;
  }
  colorFile.print((int)(currentColor >> 16));
  colorFile.print(" ");
  colorFile.print((int)((currentColor & 0xFFFF) >> 8));
  colorFile.print(" ");
  colorFile.print((int)((currentColor & 0xFF)));
  colorFile.print(" ");
  colorFile.print((int)strip.getBrightness());
  colorFile.print(" ");
  colorFile.print((int)state);
  colorFile.close();
  Serial.println("file write done");
}

// LED Methods

void initModeChange()
{
  animStep = 0;
  stripReset();
  strip.setBrightness(brightness);
  timerInputUpdate = millis();
}

// Set the colour of any particular segment
void writeSegment(int segment, uint32_t color)
{
  strip.setPixelColor(segment * 2, color);
  if ((segment * 2 + 1) < strip.numPixels())
  {
    strip.setPixelColor(segment * 2 + 1, color);
  }
}

void writeStrip(uint32_t color)
{
  for (int i = 0; i < strip.numPixels(); i++)
  {
    strip.setPixelColor(i, color);
  }
}

void stripReset()
{
  strip.clear();
}

void rainbow(uint32_t *animStep)
{
  writeSegment(SEGMENT_COUNT, strip.Color(255, 255, 255));
  for (int i = 0; i < SEGMENT_COUNT; i++)
  { // For each segment
    int pixelHue = (*animStep * 256) % (65536) + (i * 65536L / SEGMENT_COUNT);
    writeSegment(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  if (*animStep > 255)
  {
    *animStep = 0;
  }
}

void breathing(uint32_t *animStep, uint32_t color)
{
  writeStrip(color);
  if (*animStep > 255 / 2)
  {
    strip.setBrightness((510 - *animStep * 2) * brightness / 255);
  }
  else
  {
    strip.setBrightness((*animStep * 2) * brightness / 255);
  }
  if (*animStep > 255)
  {
    *animStep = 0;
  }
}
