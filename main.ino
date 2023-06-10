#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <vector>

const char* ssid = "UPCED7EFB8";        // type your WiFi name
const char* password = "tFax8Er3yycw";  // type your WiFi password

#define TFT_BG tft.color565(0, 5, 0)
#define TFT_TEXT tft.color565(255, 240, 93)
#define TFT_HEIGHT 170
#define TFT_WIDTH 320
#define DELAY_MS 30000

#define SQUARE_SIZE 15

// chose you url from https://till.mabe.at/rbl/?line=102&station=4909
#define URL "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang&rbl=1366"

std::pair<String, String> splitString(String inputString);
String getJson();

TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  delay(1);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi");
  screenInit();
}

void loop() {
  std::vector<int> countdownVector;
  String name;     // name tram lik 2, 72, D etc.
  String towards;  // tram directions.

  if (WiFi.status() == WL_CONNECTED) {
    // Parse JSON response
    {
      String response = getJson();
      DynamicJsonDocument jsonDocument(2048*2);
      DeserializationError error = deserializeJson(jsonDocument, response, DeserializationOption::NestingLimit(30));
      if (error) {
        Serial.println("Failed to parse JSON response.");
        Serial.println(error.c_str());
        screenInit();  // draw empty screen
        delay(DELAY_MS);
        return;
      }

      // Extract countdown values
      JsonObject line = jsonDocument["data"]["monitors"][0]["lines"][0];
 
      JsonArray countdownArray = line["departures"]["departure"];
      name = line["name"].as<String>();
      towards = line["towards"].as<String>();
      // currently not used
      String trafficInfos = jsonDocument["data"]["trafficInfos"][0]["description"].as<String>();

      for (const auto& departure : countdownArray) {
        int countdown = departure["departureTime"]["countdown"].as<int>();
        countdownVector.push_back(countdown);
      }
    }
    // Print countdowns.
    {
      Serial.print("Countdown values: ");
      for (auto& countdown : countdownVector) {
        Serial.print(countdown);
        Serial.print(" ");
      }
      Serial.println();
    }
  }

  unsigned int row_count = 2;

  for (int lineIndex = 0; lineIndex < min(countdownVector.size(), row_count); lineIndex++) {
    int dy = (TFT_HEIGHT / 2) * lineIndex;

    // Print name line.
    tft.setTextSize(1);
    tft.setTextColor(TFT_TEXT, TFT_BG);
    tft.setCursor(10, 25 + dy, 6);
    tft.print(name);

    // Print name station.
    tft.setTextSize(1);
    std::pair<String, String> towards_lines_pair = splitString(towards);
    tft.setCursor(48, 21 + dy, 4);
    tft.println(towards_lines_pair.first);
    tft.setCursor(48, 43 + dy, 4);
    tft.println(towards_lines_pair.second);

    // Print countdown.
    tft.setTextSize(1);
    tft.setTextColor(TFT_TEXT, TFT_BG);
    tft.setCursor(260, 25 + dy, 6);
    int countdown = countdownVector[lineIndex];
    if (countdown == 0) {
      tft.print("     ");
    } else if (countdown < 10) {
      tft.print("  ");
      tft.print(countdown);
    } else {
      tft.print(countdown);
    }
  }

  int c_down_1 = 0;
  int c_down_2 = 0;
  
  if (countdownVector.size() >= 1) {
    c_down_1 = countdownVector[0];
  }
  if (countdownVector.size() >= 2) {
    c_down_2 = countdownVector[1];
  }

  if (!c_down_1 || !c_down_2) {
    int dx = 280;
    int dy = 30;
    int dy_line = TFT_HEIGHT / 2;

    for (int i = 0; i < DELAY_MS; i += 2000) {
      if (c_down_1 == 0 && countdownVector.size() >= 1) {
        drawZero(dx, dy, true);
      }
      if (c_down_2 == 0 && countdownVector.size() >= 2) {
        drawZero(dx, dy + dy_line, true);
      }
      delay(1001);
      if (c_down_1 == 0 && countdownVector.size() >= 1) {
        drawZero(dx, dy, false);
      }
      if (c_down_2 == 0 && countdownVector.size() >= 2) {
        drawZero(dx, dy + dy_line, false);
      }
      delay(1001);
    }
    if (c_down_1 == 0 && countdownVector.size() >= 1) {
      tft.fillRect(dx, dy, SQUARE_SIZE * 2, SQUARE_SIZE * 2, TFT_BG);
    }
    if (c_down_2 == 0 && countdownVector.size() >= 2) {
      tft.fillRect(dx, dy + dy_line, SQUARE_SIZE * 2, SQUARE_SIZE * 2, TFT_BG);
    }
    return;
  }
  delay(DELAY_MS);  // Delay 20 seconds
}

void drawZero(int x, int y, bool b) {
  int square_fg = SQUARE_SIZE - 1;
  int offset = b ? SQUARE_SIZE : 0;

  tft.fillRect(x, y, SQUARE_SIZE * 2, SQUARE_SIZE * 2, TFT_BG);
  tft.fillRect(x + offset, y, square_fg, square_fg, TFT_TEXT);
  tft.fillRect(x + (offset ^ SQUARE_SIZE), y + SQUARE_SIZE, square_fg, square_fg, TFT_TEXT);
}

String getJson() {
  HTTPClient http;
  String url = URL;

  Serial.println(url);
  Serial.println("Reading data...");

  http.begin(url);
  int httpCode = http.GET();

  Serial.print("HTTP status code: ");
  Serial.println(httpCode);

  String payload;
  if (httpCode <= 0) {
    Serial.println("Error on HTTP request");
    payload = http.getString();
    Serial.println(payload);
  } else if (httpCode == HTTP_CODE_OK) {
    payload = http.getString();
  }
  http.end();

  Serial.print("Response: ");
  Serial.println(payload);
  return payload;
}


std::pair<String, String> splitString(String inputString) {
  int max_char_in_line = 17;

  String firstPart = "";
  String secondPart = "";

  if (inputString.length() > max_char_in_line) {
    int spaceIndex = inputString.lastIndexOf(' ', max_char_in_line);
    int dashIndex = inputString.lastIndexOf('-', max_char_in_line);

    if (spaceIndex > dashIndex) {
      firstPart = inputString.substring(0, spaceIndex);
      secondPart = inputString.substring(spaceIndex + 1);
    } else if (dashIndex > spaceIndex) {
      firstPart = inputString.substring(0, dashIndex);
      secondPart = inputString.substring(dashIndex + 1);
    }
  }

  return std::make_pair(firstPart, secondPart);
}

void screenInit() {
  tft.init();
  tft.setRotation(1);  // rotate 1 is equal 90 degree
  tft.fillScreen(TFT_BG);

  // Draw middle line
  int middle_line_height = 4;
  int y_coordinate = (TFT_HEIGHT - middle_line_height) / 2;
  tft.setCursor(0, 0, 4);
  tft.fillRect(0, y_coordinate, TFT_WIDTH, middle_line_height, TFT_BLACK);
}
