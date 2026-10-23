#include <Wire.h>
#include <U8g2lib.h>

// OLED 128x64, SSD1306, I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(
  U8G2_R0,
  U8X8_PIN_NONE
);

const int Pin1 = 27;
const int Pin2 = 26;
const int Pin3 = 19;
const int Pin4 = 4;
const int Pin5 = 18;

int i = 0;

bool ledState = false;
bool led1State = LOW;
bool lastButtonState = HIGH;

unsigned long previousMillis = 0;
const unsigned long interval = 1000;

unsigned long previousDisplayMillis = 0;
const unsigned long displayInterval = 200;

void setup() {
  pinMode(Pin1, OUTPUT);
  pinMode(Pin2, OUTPUT);
  pinMode(Pin3, OUTPUT);
  pinMode(Pin4, INPUT_PULLUP);
  pinMode(Pin5, INPUT_PULLUP);

  // SDA = GPIO 21, SCL = GPIO 22
  Wire.begin(21, 22);

  oled.begin();
  oled.setFont(u8g2_font_6x12_tf);

  oled.clearBuffer();
  oled.drawStr(0, 15, "ESP32 Device");
  oled.drawStr(0, 32, "Starting...");
  oled.sendBuffer();
}

void loop() {
  unsigned long currentMillis = millis();

  // Blink LED without delay()
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    led1State = !led1State;
    digitalWrite(Pin1, led1State);

    if (led1State == LOW) {
      i++;

      if (i >= 4) {
        i = 0;
      }
    }
  }

  button1();
  button2();

  // Refresh display every 200 ms
  if (currentMillis - previousDisplayMillis >= displayInterval) {
    previousDisplayMillis = currentMillis;
    updateDisplay();
  }
}

void button1() {
  if (digitalRead(Pin4) == LOW) {
    digitalWrite(Pin2, HIGH);
  }
  else {
    digitalWrite(Pin2, LOW);
  }
}

void button2() {
  bool currentButtonState = digitalRead(Pin5);

  if (currentButtonState == LOW && lastButtonState == HIGH) {
    ledState = !ledState;
  }

  lastButtonState = currentButtonState;
  digitalWrite(Pin3, ledState);
}

void updateDisplay() {
  bool button1Pressed = (digitalRead(Pin4) == LOW);

  oled.clearBuffer();

  oled.drawStr(0, 12, "LED PANEL");
  oled.drawHLine(0, 16, 128);

  char line[24];

  snprintf(line, sizeof(line), "Stage: %d / 3", i);
  oled.drawStr(0, 29, line);

  snprintf(line, sizeof(line), "LED 3: %s",
           ledState ? "ON" : "OFF");
  oled.drawStr(70, 41, line);

  snprintf(line, sizeof(line), "LED 1: %s",
           led1State ? "ON" : "OFF");
  oled.drawStr(0, 41, line);

  snprintf(line, sizeof(line), "LED 2: %s",
           digitalRead(Pin2) ? "ON" : "OFF");
  oled.drawStr(0, 53, line);

  snprintf(line, sizeof(line), "BTN 1: %s",
           button1Pressed ? "DOWN" : "UP");
  oled.drawStr(70, 53, line);

  oled.sendBuffer();
}
