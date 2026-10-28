#include <Wire.h>
#include <U8g2lib.h>
#include "anonymous_logo.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

const int BTN_UP = 32;
const int BTN_DOWN = 33;
const int BTN_LEFT = 25;
const int BTN_RIGHT = 26;
const int BTN_OK = 27;
const int BTN_POWER = 18;
const int Blue_LED = 2;
const int rowy1 = 60;
const int rowy2 = 47;
const int rowy3 = 35;
const int rowy4 = 23;
const int rowx1 = 15;
const int rowx2 = 45;
const int rowx3 = 78;
const int rowx4 = 110;
const unsigned long debounceDelay = 50;

int cursorRow = 0;
int cursorCol = 0;
int lastReading = HIGH;

bool LastPowerState = HIGH;
bool PowerState;
bool ScreenState = true;
bool OkPressed = false;
bool lastOKState = HIGH;
bool longPressHandled = false;
bool menuOpen = false;
bool toolsLoaded = false;
bool wifiOpen = false;

unsigned long lastDebounceTime = 0;
unsigned long pressStart = 0;

String expression = "";

void setup() {
  pinMode(BTN_UP, INPUT_PULLUP);
  pinMode(BTN_DOWN, INPUT_PULLUP);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_OK, INPUT_PULLUP);
  pinMode(BTN_POWER, INPUT_PULLUP);
  pinMode(Blue_LED, OUTPUT);

  Wire.begin(21, 22);
  oled.begin();
  oled.setFont(u8g2_font_6x12_tf);
  bootScreen();
}

void bootScreen() {
  digitalWrite(Blue_LED, HIGH);
  oled.clearBuffer();
  oled.drawXBMP(0, 0, 128, 64, anonymous_logo);
  oled.sendBuffer();

  delay(3500);

  for (int i = 0; i < 4; i++) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x12_tf);

    oled.drawStr(32, 25, "CALCULATOR");
    oled.drawHLine(10, 32, 108);

    if (i == 0) {
      oled.drawStr(32, 48, "Loading");
    } else if (i == 1) {
      oled.drawStr(32, 48, "Loading.");
    } else if (i == 2) {
      oled.drawStr(32, 48, "Loading..");
    } else {
      oled.drawStr(32, 48, "Loading...");
    }

    oled.sendBuffer();
    delay(350);
  }
}

void loop() {
  digitalWrite(Blue_LED, LOW);
  handlePowerButton();
  if (ScreenState) {
    oled.setPowerSave(0);
    if (menuOpen == false) {
      readButtons();
      handleOkButton();
      screen();
    } else {
      if (toolsLoaded == false) {
        HackMenu();
        toolsLoaded = true;
      }
      HackTools();
    }
  } else {
    oled.setPowerSave(1);
  }
}

void readButtons() {
  if (digitalRead(BTN_UP) == LOW) {
    if (cursorRow > 0) {
      cursorRow--;
    }
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    if (cursorRow < 3) {
      cursorRow++;
    }
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }

  if (digitalRead(BTN_LEFT) == LOW) {
    if (cursorCol > 0) {
      cursorCol--;
    }
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
  }

  if (digitalRead(BTN_RIGHT) == LOW) {
    if (cursorCol < 3) {
      cursorCol++;
    }
    while (digitalRead(BTN_RIGHT) == LOW) delay(10);
  }
}

void screen() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawHLine(0, 12, 128);
  oled.drawStr(2, 10, expression.c_str());

  for (int x = 0; x <= 128; x += 32) {
    oled.drawVLine(x, 13, 51);
  }

  for (int y = 13; y <= 64; y += 12) {
    oled.drawHLine(0, y, 128);
  }

  oled.drawStr(rowx1, rowy1, "C");
  oled.drawStr(rowx2, rowy1, "0");
  oled.drawStr(rowx3, rowy1, "=");
  oled.drawStr(rowx4, rowy1, "+");

  oled.drawStr(rowx1, rowy2, "1");
  oled.drawStr(rowx2, rowy2, "2");
  oled.drawStr(rowx3, rowy2, "3");
  oled.drawStr(rowx4, rowy2, "-");

  oled.drawStr(rowx1, rowy3, "4");
  oled.drawStr(rowx2, rowy3, "5");
  oled.drawStr(rowx3, rowy3, "6");
  oled.drawStr(rowx4, rowy3, "*");

  oled.drawStr(rowx1, rowy4, "7");
  oled.drawStr(rowx2, rowy4, "8");
  oled.drawStr(rowx3, rowy4, "9");
  oled.drawStr(rowx4, rowy4, "/");

  int cursorX = cursorCol * 32;
  int cursorY = 13 + cursorRow * 12;

  oled.drawFrame(cursorX + 1, cursorY + 1, 30, 10);

  oled.sendBuffer();
}

void selectKey() {
  char key = ' ';

  if (cursorRow == 0 && cursorCol == 0) key = '7';
  if (cursorRow == 0 && cursorCol == 1) key = '8';
  if (cursorRow == 0 && cursorCol == 2) key = '9';
  if (cursorRow == 0 && cursorCol == 3) key = '/';

  if (cursorRow == 1 && cursorCol == 0) key = '4';
  if (cursorRow == 1 && cursorCol == 1) key = '5';
  if (cursorRow == 1 && cursorCol == 2) key = '6';
  if (cursorRow == 1 && cursorCol == 3) key = '*';

  if (cursorRow == 2 && cursorCol == 0) key = '1';
  if (cursorRow == 2 && cursorCol == 1) key = '2';
  if (cursorRow == 2 && cursorCol == 2) key = '3';
  if (cursorRow == 2 && cursorCol == 3) key = '-';

  if (cursorRow == 3 && cursorCol == 0) key = 'C';
  if (cursorRow == 3 && cursorCol == 1) key = '0';
  if (cursorRow == 3 && cursorCol == 2) key = '=';
  if (cursorRow == 3 && cursorCol == 3) key = '+';

  if (key == 'C') {
    expression = "";
  } else if (key == '=') {
      if (expression == "616") {
        menuOpen = true;
        expression = "";
      } else {
        calculate();
      }
  } else if (key != ' ') {
    expression += key;
  }
}

void calculate() {
  int opPos = -1;

  for (int i = 1; i < expression.length(); i++) {
    char c = expression[i];

    if (c == '+' || c == '-' || c == '*' || c == '/') {
      opPos = i;
      break;
    }
  }

  if (opPos == -1) return;

  double a = expression.substring(0, opPos).toDouble();
  double b = expression.substring(opPos + 1).toDouble();
  char op = expression[opPos];
  double result = 0;

  if (op == '+') result = a + b;
  if (op == '-') result = a - b;
  if (op == '*') result = a * b;

  if (op == '/') {
    if (b == 0) {
      expression = "Division by zero";
      return;
    }
    result = a / b;
  }

  expression = "=" + String(result, 4);
}

void handlePowerButton() {
  int reading = digitalRead(BTN_POWER);

  if (reading != lastReading) {
    lastDebounceTime = millis();
  }

  if (millis() - lastDebounceTime > debounceDelay) {
    if (reading != LastPowerState) {
      LastPowerState = reading;

      if (LastPowerState == LOW) {
        ScreenState = !ScreenState;
        expression = "";
      }
    }
  }

  lastReading = reading;
}

void handleOkButton() {
  bool currentOKState = digitalRead(BTN_OK);

  if (currentOKState == LOW && lastOKState == HIGH) {
    pressStart = millis();
    longPressHandled = false;
  }

  if (currentOKState == LOW && millis() - pressStart >= 600 && longPressHandled == false && cursorRow == 3 && cursorCol == 1) {
    longPressHandled = !longPressHandled;
    expression += ".";
  }

  if (currentOKState == HIGH && lastOKState == LOW) {
    if (!longPressHandled) {
      selectKey();
    }
  }

  lastOKState = currentOKState;
}

void HackMenu() {
  for (int i = 0; i < 9; i++) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x12_tf);
    digitalWrite(Blue_LED, HIGH);
    oled.drawStr(32, 25, "HACK MODE");
    oled.drawHLine(10, 32, 108);

    if (i == 0) {
      oled.drawStr(15, 48, "Loading.");
    } else if (i == 1) {
      oled.drawStr(15, 48, "Loading..");
    } else if (i == 2) {
      oled.drawStr(15, 48, "Loading...");
    } else if (i == 3) {
      oled.drawStr(15, 48, "LOADING MODULES");
    } else if (i == 4) {
      oled.drawStr(15, 48, "CHECKING MEMORY");
    } else if (i == 5) {
      oled.drawStr(15, 48, "LOAD DATA");
    } else if (i == 6) {
      oled.drawStr(15, 48, "INITIALIZING");
    } else if (i == 7) {
      oled.drawStr(15, 48, "LOADING PAYLOAD");
    } else if (i == 8) {
      oled.drawStr(15, 48, "WELCOME BACK");
    }
    oled.sendBuffer();
    toolsLoaded = true;
    delay(1000);
  }
}

void WIFI() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawHLine(32, 20, 77);
  oled.drawHLine(32, 40, 77);
  oled.drawVLine(32, 20, 20);
  oled.drawVLine(109, 20, 20);
  oled.drawStr(38, 34, "WI-FI TOOLS");
  oled.sendBuffer();
}

void HackTools() {
  if (wifiOpen) {
    WIFI();
    if (digitalRead(BTN_LEFT) == LOW) {
      wifiOpen = false;
      while (digitalRead(BTN_LEFT) == LOW) delay(10);
    }
    return;
  }
  digitalWrite(Blue_LED, LOW);
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawHLine(32, 20, 70);
  oled.drawHLine(32, 40, 70);
  oled.drawVLine(32, 20, 20);
  oled.drawVLine(102, 20, 20);
  oled.drawStr(38, 34, "CALCULATOR");
  oled.sendBuffer();
  if (digitalRead(BTN_RIGHT) == LOW) {
    wifiOpen = true;
    while (digitalRead(BTN_RIGHT) == LOW) delay(10);
  } else if (digitalRead(BTN_OK) == LOW) {
    while (digitalRead(BTN_OK) == LOW) delay(10);
    menuOpen = false;
    toolsLoaded = false;
    lastOKState = HIGH;
  }
}
