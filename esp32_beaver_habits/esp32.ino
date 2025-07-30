#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "time.h"
#include <set>
#include <Fonts/FreeMonoOblique9pt7b.h>

#define TFT_CS    5  // Set to -1 if not used
#define TFT_RST   16  // Reset pin
#define TFT_DC    17  // Data/Commnd pin
#define TFT_SCK   18  // SPI Clock
#define TFT_MOSI  23  // SPI MOSI

// Initialize SPI2
SPIClass spi2(HSPI);

// --- WiFi Credentials ---
const char *ssid = "SSID";
const char *password = "PASSWORD";

// --- Beaver API ---
const char* beaverHost = "beaver_endpoint";
const String username = "username";
const String password_api = "password";

// --- Joystick Pins ---
const int joyX = 34;  // Analog pin
const int joyY = 35;  // Analog pin
const int joyBtn = 32; // Joystick button

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

String authToken;

int displayYear, displayMonth;
unsigned long lastMove = 0;
const int debounceDelay = 500;
int habitIndex = 0;
bool feedbackFlash = false;
uint32_t feedbackTime = 0;
uint16_t feedbackColor = TFT_GREEN;

DynamicJsonDocument habitsDoc(8192); 
JsonArray habits;

void connectWiFi() {
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("Wifi connected");
}

void setupTime() {
  configTime(9 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(1000);
  }
  displayYear = timeinfo.tm_year + 1900;
  displayMonth = timeinfo.tm_mon + 1;
  Serial.println("Time Setup done");
}

String loginGetToken() {
  HTTPClient http;
  http.begin(String(beaverHost)+"/auth/login");
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  http.addHeader("Accept", "application/json");

  String postData = "grant_type=password&username=" + username + "&password=" + password_api;

  int code = http.POST(postData);

  if (code == 200) {
    String response = http.getString();
    DynamicJsonDocument doc(512);
    deserializeJson(doc, response);
    Serial.println("Got Token");
    return doc["access_token"].as<String>();
  }

  Serial.print("Login failed, HTTP code: ");
  Serial.println(code);
  return "";
}

DynamicJsonDocument fetchHabitList(const String& token) {
  HTTPClient http;
  http.begin(String(beaverHost) + "/api/v1/habits");
  http.addHeader("Authorization", "Bearer " + token);
  int code = http.GET();
  DynamicJsonDocument doc(2048);
  if (code == 200) {
    deserializeJson(doc, http.getString());
    Serial.println("fetched Habits");
  }
  return doc;
}

std::set<int> fetchCompletedDays(const String& token, String habitId, int year, int month) {
  char startDate[11], endDate[11];
  int days = (month == 2) ? (year % 4 == 0 ? 29 : 28) : (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);
  sprintf(startDate, "01-%02d-%04d", month, year);
  sprintf(endDate, "%02d-%02d-%04d", days, month, year);

  String url = String(beaverHost) + "/api/v1/habits/" + habitId + "/completions?date_fmt=%d-%m-%Y&date_start=" + startDate + "&date_end=" + endDate + "&sort=asc";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + token);
  int code = http.GET();

  std::set<int> completed;
  if (code == 200) {
    DynamicJsonDocument doc(2048);
    deserializeJson(doc, http.getString());
    for (JsonVariant v : doc.as<JsonArray>()) {
      String dateStr = v.as<String>();
      int day = dateStr.substring(0, 2).toInt();
      completed.insert(day);
    }
  }
  return completed;
}

void toggleHabitToday(String habitId, std::set<int>& completed) {
  time_t now;
  time(&now);
  struct tm* timeinfo = localtime(&now);
  char today[11];
  sprintf(today, "%02d-%02d-%04d", timeinfo->tm_mday, timeinfo->tm_mon + 1, timeinfo->tm_year + 1900);

  String url = String(beaverHost) + "/api/v1/habits/" + habitId + "/completions";
  HTTPClient http;
  http.begin(url);
  http.addHeader("Authorization", "Bearer " + authToken);
  http.addHeader("Content-Type", "application/json");
  DynamicJsonDocument doc(256);
  doc["date"] = today;
  doc["date_fmt"]="%d-%m-%Y";
  String body;

  if (completed.count(timeinfo->tm_mday)) {
    doc["done"]=false;
    serializeJson(doc, body);
    int code = http.POST(body);
    feedbackColor = TFT_RED;
  } else {
    doc["done"]=true;
    serializeJson(doc, body);
    int code = http.POST(body);
    feedbackColor = TFT_GREEN;
  }
  http.end();
  feedbackFlash = true;
  feedbackTime = millis();
}

int getWeekday(int year, int month, int day) {
  tm timeinfo = {};
  timeinfo.tm_year = year - 1900;
  timeinfo.tm_mon = month - 1;
  timeinfo.tm_mday = day;
  mktime(&timeinfo);
  return timeinfo.tm_wday; // 0=Sun
}

void drawCalendar(int year, int month, int today, std::set<int>& completed, String habitName) {
  tft.fillScreen(TFT_BLACK);
  tft.setFont(&FreeMonoOblique9pt7b);
  tft.setTextColor(TFT_ORANGE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.printf("%s", habitName.c_str());
  tft.setCursor(180, 10);
  tft.setTextColor(TFT_WHITE);
  tft.printf("%02d/%04d", month, year);
   
  int boxW = 40, boxH = 30, startX = 10, startY = 30;
  const char* days[7] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};

  for (int i = 0; i < 7; i++){
    tft.setCursor(startX + i * boxW, startY);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.print(days[i]);
  }
  int startDay = getWeekday(year, month, 1);
  int daysInMonth = (month == 2) ? (year % 4 == 0 ? 29 : 28) : (month == 4 || month == 6 || month == 9 || month == 11 ? 30 : 31);

  int col = 0, row = 1;
  for (int d = 1; d <= daysInMonth; d++) {
    col = (startDay + d - 1) % 7;
    row = (startDay + d - 1) / 7 + 1;
    int x = startX + col * boxW;
    int y = startY + row * boxH;
    if (d == today)
      tft.drawRect(x, y, boxW - 2, boxH - 2, TFT_DARKGREY);
    if (completed.count(d))
      tft.fillRect(x, y, boxW - 2, boxH - 2, TFT_ORANGE);
    else
      tft.drawRect(x, y, boxW - 2, boxH - 2, TFT_WHITE);
    tft.setCursor(x + boxW/2 -10, y + boxH/2);
    tft.setTextColor(TFT_WHITE);
    tft.setTextSize(1);
    tft.print(d);
  }
  if (feedbackFlash && millis() - feedbackTime < 500) {
    tft.fillCircle(300, 10, 6, feedbackColor);
  } else {
    feedbackFlash = false;
  }
}

void updateMonth(int delta) {
  displayMonth += delta;
  if (displayMonth > 12) {
    displayMonth = 1;
    displayYear++;
  } else if (displayMonth < 1) {
    displayMonth = 12;
    displayYear--;
  }
}

void updateHabitIndex(int delta) {
  habitIndex += delta;
  if (habitIndex < 0) habitIndex = habits.size() - 1;
  if (habitIndex >= habits.size()) habitIndex = 0;
}

void checkJoystick() {
  int xVal = analogRead(joyX);
  int yVal = analogRead(joyY);
  int btnVal = digitalRead(joyBtn);
  unsigned long now = millis();

  if (now - lastMove > debounceDelay) {
    if (xVal < 1000) {
      updateMonth(-1);
      lastMove = now;
      displayHabits();
    } else if (xVal > 3000) {
      updateMonth(1);
      lastMove = now;
      displayHabits();
    } else if (yVal < 1000) {
      updateHabitIndex(-1);
      lastMove = now;
      displayHabits();
    } else if (yVal > 3000) {
      updateHabitIndex(1);
      lastMove = now;
      displayHabits();
    } else if (btnVal == LOW) {
      if (habits.size() > 0) {
        String id = habits[habitIndex]["id"].as<String>();
        std::set<int> completed = fetchCompletedDays(authToken, id, displayYear, displayMonth);
        toggleHabitToday(id, completed);
        lastMove = now;
      }
      displayHabits();
    }
  }
}

void displayHabits() {
  struct tm timeinfo;
  getLocalTime(&timeinfo);
  int todayDay = timeinfo.tm_mday;

  if (habits.size() == 0) return;

  JsonObject h = habits[habitIndex];
  String name = h["name"].as<String>();
  String id = h["id"].as<String>();
  std::set<int> completed = fetchCompletedDays(authToken, id, displayYear, displayMonth);
  drawCalendar(displayYear, displayMonth, todayDay, completed, name);
}


void setup() {
  Serial.begin(115200);
  // Initialize SPI
  spi2.begin(TFT_SCK, -1, TFT_MOSI, TFT_CS);

  // Initialize display
  tft.init(240, 320);  // Adjust resolution if needed
  tft.setRotation(1);  // Adjust orientation (0-3)
  connectWiFi();
  setupTime();
  authToken = loginGetToken();
  pinMode(joyX, INPUT);
  pinMode(joyY, INPUT);
  pinMode(joyBtn, INPUT_PULLUP);

  habitsDoc = fetchHabitList(authToken);
  habits = habitsDoc.as<JsonArray>();
  displayHabits();
}

void loop() {
  checkJoystick();
}

