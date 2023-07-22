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

// chose you RBL from https://till.mabe.at/rbl/?line=102&station=4909
#define URL "https://www.wienerlinien.at/ogd_realtime/monitor?activateTrafficInfo=stoerunglang&rbl=1366"

const std::pair<String, String> splitString(String inputString);
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
  String name;         // Line name 2, 72, D etc.
  String towards;      // Tram directions.
  String description;  // Line alarm

  // Parse JSON response
  {
    String response = getJson();
    DynamicJsonDocument root(2048 * 4);
    DeserializationError error = deserializeJson(root, response, DeserializationOption::NestingLimit(32));
    if (error) {
      Serial.println("Failed to parse JSON response.");
      Serial.println(error.c_str());
      screenInit();  // draw empty screen
      delay(DELAY_MS);
      return;
    }

    /*
      root
      |- data
      |  |- trafficInfos
      |  |  |- trafficInfo[]
      |  |  |  |- description[0]
      |  |- monitors[]
      |  |  |- monitor[0]
      |  |  |  |- lines[]
      |  |  |  |  |- line[0]
      |  |  |  |  |  |- name
      |  |  |  |  |  |- towards
      |  |  |  |  |  |- departures
      |  |  |  |  |  |  |- departure
    */
    JsonObject data = root["data"];
    JsonArray trafficInfos = data["trafficInfos"];
    JsonObject trafficInfo = trafficInfos[0];
    description = trafficInfo["description"].as<String>();
    JsonArray monitors = data["monitors"];
    JsonObject moniror = monitors[0];
    JsonArray lines = moniror["lines"];
    JsonObject line = lines[0];
    name = line["name"].as<String>();
    towards = line["towards"].as<String>();
    JsonObject departures = line["departures"];
    JsonArray departure = departures["departure"];

    for (const auto& i : departure) {
      JsonObject departureTime = i["departureTime"];
      int countdown = departureTime["countdown"].as<int>();
      countdownVector.push_back(countdown);
    }
  }

  // Print countdowns.
  Serial.print("Countdown values: ");
  for (auto& countdown : countdownVector) {
    Serial.print(countdown);
    Serial.print(" ");
  }
  Serial.println();

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
  if (WiFi.status() != WL_CONNECTED) { return ""; }

  HTTPClient http;
  String url = URL;

  // Print the URL to the serial port
  Serial.println(url);

  // Start the HTTP request
  http.begin(url);

  // Get the status code of the response
  int httpCode = http.GET();

  // Print the status code to the serial port
  Serial.print("HTTP status code: ");
  Serial.println(httpCode);

  // Check if the request was successful
  if (httpCode <= 0) {
    // The request failed
    Serial.println("Error on HTTP request");
    return "";
  } else if (httpCode == HTTP_CODE_OK) {
    // The request was successful
    // Get the response payload as a string
    String payload = http.getString();
    return payload;
  }
}


const std::pair<String, String> splitString(String inputString) {
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
  else
  {
    firstPart = inputString;
  }

  return std::make_pair(firstPart, secondPart);
}

void screenInit() {
  // Initialize the display
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BG);

  // Draw middle line
  const int middle_line_height_px = 4;
  int y_coordinate_middle = (tft.height() - middle_line_height_px) / 2;

  // Use a single function call to draw the line
  tft.drawFastHLine(0, y_coordinate_middle, tft.width(), TFT_BLACK);
}
