/////////////////////////////////////////////////////////////////
//      _____                             ___________ ____     //
//     / ___/__  ______  ___  _____      / ____/ ___// __ \    //
//     \__ \/ / / / __ \/ _ \/ ___/_____/ __/  \__ \/ /_/ /    //
//    ___/ / /_/ / /_/ /  __/ /  /_____/ /___ ___/ / ____/     //
//             /_/                                             //
//   ---------- By https://Github.com/Riktastic ------------   //
//                                                             //
/////////////////////////////////////////////////////////////////
#define SUPER_ESP_VERSION 1.0

////////////////////////
// What is it?

// Super-ESP is a easy to use fullblown template for your own creations.
// This template has been inspired by Tuya's ESP32 WiFi lightbulbs.
// It allows us to:
// - Join a WiFi network (while making sure DNS works and an internet connection can be established),
// - Provide a lightweight webpage, with authentication and an API (making use of the SPIFSS filesystem for easily storing files. Just as you would on a normal webserver),
// - The webpage is templatable. By adding custom tags you can insert new informatie or open the .html-files to change the style of the webpage.
// - Sync the time with a NTP server,
// - Connect to a secure (as in SSL/TLS) MQTT broker,
// - Configure the device using the "configuration.json"-file on the SPIFFS filesystem,
// - Configure the device using MQTT and using the Web API,
// - Update the firmware usng MQTT and the Web API (can be done by providing a URL to a firmware file). Also known as OTA,
// - Has a 2D array to easily add GPIO-pins as new MQTT/website/API on/off-switches.
// - Modules such as: MQTT and the website/api can easily be removed from the compiled firmware.
// - It provides stability by checking for corrupt configurations and trying multiple times to connect to WiFi, NTP and MQTT.

////////////////////////
// Why is everything in 1 file?

// This is everything you will need to customize the code. All of the used libraries have a great documentation.
// It does not become a spaghetti code, atleast as long as everything has been correctly labeled, seperated and commented.
// All of these design choices make it easy to implement your own functionality.

////////////////////////
// ToDo:

// 1. The webserver library does not work with HTTPS. We should either wait for a fix or implement a different library. See: https://github.com/me-no-dev/AsyncTCP/pull/90
//   - This one seems great. But to make this sketch ESP32 compatible and EPS8266 compatible we would have to implement both in the same script.


///////////////////////
// Static configuration. Dynamic configuration is stored in (this can be changed using the API): configuration.json

// The configuration file has to be loaded within a buffer of a predefined length. Please make this number as small as possbi
#define configuration_file_size 1024

// Enable or disable the MQTT module. If not used disable it. This allows the firmware file to be smaller and to use less resources.
#define MQTT 1
#define MQTT_SSL 0

// Pins that are automatically configured as on/off switches on the website and on MQTT.
int gpio_outputs[] =          {2}; // Signifies each GPIO pin that is going to be turned on and off using the API and website.
int gpio_outputs_defaults[] = {1}; // Adds to the '2D' 'gpio_outpus' matrix. Signifies the default power mode on boot of each pin. 1 to turn on during boot, 0 to do the opposite.

// GPIO inputs:
const int ts_input = 13; // Yellow cable of the DS18B20 temperature sensor.
const int cs_input = 12; // Grey cable of the SCT013-050 currentsensor.
const int ls_sda_input = 14; // Green cable of the GY-30 module for the BH1750 lightsensor.
const int ls_scl_input = 27; // Yellow cable of the GY-30 module for the BH1750 lightsensor.

// Variables that represent the owner, location and other info of the device
#define OWNERID 1
#define DEVICEID 1
#define SENDINTERVAL 30
const String installationDate = "2022-01-20 15:20:00";

#include <math.h>

// Required for the temperature measurement:
#include <DallasTemperature.h> // Can be downloaded using the Arduino Library Manager
#include <OneWire.h> // Can be downloaded using the Arduino Library Manager
OneWire tsOneWire(ts_input); // Setup a OneWire instance to communicatith with the OneWire device on GPIO 13.
DallasTemperature ts(&tsOneWire); // Pass the OneWire reference to the Dallas Temperature sensor library.


// Required for the light sensor:
#include <Wire.h>
#include <BH1750.h> // Can be downloaded using the Arduino Library Manager
BH1750 ls;


// Required for the current sensor:
#include "EmonLib.h" // Can be downloaded using the library manager.
#include <driver/adc.h> // Make sure that the ESP32 or a ESP8266 board has been selected.
#define ADC_BITS    10
#define ADC_COUNTS  (1<<ADC_BITS)
#define CS_CALIBRATION 30
EnergyMonitor es;

///////////////////////
// Required for loading an storing files.

// Filesystem manager. ToDo: Update to LittleFS. But LittleFS hasn't been fully integrated into ESPAsyncWebServer.
#include "SPIFFS.h"


///////////////////////
// Required for Over-The-Air updates
#include <HTTPClient.h>
#include <Update.h>


///////////////////////
// Required for wifi management.

#include <WiFi.h> // Available from: https://github.com/espressif/arduino-esp32 or https://github.com/esp8266/Arduino
#ifdef ESP8266
#include <ESP8266Ping.h> // Library that makes sending ICMP pings possible. Available from: https://github.com/dancol90/ESP8266Ping
#endif
#ifdef ESP32
#include <ESP32Ping.h> // Library that makes sending ICMP pings possible. Available from: https://github.com/marian-craciunescu/ESP32Ping
#endif

///////////////////////
// Required for interpreting JSON files.

// Lets ArduinoJson ignore JSON-comments.
#define ARDUINOJSON_ENABLE_COMMENTS 1

// Interpretes JSON.
#include <ArduinoJson.h> // Available from the library manager. 


///////////////////////
// Required for MQTT

#if MQTT == true
#include <PubSubClient.h> // Available from: https://github.com/knolleary/pubsubclient

#if MQTT_SSL == true
#include <WiFiClientSecure.h>
#endif
#endif


///////////////////////
// Structs
///////////////////////

typedef struct { // Defines all possible dynamic configuration options. These can be changed using the API.
  char general_hostname[32];

  char wifi_ssid[32];
  char wifi_password[64];
  bool wifi_dhcp;
  IPAddress wifi_ipaddress;
  IPAddress wifi_subnetmask;
  IPAddress wifi_gateway;
  IPAddress wifi_dns;
  char wifi_ntp[64];
  char wifi_timezone[128];

#if MQTT == true
  char mqtt_server[64];
  int mqtt_port;
  char mqtt_username[32];
  char mqtt_password[64];
  char mqtt_topic[128];
#endif
} Configuration;


///////////////////////
// Global variables
///////////////////////
Configuration configuration; // Create an instance of the Configuration struct.

///////////////////////
// Functions
///////////////////////

///////////////////////
// General functions

void newline() {
  Serial.println("");
}

// Converts a string into an array. Useful for decoding CSV and IP addresses.
void stringToIntArray(String data, uint8_t* output, char separator) {
  int r = 0, t = 0;
  for (int i = 0; i < data.length(); i++) {
    if (data.charAt(i) == separator) {
      output[t] = data.substring(r, i).toInt();
      r = (i + 1);
      t++;
    }
  }
}

// Simple function to check if a element is within an array.
bool IsInArray(int x[], int size, int element) {
  for (int i = 0; i < size; i++) {
    if (x[i] == element) {
      return true;
    }
  }
  return false;
}

// Checks if a string contains only numbers.
boolean isNumeric(String str) {
  unsigned int stringLength = str.length();
  if (stringLength == 0) {
    return false;
  }
  boolean seenDecimal = false;
  for (unsigned int i = 0; i < stringLength; ++i) {
    if (isDigit(str.charAt(i))) {
      continue;
    }
    if (str.charAt(i) == '.') {
      if (seenDecimal) {
        return false;
      }
      seenDecimal = true;
      continue;
    }
    return false;
  }
  return true;
}


///////////////////////
// Over-The-Air updatess

int total_length = 0;
int current_length = 0;

// Function to update firmware incrementally
// Buffer is declared to be 128 so chunks of 128 bytes
// from firmware is written to device until server closes
void updateFirmware(uint8_t *data, size_t len) {
  Update.write(data, len);
  current_length += len;
  Serial.print('.'); // Print dots while waiting for update to finish.
  if (current_length != total_length) return; // If current length of written firmware is not equal to total firmware size, continu downloading the firmware.
  Update.end(true); // If the total file size has been repeated end the update.
  Serial.println("\n[✓] Update successful. Rebooting the system.");
  ESP.restart(); // Restart the device.
}

// Checks if a firmware has been
void getFirmware(String url) {
  Serial.println("[*] Checking if there is firmware available at:" + String(url));
  HTTPClient client;

  // Connect to external web server
  client.begin(url);
  // Get file, just to check if each reachable
  int response = client.GET();

  // If file is reachable, start downloading
  if (response > 0) {
    Serial.println("[✓] Firmware has been found. Trying to update.");
    total_length = client.getSize(); // Get the length of the file (is -1 when the webserver sends no "Content-Length" header).
    int temp_length = total_length;
    Update.begin(UPDATE_SIZE_UNKNOWN); // This is required to start firmware update process.
    Serial.println("[i] Firmware size: %u\n" + String(temp_length));
    uint8_t data_buffer[128] = { 0 }; // Create buffer for reading the firmware.
    WiFiClient * data_stream = client.getStreamPtr(); // Get the incoming TCP stream of the file.
    Serial.println("[*] Updating firmware...");
    while (client.connected() && (temp_length > 0 || temp_length == -1)) { // Loop while connected, till the file_length has not been reached.
      size_t data_size = data_stream->available(); // Get available data size.
      if (data_size) {
        int c = data_stream->readBytes(data_buffer, ((data_size > sizeof(data_buffer)) ? sizeof(data_buffer) : data_size)); // Fill the data stream with the current bytes.

        Serial.println("[✓] The firmware has been downloaded.");
        updateFirmware(data_buffer, c);
        if (temp_length > 0) {
          temp_length -= c;
        }
      }
      delay(1);
    }
  } else {
    Serial.println("[✗] The specified URL could not be resolved.");
  }
  client.end();
}


///////////////////////
// Configuration management

// Opens the current configuration.
bool loadConfiguration(const char *filename, Configuration &configuration) {
  Serial.println("[*] Trying to load the configuration file.");
  if (SPIFFS.begin()) { // If this function does not return true. Something might have gone wrong while loading the SPIFFS library.
    File configuration_file = SPIFFS.open(filename, "r"); // Open 'filename' as read-only.
    if (configuration_file) { // Checks if the configuration file could be found.
      size_t configuration_file_temp_size = configuration_file.size(); // Gets the size in bytes of the loaded configuration file.
      if (configuration_file_temp_size <= configuration_file_size) { // The size of the configuration file has to be predefined. The configuration file in SPIFFS should match this predefined setting.

        StaticJsonDocument<configuration_file_size> configuration_json; // Create a JSON structure to house the configuration file.
        if (!deserializeJson(configuration_json, configuration_file)) { // Fill the configuration JSON structure with the contents of the configuration file.

          // Load the general configuration.
          if (configuration_json.containsKey("general")) {
            JsonObject configuration_json_general = configuration_json["general"];
            if (configuration_json_general.containsKey("hostname")) {
              strcpy(configuration.general_hostname, configuration_json_general["hostname"]);
            } else {
              Serial.println("[✗] Failed to find configuration option 'general.hostname'. Falling back to 'Super-ESP'.");
              strcpy(configuration.general_hostname, "Super-ESP");
            }
          }

          // Load the WiFi configuration.
          if (configuration_json.containsKey("wifi")) {
            JsonObject configuration_json_wifi = configuration_json["wifi"];

            if (configuration_json_wifi.containsKey("ssid")) {
              strcpy(configuration.wifi_ssid, configuration_json_wifi["ssid"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'wifi.ssid'.");
              return false;
            }

            if (configuration_json_wifi.containsKey("password")) {
              strcpy(configuration.wifi_password, configuration_json_wifi["password"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'wifi.password'.");
              return false;
            }

            if (configuration_json_wifi.containsKey("dhcp")) {
              if (configuration_json_wifi["dhcp"] == false) {
                configuration.wifi_dhcp = false;

                if (configuration_json_wifi.containsKey("ipaddress")) {
                  IPAddress ipaddress;
                  if (ipaddress.fromString(configuration_json_wifi["ipaddress"].as<String>())) { // Fills the "ipaddress" variable with with the IPaddress option of the configuration JSON.
                    configuration.wifi_ipaddress = ipaddress;
                  } else {
                    Serial.println("[✗] Failed to find configuration option 'wifi.ipaddress'. Falling back to DHCP."); configuration.wifi_dhcp = true;
                  }
                }

                if (configuration_json_wifi.containsKey("subnetmask")) {
                  IPAddress subnetmask;
                  if (subnetmask.fromString(configuration_json_wifi["subnetmask"].as<String>())) {
                    configuration.wifi_subnetmask = subnetmask;
                  } else {
                    Serial.println("[✗] Failed to find configuration option 'wifi.subnetmask'. Falling back to DHCP."); configuration.wifi_dhcp = true;
                  }
                }

                if (configuration_json_wifi.containsKey("gateway")) {
                  IPAddress gateway;
                  if (gateway.fromString(configuration_json_wifi["gateway"].as<String>())) {
                    configuration.wifi_gateway = gateway;
                  } else {
                    Serial.println("[✗] Failed to find configuration option 'wifi.gateway'. Falling back to DHCP."); configuration.wifi_dhcp = true;
                  }
                }

                if (configuration_json_wifi.containsKey("dns")) {
                  IPAddress dns;
                  if (dns.fromString(configuration_json_wifi["dns"].as<String>())) {
                    configuration.wifi_dns = dns;
                  } else {
                    Serial.println("[✗] Failed to find configuration option 'wifi.dns'. Falling back to DHCP."); configuration.wifi_dhcp = true;
                  }
                }

              }
            } else {
              configuration.wifi_dhcp = true; // If the above options can't be parsed enable DHCP as a fallback option. This way the device will make a connection in most situation.
            }

            if (configuration_json_wifi.containsKey("ntp")) {
              strcpy(configuration.wifi_ntp, configuration_json_wifi["ntp"]);
            } else {
              Serial.println("[✗] Failed to find configuration option 'wifi.ntp'. Falling back to 'pool.ntp.org'."); strcpy(configuration.wifi_ntp, "pool.ntp.org");
            }

            if (configuration_json_wifi.containsKey("timezone")) {
              strcpy(configuration.wifi_timezone, configuration_json_wifi["timezone"]);
            } else {
              Serial.println("[✗] Failed to find configuration option 'wifi.timezone'. Falling back to 'CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00'");
              strcpy(configuration.wifi_timezone, "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00");
            }
          } else {
            Serial.println("[✗] The required 'wifi' configuration section could not be found.");
            return false;
          }

#if MQTT == true
          // Load the MQTT configuration.
          if (configuration_json.containsKey("mqtt")) {
            JsonObject configuration_json_mqtt = configuration_json["mqtt"];

            if (configuration_json_mqtt.containsKey("server")) {
              strcpy(configuration.mqtt_server, configuration_json_mqtt["server"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'mqtt.server'.");
              return false;
            }

            if (configuration_json_mqtt.containsKey("port")) {
              configuration.mqtt_port = configuration_json_mqtt["port"];
            } else {
              Serial.println("[✗] Failed to find required configuration option 'mqtt.port'.");
              return false;
            }

            if (configuration_json_mqtt.containsKey("username")) {
              strcpy(configuration.mqtt_username, configuration_json_mqtt["username"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'mqtt.username'.");
              return false;
            }

            if (configuration_json_mqtt.containsKey("password")) {
              strcpy(configuration.mqtt_password, configuration_json_mqtt["password"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'mqtt.password'.");
              return false;
            }

            if (configuration_json_mqtt.containsKey("topic")) {
              strcpy(configuration.mqtt_topic, configuration_json_mqtt["topic"]);
            } else {
              Serial.println("[✗] Failed to find required configuration option 'mqtt.topic'.");
              return false;
            }

          } else {
            Serial.println("[✗] The required 'mqtt' configuration section could not be found.");
            return false;
          }
#endif
          configuration_file.close();
        } else {
          Serial.println("[✗] The configuration could not be read as JSON. Please check your configuration file. Make sure that you are using SPIFFS.");
          return false;
        }
      } else {
        Serial.println("[✗] The configuration file is larger then " + String(configuration_file_size) + " bytes.");
        return false;
      }
    } else {
      Serial.println("[✗] Failed to find the configuration file.");
      return false;
    }
  } else {
    Serial.println("[✗] Filesystem library does not seem te be running.");
    return false;
  }
  // If everything is correctly being loaded.
  Serial.println("[✓] Configuration has been loaded.");
  return true;
}


// Saves a configuration.
bool saveConfiguration(const char *filename, const Configuration &configuration) {
  Serial.println("[*] Trying to save the active configuration.");
  if (SPIFFS.begin()) { // Checks if the SPIFFS library has been loaded.
    SPIFFS.remove(filename); // Deletes the current configuration file. Just to make sure this function won't add the new configuration to the old configuration.
    File configuration_file = SPIFFS.open(filename, "w"); // Opens/creates the new configuration file, as "write"-only.
    if (configuration_file) { // Checks if the configuration has been sucessfully opened.
      StaticJsonDocument<configuration_file_size> configuration_json; // Create a JSON document to store the configuration in.

      // The following sections will create various objects within the JSON document. And fill those with fields.
      JsonObject configuration_json_general = configuration_json.createNestedObject("general");
      configuration_json_general["hostname"] = configuration.general_hostname;

      JsonObject configuration_json_wifi = configuration_json.createNestedObject("wifi");
      configuration_json_wifi["ssid"] = configuration.wifi_ssid;
      configuration_json_wifi["password"] = configuration.wifi_password;
      configuration_json_wifi["dhcp"] = configuration.wifi_dhcp;
      configuration_json_wifi["ipaddress"] = configuration.wifi_ipaddress.toString();
      configuration_json_wifi["subnetmask"] = configuration.wifi_subnetmask.toString();
      configuration_json_wifi["gateway"] = configuration.wifi_gateway.toString();
      configuration_json_wifi["dns"] = configuration.wifi_dns.toString();
      configuration_json_wifi["ntp"] = configuration.wifi_ntp;

#if MQTT == true
      JsonObject configuration_json_mqtt = configuration_json.createNestedObject("mqtt");
      configuration_json_mqtt["server"] = configuration.mqtt_server;
      configuration_json_mqtt["port"] = configuration.mqtt_port;
      configuration_json_mqtt["username"] = configuration.mqtt_username;
      configuration_json_mqtt["password"] = configuration.mqtt_password;
      configuration_json_mqtt["topic"] = configuration.mqtt_topic;
#endif

      if (serializeJson(configuration_json, configuration_file) != 0) {
        configuration_file.close();
      } else {
        Serial.println("[✗] Failed to parse the configuration file."); return false;
      }
    } else {
      Serial.println("[✗] Failed to create the configuration file."); return false;
    }
  } else {
    Serial.println("[✗] Filesystem library does not seem te be running."); return false;
  }

  // If everything is correctly being loaded.
  Serial.println("[✓] Configuration has been saved.");
  return true;
}


///////////////////////
// Wifi management

// Connects to a wifi network.
bool connectWiFi(Configuration &configuration) {
  Serial.println("[*] Enabling the WiFi module.");
  WiFi.mode(WIFI_STA); // The mode option allows to specify if the WiFi module should be turned off, used as a access point or in station mode, where it will connect to an already existing network.

  WiFi.hostname(configuration.general_hostname); // The DNS hostname.

  WiFi.begin(configuration.wifi_ssid, configuration.wifi_password); // Connects to a WiFi network.

  if (configuration.wifi_dhcp == false) { // If DHCP is not required a static configuration will be used. Thus the ipaddress, gateway address, subnetmask and DNS server have to be supplied manually.
    if (!WiFi.config(configuration.wifi_ipaddress, configuration.wifi_gateway, configuration.wifi_subnetmask, configuration.wifi_dns)) { // Configure the WiFi module using the supplied settings. If these are unsuccesful show a warning.
      Serial.println("[✗] Aborting the setup. Incorrect static IP configured");
      return false;
    }
  }

  // Waiting for WiFi connection:
  int attempt = 1, attemptsMax = 10;
  while (WiFi.status() != WL_CONNECTED) { // While the connection ti the WiFi has not been made, do the following:
    if (attempt >= attemptsMax) { // Check if the max attempts has been reached. If it has been reached show a warning and stop the WiFi setup.
      Serial.println("[✗] Aborting the setup. Could not connect to: " + String(configuration.wifi_ssid));
      return false;
    }
    else {
      delay(5000);
      if (attempt > 1) { // Only show this message if the first atempt was unsuccesful. Makes the UART output a bit cleaner.
        Serial.println("[*] Trying to connect to (" + String(attempt) + "/" + String(attemptsMax) + "): " + String(configuration.wifi_ssid));
      }
      attempt++;
    }
  }

  Serial.println("[✓] Connected to:    " + String(WiFi.SSID()));
  Serial.println("  - Hostname:        " + String(configuration.general_hostname));
  Serial.println("  - IP address:      " + WiFi.localIP().toString());
  Serial.println("  - Subnetmask:      " + WiFi.subnetMask().toString());
  Serial.println("  - Gateway:         " + WiFi.gatewayIP().toString());
  Serial.println("  - DNS:             " + WiFi.dnsIP().toString());

  bool success = Ping.ping("www.google.com", 3); // Send 3 ICMP pings to google.com. To check if the device has internet.
  if (!success) {
    Serial.println("[✗] Aborting the setup. Could not connect to the internet");
    return false;
  }
  else {
    Serial.println("[✓] Connected to the internet.");
    return true;
  }
}


///////////////////////
// NTP management

#include "time.h"

// Gets the time from the configured timezone.
bool setTime(Configuration &configuration) {
  Serial.println("[*] Trying to get the time from the NTP server.");
  configTime(0, 0, configuration.wifi_ntp); // Configures the timechip using the configured NTP server.
  setenv("TZ", configuration.wifi_timezone, 1); // Sets the timezone variable to the configured timezone.

  int attempt = 1, attemptsMax = 10;
  struct tm timeinfo; // Creates a tm object. This structure allows us to display the time.
  while (!getLocalTime(&timeinfo)) { // Fill timeinfo with the current time. If a False has been returned something must have gone wrong. In such case display a warning and stop this function with an error.
    if (attempt >= attemptsMax) { // Check if the max attempts has been reached. If it has been reached show a warning and stop the WiFi setup.
      Serial.println("[✗] Could not get the network time!");
      return false;
    }
    else {
      delay(5000);
      if (attempt > 1) { // Only show this message if the first atempt was unsuccesful. Makes the UART output a bit cleaner.
        Serial.println("[*] Trying to get the time from (" + String(attempt) + "/" + String(attemptsMax) + "): " + String(configuration.wifi_ntp));
      }
      attempt++;
    }
  }
  Serial.print("[✓] Got the time from the NTP server: ");
  Serial.println(&timeinfo, "%Y-%m-%d %H:%M:%S"); // Print the current time.
  return true;
}

// Gets the current time.
String getTime() {
  struct tm timeinfo; // Creates a tm object. This structure allows us to display the time.
  if (!getLocalTime(&timeinfo)) { // Fill timeinfo with the current time. If a False has been returned something must have gone wrong. In such case display a warning and stop this function with an error.
    Serial.println("Could not get time!");
  }
  char timeStringBuff[50]; // Create a temporary buffer of chars.
  strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%d %H:%M:%S", &timeinfo); // Fill the temporary buffer of chars with the current time.
  return String(timeStringBuff); // Convert the temporary buffer of chars to string and return its output.
}


///////////////////////
// MQTT management

#if MQTT == true

#if MQTT_SSL == true
WiFiClientSecure mqtt_temp_wifi_client;
#else
WiFiClient mqtt_temp_wifi_client;
#endif

PubSubClient mqtt_client(mqtt_temp_wifi_client);

// Listens for new topics. Acts based on the received topic.
void mqtt_callback(char* topic, byte* message, unsigned int length) {
  Serial.print(getTime() + "[MQTT] Message arrived on topic '" + String(topic) + "'. Message: ");

  String complete_message;

  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    complete_message += (char)message[i];
  }

  // If a message is received on the topic esp32/output, you check if the message is either "on" or "off".
  // Changes the output state according to the message
  //  if (String(topic) == "esp32/output") {
  //    Serial.print("Changing output to ");
  //    if(complete_message == "on"){
  //      Serial.println("on");
  //      digitalWrite(ledPin, HIGH);
  //    }
  //    else if(complete_message == "off"){
  //      Serial.println("off");
  //      digitalWrite(ledPin, LOW);
  //    }
  //  }
  Serial.print("\n");
}

#endif

///////////////////////
// Setup
///////////////////////

void setup(void) {
  Serial.begin(115200); // Starts a serial connection on the UART port..
  Serial.println("\n\n\n*------------------*");
  Serial.println(      "|                  |");
  Serial.println(      "|  Super-ESP v1.0  |");
  Serial.println(      "|                  |");
  Serial.println(      "*------------------*");
  Serial.println("[✓] Connected to serial using UART.\n");
  Serial.println("[*] Booting up.\n");


  ///////////////////////
  // Basic initialization

  if (!loadConfiguration("/configuration.json", configuration)) {
    exit(0); // Loads the configuration file in to the main configuration variable). If this does not succeed exit the script.
  }

  newline();

  if (!connectWiFi(configuration)) {
    exit(0); // Uses the main configuration to setup an Internet connection using WiFi.
  }

  newline();

  if (!setTime(configuration)) {
    exit(0); // Connects to the Internet and tries to get the current time.
  }

#if MQTT == true

  newline();

  Serial.println("[*] Initializing the MQTT module.");
#if MQTT_SSL == true
  Serial.println("[i] Loading the root CA certificate.");
  if (SPIFFS.begin()) {
    File root_ca_file = SPIFFS.open("Root.pem", "r");
    if (root_ca_file) {
      String root_ca_string = root_ca_file.readString();
      char* root_ca_char;
      root_ca_char = (char *)malloc(sizeof(char) * (root_ca_string.length() + 1));
      strcpy(root_ca_char, root_ca_string.c_str());
      mqtt_temp_wifi_client.setCACert(root_ca_char);
    }
  }
#endif
  mqtt_client.setServer(configuration.mqtt_server, configuration.mqtt_port);
  Serial.println("[i] Connecting to: mqtt://" + String(configuration.mqtt_server) + ":" + String(configuration.mqtt_port));
  mqtt_client.setCallback(mqtt_callback);
  Serial.println("[i] Username: '" + String(configuration.mqtt_username) + "', password: '" + String(configuration.mqtt_password).substring(0, 3) + "..." + "'");

  int attempt = 1, attemptsMax = 10;
  while (!mqtt_client.connect(configuration.general_hostname, configuration.mqtt_username, configuration.mqtt_password)) {
    if (attempt >= attemptsMax) { // Check if the max attempts has been reached. If it has been reached show a warning and stop the WiFi setup.
      Serial.println("[✗] The MQTT module could not initialize. Please check its configuration.");
      exit(0);
    } else {
      if (attempt > 1) { // Only show this message if the first atempt was unsuccesful. Makes the UART output a bit cleaner.
        Serial.println("[*] Trying to connect to MQTT-server (" + String(attempt) + "/" + String(attemptsMax) + "): " + String(configuration.mqtt_server));
      }
      attempt++;
    }
  }
  mqtt_client.subscribe(configuration.mqtt_topic);
  Serial.println("[✓] The MQTT module has been intialized.");
#endif

  newline();

  Serial.println("[*] Initializing the GPIO pins."); // Loops through the configured GPIO output pin and sets them as output ports. After that each configured GPIO output pin is turned on/off using thier configured default setting.
  for (int i = 0; i < sizeof gpio_outputs / sizeof gpio_outputs[0]; i++) {
    pinMode(gpio_outputs[i], OUTPUT); // Sets the pin to the OUTPUT mode.
    digitalWrite(gpio_outputs[i], gpio_outputs_defaults[i]); // Turns the port on or off.
    Serial.print("  - Set GPIO pin '"  + String(gpio_outputs[i]) + "' as output. ");
    if (gpio_outputs_defaults[i] == 1) {
      Serial.println("As configured, it has been turned on.");
    } else {
      Serial.println("As configured, it has been turned off.");
    }
  }
  Serial.println("[✓] GPIO pins have been initialized.");

  newline();

  //////////////////////////
  // Custom boot routine


  /////////////////////////

  es.current(cs_input, CS_CALIBRATION);

  pinMode(ls_sda_input, INPUT_PULLUP);
  pinMode(ls_scl_input, INPUT_PULLUP);

  Wire.begin(ls_scl_input, ls_sda_input); // Initialize the I2C bus (BH1750 library doesn't do this automatically). First value is the SCL, seconds value is the SDA.

  if (ls.begin(BH1750::ONE_TIME_HIGH_RES_MODE)) {
    Serial.println(F("[✓] Lightsensor has been initialized."));
  }
  else {
    Serial.println(F("[✗] Error initialising the lightsensor."));
  }

  newline();

  Serial.println("[✓] Succesfully finished booting up. \n--------------\n");
}

void loop(void) {
  Serial.println("[i] Getting the measurements...");

  Serial.println("[*] Checking if the device is still connected to the internet.");
  bool success = Ping.ping("www.google.com", 3); // Send 3 ICMP pings to google.com. To check if the device has internet.
  if (!success) {
    Serial.println("[✗] Could not connect to the internet. Rebooting the device. To allow it to reconfigure itself.");
  }
  else {
    Serial.println("[✓] Still connected to the internet. Getting the sensor values...");
  }

  // Get the results of the temperaturesensor:
  float temperatureC = ts.getTempCByIndex(0);

  // Get the results of the lightsensor:
  while (!ls.measurementReady(true)) {
    yield();
  }
  float lux = ls.readLightLevel();
  ls.configure(BH1750::ONE_TIME_HIGH_RES_MODE);

  // Get the results of the currentsensor
  double amps = es.calcIrms(1480); // Calculate Irms only

  int uptime = round(esp_timer_get_time() / 1000); // "esp_timer_get_time()" returns values in milliseconds but we need to have the value in seconds, thus we are converting it to seconds.

  int wifiSignalStrength = WiFi.RSSI();

  String message = getTime() + "," + String(DEVICEID) + "," + String(OWNERID) + "," + String(installationDate) + "," + String(uptime) + "," + wifiSignalStrength + "," + String(temperatureC) + "," + String(lux) + "," + String(amps);

  Serial.print("[i] New measurements: ");
  Serial.println(message);

#if MQTT == true
  Serial.print("[*] Sending the data to the MQTT broker.");

  // Convert the message to a Char*
  char message_buff[message.length() + 1];
  message.toCharArray(message_buff, message.length() + 1) ;

  if (mqtt_client.publish(configuration.mqtt_topic, message_buff)) {
    Serial.print("[✓] The data has been receive by the broker.\n");
  } else {
    Serial.print("[✗] Could not send the data to the broker.\n");
  }
  mqtt_client.loop();
#endif
  Serial.print("[✓] Finished sending data. Next measurement will take place in " + String(SENDINTERVAL) + " minutes.\n");
  delay(SENDINTERVAL * 60 * 1000);

}

/////////////////////////////////////////////
