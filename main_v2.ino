#include <ArduinoJson.h>  // by Benoit Blanchon 6.21.3
#include <FS.h>
#include <HTTPClient.h>
#include <SPI.h>
#include <SPIFFS.h>
#include <TFT_eSPI.h>     // by Bodmer 2.5.0 installed manual
#include <WiFiManager.h>  // by tzapu 2.0.16
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <limits>
#include <map>
#include <unordered_map>
#include <vector>

//#define DEBUG_SERIAL_WIEN_MONITOR 0
#define TFT_BG tft.color565(0, 5, 0)
#define TFT_TEXT tft.color565(255, 240, 93)
#define URL_BASE \
  "https://www.wienerlinien.at/ogd_realtime/monitor?" \
  "activateTrafficInfo=stoerunglang&rbl="
#define JSON_CONFIG_FILE "/sample_config.json"

// Forvard declaration
class Screen;
class TraficManager;
class TraficClock;

struct Config;
struct ConfigFileHandler;
struct GlobalSettings;
struct Monitor;
struct ScreenEntity;
struct StringDatase;

SemaphoreHandle_t dataMutex;
TFT_eSPI tft = TFT_eSPI();
Screen* p_screen = nullptr;
TraficManager* pTraficManager = nullptr;

/**
 * @struct StringDatabase
 *
 * @brief A utility struct for managing string prompts and instructions.
 */
struct StringDatabase {
private:
  /**
   * @brief The prompt for finding RBL/Stop ID.
   */
  const static constexpr char* RBLPrompt =
    "Find your RBL on https://till.mabe.at/rbl/."
    "<br>Example: \"49\".<br><br><b>RBL/Stop ID:</b>";

  /**
   * @brief The prompt for filtering lines.
   */
  const static constexpr char* LineFilterPrompt =
    "<i>Optional.</i>"
    "Filter the lines to show by comma-separating the directions."
    "If empty, all directions will be shown.<br>"
    "Example: \"D,2,UZ2\".<br><br>"
    "<b>Filter lines:</b>";

public:
  /**
   * @brief Get the Wi-Fi SSID string.
   * @return The Wi-Fi SSID string.
   */
  static String GetWiFissid() {
    return "Wien Transport 🚇⏱️";
  }

  /**
   * @brief Get the instructions text.
   * @return The instructions text.
   */
  static String GetInstructionsText() {
    String instruction_start =
      "Wait a few seconds or:\n"
      "1) Take a smartphone.\n"
      "2) Connect to Wi-Fi:\n"
      " \"";
    String instruction_end = "\"\n3) And follow prompts. (^_^)\n";
    return instruction_start + GetWiFissid() + instruction_end;
  }

  /**
   * @brief Get the RBL/Stop ID prompt.
   * @return The RBL/Stop ID prompt.
   */
  static String GetRBLPrompt() {
    return String(RBLPrompt);
  }

  /**
   * @brief Get the line filter prompt.
   * @return The line filter prompt.
   */
  static String GetLineFilterPrompt() {
    return String(LineFilterPrompt);
  }

  /**
   * @brief Get the prompt for specifying the number of lines to show.
   * @param min The minimum number of lines.
   * @param max The maximum number of lines.
   * @param def The default number of lines.
   * @return The prompt for specifying the number of lines to show.
   */
  static String GetLineCountPrompt(int min, int max, int def) {
    String range = GetFormatRange(min, max);
    String result =
      "How many lines do you want to show at the same time on monitor "
      + range + "? (Default: "
      + String(def) + "). <br>"
                      "Example: \""
      + String(def) + "\".<br><br>"
                      "<b>Lines to show:</b>";
    return result;
  }

private:
  /**
   * @brief Format a range of values as a string.
   * @param min The minimum value.
   * @param max The maximum value.
   * @return The formatted range string.
   *
   * Example: GetFormatRange(1, 3) returns "1, 2, or 3".
   */
  static String GetFormatRange(int min, int max) {
    if (min > max) {
      // Swap min and max if min is greater than max
      int temp = min;
      min = max;
      max = temp;
    }

    String result = "";

    if (min == max) {
      result += String(min);
    } else if (max - min == 1) {
      result += String(min) + " or " + String(max);
    } else {
      for (int i = min; i < max; i++) {
        result += String(i) + ", ";
      }
      result += "or " + String(max);
    }

    return result;
  }
};

struct Config {
  static const int cnt_min_lines = 1;
  static const int cnt_max_lines = 3;
  static const int cnt_default_lines = 2;

  /**
   * @brief Default constructor.
   *
   * Sets the number of lines to the default of cnt_default_lines.
   */
  explicit Config()
    : cnt_lines(cnt_default_lines) {}

  /**
   * @brief Copy constructor.
   *
   * Copies all of the members of the other Config object to this one.
   *
   * @param other The Config object to copy from.
   */
  Config(const Config& other)
    : cnt_lines(other.cnt_lines),
      lines_filter(other.lines_filter),
      lines_rbl(other.lines_rbl) {}
  /**
   * @brief Assignment operator.
   *
   * Copies all of the members of the other Config object to this one.
   *
   * @param other The Config object to copy from.
   * @return A reference to this Config object.
   */
  Config& operator=(const Config& other) {
    if (this != &other) {
      lines_filter = other.lines_filter;
      lines_rbl = other.lines_rbl;
      cnt_lines = other.cnt_lines;
    }
    return *this;
  }

  /**
   * @brief Equality operator.
   *
   * Compares two Config objects for equality.
   *
   * @param other The Config object to compare to.
   * @return True if the two Config objects are equal, false otherwise.
   */
  bool operator==(const Config& other) const {
    return lines_filter == other.lines_filter 
    && lines_rbl == other.lines_rbl 
    && cnt_lines == other.cnt_lines;
  }
  /**
   * @brief Inequality operator.
   *
   * Compares two Config objects for inequality.
   *
   * @param other The Config object to compare to.
   * @return True if the two Config objects are not equal, false otherwise.
   */
  bool operator!=(const Config& other) const {
    return !(*this == other);
  }

  /**
   * @brief Sets the number of lines.
   *
   * Valid values are from cnt_min_lines to cnt_max_lines.
   * If an invalid value is passed, the default
   * value of cnt_default_lines will be used.
   *
   * @param count The number of lines to set.
   */
  void SetLinesCount(int c) {
    cnt_lines = verifyLinesCountInput(c);
  }
  /**
   * @brief Sets the number of lines.
   *
   * Valid values are from cnt_min_lines to cnt_max_lines.
   * If an invalid value is passed, the default
   * value of cnt_default_lines will be used.
   *
   * @param str The number of lines to set as a string.
   */
  void SetLinesCount(const String& c) {
    cnt_lines = verifyLinesCountInput(c.toInt());
  }

  /**
   * @brief Gets the number of lines as an integer.
   *
   * @return The number of lines.
   */
  int GetLinesCountAsInt() const {
    return cnt_lines;
  }
  /**
   * @brief Gets the number of lines as a string.
   *
   * @return The number of lines as a string.
   */
  const String GetLinesCountAsString() const {
    return String(cnt_lines);
  }

  /**
   * @brief Sets the lines filter.
   *
   * @param str The lines filter to set.
   */
  void SetLinesFilter(const String& str) {
    lines_filter = str;
  }
  /**
   * @brief Gets the lines filter as a string.
   *
   * @return The lines filter as a string.
   */
  const String& GetLinesFilterAsString() const {
    return lines_filter;
  }

  /**
   * @brief Sets the lines RBL.
   *
   * @param str The lines RBL to set.
   */
  void SetLinesRBL(const String& str) {
    lines_rbl = str;
  }
  /**
   * @brief Sets the lines RBL.
   *
   * @param str The lines RBL to set as an integer.
   */
  void SetLinesRBL(int str) {
    lines_rbl = String(str, DEC);
  }
  /**
   * @brief Gets the lines RBL as a string.
   *
   * @return The lines RBL as a string.
   */
  const String& GetLinesRblAsString() const {
    return lines_rbl;
  }
  /**
   * @brief Gets the lines RBL as an integer.
   *
   * @return The lines RBL as an integer.
   */
  int GetLinesRblAsInt() {
    return lines_rbl.toInt();
  }

private:
  /**
   * @brief Verifies that the lines count input is valid.
   *
   * Valid values are from cnt_min_lines to cnt_max_lines.
   * If an invalid value is passed, the default
   * value of cnt_default_lines will be used.
   *
   * @param count The lines count input.
   * @return The lines count input, if it is valid. Otherwise, the default
   * count.
   */
  static int verifyLinesCountInput(int count) {
    switch (count) {
      case cnt_min_lines ... cnt_max_lines:
        return count;
      default:
        return cnt_default_lines;
    }
  }

private:
  ///< The number of lines of text that can be displayed in each idx_row.
  int cnt_lines;

  ///< The raw lines filter. Example "D,2,6".
  String lines_filter;

  ///< The lines RBL.
  String lines_rbl;
};

/**
 * @brief Class for handling configuration files.
 *
 * This class provides functions for saving and loading configuration files to
 * and from SPIFFS.
 */
struct ConfigFileHandler {
  /**
   * @brief Saves the configuration file to SPIFFS.
   *
   * @param cfg The configuration to save.
   */
  static void SaveConfigFile(Config& cfg) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    Serial.println(F("Saving config"));
#endif
    StaticJsonDocument<512> json;
    json["lines_filter"] = cfg.GetLinesFilterAsString();
    json["rbl_id"] = cfg.GetLinesRblAsString();
    json["lines_count"] = cfg.GetLinesCountAsString();

    File file_config = SPIFFS.open(JSON_CONFIG_FILE, "w");
    if (!file_config) {
      Serial.println("failed to open config file for writing");
    }

    serializeJsonPretty(json, Serial);
    if (serializeJson(json, file_config) == 0) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
      Serial.println(F("Failed to write to file"));
#endif
    }
    file_config.close();
  }

  /**
   * @brief Loads the configuration file from SPIFFS.
   *
   * @param cfg The configuration to load into.
   * @return True if the config file was loaded successfully, false otherwise.
   */
  static bool LoadConfigFile(Config& cfg) {
    // clean FS, for testing
    // SPIFFS.format();
    // read configuration from FS json
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    Serial.println("mounting FS...");
#endif

    if (SPIFFS.begin(false) || SPIFFS.begin(true)) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
      Serial.println("mounted file system");
#endif
      if (SPIFFS.exists(JSON_CONFIG_FILE)) {
// file exists, reading and loading
#ifdef DEBUG_SERIAL_WIEN_MONITOR
        Serial.println("reading config file");
#endif
        File configFile = SPIFFS.open(JSON_CONFIG_FILE, "r");
        if (configFile) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
          Serial.println("opened config file");
#endif
          StaticJsonDocument<512> json;
          DeserializationError error = deserializeJson(json, configFile);
          serializeJsonPretty(json, Serial);
          if (!error) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
            Serial.println("\nparsed json");
#endif

            cfg.SetLinesFilter(json["lines_filter"].as<String>());
            cfg.SetLinesRBL(json["rbl_id"].as<String>());
            cfg.SetLinesCount(json["lines_count"].as<String>());
            Serial.println("\nparsed json end");
            return true;
          } else {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
            Serial.println("failed to load json config");
#endif
          }
        }
        configFile.close();
      }
    } else {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
      Serial.println("failed to mount FS");
#endif
    }
    // end read
    return false;
  }

  /**
   * @brief Deletes the configuration file from SPIFFS.
   */
  static void DeleteConfigFile() {
    if (SPIFFS.begin()) {
      if (SPIFFS.exists(JSON_CONFIG_FILE)) {
        if (SPIFFS.remove(JSON_CONFIG_FILE)) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
          Serial.println("Config file successfully deleted");
#endif
        } else {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
          Serial.println("Failed to delete config file");
#endif
        }
      } else {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
        Serial.println("Config file does not exist");
#endif
      }
      SPIFFS.end();
    } else {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
      Serial.println("Failed to initialize SPIFFS");
#endif
    }
  }
};

struct GlobalSettings {
  ///< Number of lines of text that can be displayed on each idx_row of the
  ///< screen.
  static constexpr int cnt_screen_lines_per_rows = 2;

  ///< The number queue. Indicates how many countdown shows per cycle.
  static constexpr int cnt_shows_countdows = 2;

  ///< Number of seconds to press the soft reset button to restart the device.
  static constexpr int sec_press_soft_resset_button = 5;

  ///< Number of seconds to press the hard reset button to restart the device.
  static constexpr int sec_press_hard_resset_button = 30;

  ///< GPIO pin that the reset button is connected to.
  static constexpr int pin_reset_button = 0;

  ///< Number of milliseconds to delay before resetting the device if an error.
  static constexpr int ms_error_resset_delay = 10 * 1000;

  ///< Number of milliseconds to reset esp.
  static constexpr int ms_reboot_interval = 3 * 60 * 60 * 1000;

  ///< Number of milliseconds to delay between checking the reset button state.
  static constexpr int ms_task_delay_resset_button = 1000;

  ///< Number of milliseconds to delay between updating the data.
  static constexpr int ms_task_delay_data_update = 20 * 1000;

  ///< Number of milliseconds to delay between updating the screen.
  static constexpr int ms_task_delay_screen_update = 10;

  ///< real count down can be faster thet real ms_task_delay_data_update
  ///< this offset time help meke more smoth transition
  static constexpr int ms_additional_time_for_countdown = 100;

  ///< Size of font that show in first second plug in
  static constexpr int size_start_instruction_font = 4;

  ///< how much pixels scroll per frame
  static constexpr int px_scrool_per_frame = 2;

private:
  ///< The configuration object.
  Config config;

  ///< Whether or not the configuration file has been loaded.
  bool is_config_loaded = false;

public:
  /**
   * @brief Gets the global settings.
   *
   * @return A reference to the global settings object.
   */
  const Config& GetConfig() {
    if (!is_config_loaded) {
      is_config_loaded = ConfigFileHandler::LoadConfigFile(config);
    }
    return config;
  }

  /**
   * @brief Sets the global settings.
   *
   * @param new_config The new global settings.
   */
  void SetConfig(const Config& new_config) {
    config = new_config;
    ConfigFileHandler::SaveConfigFile(config);
    is_config_loaded = true;
  }

  /**
   * @brief Deletes the configuration file.
   */
  void DeleteFile() {
    ConfigFileHandler::DeleteConfigFile();
  }
} global_settings;

class SmartWatch {
public:
  SmartWatch(const char* function_name)
    : function_name(function_name), start_millis(millis()) {}

  ~SmartWatch() {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    unsigned long end_millis = millis();
    unsigned long execution_time = end_millis - start_millis;
    Serial.print("Function '");
    Serial.print(function_name);
    Serial.print("' executed in ");
    Serial.print(execution_time);
    Serial.println(" milliseconds.");
#endif
  }

private:
  const char* function_name;
  unsigned long start_millis;
};

struct Monitor {
  String name;         // Line name 2, 72, D etc.
  String towards;      // Tram directions.
  String description;  // Line alarm
  std::vector<int> countdown;
};

void PrintMonitorDegbug(const Monitor& monitor) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
  Serial.print("Name: ");
  Serial.print(monitor.name);
  // Serial.print(" Towards: ");
  // Serial.print(monitor.towards);
  Serial.print(" Description: ");
  Serial.print(monitor.description);

  /*Serial.println("Countdown:");
  for (int i = 0; i < monitor.countdown.size(); i++) {
    Serial.print("  ");
    Serial.print(monitor.countdown[i]);
  }*/
  Serial.println();
#endif
}

void PrintMonitorDegbug(const std::vector<Monitor>& monitors) {
  for (const auto& monitor : monitors) {
    PrintMonitorDegbug(monitor);
  }
}

void PrintResourceUsage();
std::vector<Monitor> DeserilizeJson(const String& json);
String GetJson(const String& rbl_id);

struct ScreenEntity {
  String right_txt;  // Line name 2, 72, D etc.
  std::vector<String> lines;
  String left_txt;
};

/**
 * @brief The `Screen` class represents a screen with multiple rows, each
 * displaying text and countdown information.
 *
 * This class is designed for use with TFT displays and provides methods for
 * setting and drawing text on each idx_row, as well as managing scrolling text
 * and drawing separator lines.
 */
class Screen {
public:
  /**
   * @brief Constructor to initialize a `Screen` object with the specified
   * number of rows and lines per idx_row.
   *
   * @param cnt_rows The number of rows on the screen.
   * @param cnt_lines_in_row The number of lines of text that can be displayed
   * in each idx_row.
   */
  Screen(int cnt_rows, int nLinesInRowCount)
    : px_max_width_name_text(0),
      px_max_width_countdown_text(0),
      px_separate_line_height(3),
      px_margin_lines(2),
      cnt_rows(cnt_rows),
      cnt_lines_in_row(nLinesInRowCount),
      px_margin(8),
      px_min_text_sprite(std::numeric_limits<int>::max()) {
    vec_scrolls_coords.resize(cnt_rows, std::vector<int>(nLinesInRowCount, 0));
    vec_init_scrolls_coords.resize(cnt_rows,
                                   std::vector<bool>(nLinesInRowCount, false));
  }

public:
  /**
   * @brief Sets the content for a specific idx_row on the screen and updates
   * its display.
   *
   * @param monitor The `Monitor` object containing the text and countdown
   * information for the idx_row.
   * @param idx_row The idx_row index to set the content for.
   */
  void SetRow(const ScreenEntity& monitor, int idx_row) {
    if (idx_row > cnt_rows - 1) {
      return;
    }
    const GFXfont* p_font = &FreeSansBold24pt7b;

    // Calculate max Sides text block size.
    int nameTextWidth_px = CalculateFontWidth_px(p_font, monitor.right_txt);
    SetMaxNameTextWidth_px(nameTextWidth_px);
    int countdownTextWidth_px = CalculateFontWidth_px(p_font, monitor.left_txt);
    SetMaxCountdownTextWidth_px(countdownTextWidth_px);

    // Draw Text;
    DrawName(monitor.right_txt, idx_row, p_font);
    DrawCountdown(monitor.left_txt, idx_row, p_font);
    DrawMiddleText(monitor.lines, idx_row);

    // Draw Separet Lines
    drawLines();
  }
  bool IsEnoughSpaceForMiddleText(const String& str) {
    const GFXfont* p_font = &FreeSansBold12pt7b;
    return GetMinTextSprite_px() >= CalculateFontWidth_px(p_font, str);
  }

  void PrintCordDebug() {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    // Print the contents of the vectors
    for (int i = 0; i < vec_scrolls_coords.size(); i++) {
      Serial.print("[");
      for (int j = 0; j < vec_scrolls_coords[i].size(); j++) {
        Serial.print(vec_scrolls_coords[i][j]);
        if (j != vec_scrolls_coords[i].size() - 1) {
          Serial.print(", ");
        }
      }
      Serial.print("]");
    }

    for (int i = 0; i < vec_init_scrolls_coords.size(); i++) {
      Serial.print("[");
      for (int j = 0; j < vec_init_scrolls_coords[i].size(); j++) {
        Serial.print(vec_init_scrolls_coords[i][j]);
        if (j != vec_init_scrolls_coords[i].size() - 1) {
          Serial.print(", ");
        }
      }
      Serial.print("]");
    }
    Serial.println("");
#endif
  }

private:
  /**
   * @brief Converts a vector of German strings to Latin alphabet.
   *
   * Takes a vector of German strings and converts each string from
   * German characters (e.g., ä, Ä, ö, Ö, ü, Ü, ß) to their Latin alphabet
   * equivalents (a, A, o, O, u, U, s). The converted strings are then returned
   * in a new vector.
   *
   * @param vecGerman A vector of German strings to be converted.
   *
   * @return A vector of strings in Latin alphabet.
   */
  std::vector<String> ConvertGermanToLatin(
    const std::vector<String>& vecGerman) {
    std::vector<String> output;
    for (auto& str : vecGerman) {
      output.push_back(ConvertGermanToLatin(str));
      // Serial.println(ConvertGermanToLatin(str));
    }
    return output;
  }

public:
  /**
   * @brief Converts a single German string to Latin alphabet.
   *
   * This function takes a single German string and converts it from German
   * characters (e.g., ä, Ä, ö, Ö, ü, Ü, ß) to their Latin alphabet equivalents
   * (a, A, o, O, u, U, s).
   *
   * @param input The German string to be converted.
   *
   * @return The converted string in Latin alphabet.
   */
  static String ConvertGermanToLatin(String input) {
    // TODO: optimize it.
    input.replace("ä", "a");
    input.replace("Ä", "A");
    input.replace("ö", "o");
    input.replace("Ö", "O");
    input.replace("ü", "u");
    input.replace("Ü", "U");
    input.replace("ß", "ss");
    return input;
  }

  int GetMinTextSprite_px() {
    return px_min_text_sprite;
  }
private:
  /**
   * @brief Draws the countdown text on the screen for a specific idx_row.
   *
   * @param countdown The countdown text to be displayed.
   * @param idx_row The idx_row index where the countdown text should be drawn.
   * @param p_font The pointer to the p_font to be used for rendering the text.
   */
  void DrawCountdown(const String& countdown, int idx_row,
                     const GFXfont* pFont) const {
    int px_font_width = CalculateFontWidth_px(pFont, countdown);

    int px_dx_countdown_margin = tft.width() - px_font_width - px_margin;

    DrawTextOnSprite(countdown, idx_row, px_dx_countdown_margin, 0, pFont,
                     GetMaxCountdownTextWidth_px());
  }
  /**
   * @brief Draws the name text on the screen for a specific idx_row.
   *
   * @param name The name text to be displayed.
   * @param idx_row The idx_row index where the name text should be drawn.
   * @param p_font The pointer to the p_font to be used for rendering the text.
   */
  void DrawName(const String& name, int row, const GFXfont* pFont) const {
    int dxNameMargin_px = px_margin;
    DrawTextOnSprite(name, row, dxNameMargin_px, 0, pFont,
                     GetMaxNameTextWidth_px());
  }
  /**
   * @brief Sets and draws the middle lines and text content for a specific
   * idx_row on the screen.
   *
   * @param vec_text_lines The vector of text lines to be displayed in the
   * idx_row.
   * @param idx_row The idx_row index where the text content should be set and
   * drawn.
   */
  void DrawMiddleText(const std::vector<String>& vec_text_lines, int idx_row) {

    const GFXfont* p_font = &FreeSansBold12pt7b;

    // Calculate dimensions
    auto& vec_scroll_cord = vec_scrolls_coords[idx_row];
    auto& is_init_cords = vec_init_scrolls_coords[idx_row];
    int px_width_countdown = GetMaxCountdownTextWidth_px();
    int px_width_stopcode = GetMaxNameTextWidth_px();
    int px_height_font = CalculatefontHeight_px(p_font);
    // Minus two margins for name and two margins for countdown
    // 4 is count margin around Coundown and Stop code
    int px_width =
      tft.width() - px_width_countdown - px_width_stopcode - px_margin * 4;

    SetMinTextSprite_px(px_width);

    // Create and configure sprite
    TFT_eSprite sprite(&tft);
    sprite.createSprite(px_width, px_height_font);
    // sprite.setTextSize(0);

    int cnt_lines_actual = static_cast<int>(vec_text_lines.size());
    for (int i = 0; i < std::min(cnt_lines_in_row, cnt_lines_actual); ++i) {
      int px_height_full = static_cast<double>(tft.height() / cnt_rows);
      int dy = CalculateLineDistance_px(px_height_full, px_margin_lines,
                                        px_height_font, cnt_lines_in_row, i);
      int px_full_string =
        CalculateFontWidth_px(p_font, vec_text_lines[i].c_str());

      // Reset to the right edge of the screen
      if (vec_scroll_cord[i] < (px_full_string * -1)) {
        vec_scroll_cord[i] = sprite.width();
      }


      if (px_full_string > px_width) {
        //scroll text 2 is mean px per frame
        vec_scroll_cord[i] -= global_settings.px_scrool_per_frame;
        if (!is_init_cords[i] && i > 0 && vec_text_lines[i].length()) {
          // Scroll if text is bigger than available space
          vec_scroll_cord[i] = sprite.width();
          is_init_cords[i] = true;
        }
      } else if (px_full_string < vec_scroll_cord[i]) {
        // reset sscrolls cord
        vec_scroll_cord[i] = 0;
        is_init_cords[i] = false;
      }

      // Draw text on the sprite
      sprite.setTextColor(TFT_TEXT);
      sprite.fillSprite(TFT_BG);
      sprite.setFreeFont(p_font);
      sprite.drawString(vec_text_lines[i].c_str(), vec_scroll_cord[i], 0);

      // Push sprite to display
      int x_cord = px_margin + px_margin + px_width_stopcode;
      int y_cord = dy + ((tft.height() / cnt_rows) * idx_row);
      sprite.pushSprite(x_cord, y_cord);
    }
    sprite.deleteSprite();
    //

    //
  }
  /**
   * @brief Draws text on a sprite and pushes it to the screen at the specified
   * coordinates.
   *
   * @param text The text to be drawn on the sprite.
   * @param idx_row The idx_row index where the text should be displayed.
   * @param x The X-coordinate where the text should be drawn.
   * @param y The Y-coordinate where the text should be drawn.
   * @param p_font The pointer to the p_font to be used for rendering the text.
   */
  void DrawTextOnSprite(const String& text, int idx_row, int x, int y,
                        const GFXfont* p_font, int px_max_width) const {
    const int px_width_sprite = CalculateFontWidth_px(p_font, text);
    const int px_height_font = CalculatefontHeight_px(p_font);
    const int px_height_full = tft.height() / cnt_rows;
    const int dy =
      CalculateLineDistance_px(px_height_full, 0, px_height_font, 1, 0);

    TFT_eSprite sprite(&tft);
    TFT_eSprite bg_left_sprite(&tft);
    TFT_eSprite bg_right_sprite(&tft);
    // TFT_eSprite bg_sprite(&tft);  // Отдельный спрайт для квадратов

    // Set background colors based on DEBUG mode
    uint16_t color_left, color_middle, color_right;
    //#define DEBUG_DrawTextOnSprite
#ifdef DEBUG_DrawTextOnSprite
    leftColor = TFT_GREEN;
    middleColor = TFT_RED;
    rightColor = TFT_BLUE;
#else
    color_left = color_middle = color_right = TFT_BG;
#endif

    // Special symbols that draw manualy
    bool drawTopRightSquare = text == String("◱");
    bool drawBottomLeftSquare = text == String("◳");

    if (drawTopRightSquare || drawBottomLeftSquare) {
      sprite.createSprite(px_height_font, px_height_font);
      sprite.fillSprite(color_middle);
      int px_sqaure_size = (px_height_font / 2) - 6;
      if (drawTopRightSquare) {
        // down left
        sprite.fillRect(px_height_font / 2, 0, px_sqaure_size, px_sqaure_size,
                        TFT_YELLOW);
        // up right
        sprite.fillRect(0, px_height_font / 2, px_sqaure_size, px_sqaure_size,
                        TFT_YELLOW);
      } else if (drawBottomLeftSquare) {
        // left up
        sprite.fillRect(0, 0, px_sqaure_size, px_sqaure_size, TFT_YELLOW);
        // down right
        sprite.fillRect(px_height_font / 2, px_height_font / 2, px_sqaure_size,
                        px_sqaure_size, TFT_YELLOW);
      }

    } else {
      sprite.createSprite(px_width_sprite, px_height_font);
      sprite.fillSprite(color_middle);
      sprite.setTextColor(TFT_TEXT);
      sprite.setFreeFont(p_font);
      sprite.drawString(text, 0, 0);
    }
    // if (px_width_sprite < px_max_width) {
    //  Create and display left background sprite
    bg_left_sprite.createSprite(px_max_width - px_width_sprite, px_height_font);
    bg_left_sprite.fillSprite(color_left);
    int bg_left_x_cord = x - (px_max_width - px_width_sprite);
    int bg_left_y_cord = y + dy + (px_height_full * idx_row);
    bg_left_sprite.pushSprite(bg_left_x_cord, bg_left_y_cord);
    bg_left_sprite.deleteSprite();

    // Create and display right background sprite
    const int bg_right_x = x + px_width_sprite;
    const int bg_right_width = px_max_width - px_width_sprite;
    bg_right_sprite.createSprite(bg_right_width, px_height_font);
    bg_right_sprite.fillSprite(color_right);
    bg_right_sprite.pushSprite(bg_right_x, y + dy + (px_height_full * idx_row));
    bg_right_sprite.deleteSprite();
    //}

    // Create and display text sprite
    /*sprite.createSprite(px_width_sprite, px_height_font);
        sprite.fillSprite(color_middle);
        sprite.setTextColor(TFT_TEXT);
        sprite.setFreeFont(p_font);
        sprite.drawString(text, 0, 0);*/
    sprite.pushSprite(x, y + dy + (px_height_full * idx_row));
    sprite.deleteSprite();
  }

  /**
   * @brief Draws separator lines between rows on the screen.
   */
  void drawLines() const {
    // Create a TFT_eSprite for drawing separator lines
    TFT_eSprite lines_separator(&tft);
    // Create a N-pixel tall sprite
    lines_separator.createSprite(tft.width(), px_separate_line_height);

    for (int i = 1; i < cnt_rows; ++i) {
      int px_height_and_sprite = (tft.height() - (px_separate_line_height * i));
      int y_cord = px_height_and_sprite / cnt_rows * i;
      // Clear the sprite and draw a horizontal line
      lines_separator.fillSprite(TFT_BLACK);
      lines_separator.drawFastHLine(0, 0, tft.width(), TFT_BLACK);

      // Push the sprite to display at the specified Y-coordinate
      lines_separator.pushSprite(0, y_cord);
    }
    // Delete the sprite to free up memory
    lines_separator.deleteSprite();
  }
  /**
   * @brief Sets the maximum px_width of the name text in pixels.
   *
   * @param px_w Width of the name text in pixels to compare with the current
   * maximum.
   */
  void SetMaxNameTextWidth_px(int px_w) {
    if (px_w > px_max_width_name_text) {
      px_max_width_name_text = px_w;
      tft.fillScreen(TFT_BG);
    }
  }
  /**
   * @brief Sets the maximum px_width of the countdown text in pixels.
   *
   * @param px_w The px_width of the countdown text in pixels to compare with
   * the current maximum.
   */
  void SetMaxCountdownTextWidth_px(int px_w) {
    if (px_w > px_max_width_countdown_text) {
      px_max_width_countdown_text = px_w;
      tft.fillScreen(TFT_BG);
    }
  }
  /**
   * @brief Gets the maximum px_width of the countdown text in pixels.
   *
   * @return The maximum px_width of the countdown text in pixels.
   */
  int GetMaxCountdownTextWidth_px() const {
    return px_max_width_countdown_text;
  }
  /**
   * @brief Gets the maximum px_width of the name text in pixels.
   *
   * @return The maximum px_width of the name text in pixels.
   */
  int GetMaxNameTextWidth_px() const {
    return px_max_width_name_text;
  }
  /**
   * @brief Calculates the coordinate position of a line based on the specified
   * parameters.
   *
   * @param bodyLength The total length of the body where lines are positioned.
   * @param segmentMargin The margin between segments.
   * @param segmentLength The length of each segment.
   * @param segmentCount The total number of segments.
   * @param n The index of the line.
   *
   * @return The Coordinate position of the line.
   */
  int CalculateLineDistance_px(double bodyLength, double segmentMargin,
                               double segmentLength, int segmentCount,
                               int n) const {
    // Calculate the length occupied by segments
    double segmentsLength = segmentCount * segmentLength;

    // Calculate the length occupied by margins
    double marginsLength = (segmentCount - 1) * segmentMargin;

    // Add the lengths to get the total
    double totalSegmentsLength = segmentsLength + marginsLength;

    // Calculate the distance from the beginning of the body to the first line
    double x_firstLine = (bodyLength - totalSegmentsLength) / 2;

    // Calculate the final position based on the first line and segment spacing
    double finalPosition = x_firstLine + (n * (segmentLength + segmentMargin));

    // Convert the final position to an integer and return it
    return static_cast<int>(finalPosition);
  }

  /**
   * @brief Calculates the px_width of a text string using the specified p_font.
   *
   * @param p_font The p_font used for rendering the text.
   * @param str The text string to calculate the px_width for.
   *
   * @return The px_width of the text string in pixels.
   */
  int CalculateFontWidth_px(const GFXfont* p_font, const String& str) const {
    // This symbol not exist in p_font this squares draw manuany
    // In squeres Width == Height
    if (str == String("◱") || str == String("◳")) {
      return CalculatefontHeight_px(p_font);  // height == px_width in square
    }

    TFT_eSprite Calculator = TFT_eSprite(&tft);
    Calculator.setFreeFont(p_font);
    int px_w = Calculator.textWidth(str);
    Calculator.deleteSprite();
    return px_w;
  }
  /**
   * @brief Calculates the height of the text when rendered using the specified
   * p_font.
   *
   * This function creates a temporary sprite, sets the provided p_font, and
   * then retrieves the p_font height.
   *
   * @param p_font The p_font used for rendering the text.
   *
   * @return The height of the text when rendered with the specified p_font in
   * pixels.
   */
  int CalculatefontHeight_px(const GFXfont* p_font) const {
    // Getter V1
    // TFT_eSprite Calculator = TFT_eSprite(&tft);
    // Calculator.createSprite(tft.px_width() * 8, tft.height());
    // Calculator.setFreeFont(p_font);
    // return Calculator.px_height_font();

    // Getter V2
    // Check if the height has already been calculated for this p_font
    auto it = font_height_cache.find(p_font);
    if (it != font_height_cache.end()) {
      //   If yes, return the stored value
      return it->second;
    }

    const char* t =
      "abcdefghijklmnopqrstuvwxyz"
      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
      "0123456789";
    TFT_eSprite Calculator = TFT_eSprite(&tft);
    Calculator.setFreeFont(p_font);

    uint16_t len = strlen(t);
    uint16_t maxH = 0;

    for (uint16_t i = 0; i < len; i++) {
      uint16_t unicode = t[i];
      uint16_t gNum = unicode - p_font->first;
      if (gNum >= p_font->last) continue;  // Skip undefined characters

      GFXglyph* glyph = &(((GFXglyph*)p_font->glyph))[gNum];
      uint16_t h = glyph->height;

      if (h > maxH) maxH = h;
    }

    int height = maxH ? maxH + 1 : 0;

    // Сохраняем рассчитанное значение в кэше
    font_height_cache[p_font] = height;

    return height;
  }

  void SetMinTextSprite_px(int px_min) {
    // Serial.println(px_min_text_sprite);
    px_min_text_sprite = min(px_min_text_sprite, px_min);
  }

  ///< The maximum px_width of the name text in pixels.
  int px_max_width_name_text;

  ///< The maximum px_width of the countdown text in pixels.
  int px_max_width_countdown_text;

  ///< The px_width bewwen tables.
  int px_separate_line_height;

  ///< The px_width bewwen tables.
  int px_margin_lines;

  ///< The number of rows on the screen.
  int cnt_rows;

  ///< The number of lines of text that can be displayed in each idx_row.
  int cnt_lines_in_row;

  ///< The margin value for text and components.
  const int px_margin;

  ///< Coordinates for text scrolling in each idx_row.
  std::vector<std::vector<int>> vec_scrolls_coords;
  std::vector<std::vector<bool>> vec_init_scrolls_coords;

  int px_min_text_sprite;
  mutable std::map<const GFXfont*, int> font_height_cache;

public:
  void FullResetScroll() {
    for (size_t i = 0; i < vec_scrolls_coords.size(); ++i) {
      for (size_t j = 0; j < vec_scrolls_coords[i].size(); ++j) {
        vec_scrolls_coords[i][j] = 0;
      }
    }

    for (size_t i = 0; i < vec_init_scrolls_coords.size(); ++i) {
      for (size_t j = 0; j < vec_init_scrolls_coords[i].size(); ++j) {
        vec_init_scrolls_coords[i][j] = false;
      }
    }
  }

  void SelectiveResetScroll(const std::vector<bool>& isNeedReset) {
    // printf("\n\n%d %d\n\n ", vec_init_scrolls_coords.size(),
    // isNeedReset.size());
    for (size_t i = 0; i < vec_scrolls_coords.size(); ++i) {
      for (size_t j = 0; j < vec_scrolls_coords[i].size(); ++j) {
        if (isNeedReset[i]) {
          vec_scrolls_coords[i][j] = 0;
        }
      }
    }

    for (size_t i = 0; i < vec_init_scrolls_coords.size(); ++i) {
      // printf("%d ", isNeedReset[i] ? 1 : 0);
      for (size_t j = 0; j < vec_init_scrolls_coords[i].size(); ++j) {
        if (isNeedReset[i]) {
          vec_init_scrolls_coords[i][j] = false;
        }
      }
    }
    // printf("\n\n");
  }
};

void printTransport(const Monitor& t) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
  Serial.println("==========");
  Serial.println("Name: " + t.name);

  // tft.setAddrWindow(10, 20, 100, 80);

  Serial.println("Description: " + t.description);
  Serial.print("Countdown: ");
  for (auto& c : t.countdown) {
    Serial.print(c);
    Serial.print(", ");
  }
  Serial.println();
  Serial.println("==========");
#endif
}

String FixJsonMistake(String word) {
  word = Screen::ConvertGermanToLatin(word);
// Проверяем, содержит ли входная строка пробелы
#if DEBUG_SERIAL_WIEN_MONITOR
  Serial.println(word);
#endif
  word.trim();
  if (word.indexOf('-') == -1
      && word.indexOf(' ') == -1
      && word.length() > 1) {
    for (int i = 0; i < word.length(); i++) {
      word[i] = tolower(word[i]);  // Преобразуем все буквы в нижний регистр
    }
    word[0] = toupper(word[0]);  // Преобразуем первую букву в верхний регистр
  }
#if DEBUG_SERIAL_WIEN_MONITOR
  Serial.println(word);
#endif
  return word;
}

std::vector<Monitor> DeserilizeJson(const String& json) {
  SmartWatch sm(__FUNCTION__);
  std::vector<Monitor> monitors_vec;

  DynamicJsonDocument root(2048 * 8);
  DeserializationError error =
    deserializeJson(root, json, DeserializationOption::NestingLimit(64));

  if (error) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    Serial.println("Failed to parse JSON response.");
    Serial.println(error.c_str());
#endif
    // screenInit();  // draw empty screen
    delay(global_settings.ms_error_resset_delay);

    return monitors_vec;
  }

  const JsonObject data = root["data"];
  const JsonArray traffic_infos = data["trafficInfos"];
  std::map<String, String> lines_to_description_map;
  for (const auto& traffic_info : traffic_infos) {
    if (traffic_info.containsKey("description")) {
      const String description = Screen::ConvertGermanToLatin(
        traffic_info["description"].as<String>());
      const JsonArray related_lines = traffic_info["relatedLines"];

      for (const auto& related_line : related_lines) {
        String name_transport_with_descr = related_line.as<String>();

        lines_to_description_map[name_transport_with_descr] = description;
        Serial.print("descr");
        Serial.print(name_transport_with_descr);
        Serial.println(description);
      }
    }
  }

  const JsonArray monitors = data["monitors"];
  for (const auto& monitor : monitors) {
    const JsonArray lines = monitor["lines"];
    for (const auto& line : lines) {
      const JsonObject departures = line["departures"];
      Monitor cur_monitor;

      const JsonVariant departure = departures["departure"];

      if (departure.is<JsonArray>()) {
        const JsonArray& departuresArray = departure.as<JsonArray>();
        // cur_monitor.countdown.reserve(departuresArray.size());

        for (const auto& departureItem : departuresArray) {
          Monitor cur_monitor;

          int countdown = -1;
          if (departureItem.containsKey("vehicle")
              && departureItem.containsKey("departureTime")) {
            // Serial.println("_____2");

            const JsonObject vehicle = departureItem["vehicle"];
            cur_monitor.name = vehicle["name"].as<String>();
            cur_monitor.towards =
              FixJsonMistake(vehicle["towards"].as<String>());

            const JsonObject departureTime = departureItem["departureTime"];
            countdown = departureTime["countdown"].as<int>();
            countdown = departureTime["countdown"].as<int>();
            if (countdown != -1) {
              cur_monitor.countdown.push_back(countdown);
            }
            //serializeJsonPretty(departureItem, Serial);

          } else if (departureItem.containsKey("departureTime")) {
            // Serial.println("_____1");
            // teake information in upper lavel
            cur_monitor.name = line["name"].as<String>();
            cur_monitor.towards = FixJsonMistake(line["towards"].as<String>());

            const JsonObject departureTime = departureItem["departureTime"];
            countdown = departureTime["countdown"].as<int>();
            if (countdown != -1) {
              cur_monitor.countdown.push_back(countdown);
            }

            // serializeJsonPretty(departureItem, Serial);
          }
          //////////////////////////
          // description
          cur_monitor.description = lines_to_description_map[cur_monitor.name];
          /////////////////////////
          auto it =
            std::find_if(monitors_vec.begin(), monitors_vec.end(),
                         [&cur_monitor](const Monitor& monitor) {
                           return monitor.name == cur_monitor.name 
                           && monitor.towards == cur_monitor.towards;
                         });

          if (it == monitors_vec.end()) {
            monitors_vec.push_back(
              cur_monitor);  // Если не найден, добавляем новый Monitor
          } else {
            // Если найден, обновляем существующий Monitor счетчиком countdown
            if (countdown != -1) {
              it->countdown.push_back(countdown);
            }
          }
        }
      }
    }
  }

  return monitors_vec;
}

template<typename T>
std::vector<T> cyclicSubset(const std::vector<T>& input, size_t N,
                            size_t start) {
  std::vector<T> result;
  size_t size = input.size();

  // Если входной вектор пустой или N равно 0, вернуть пустой результат.
  if (input.empty() || N == 0) {
    return result;
  }

  // Начнем с элемента start и будем добавлять элементы в результат.
  for (size_t i = start; i < start + N; ++i) {
    result.push_back(input[i % size]);
  }

  return result;
}

class TraficClock {
public:
  TraficClock(long ms_perCountdown, long cd_perIterations, long it_perHour)
    : kMillisecondsPerCountdown(ms_perCountdown),
      kCountdownsPerIteration(cd_perIterations),
      kIterationsPerHour(it_perHour) {
    Reset();
  }

private:
  const unsigned long kMillisecondsPerCountdown = 5000;
  const long kCountdownsPerIteration = 2;
  const long kIterationsPerHour = 2;
  unsigned long start_time_;

  unsigned long Milliseconds() const {
    return millis();
  }

  long GetTotalCountdown() const {
    unsigned long totalMilliseconds = Milliseconds();
    return totalMilliseconds / kMillisecondsPerCountdown;
  }

  long GetTotalIteration() const {
    return GetTotalCountdown() / kCountdownsPerIteration;
  }

public:
  void Reset() {
    start_time_ = millis();
  }

  long GetCountdown() const {
    long seconds = GetTotalCountdown() % kCountdownsPerIteration;
    return seconds;
  }

  long GetIteration() const {
    long iterations = GetTotalIteration() % kIterationsPerHour;
    return iterations;
  }

  long GetFullCycle() const {
    long cycle = GetTotalIteration() / kIterationsPerHour;
    return cycle;
  }

  void PrintTime() const {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
    Serial.print("Time: ");
    Serial.print(GetFullCycle());
    Serial.print(":");
    Serial.print(GetIteration());
    Serial.print(":");
    Serial.print(GetCountdown());
    Serial.print(".");
    Serial.println(Milliseconds());
#endif
  }
};

class TraficManager {
public:
  TraficManager()
    : shift_cnt(0), coundown_idx(0), p_trafic_clock(nullptr) {}

  ~TraficManager() {
    delete p_trafic_clock;
  }

  // Block 1: Update Traffic Data
  void update(const std::vector<Monitor>& vec) {
    shift_cnt = 0;
    prev_iterations = 0;
    SmartWatch sm(__FUNCTION__);

    if (p_trafic_clock) {
      p_trafic_clock->Reset();
      const int& cnt_lines = global_settings.GetConfig().GetLinesCountAsInt();
      const int trafic_set_size = static_cast<int>(all_trafic_set.size());
      int rows_in_screen_cnt = std::min(cnt_lines, trafic_set_size);
      auto currentTraficSubset =
        cyclicSubset(all_trafic_set, rows_in_screen_cnt, shift_cnt);
      auto futureTraficSubset =
        cyclicSubset(vec, rows_in_screen_cnt, shift_cnt);

      sortTrafic(currentTraficSubset);
      sortTrafic(futureTraficSubset);

#ifdef DEBUG_SERIAL_WIEN_MONITOR

      Serial.println("updater_");
#endif
      SelectiveReset(currentTraficSubset, futureTraficSubset);
    }
    all_trafic_set = vec;
  }

  /**
 * @brief Updates the screen with traffic information.
 */
  void updateScreen() {
    if (all_trafic_set.empty()) {
      return;
    }

    const int& cnt_lines = global_settings.GetConfig().GetLinesCountAsInt();
    const int& cnt_countdows = global_settings.cnt_shows_countdows;
    const int& ms_additional = global_settings.ms_additional_time_for_countdown;
    const int& ms_full_cycle = global_settings.ms_task_delay_data_update;

    const int trafic_set_size = static_cast<int>(all_trafic_set.size());
    int rows_in_screen_cnt = std::min(cnt_lines, trafic_set_size);
    auto currentTraficSubset =
      cyclicSubset(all_trafic_set, rows_in_screen_cnt, shift_cnt);

    sortTrafic(currentTraficSubset);

    if (!p_trafic_clock) {
      double f_size = static_cast<double>(all_trafic_set.size());
      double f_sceen_cells = static_cast<double>(cnt_lines);

      long iterations_cnt = static_cast<long>(ceil(f_size / f_sceen_cells));

      long iteration_ms = ms_full_cycle / iterations_cnt;
      long countdown_ms = iteration_ms / cnt_countdows;
      p_trafic_clock = new TraficClock(countdown_ms + ms_additional,
                                       cnt_countdows, iterations_cnt);
      return;
    }

    if (p_trafic_clock) {
      // p_trafic_clock->PrintTime();
      long cur_iterations = p_trafic_clock->GetIteration();
      coundown_idx = p_trafic_clock->GetCountdown();
      if (cur_iterations != prev_iterations) {
        prev_iterations = cur_iterations;

        auto futureSubset = cyclicSubset(all_trafic_set, rows_in_screen_cnt,
                                         shift_cnt + cnt_lines);

        sortTrafic(futureSubset);
#ifdef DEBUG_SERIAL_WIEN_MONITOR
        Serial.println("inner_");
#endif
        SelectiveReset(currentTraficSubset, futureSubset);
        shift_cnt += global_settings.GetConfig().GetLinesCountAsInt();
      }
    }

    DrawTraficOnScreen(currentTraficSubset);
  }


  /**
 * @brief Resets the scrolling for monitors that have different descriptions.
 * @param currentTrafficSubset The current subset of traffic monitors.
 * @param futureSubset The future subset of traffic monitors.
 */
  void SelectiveReset(const std::vector<Monitor>& currentTraficSubset,
                      const std::vector<Monitor>& futureSubset) {
    // p_screen->PrintCordDebug();
    if (currentTraficSubset.size() == futureSubset.size()) {
      PrintMonitorDegbug(currentTraficSubset);
      PrintMonitorDegbug(futureSubset);

      size_t size = futureSubset.size();
      std::vector<bool> isNeedReset(size, true);
      for (size_t i = 0; i < size; ++i) {
        if (currentTraficSubset[i].description.isEmpty()
            || futureSubset[i].description.isEmpty()) {
          isNeedReset[i] = true;
        } else if (currentTraficSubset[i].description
                   == futureSubset[i].description) {
          isNeedReset[i] = false;
        }
      }

      // update only for difrent names
      if (currentTraficSubset.size() > 1) {
#ifdef DEBUG_SERIAL_WIEN_MONITOR
        for (size_t k = 0; k < isNeedReset.size(); k++) {
          Serial.print(isNeedReset[k]);
          Serial.print(" ");
        }
        Serial.println("");
#endif
        p_screen->SelectiveResetScroll(isNeedReset);
      }
    }

    // p_screen->PrintCordDebug();
  }


  /**
 * @brief Sorts traffic monitors based on countdown values.
 * @param v The vector of traffic monitors to be sorted.
 */
  void sortTrafic(std::vector<Monitor>& v) {
    std::sort(v.begin(), v.end(), [](const Monitor& a, const Monitor& b) {
      return a.countdown < b.countdown;
    });
  }


  /**
 * @brief Returns a valid countdown string at the given index.
 * @param c The vector of countdown values.
 * @param index The index to retrieve the countdown value from.
 * @return A valid countdown string.
 */
  String GetValidCountdown(const std::vector<int>& c, size_t index) {
    if (c.empty()) {
      return String();
    }

    size_t best_index = std::min(index, c.size() - 1);
    return String(c[best_index], DEC);
  }

  /**
 * @brief Draws traffic information on the screen using the provided data.
 * @param currentTrafficSubset A vector of Monitor objects representing the
 *                            current traffic data to display.
 */
  void DrawTraficOnScreen(const std::vector<Monitor>& currentTrafficSubset) {
    // Get the number of lines to display
    size_t numLines = global_settings.GetConfig().GetLinesCountAsInt();

    // Iterate through each line on the screen
    for (size_t i = 0; i < numLines; ++i) {
      ScreenEntity monitor;
      std::vector<String> clean_str;
      for (size_t x = 0; x < global_settings.cnt_screen_lines_per_rows; ++x) {
        clean_str.push_back("");
        clean_str.push_back("");
      }

      // Check if there's at least one monitor in the subset
      if (!currentTrafficSubset.empty()) {
        size_t monior_idx = i % currentTrafficSubset.size();
        // dont heve cyrcular arry
        if (i > (currentTrafficSubset.size() - 1) 
        && currentTrafficSubset.size() > 1) {
          break;
        }

        const Monitor& currentMonitor = currentTrafficSubset[monior_idx];
        if (i > currentMonitor.countdown.size() - 1 
        && currentTrafficSubset.size() == 1) {
          // break if this cyclig getting
          break;
        }
        size_t idx = currentTrafficSubset.size() == 1 ? i : coundown_idx;
        if (currentTrafficSubset.size() == 1 
        && idx > currentMonitor.countdown.size()) {
          // break if this single line and countdown for this line cycle
          break;
        }
        // Set the right text
        monitor.right_txt = currentMonitor.name;

        if (!currentMonitor.countdown.empty()) {
          if (currentMonitor.countdown[idx] == 0) {
            if ((millis() / 1000) % 2) {
              monitor.left_txt = String("◱");
            } else {
              monitor.left_txt = String("◳");
            }
          } else {
            monitor.left_txt = GetValidCountdown(currentMonitor.countdown, idx);
          }
        }
        // Serial.println("block 2");
        // Set lines for display

        auto splitted_string = getSplittedStringFromCache(currentMonitor.towards);

        for (size_t j = 0;
             j < min(static_cast<size_t>(2), splitted_string.size()); ++j) {
          clean_str[j] = splitted_string[j];
        }

        // if splitted string error
        if (splitted_string.empty()) {
          clean_str[0] = currentMonitor.towards;
        }
        // TODO: oprimize it
        if (currentMonitor.description.length()) {
          clean_str[1] = currentMonitor.description;
        }
      }
      monitor.lines = clean_str;
      // Serial.println("block 3");
      //  Set the idx_row on the screen
      p_screen->SetRow(monitor, i);
// p_screen->PrintCordDebug();
#ifdef DEBUG_SERIAL_WIEN_MONITOR
      for (size_t m = 0; m < monitor.lines.size(); m++) {
        Serial.print(i);
        Serial.print(": ");
        Serial.println(monitor.lines[m]);
      }
#endif
      // Serial.println("roww setted;");
    }
  }

  int last_min_size = -1;  // Initialize last_min_size to an invalid value
  std::map<String, std::vector<String>> SplittedStringCache;

  /**
 * @brief Retrieves or generates a vector of split strings from a cache.
 * @param key_string The key string used to retrieve or generate split strings.
 * @return A vector of split strings either from the cache or generated.
 */
  std::vector<String> getSplittedStringFromCache(const String& key_string) {
    int min_size = p_screen->GetMinTextSprite_px();

    if (last_min_size != min_size) {
      // Clear the cache if min_size has changed
      SplittedStringCache.clear();
      last_min_size = min_size;
    }

    auto cacheIt = SplittedStringCache.find(key_string);
    if (cacheIt != SplittedStringCache.end()) {
      // Found in the cache, return it
      //Serial.println("return from cache");
      return cacheIt->second;
    }

    // Generate and cache the result
    std::vector<String> generatedStrings = splitToString(key_string);
    SplittedStringCache[key_string] = generatedStrings;
    return generatedStrings;
  }

  /**
   * @brief Splits a string into individual words and stores them in a vector.
   * @param str The input string to be split.
   * @param words A vector to store the individual words.
   */
  void splitStringToWords(const String& str, std::vector<String>& words) {
    // SmartWatch sm(__FUNCTION__);
    String currentWord = "";

    for (size_t i = 0; i < str.length(); i++) {
      currentWord += str.charAt(i);

      // If the current character is a space or a hyphen
      if (str.charAt(i) == ' ' || str.charAt(i) == '-') {
        // Add the current word to the words vector
        if (p_screen->IsEnoughSpaceForMiddleText(currentWord)) {
          words.push_back(currentWord);
        }
        currentWord = "";
      }
    }

    // Add the remaining word if it's not empty
    if (!currentWord.isEmpty() 
    && p_screen->IsEnoughSpaceForMiddleText(currentWord)) {
      words.push_back(currentWord);
    }
  }

  /**
   * @brief Splits a string into subtexts that fit within one line.
   * @param input The input string to be split into subtexts.
   * @return A vector of subtexts that fit within one line.
   */
  std::vector<String> splitToString(const String& input) {
    // SmartWatch sm(__FUNCTION__);
    std::vector<String> words;
    splitStringToWords(input, words);

    // Create a list of subtexts
    std::vector<String> subtexts;
    String currentSubtext = "";

    // Add words to the subtexts list as long as they fit in one line
    for (size_t i = 0; i < words.size(); i++) {
      if (p_screen->IsEnoughSpaceForMiddleText(currentSubtext + " " + words[i])){
        currentSubtext += words[i];
      } else {
        if (!currentSubtext.isEmpty()) {
          subtexts.push_back(currentSubtext);
        }
        currentSubtext = words[i];
      }
    }

    if (!currentSubtext.isEmpty()) {
      subtexts.push_back(currentSubtext);
    }

    for (size_t i = 0; i < subtexts.size(); i++) {
      if (!subtexts[i].isEmpty() && (subtexts.size() - 1 != i)) {
        subtexts[i] = subtexts[i].substring(0, subtexts[i].length() - 1);
      }
    }

    return subtexts;
  }

private:
  std::vector<Monitor> all_trafic_set;
  int shift_cnt;
  int coundown_idx;
  // unsigned long previousMillisTraficSet;
  // unsigned long previousMillisCountDown;
  // const int INTERVAL_UPDATE = global_settings.ms_task_delay_data_update;
  TraficClock* p_trafic_clock;
  long prev_iterations = 0;
};

String generateRandomString(int maxLength) {
  // Определяем допустимые символы
  String validChars =
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

  // Получаем случайную длину строки от 1 до maxLength
  int length = random(1, maxLength + 1);

  // Генерируем случайную строку
  String randomString = "";
  for (int i = 0; i < length; i++) {
    int randomIndex = random(validChars.length());
    randomString += validChars[randomIndex];
  }

  return randomString;
}

/**
 * @brief Splits a string by a specified separator character.
 * @param data The input string to be split.
 * @param separator The character used for splitting.
 * @return A vector of substrings resulting from the split operation.
 */
std::vector<String> SplitString(String data, char separator) {
  int separatorIndex = 0;
  std::vector<String> result;

  while (separatorIndex != -1) {
    separatorIndex = data.indexOf(separator);
    String chunk = data.substring(0, separatorIndex);
    result.push_back(chunk);
    if (separatorIndex != -1) {
      data = data.substring(separatorIndex + 1);
    }
  }

  return result;
}

/**
 * @brief Filters a vector of Monitor objects based on a filter string.
 * @param data The input vector of Monitor objects.
 * @param filter The filter string to match against Monitor names.
 * @return A vector containing Monitor objects that match the filter.
 */
std::vector<Monitor> Filter(const std::vector<Monitor>& data,
                            const String& filter) {
  if (filter.isEmpty()) {
    return data;
  }
  std::vector<String> custom_names = SplitString(filter, ',');
  std::vector<Monitor> result;

  for (auto& monitor : data) {
    auto it = std::find(custom_names.begin(), custom_names.end(), monitor.name);
    if (it != custom_names.end()) {
      result.push_back(monitor);
    }
  }

  return result;
}

/**
 * @brief Updates data for a specific task.
 * @param pvParameters Pointer to task-specific parameters.
 */
void UpdateDataTask(void* pvParameters) {
  std::vector<Monitor> allTrafficSetInit;
  const int delay_ms = global_settings.ms_task_delay_data_update;

  while (true) {
    // Fetch JSON data (may take several seconds)
    const auto& cfg = global_settings.GetConfig();

    const String& rbl_id = GetJson(cfg.GetLinesRblAsString());
    allTrafficSetInit = DeserilizeJson(rbl_id);
    const String& raw_filter = cfg.GetLinesFilterAsString();
    allTrafficSetInit = Filter(allTrafficSetInit, raw_filter);

    // Acquire the data mutex
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      /* //DEBUG
      for (auto& c : allTrafficSetInit) {
        static int k = 0;
        if (k % 2 == 0) {
          String random = generateRandomString(25);
          if (random.length() > 15) {
            c.description = random;
          }
        }
        k++;
      }
      */
      if (!allTrafficSetInit.empty()) {
        pTraficManager->update(allTrafficSetInit);
#ifdef DEBUG_SERIAL_WIEN_MONITOR
        Serial.println("Monitor data updated.");
#endif
      }
      // Release the data mutex
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

/**
 * @brief Updates the screen for a specific task.
 * @param pvParameters Pointer to task-specific parameters.
 */
void ScreenUpdateTask(void* pvParameters) {
  const int delay_ms = global_settings.ms_task_delay_screen_update;

  while (true) {
    // Acquire the data mutex
    if (xSemaphoreTake(dataMutex, portMAX_DELAY) == pdTRUE) {
      pTraficManager->updateScreen();
      // Serial.println("Screen updated.");

      // Release the data mutex
      xSemaphoreGive(dataMutex);
    }

    vTaskDelay(pdMS_TO_TICKS(delay_ms));
  }
}

/**
 * @brief Retrieves JSON data from a specified URL.
 * @param rbl_id The resource identifier to fetch data from.
 * @return The JSON data as a string or an empty string if unsuccessful.
 */
String GetJson(const String& rbl_id) {
  SmartWatch sm(__FUNCTION__);

  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }

  HTTPClient http;
  String url = URL_BASE + rbl_id;

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
  } else if (httpCode == HTTP_CODE_OK) {
    // The request was successful
    // Get the response payload as a string
    String payload = http.getString();
    http.end();
    return payload;
  }
  http.end();
  return "";
}

/**
 * @brief Manages Wi-Fi configuration using WiFiManager.
 */
void WiFiManagerTask() {
  SmartWatch sm(__FUNCTION__);
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(true);
  const char* custom_html = "<style>button{background-color:red;}</style>";
  wifiManager.setCustomHeadElement(custom_html);

  // Load saved configuration
  const Config& old_config = global_settings.GetConfig();
  Serial.println("getter");
  Config new_config;
  // Convert configuration values to strings
  const String& rblValue = old_config.GetLinesRblAsString();
  const String& countValue = old_config.GetLinesCountAsString();
  const String& filterValue = old_config.GetLinesFilterAsString();

  // Create Wi-FiManager parameters
  WiFiManagerParameter custom_html_elem_hr("<hr>");
  String cpp_lines_rbl_promt = StringDatabase::GetRBLPrompt();

  String cpp_line_count_promt = StringDatabase::GetLineCountPrompt(
    Config::cnt_min_lines, Config::cnt_max_lines, Config::cnt_default_lines);

  String cpp_line_filter_promt = StringDatabase::GetLineFilterPrompt();
  String cpp_ssid = StringDatabase::GetWiFissid();
  String cpp_instruction = StringDatabase::GetInstructionsText();
  WiFiManagerParameter rblParam("lines_rbl", cpp_lines_rbl_promt.c_str(),
                                rblValue.c_str(), 64);

  WiFiManagerParameter countParam("lines_count", cpp_line_count_promt.c_str(),
                                  countValue.c_str(), 64);
  WiFiManagerParameter filterParam(
    "lines_filter", cpp_line_filter_promt.c_str(), filterValue.c_str(), 64);

  // Add parameters to Wi-FiManager
  wifiManager.addParameter(&rblParam);
  wifiManager.addParameter(&custom_html_elem_hr);
  wifiManager.addParameter(&countParam);
  wifiManager.addParameter(&custom_html_elem_hr);
  wifiManager.addParameter(&filterParam);

  if (WiFi.psk().length() == 0) {
    // show screen wifi hint
    const int FONT_SIZE = global_settings.size_start_instruction_font;
    tft.setTextSize(0);
    tft.setTextColor(TFT_TEXT, TFT_BG);
    tft.setCursor(0, 0, FONT_SIZE);
    tft.print(cpp_instruction);
  }

  // Attempt to connect to Wi-Fi
  bool isConnected = wifiManager.autoConnect(cpp_ssid.c_str());

  new_config.SetLinesFilter(filterParam.getValue());
  new_config.SetLinesRBL(rblParam.getValue());
  new_config.SetLinesCount(countParam.getValue());

  Serial.println(new_config.GetLinesFilterAsString());
  Serial.println(new_config.GetLinesRblAsString());
  Serial.println(new_config.GetLinesCountAsString());

  if (!isConnected) {
    Serial.println("Failed to connect");
    ESP.restart();
  } else {
    // Save the configuration to EEPROM
    tft.fillScreen(TFT_BG);
    if (old_config != new_config) {
      global_settings.SetConfig(new_config);
    }
    Serial.println("Saved!");
  }
}

/**
 * @brief Prints system information including free heap memory.
 */
void printSystemInfo() {
  // Get the free heap memory
  uint32_t freeHeap = ESP.getFreeHeap();
  Serial.print("FreeHeap:");
  Serial.print(freeHeap);
  Serial.print(", ");
  Serial.println();
}

/**
 * @brief Task to monitor and handle reset button presses.
 * @param pvParameters Pointer to task-specific parameters.
 */
void ResetButtonTask(void* pvParameters) {
  // Define the button pin
  const int buttonPin = global_settings.pin_reset_button;

  // Set the button pin as an input
  pinMode(buttonPin, INPUT);
  // Initialize button state to HIGH (assuming active LOW button)
  int buttonState = HIGH;
  // Counter for consecutive LOW readings
  int consecutiveLowReads = 0;
  const int& hard_threshold = global_settings.sec_press_hard_resset_button;
  const int& soft_threshold = global_settings.sec_press_soft_resset_button;

  while (1) {
    // Read the state of the button
    buttonState = digitalRead(buttonPin);

    // Check if the button is pressed (LOW)
    if (buttonState == LOW) {
      consecutiveLowReads++;
    } else {
      if (consecutiveLowReads > soft_threshold 
      && consecutiveLowReads <= hard_threshold) {
        // Perform softReset here
        Serial.println("SOFT RESETTING");
        WiFiManager wifiManager;
        wifiManager.resetSettings();

        ESP.restart();
      }
      consecutiveLowReads = 0;  // Reset the consecutive LOW reads counter
    }

    if (consecutiveLowReads > hard_threshold) {
      // Reset WiFi settings and restart the ESP

      Serial.println("HARD RESETTING");
      global_settings.DeleteFile();
      WiFiManager wifiManager;
      wifiManager.resetSettings();

      ESP.restart();
    }
    unsigned long currentMillis = millis();
    if (currentMillis >= global_settings.ms_reboot_interval) {
      ESP.restart();
    }

    int delay_ms = global_settings.ms_task_delay_resset_button;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));  // Delay for 1 second
    printSystemInfo();
  }
}

/**
 * @brief Setup function for initializing the application.
 */
void setup() {
  // Initialize Serial communication
  Serial.begin(115200);
  Serial.println("APP START");

  // Initialize TFT display
  tft.begin();
  tft.setRotation(1);
  tft.fillScreen(TFT_BG);

  // Initialize SPIFFS (SPI Flash File System)
  if (!SPIFFS.begin(true)) {
    Serial.println("An error occurred while mounting SPIFFS");
    delay(global_settings.ms_error_resset_delay);
    ESP.restart();  // Restart the ESP32
  }

  // Create a task for reading the reset button state
  xTaskCreate(ResetButtonTask, "ResetButtonTask", 2048 * 4, NULL, 1, NULL);

  // Run WiFiManagerTask to manage WiFi connection
  WiFiManagerTask();

  pTraficManager = new TraficManager;
  p_screen = new Screen(global_settings.GetConfig().GetLinesCountAsInt(),
                        global_settings.cnt_screen_lines_per_rows);

  // Check if WiFi is successfully connected
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi!");
    delay(global_settings.ms_error_resset_delay);
    ESP.restart();  // Restart the ESP32
  }

  // Create a mutex for synchronization
  dataMutex = xSemaphoreCreateMutex();

  // Create tasks for data updating and screen updating
  xTaskCreate(UpdateDataTask, "UpdateDataTask", 2048 * 64, NULL, 2, NULL);
  xTaskCreate(ScreenUpdateTask, "ScreenUpdateTask", 2048 * 16, NULL, 1, NULL);
}

/**
 * @brief Main loop function (not actively used in this application).
 */
void loop() {
  // This loop is intentionally left empty since the application is task-based.
}