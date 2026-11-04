#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "poison_logo_128x64.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// Pins
const int BTN_UP = 32;
const int BTN_DOWN = 33;
const int BTN_LEFT = 25;
const int BTN_RIGHT = 26;
const int BTN_OK = 27;
const int BTN_POWER = 4; // RTC-пин, нужен для пробуждения из deep sleep
const int Blue_LED = 2;

// Calculator layout
const int rowy1 = 60;
const int rowy2 = 47;
const int rowy3 = 35;
const int rowy4 = 23;
const int rowx1 = 15;
const int rowx2 = 45;
const int rowx3 = 78;
const int rowx4 = 110;

enum AppState {
  STATE_MAIN_MENU, STATE_SET_TIME, STATE_CALCULATOR,
  STATE_SCI_MENU, STATE_SETTINGS, STATE_INFO
};
AppState appState = STATE_MAIN_MENU;

// Calculator state
int cursorRow = 0;
int cursorCol = 0;
String expression = "";
bool hasOperator = false;
bool hasDecimal = false;

// OK button: неблокирующий опрос, срабатывает сразу на 600мс без ожидания отпускания
bool lastOKState = HIGH;
bool longPressHandled = false;
unsigned long pressStart = 0;
bool okShortFired = false;
bool okLongFired = false;

// Main menu
const int mainMenuCount = 4;
const char* mainMenuItems[mainMenuCount] = {"Calculator", "MP3 Player", "Settings", "Info"};
int mainMenuIndex = 0;

// Settings menu
const int settingsMenuCount = 2;
const char* settingsMenuItems[settingsMenuCount] = {"Set Time", "Back"};
int settingsMenuIndex = 0;

// Scientific menu
const int sciMenuCount = 7;
const char* sciMenuItems[sciMenuCount] = {
  "sin", "cos", "tan", "ctg", "sqrt(x)", "x^2", "Back"
};
int sciMenuIndex = 0;

RTC_DATA_ATTR int rtcHours = 12;
RTC_DATA_ATTR int rtcMinutes = 0;
int clockSeconds = 0;
unsigned long lastSecMillis = 0;
int timeSetField = 0; // 0 = часы, 1 = минуты

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

  lastSecMillis = millis();
  bootScreen();
}

void bootScreen() {
  digitalWrite(Blue_LED, HIGH);
  oled.clearBuffer();
  oled.drawXBMP(0, 0, 128, 64, poison_logo);
  oled.sendBuffer();
  delay(3500);

  for (int i = 0; i < 5; i++) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x12_tf);
    oled.drawStr(32, 25, "Raze");
    oled.drawHLine(10, 32, 108);

    if (i == 0) oled.drawStr(32, 48, "Loading");
    else if (i == 1) oled.drawStr(32, 48, "Loading.");
    else if (i == 2) oled.drawStr(32, 48, "Loading..");
    else if (i == 3) oled.drawStr(32, 48, "Loading...");
    else if (i == 4) oled.drawStr(10, 48, "Loading complete");

    oled.sendBuffer();
    delay(350);
  }
  digitalWrite(Blue_LED, LOW);
}

void loop() {
  checkPowerButton();
  updateClock();
  updateOkButton();

  switch (appState) {
    case STATE_MAIN_MENU:
      handleMainMenu();
      drawMainMenu();
      break;
    case STATE_SET_TIME:
      handleSetTime();
      drawSetTime();
      break;
    case STATE_CALCULATOR:
      handleCalculatorOk();
      readButtons();
      screen();
      break;
    case STATE_SCI_MENU:
      handleSciMenu();
      drawSciMenu();
      break;
    case STATE_SETTINGS:
      handleSettingsMenu();
      drawSettingsMenu();
      break;
    case STATE_INFO:
      handleInfo();
      drawInfo();
      break;
  }
}

void updateOkButton() {
  bool current = digitalRead(BTN_OK);
  okShortFired = false;
  okLongFired = false;

  if (current == LOW && lastOKState == HIGH) {
    pressStart = millis();
    longPressHandled = false;
  }

  if (current == LOW && !longPressHandled && millis() - pressStart >= 600) {
    longPressHandled = true;
    okLongFired = true;
  }

  if (current == HIGH && lastOKState == LOW && !longPressHandled) {
    okShortFired = true;
  }

  lastOKState = current;
}

// ======================= POWER =======================

void checkPowerButton() {
  if (digitalRead(BTN_POWER) == LOW) {
    delay(30);
    if (digitalRead(BTN_POWER) == LOW) {
      while (digitalRead(BTN_POWER) == LOW) delay(10);
      enterDeepSleep();
    }
  }
}

void enterDeepSleep() {
  oled.clearBuffer();
  oled.sendBuffer();
  oled.setPowerSave(1);
  digitalWrite(Blue_LED, LOW);

  rtc_gpio_pullup_en((gpio_num_t)BTN_POWER);
  rtc_gpio_pulldown_dis((gpio_num_t)BTN_POWER);
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_POWER, 0);

  esp_deep_sleep_start();
}

// ============================= CLOCK ================================

void updateClock() {
  if (millis() - lastSecMillis >= 1000) {
    lastSecMillis += 1000;
    clockSeconds++;
    if (clockSeconds >= 60) {
      clockSeconds = 0;
      rtcMinutes++;
      if (rtcMinutes >= 60) {
        rtcMinutes = 0;
        rtcHours = (rtcHours + 1) % 24;
      }
    }
  }
}

void handleSetTime() {
  if (digitalRead(BTN_UP) == LOW) {
    if (timeSetField == 0) rtcHours = (rtcHours + 1) % 24;
    else rtcMinutes = (rtcMinutes + 1) % 60;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (timeSetField == 0) rtcHours = (rtcHours + 23) % 24;
    else rtcMinutes = (rtcMinutes + 59) % 60;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    timeSetField = 0;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    timeSetField = 1;
    while (digitalRead(BTN_RIGHT) == LOW) delay(10);
  }
  if (okShortFired) {
    clockSeconds = 0;
    lastSecMillis = millis();
    appState = STATE_SETTINGS;
  }
}

void drawSetTime() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(30, 12, "SET TIME");
  oled.drawHLine(0, 16, 128);

  char buf[6];
  snprintf(buf, sizeof(buf), "%02d:%02d", rtcHours, rtcMinutes);
  oled.drawStr(44, 40, buf);

  if (timeSetField == 0) oled.drawStr(44, 50, "^^");
  else oled.drawStr(68, 50, "^^");

  oled.drawStr(2, 62, "UP/DW OK:save");
  oled.sendBuffer();
}

// =========================== MAIN MENU ==============================

void handleMainMenu() {
  if (digitalRead(BTN_UP) == LOW) {
    if (mainMenuIndex > 0) mainMenuIndex--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (mainMenuIndex < mainMenuCount - 1) mainMenuIndex++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }

  if (okShortFired) {
    switch (mainMenuIndex) {
      case 0:
        expression = "";
        hasOperator = false;
        hasDecimal = false;
        appState = STATE_CALCULATOR;
        break;
      case 1:
        showComingSoonMP3();
        break;
      case 2:
        settingsMenuIndex = 0;
        appState = STATE_SETTINGS;
        break;
      case 3:
        appState = STATE_INFO;
        break;
    }
  }
}

void drawMainMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", rtcHours, rtcMinutes);
  oled.drawStr(2, 10, "Raze");
  oled.drawStr(96, 10, timeStr);
  oled.drawHLine(0, 13, 128);

  for (int i = 0; i < mainMenuCount; i++) {
    int y = 25 + i * 12;
    if (i == mainMenuIndex) {
      oled.drawBox(0, y - 9, 128, 11);
      oled.setDrawColor(0);
      oled.drawStr(6, y, mainMenuItems[i]);
      if (i == 1) oled.drawStr(90, y, "(soon)");
      oled.setDrawColor(1);
    } else {
      oled.drawStr(6, y, mainMenuItems[i]);
      if (i == 1) oled.drawStr(90, y, "(soon)");
    }
  }
  oled.sendBuffer();
}

void showComingSoonMP3() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(20, 30, "MP3 Player");
  oled.drawStr(15, 45, "Coming soon");
  oled.sendBuffer();
  delay(1000);
}

// =========================== SETTINGS ================================

void handleSettingsMenu() {
  if (digitalRead(BTN_UP) == LOW) {
    if (settingsMenuIndex > 0) settingsMenuIndex--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (settingsMenuIndex < settingsMenuCount - 1) settingsMenuIndex++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    appState = STATE_MAIN_MENU;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
    return;
  }
  if (okShortFired) {
    if (settingsMenuIndex == 0) {
      timeSetField = 0;
      appState = STATE_SET_TIME;
    } else {
      appState = STATE_MAIN_MENU;
    }
  }
}

void drawSettingsMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(30, 10, "SETTINGS");
  oled.drawHLine(0, 13, 128);

  for (int i = 0; i < settingsMenuCount; i++) {
    int y = 28 + i * 14;
    if (i == settingsMenuIndex) {
      oled.drawBox(0, y - 9, 128, 11);
      oled.setDrawColor(0);
      oled.drawStr(6, y, settingsMenuItems[i]);
      oled.setDrawColor(1);
    } else {
      oled.drawStr(6, y, settingsMenuItems[i]);
    }
  }
  oled.sendBuffer();
}

// =========================== INFO ================================

void handleInfo() {
  if (digitalRead(BTN_LEFT) == LOW) {
    appState = STATE_MAIN_MENU;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
    return;
  }
  if (okShortFired) {
    appState = STATE_MAIN_MENU;
  }
}

void drawInfo() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(45, 10, "INFO");
  oled.drawHLine(0, 13, 128);

  oled.drawStr(5, 26, "Diamond eyes calc");
  oled.drawStr(5, 38, "Firmware v1.0");
  oled.drawStr(5, 50, "ESP32 + SSD1306");
  oled.drawStr(5, 62, "LEFT/OK: back");

  oled.sendBuffer();
}

// =========================== CALCULATOR =============================

void readButtons() {
  if (digitalRead(BTN_UP) == LOW) {
    if (cursorRow > 0) cursorRow--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (cursorRow < 3) cursorRow++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    if (cursorCol > 0) cursorCol--;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
  }
  if (digitalRead(BTN_RIGHT) == LOW) {
    if (cursorCol < 3) cursorCol++;
    while (digitalRead(BTN_RIGHT) == LOW) delay(10);
  }
}

void screen() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawHLine(0, 12, 128);
  oled.drawStr(2, 10, expression.c_str());

  for (int x = 0; x <= 128; x += 32) oled.drawVLine(x, 13, 51);
  for (int y = 13; y <= 64; y += 12) oled.drawHLine(0, y, 128);

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
    hasOperator = false;
    hasDecimal = false;
  } else if (key == '=') {
    calculate();
  } else if (key == '+' || key == '-' || key == '*' || key == '/') {
    if (expression.startsWith("=")) {
      expression = expression.substring(1);
      hasOperator = false;
      hasDecimal = false;
    }
    if (expression.length() == 0) {
      if (key == '-') expression += key;
    } else if (!hasOperator) {
      expression += key;
      hasOperator = true;
      hasDecimal = false;
    }
  } else if (key != ' ') {
    if (expression.startsWith("=")) {
      expression = "";
      hasOperator = false;
      hasDecimal = false;
    }
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
      hasOperator = false;
      hasDecimal = false;
      return;
    }
    result = a / b;
  }

  expression = "=" + formatResult(result);
  hasOperator = false;
  hasDecimal = false;
}

// Целые - без хвоста нулей, дробные - без лишних нулей
String formatResult(double val) {
  if (isnan(val) || isinf(val)) return "Error";

  char buf[32];
  if (fabs(val - round(val)) < 0.0001) {
    snprintf(buf, sizeof(buf), "%.0f", round(val));
    return String(buf);
  }

  snprintf(buf, sizeof(buf), "%.4f", val);
  String s = String(buf);
  while (s.endsWith("0")) s.remove(s.length() - 1);
  if (s.endsWith(".")) s.remove(s.length() - 1);
  return s;
}

void handleCalculatorOk() {
  if (okLongFired) {
    if (cursorRow == 3 && cursorCol == 1) { // "0" -> точка
      if (!hasDecimal) {
        expression += ".";
        hasDecimal = true;
      }
    } else if (cursorRow == 3 && cursorCol == 0) { // "C" -> главное меню
      appState = STATE_MAIN_MENU;
      return;
    } else if (cursorRow == 3 && cursorCol == 2) { // "=" -> меню функций
      sciMenuIndex = 0;
      appState = STATE_SCI_MENU;
      return;
    }
    return;
  }

  if (okShortFired) selectKey();
}

// ======================= SCIENTIFIC FUNCTIONS =======================

void handleSciMenu() {
  if (digitalRead(BTN_UP) == LOW) {
    if (sciMenuIndex > 0) sciMenuIndex--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (sciMenuIndex < sciMenuCount - 1) sciMenuIndex++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }
  if (digitalRead(BTN_LEFT) == LOW) {
    appState = STATE_CALCULATOR;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
    return;
  }
  if (okShortFired) selectSciFunction();
}

void drawSciMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(28, 10, "SCIENTIFIC FUNCTIONS");
  oled.drawHLine(0, 13, 128);

  int visibleItems = 4;
  int startIndex = 0;
  if (sciMenuIndex >= visibleItems) startIndex = sciMenuIndex - visibleItems + 1;

  for (int i = 0; i < visibleItems; i++) {
    int itemIndex = startIndex + i;
    if (itemIndex >= sciMenuCount) break;
    int y = 25 + i * 12;

    if (itemIndex == sciMenuIndex) {
      oled.drawBox(0, y - 9, 128, 11);
      oled.setDrawColor(0);
      oled.drawStr(6, y, sciMenuItems[itemIndex]);
      oled.setDrawColor(1);
    } else {
      oled.drawStr(6, y, sciMenuItems[itemIndex]);
    }
  }

  oled.sendBuffer();
}

void selectSciFunction() {
  String choice = sciMenuItems[sciMenuIndex];
  if (choice == "Back") {
    appState = STATE_CALCULATOR;
    return;
  }
  applySciFunction(choice);
  appState = STATE_CALCULATOR;
}

// Применяет функцию к последнему введённому числу
void applySciFunction(String func) {
  String numStr = expression;
  if (numStr.startsWith("=")) numStr = numStr.substring(1);

  int opPos = -1;
  for (int i = 1; i < numStr.length(); i++) {
    char c = numStr[i];
    if (c == '+' || c == '-' || c == '*' || c == '/') {
      opPos = i;
      break;
    }
  }
  if (opPos != -1) numStr = numStr.substring(opPos + 1);

  double x = numStr.toDouble();
  double result = 0;
  bool err = false;

  if (func == "sin") {
    result = sin(x * PI / 180.0);
  } else if (func == "cos") {
    result = cos(x * PI / 180.0);
  } else if (func == "tan") {
    double c = cos(x * PI / 180.0);
    if (fabs(c) < 1e-9) err = true; else result = sin(x * PI / 180.0) / c;
  } else if (func == "ctg") {
    double s = sin(x * PI / 180.0);
    if (fabs(s) < 1e-9) err = true; else result = cos(x * PI / 180.0) / s;
  } else if (func == "sqrt(x)") {
    if (x < 0) err = true; else result = sqrt(x);
  } else if (func == "x^2") {
    result = x * x;
  }

  expression = err ? "Error" : ("=" + formatResult(result));
  hasOperator = false;
  hasDecimal = false;
}
