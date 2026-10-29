#include <Wire.h>
#include <U8g2lib.h>
#include "anonymous_logo.h"
#include <WiFi.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

// значения порога: 0 (самый низкий, ~2.43V) ... 7 (самый высокий, ~2.76V)
// чем меньше число — тем ниже порог срабатывания (более терпимо к просадке)
void setBrownoutThreshold(uint8_t level) {
  uint32_t reg = READ_PERI_REG(RTC_CNTL_BROWN_OUT_REG);
  reg = (reg & ~(0x7 << 16)) | (level << 16);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, reg);
}

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
const int wifiMenuCount = 5;
const unsigned long debounceDelay = 50;
const char* wifiMenuItems[wifiMenuCount] = {
  "Scan Networks","Deauth","Evil Portal","Beacon Spam","Back"
  };

int cursorRow = 0;
int cursorCol = 0;
int lastReading = HIGH;
int wifiScanResultCount = -2; // -2 = ещё не сканировали
int wifiScanScroll = 0;
int wifiMenuIndex = 0;

bool LastPowerState = HIGH;
bool PowerState;
bool ScreenState = true;
bool OkPressed = false;
bool lastOKState = HIGH;
bool longPressHandled = false;
bool menuOpen = false;
bool toolsLoaded = false;
bool wifiOpen = false;
bool wifiScanView = false;
bool wifiScanRunning = false;

unsigned long lastDebounceTime = 0;
unsigned long pressStart = 0;

String expression = "";

void setup() {
  setBrownoutThreshold(2);
  setCpuFrequencyMhz(160);
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

  esp_reset_reason_t reason = esp_reset_reason();
  String reasonText;
  switch (reason) {
    case ESP_RST_POWERON:   reasonText = "POWERON";   break;
    case ESP_RST_EXT:       reasonText = "EXT RESET";  break;
    case ESP_RST_SW:        reasonText = "SW RESET";   break;
    case ESP_RST_PANIC:     reasonText = "PANIC";      break;
    case ESP_RST_INT_WDT:   reasonText = "INT WDT";    break;
    case ESP_RST_TASK_WDT:  reasonText = "TASK WDT";   break;
    case ESP_RST_WDT:       reasonText = "WDT";        break;
    case ESP_RST_BROWNOUT:  reasonText = "BROWNOUT";   break;
    case ESP_RST_SDIO:      reasonText = "SDIO";       break;
    default:                reasonText = "OTHER:" + String((int)reason); break;
  }
  if (reason != ESP_RST_POWERON) {
    oled.clearBuffer();
    oled.drawStr(5, 25, "RESET REASON:");
    oled.drawStr(5, 45, reasonText.c_str());
    oled.sendBuffer();
    delay(4000);
  }
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
  if (wifiScanView) {
    drawWifiScanResults();
    return;
  }
  if (wifiOpen) {
    drawWifiMenu();
    handleWifiMenuInput();
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
    wifiMenuIndex = 0;
    while (digitalRead(BTN_RIGHT) == LOW) delay(10);
  } else if (digitalRead(BTN_OK) == LOW) {
    while (digitalRead(BTN_OK) == LOW) delay(10);
    menuOpen = false;
    toolsLoaded = false;
    lastOKState = HIGH;
  }
}

void drawWifiMenu() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);

  oled.drawStr(33, 10, "WI-FI TOOLS");
  oled.drawHLine(0, 13, 128);

  int visibleItems = 4;
  int startIndex = 0;

  if (wifiMenuIndex >= visibleItems) {
    startIndex = wifiMenuIndex - visibleItems + 1;
  }

  for (int i = 0; i < visibleItems; i++) {
    int itemIndex = startIndex + i;
    if (itemIndex >= wifiMenuCount) break;

    int y = 25 + i * 12;

    if (itemIndex == wifiMenuIndex) {
      oled.drawBox(0, y - 9, 128, 11);
      oled.setDrawColor(0);
      oled.drawStr(6, y, wifiMenuItems[itemIndex]);
      oled.setDrawColor(1);
    } else {
      oled.drawStr(6, y, wifiMenuItems[itemIndex]);
    }
  }

  oled.sendBuffer();
}

void handleWifiMenuInput() {
  if (digitalRead(BTN_UP) == LOW) {
    if (wifiMenuIndex > 0) wifiMenuIndex--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }

  if (digitalRead(BTN_DOWN) == LOW) {
    if (wifiMenuIndex < wifiMenuCount - 1) wifiMenuIndex++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }

  if (digitalRead(BTN_LEFT) == LOW) {
    wifiOpen = false;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
    return;
  }

  if (digitalRead(BTN_OK) == LOW) {
    while (digitalRead(BTN_OK) == LOW) delay(10);
    selectWifiTool();
  }
}

void selectWifiTool() {
  String choice = wifiMenuItems[wifiMenuIndex];

  if (choice == "Back") {
    wifiOpen = false;
    return;
  }

  if (choice == "Scan Networks") {
    startWifiScan();
    return;
  }
  // Deauth / Evil Portal / Beacon Spam: не реализовано
  showComingSoon(choice);
}

void showComingSoon(String name) {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(20, 30, name.c_str());
  oled.drawStr(15, 45, "Not available");
  oled.sendBuffer();
  delay(1200);
}

void startWifiScan() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(20, 34, "Scanning...");
  oled.sendBuffer();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(300);

  esp_task_wdt_delete(NULL);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  wifiScanResultCount = WiFi.scanNetworks();
  esp_task_wdt_add(NULL);

  wifiScanScroll = 0;
  wifiScanView = true;
}

void drawWifiScanResults() {
  oled.clearBuffer();
  oled.setFont(u8g2_font_6x12_tf);
  oled.drawStr(30, 10, "NETWORKS");
  oled.drawHLine(0, 13, 128);

  if (wifiScanResultCount <= 0) {
    oled.drawStr(15, 35, "No networks found");
  } else {
    int visibleItems = 4;
    for (int i = 0; i < visibleItems; i++) {
      int idx = wifiScanScroll + i;
      if (idx >= wifiScanResultCount) break;

      String line = WiFi.SSID(idx);
      line += " (" + String(WiFi.RSSI(idx)) + ")";

      oled.drawStr(2, 25 + i * 12, line.c_str());
    }
  }

  oled.sendBuffer();

  if (digitalRead(BTN_DOWN) == LOW) {
    if (wifiScanScroll + 4 < wifiScanResultCount) wifiScanScroll++;
    while (digitalRead(BTN_DOWN) == LOW) delay(10);
  }

  if (digitalRead(BTN_UP) == LOW) {
    if (wifiScanScroll > 0) wifiScanScroll--;
    while (digitalRead(BTN_UP) == LOW) delay(10);
  }

  if (digitalRead(BTN_LEFT) == LOW) {
    WiFi.scanDelete();
    wifiScanView = false;
    while (digitalRead(BTN_LEFT) == LOW) delay(10);
  }
}
