#include <Wire.h>
#include <U8g2lib.h>
#include <math.h>
#include "esp_sleep.h"
#include "driver/rtc_io.h"
#include "anonymous_logo.h"

U8G2_SSD1306_128X64_NONAME_F_HW_I2C oled(U8G2_R0, U8X8_PIN_NONE);

// --- Pins ---
const int BTN_UP = 32;
const int BTN_DOWN = 33;
const int BTN_LEFT = 25;
const int BTN_RIGHT = 26;
const int BTN_OK = 27;
// ВАЖНО: GPIO18 не умеет будить ESP32 из deep sleep (не RTC-пин).
// Кнопку питания нужно перевесить на GPIO4 (RTC-способный пин), иначе
// "настоящее" выключение/включение работать не будет.
const int BTN_POWER = 4;
const int Blue_LED = 2;

// --- Calculator layout ---
const int rowy1 = 60;
const int rowy2 = 47;
const int rowy3 = 35;
const int rowy4 = 23;
const int rowx1 = 15;
const int rowx2 = 45;
const int rowx3 = 78;
const int rowx4 = 110;

// --- App states ---
enum AppState { STATE_MAIN_MENU, STATE_SET_TIME, STATE_CALCULATOR, STATE_SCI_MENU };
AppState appState = STATE_MAIN_MENU;

// --- Calculator state ---
int cursorRow = 0;
int cursorCol = 0;
bool lastOKState = HIGH;
bool longPressHandled = false;
unsigned long pressStart = 0;
String expression = "";
bool hasOperator = false;
bool hasDecimal = false;

// --- Main menu state ---
int mainMenuIndex = 0; // 0 = Calculator, 1 = MP3 Player

// --- Scientific menu state ---
const int sciMenuCount = 7;
const char* sciMenuItems[sciMenuCount] = {
  "sin", "cos", "tan", "cot", "sqrt(x)", "x^2", "Back"
};
int sciMenuIndex = 0;

// --- Clock state (keeps ticking across deep sleep thanks to RTC memory,
// but still needs to be set once after a full power loss) ---
RTC_DATA_ATTR int rtcHours = 12;
RTC_DATA_ATTR int rtcMinutes = 0;
int clockSeconds = 0;
unsigned long lastSecMillis = 0;
int timeSetField = 0; // 0 = hours, 1 = minutes

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
  oled.drawXBMP(0, 0, 128, 64, anonymous_logo);
  oled.sendBuffer();
  delay(3500);

  for (int i = 0; i < 4; i++) {
    oled.clearBuffer();
    oled.setFont(u8g2_font_6x12_tf);
    oled.drawStr(32, 25, "CALCULATOR");
    oled.drawHLine(10, 32, 108);

    if (i == 0) oled.drawStr(32, 48, "Loading");
    else if (i == 1) oled.drawStr(32, 48, "Loading.");
    else if (i == 2) oled.drawStr(32, 48, "Loading..");
    else oled.drawStr(32, 48, "Loading...");

    oled.sendBuffer();
    delay(350);
  }
  digitalWrite(Blue_LED, LOW);
}

void loop() {
  checkPowerButton();
  updateClock();

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
      readButtons();
      handleOkButton();
      screen();
      break;
    case STATE_SCI_MENU:
      handleSciMenu();
      drawSciMenu();
      break;
  }
}

// ======================= POWER (real on/off) =======================

void checkPowerButton() {
  if (digitalRead(BTN_POWER) == LOW) {
    delay(30); // debounce
    if (digitalRead(BTN_POWER) == LOW) {
      while (digitalRead(BTN_POWER) == LOW) delay(10); // ждём отпускания
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
  esp_sleep_enable_ext0_wakeup((gpio_num_t)BTN_POWER, 0); // будим по LOW

  esp_deep_sleep_start();
  // сюда управление не вернётся: следующее нажатие кнопки = полная
  // перезагрузка и заново setup() -> bootScreen() -> главное меню
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
  if (digitalRead(BTN_OK) == LOW) {
    while (digitalRead(BTN_OK) == LOW) delay(10);
    clockSeconds = 0;
    lastSecMillis = millis();
    appState = STATE_MAIN_MENU;
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

  oled.drawStr(2, 62, "UP/DN LR OK:save");
  oled.sendBuffer();
}

// =========================== MAIN MENU ==============================

void handleMainMenu() {
  if (digitalRead(BTN_UP) == LOW) {
    if (mainMenuIndex > 0) mainMenuIndex--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }
  if (digitalRead(BTN_DOWN) == LOW) {
    if (mainMenuIndex < 1) mainMenuIndex++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }

  if (digitalRead(BTN_OK) == LOW) {
    unsigned long start = millis();
    bool isLong = false;
    while (digitalRead(BTN_OK) == LOW) {
      if (millis() - start >= 600) {
        isLong = true;
        break;
      }
      delay(10);
    }
    while (digitalRead(BTN_OK) == LOW) delay(10); // ждём отпускания

    if (isLong) {
      timeSetField = 0;
      appState = STATE_SET_TIME;
      return;
    }

    if (mainMenuIndex == 0) {
      expression = "";
      hasOperator = false;
      hasDecimal = false;
      appState = STATE_CALCULATOR;
    } else {
      showComingSoonMP3();
    }
  }
}

void drawMainMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  char timeStr[6];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d", rtcHours, rtcMinutes);
  oled.drawStr(2, 10, "MAIN MENU");
  oled.drawStr(96, 10, timeStr);
  oled.drawHLine(0, 13, 128);

  const char* items[2] = {"Calculator", "MP3 Player"};
  for (int i = 0; i < 2; i++) {
    int y = 32 + i * 20;
    if (i == mainMenuIndex) {
      oled.drawRFrame(10, y - 12, 108, 18, 3);
    }
    oled.drawStr(20, y, items[i]);
    if (i == 1) oled.drawStr(85, y, "(soon)");
  }

  oled.drawStr(2, 62, "Hold OK: set clock");
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
      expression = expression.substring(1); // продолжаем от результата
      hasOperator = false;
      hasDecimal = false;
    }
    if (expression.length() == 0) {
      if (key == '-') expression += key; // разрешаем ведущий минус
      // другие знаки при пустом вводе игнорируем
    } else if (!hasOperator) {
      expression += key;
      hasOperator = true;
      hasDecimal = false; // сбрасываем для второго числа
    }
    // если знак уже есть - повторное нажатие игнорируется
  } else if (key != ' ') { // цифры
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

// Красивое форматирование: целые без хвоста нулей, дробные - без лишних нулей
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

void handleOkButton() {
  bool currentOKState = digitalRead(BTN_OK);

  if (currentOKState == LOW && lastOKState == HIGH) {
    pressStart = millis();
    longPressHandled = false;
  }

  if (currentOKState == LOW && millis() - pressStart >= 600 && !longPressHandled) {
    if (cursorRow == 3 && cursorCol == 1) {
      // долгое нажатие на "0" - точка
      if (!hasDecimal) {
        expression += ".";
        hasDecimal = true;
      }
      longPressHandled = true;
    } else if (cursorRow == 3 && cursorCol == 0) {
      // долгое нажатие на "C" - выход в главное меню
      longPressHandled = true;
      while (digitalRead(BTN_OK) == LOW) delay(10);
      lastOKState = HIGH;
      appState = STATE_MAIN_MENU;
      return;
    } else if (cursorRow == 3 && cursorCol == 2) {
      // долгое нажатие на "=" - меню функций
      longPressHandled = true;
      while (digitalRead(BTN_OK) == LOW) delay(10);
      lastOKState = HIGH;
      sciMenuIndex = 0;
      appState = STATE_SCI_MENU;
      return;
    }
  }

  if (currentOKState == HIGH && lastOKState == LOW) {
    if (!longPressHandled) selectKey();
  }

  lastOKState = currentOKState;
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
  if (digitalRead(BTN_OK) == LOW) {
    while (digitalRead(BTN_OK) == LOW) delay(10);
    selectSciFunction();
  }
}

void drawSciMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(28, 10, "FUNCTIONS");
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

// Применяет функцию к текущему введённому числу (последнему операнду)
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
  } else if (func == "cot") {
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
