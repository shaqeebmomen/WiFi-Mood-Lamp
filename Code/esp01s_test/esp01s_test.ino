#include <Adafruit_NeoPixel.h>
#define PIN_BUTTON 0
#define PIN_LED 2
#define INPUT_TIME 150
#define LED_COUNT 17
#define SEGMENT_COUNT ((int)(LED_COUNT/2))


Adafruit_NeoPixel strip(LED_COUNT, PIN_LED, NEO_GRB + NEO_KHZ800);

uint32_t state = 0;
const int stateCount = 4;
uint32_t currentColor = strip.Color(252, 186, 3);
uint32_t animStep = 0;
unsigned long ledUpdateTime = 10;
unsigned long timerLEDUpdate = 0;
unsigned long timerInputUpdate = 0;

int prevButtonState;

void setup() {
  strip.begin();
  strip.show();
  strip.setBrightness(250);
  pinMode(PIN_BUTTON, INPUT);
  prevButtonState = digitalRead(PIN_BUTTON);
  delay(100);
  timerLEDUpdate = millis();
  timerInputUpdate = millis();
}

void loop() {
  // Wifi Handler
  
  // Input read cycle
  if (millis() - timerInputUpdate > INPUT_TIME) {
    if (digitalRead(PIN_BUTTON) == LOW && prevButtonState == HIGH) {
      state = (state + 1 ) % stateCount;
      animStep = 0;
      stripReset();
      timerInputUpdate = millis();
    }
    prevButtonState =  digitalRead(PIN_BUTTON);
  }
  
// Mode handler
  switch (state) {
    case 0:
      writeStrip(strip.Color(255, 255, 255));
      break;
    case 1:
      breathing(&animStep, currentColor);
      break;
    case 2:
      rainbow(&animStep);
      break;
    default:
      breathing(&animStep, strip.Color(255, 255, 255));
      break;
  }
  
  // Strip update cycle
  if (millis() - timerLEDUpdate > ledUpdateTime) {
    strip.show();
    animStep++;
    timerLEDUpdate = millis();
  }
}

// Set the colour of any particular segment
void writeSegment(int segment, uint32_t color) {
  strip.setPixelColor(segment * 2, color);
  if ((segment * 2 + 1) < strip.numPixels()) {
    strip.setPixelColor(segment * 2 + 1, color);
  }
}

void writeStrip(uint32_t color) {
  for (int i = 0; i < strip.numPixels(); i++) {
    strip.setPixelColor(i, color);
  }
}

void stripReset(){
  strip.setBrightness(250);
  strip.clear();
}

void rainbow(uint32_t* animStep) {
  writeSegment(SEGMENT_COUNT, strip.Color(255, 255, 255));
  for (int i = 0; i < SEGMENT_COUNT; i++) { // For each segment
    int pixelHue = (*animStep * 256) % (65536) + (i * 65536L / SEGMENT_COUNT);
    writeSegment(i, strip.gamma32(strip.ColorHSV(pixelHue)));
  }
  if (*animStep > 255) {
    *animStep = 0;
  }
}

void breathing(uint32_t* animStep, uint32_t color) {
  writeStrip(color);
  if (*animStep > 255 / 2) {
    strip.setBrightness(255 - *animStep * 2);
  }
  else {
    strip.setBrightness(*animStep * 2);
  }

  if (*animStep > 255) {
    *animStep = 0;
  }
}
