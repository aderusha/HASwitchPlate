////////////////////////////////////////////////////////////////////////////////////////////////////
//           _____ _____ _____ _____
//          |  |  |  _  |   __|  _  |
//          |     |     |__   |   __|
//          |__|__|__|__|_____|__|
//        Home Automation Switch Plate
// https://github.com/aderusha/HASwitchPlate
//
// Copyright (c) 2019 Allen Derusha allen@derusha.org
//
// MIT License
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this hardware,
// software, and associated documentation files (the "Product"), to deal in the Product without
// restriction, including without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Product, and to permit persons to whom the
// Product is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Product.
//
// THE PRODUCT IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE PRODUCT OR THE USE OR OTHER DEALINGS IN THE PRODUCT.
////////////////////////////////////////////////////////////////////////////////////////////////////

// OPTIONAL: Assign default values here.
char wifiSSID[32] = ""; // Leave unset for wireless autoconfig. Note that these values will be lost
char wifiPass[64] = ""; // when updating, but that's probably OK because they will be saved in EEPROM.

////////////////////////////////////////////////////////////////////////////////////////////////////
// These defaults may be overwritten with values saved by the web interface
char mqttServer[64] = "";
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
char groupName[16] = "plates";
char configUser[32] = "admin";
char configPassword[32] = "";
char motionPinConfig[3] = "0";
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <DNSServer.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <MQTT.h>
#include <EEPROM.h>
#include <SoftwareSerial.h>

const float haspVersion = 0.38;                     // Current HASP software release version
byte nextionReturnBuffer[128];                      // Byte array to pass around data coming from the panel
uint8_t nextionReturnIndex = 0;                     // Index for nextionReturnBuffer
uint8_t nextionActivePage = 0;                      // Track active LCD page
bool lcdConnected = false;                          // Set to true when we've heard something from the LCD
char wifiConfigPass[9];                             // AP config password, always 8 chars + NUL
char wifiConfigAP[19];                              // AP config SSID, haspNode + 3 chars
bool shouldSaveConfig = false;                      // Flag to save json config to SPIFFS
bool nextionReportPage0 = false;                    // If false, don't report page 0 sendme
const unsigned long updateCheckInterval = 43200000; // Time in msec between update checks (12 hours)
unsigned long updateCheckTimer = 0;                 // Timer for update check
const unsigned long nextionCheckInterval = 5000;    // Time in msec between nextion connection checks
unsigned long nextionCheckTimer = 0;                // Timer for nextion connection checks
unsigned int nextionRetryMax = 5;                   // Attempt to connect to panel this many times
bool updateEspAvailable = false;                    // Flag for update check to report new ESP FW version
float updateEspAvailableVersion;                    // Float to hold the new ESP FW version number
bool updateLcdAvailable = false;                    // Flag for update check to report new LCD FW version
bool debugSerialEnabled = true;                     // Enable USB serial debug output
bool debugTelnetEnabled = false;                    // Enable telnet debug output
bool debugSerialD8Enabled = true;                   // Enable hardware serial debug output on pin D8
const unsigned long telnetInputMax = 128;           // Size of user input buffer for user telnet session
bool motionEnabled = false;                         // Motion sensor is enabled
bool mdnsEnabled = true;                            // mDNS enabled
uint8_t motionPin = 0;                              // GPIO input pin for motion sensor if connected and enabled
bool motionActive = false;                          // Motion is being detected
const unsigned long motionLatchTimeout = 30000;     // Latch time for motion sensor
const unsigned long motionBufferTimeout = 1000;     // Latch time for motion sensor
unsigned long lcdVersion = 0;                       // Int to hold current LCD FW version number
unsigned long updateLcdAvailableVersion;            // Int to hold the new LCD FW version number
bool lcdVersionQueryFlag = false;                   // Flag to set if we've queried lcdVersion
const String lcdVersionQuery = "p[0].b[2].val";     // Object ID for lcdVersion in HMI
bool startupCompleteFlag = false;                   // Startup process has completed
const long statusUpdateInterval = 300000;           // Time in msec between publishing MQTT status updates (5 minutes)
long statusUpdateTimer = 0;                         // Timer for update check
const unsigned long connectTimeout = 300;           // Timeout for WiFi and MQTT connection attempts in seconds
const unsigned long reConnectTimeout = 15;          // Timeout for WiFi reconnection attempts in seconds
byte espMac[6];                                     // Byte array to store our MAC address
const uint16_t mqttMaxPacketSize = 4096;            // Size of buffer for incoming MQTT message
String mqttClientId;                                // Auto-generated MQTT ClientID
String mqttGetSubtopic;                             // MQTT subtopic for incoming commands requesting .val
String mqttGetSubtopicJSON;                         // MQTT object buffer for JSON status when requesting .val
String mqttStateTopic;                              // MQTT topic for outgoing panel interactions
String mqttStateJSONTopic;                          // MQTT topic for outgoing panel interactions in JSON format
String mqttCommandTopic;                            // MQTT topic for incoming panel commands
String mqttGroupCommandTopic;                       // MQTT topic for incoming group panel commands
String mqttStatusTopic;                             // MQTT topic for publishing device connectivity state
String mqttSensorTopic;                             // MQTT topic for publishing device information in JSON format
String mqttLightCommandTopic;                       // MQTT topic for incoming panel backlight on/off commands
String mqttLightStateTopic;                         // MQTT topic for outgoing panel backlight on/off state
String mqttLightBrightCommandTopic;                 // MQTT topic for incoming panel backlight dimmer commands
String mqttLightBrightStateTopic;                   // MQTT topic for outgoing panel backlight dimmer state
String mqttMotionStateTopic;                        // MQTT topic for outgoing motion sensor state
String nextionModel;                                // Record reported model number of LCD panel
const byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};    // Standard suffix for Nextion commands
uint32_t tftFileSize = 0;                           // Filesize for TFT firmware upload
uint8_t nextionResetPin = D6;                       // Pin for Nextion power rail switch (GPIO12/D6)

WiFiClient wifiClient;
WiFiClient wifiMQTTClient;
MQTTClient mqttClient(mqttMaxPacketSize);
ESP8266WebServer webServer(80);
ESP8266HTTPUpdateServer httpOTAUpdate;
WiFiServer telnetServer(23);
WiFiClient telnetClient;
MDNSResponder::hMDNSService hMDNSService;

// Additional CSS style to match Hass theme
const char HASP_STYLE[] = "<style>button{background-color:#03A9F4;}body{width:60%;margin:auto;}input:invalid{border:1px solid red;}input[type=checkbox]{width:20px;}</style>";
// URL for auto-update "version.json"
const char UPDATE_URL[] = "http://haswitchplate.com/update/version.json";
// Default link to compiled Arduino firmware image
String espFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.ino.d1_mini.bin";
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.tft";

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup
  pinMode(nextionResetPin, OUTPUT);
  digitalWrite(nextionResetPin, HIGH);
  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  Serial.swap();

  debugPrintln(String(F("SYSTEM: Starting HASwitchPlate v")) + String(haspVersion));
  debugPrintln(String(F("SYSTEM: Last reset reason: ")) + String(ESP.getResetInfo()));

  configRead(); // Check filesystem for a saved config.json

  while (!lcdConnected && (millis() < 5000))
  { // Wait up to 5 seconds for serial input from LCD
    nextionHandleInput();
  }
  if (lcdConnected)
  {
    debugPrintln(F("HMI: LCD responding, continuing program load"));
    nextionSendCmd("connect");
  }
  else
  {
    debugPrintln(F("HMI: LCD not responding, continuing program load"));
  }

  espWifiSetup(); // Start up networking

  if (mdnsEnabled)
  { // Setup mDNS service discovery if enabled
    hMDNSService = MDNS.addService(haspNode, "http", "tcp", 80);
    if (debugTelnetEnabled)
    {
      MDNS.addService(haspNode, "telnet", "tcp", 23);
    }
    MDNS.addServiceTxt(hMDNSService, "app_name", "HASwitchPlate");
    MDNS.addServiceTxt(hMDNSService, "app_version", String(haspVersion).c_str());
    MDNS.update();
  }

  if ((configPassword[0] != '\0') && (configUser[0] != '\0'))
  { // Start the webserver with our assigned password if it's been configured...
    httpOTAUpdate.setup(&webServer, "/update", configUser, configPassword);
  }
  else
  { // or without a password if not
    httpOTAUpdate.setup(&webServer, "/update");
  }
  webServer.on("/", webHandleRoot);
  webServer.on("/saveConfig", webHandleSaveConfig);
  webServer.on("/resetConfig", webHandleResetConfig);
  webServer.on("/firmware", webHandleFirmware);
  webServer.on("/espfirmware", webHandleEspFirmware);
  webServer.on("/lcdupload", HTTP_POST, []() { webServer.send(200); }, webHandleLcdUpload);
  webServer.on("/tftFileSize", webHandleTftFileSize);
  webServer.on("/lcddownload", webHandleLcdDownload);
  webServer.on("/lcdOtaSuccess", webHandleLcdUpdateSuccess);
  webServer.on("/lcdOtaFailure", webHandleLcdUpdateFailure);
  webServer.on("/reboot", webHandleReboot);
  webServer.onNotFound(webHandleNotFound);
  webServer.begin();
  debugPrintln(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());

  espSetupOta(); // Start OTA firmware update

  mqttClient.begin(mqttServer, atoi(mqttPort), wifiMQTTClient); // Create MQTT service object
  mqttClient.onMessage(mqttCallback);                           // Setup MQTT callback function
  mqttConnect();                                                // Connect to MQTT

  motionSetup(); // Setup motion sensor if configured

  if (debugTelnetEnabled)
  { // Setup telnet server for remote debug output
    telnetServer.setNoDelay(true);
    telnetServer.begin();
    debugPrintln(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
  }

  debugPrintln(F("SYSTEM: System init complete."));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{ // Main execution loop

  if (nextionHandleInput())
  { // Process user input from HMI
    nextionProcessInput();
  }

  while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
  { // Check WiFi is connected and that we have a valid IP, retry until we do.
    if (WiFi.status() == WL_CONNECTED)
    { // If we're currently connected, disconnect so we can try again
      WiFi.disconnect();
    }
    espWifiReconnect();
  }

  if (!mqttClient.connected())
  { // Check MQTT connection
    debugPrintln("MQTT: not connected, connecting.");
    mqttConnect();
  }

  mqttClient.loop();        // MQTT client loop
  ArduinoOTA.handle();      // Arduino OTA loop
  webServer.handleClient(); // webServer loop
  if (mdnsEnabled)
  {
    MDNS.update();
  }

  if ((lcdVersion < 1) && (millis() <= (nextionRetryMax * nextionCheckInterval)))
  { // Attempt to connect to LCD panel to collect model and version info during startup
    nextionConnect();
  }
  else if ((lcdVersion > 0) && (millis() <= (nextionRetryMax * nextionCheckInterval)) && !startupCompleteFlag)
  { // We have LCD info, so trigger an update check + report
    if (updateCheck())
    { // Send a status update if the update check worked
      mqttStatusUpdate();
      startupCompleteFlag = true;
    }
  }
  else if ((millis() > (nextionRetryMax * nextionCheckInterval)) && !startupCompleteFlag)
  { // We still don't have LCD info so go ahead and run the rest of the checks once at startup anyway
    updateCheck();
    mqttStatusUpdate();
    startupCompleteFlag = true;
  }

  if ((millis() - statusUpdateTimer) >= statusUpdateInterval)
  { // Run periodic status update
    statusUpdateTimer = millis();
    mqttStatusUpdate();
  }

  if ((millis() - updateCheckTimer) >= updateCheckInterval)
  { // Run periodic update check
    updateCheckTimer = millis();
    if (updateCheck())
    { // Send a status update if the update check worked
      mqttStatusUpdate();
    }
  }

  if (motionEnabled)
  { // Check on our motion sensor
    motionUpdate();
  }

  if (debugTelnetEnabled)
  {
    handleTelnetClient(); // telnetClient loop
  }

  delay(10); // Spooky voodoo which claims to improve WiFi stability.
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttConnect()
{ // MQTT connection and subscriptions

  static bool mqttFirstConnect = true; // For the first connection, we want to send an OFF/ON state to
                                       // trigger any automations, but skip that if we reconnect while
                                       // still running the sketch

  // Check to see if we have a broker configured and notify the user if not
  if (mqttServer[0] == 0)
  {
    nextionSendCmd("page 0");
    nextionSetAttr("p[0].b[1].txt", "\"WiFi Connected!\\rConfigure MQTT:\\r" + WiFi.localIP().toString() + "\\r\\r\\r\\r\\r\\r\\r \"");
    nextionSetAttr("p[0].b[3].txt", "\"http://" + WiFi.localIP().toString() + "\"");
    nextionSendCmd("vis 3,1");
    while (mqttServer[0] == 0)
    { // Handle HTTP and OTA while we're waiting for MQTT to be configured
      yield();
      if (nextionHandleInput())
      { // Process user input from HMI
        nextionProcessInput();
      }
      webServer.handleClient();
      ArduinoOTA.handle();
    }
  }
  // MQTT topic string definitions
  mqttStateTopic = "hasp/" + String(haspNode) + "/state";
  mqttStateJSONTopic = "hasp/" + String(haspNode) + "/state/json";
  mqttCommandTopic = "hasp/" + String(haspNode) + "/command";
  mqttGroupCommandTopic = "hasp/" + String(groupName) + "/command";
  mqttStatusTopic = "hasp/" + String(haspNode) + "/status";
  mqttSensorTopic = "hasp/" + String(haspNode) + "/sensor";
  mqttLightCommandTopic = "hasp/" + String(haspNode) + "/light/switch";
  mqttLightStateTopic = "hasp/" + String(haspNode) + "/light/state";
  mqttLightBrightCommandTopic = "hasp/" + String(haspNode) + "/brightness/set";
  mqttLightBrightStateTopic = "hasp/" + String(haspNode) + "/brightness/state";
  mqttMotionStateTopic = "hasp/" + String(haspNode) + "/motion/state";

  const String mqttCommandSubscription = mqttCommandTopic + "/#";
  const String mqttGroupCommandSubscription = mqttGroupCommandTopic + "/#";
  const String mqttLightSubscription = "hasp/" + String(haspNode) + "/light/#";
  const String mqttLightBrightSubscription = "hasp/" + String(haspNode) + "/brightness/#";

  // Loop until we're reconnected to MQTT
  while (!mqttClient.connected())
  {
    // Create a reconnect counter
    static uint8_t mqttReconnectCount = 0;

    // Generate an MQTT client ID as haspNode + our MAC address
    mqttClientId = String(haspNode) + "-" + String(espMac[0], HEX) + String(espMac[1], HEX) + String(espMac[2], HEX) + String(espMac[3], HEX) + String(espMac[4], HEX) + String(espMac[5], HEX);
    nextionSendCmd("page 0");
    nextionSetAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\\rMQTT Connecting" + String(mqttServer) + "\"");
    debugPrintln(String(F("MQTT: Attempting connection to broker ")) + String(mqttServer) + " as clientID " + mqttClientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(30, true, 5000);

    // declare LWT
    mqttClient.setWill(mqttStatusTopic.c_str(), "OFF");

    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(mqttCommandSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttCommandSubscription);
      }
      if (mqttClient.subscribe(mqttGroupCommandSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttGroupCommandSubscription);
      }
      if (mqttClient.subscribe(mqttLightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttLightBrightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttStatusTopic))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttStatusTopic);
      }

      if (mqttFirstConnect)
      { // Force any subscribed clients to toggle OFF/ON when we first connect to
        // make sure we get a full panel refresh at power on.  Sending OFF,
        // "ON" will be sent by the mqttStatusTopic subscription action.
        debugPrintln(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [OFF]");
        mqttClient.publish(mqttStatusTopic, "OFF", true, 1);
        mqttFirstConnect = false;
      }
      else
      {
        debugPrintln(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [ON]");
        mqttClient.publish(mqttStatusTopic, "ON", true, 1);
      }

      mqttReconnectCount = 0;

      // Update panel with MQTT status
      nextionSetAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\\rMQTT Connected:\\r" + String(mqttServer) + "\"");
      debugPrintln(F("MQTT: connected"));
      if (nextionActivePage)
      {
        nextionSendCmd("page " + String(nextionActivePage));
      }
    }
    else
    { // Retry until we give up and restart after connectTimeout seconds
      mqttReconnectCount++;
      if (mqttReconnectCount > ((connectTimeout / 10) - 1))
      {
        debugPrintln(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Restarting device.")));
        espReset();
      }
      debugPrintln(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 30 seconds.")));
      nextionSetAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\\rMQTT Connect to" + String(mqttServer) + "\\rFAILED rc=" + String(mqttClient.returnCode()) + "\\rRetry in 30 sec\"");
      unsigned long mqttReconnectTimer = millis(); // record current time for our timeout
      while ((millis() - mqttReconnectTimer) < 30000)
      { // Handle HTTP and OTA while we're waiting 30sec for MQTT to reconnect
        if (nextionHandleInput())
        { // Process user input from HMI
          nextionProcessInput();
        }
        webServer.handleClient();
        ArduinoOTA.handle();
        delay(10);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttCallback(String &strTopic, String &strPayload)
{ // Handle incoming commands from MQTT

  // strTopic: homeassistant/haswitchplate/devicename/command/p[1].b[4].txt
  // strPayload: "Lights On"
  // subTopic: p[1].b[4].txt

  // Incoming Namespace (replace /device/ with /group/ for group commands)
  // '[...]/device/command' -m '' = No command requested, respond with mqttStatusUpdate()
  // '[...]/device/command' -m 'dim=50' = nextionSendCmd("dim=50")
  // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextionSendCmd("dim=50"), nextionSendCmd("page 1")
  // '[...]/device/command/page' -m '1' = nextionSendCmd("page 1")
  // '[...]/device/command/statusupdate' -m '' = mqttStatusUpdate()
  // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = nextionStartOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/lcdupdate' -m '' = nextionStartOtaDownload("lcdFirmwareUrl")
  // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' = espStartOta("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
  // '[...]/device/command/espupdate' -m '' = espStartOta("espFirmwareUrl")
  // '[...]/device/command/p[1].b[4].txt' -m '' = nextionGetAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = nextionSetAttr("p[1].b[4].txt", "\"Lights On\"")

  debugPrintln(String(F("MQTT IN: '")) + strTopic + "' : '" + strPayload + "'");

  if (((strTopic == mqttCommandTopic) || (strTopic == mqttGroupCommandTopic)) && (strPayload == ""))
  {                     // '[...]/device/command' -m '' = No command requested, respond with mqttStatusUpdate()
    mqttStatusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == mqttCommandTopic || strTopic == mqttGroupCommandTopic)
  { // '[...]/device/command' -m 'dim=50' == nextionSendCmd("dim=50")
    nextionSendCmd(strPayload);
  }
  else if (strTopic == (mqttCommandTopic + "/page") || strTopic == (mqttGroupCommandTopic + "/page"))
  { // '[...]/device/command/page' -m '1' == nextionSendCmd("page 1")
    if (nextionActivePage != strPayload.toInt())
    { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
      nextionActivePage = strPayload.toInt();
      nextionSendCmd("page " + strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/json") || strTopic == (mqttGroupCommandTopic + "/json"))
  {                               // '[...]/device/command/json' -m '["dim=5", "page 1"]' = nextionSendCmd("dim=50"), nextionSendCmd("page 1")
    nextionParseJson(strPayload); // Send to nextionParseJson()
  }
  else if (strTopic == (mqttCommandTopic + "/statusupdate") || strTopic == (mqttGroupCommandTopic + "/statusupdate"))
  {                     // '[...]/device/command/statusupdate' == mqttStatusUpdate()
    mqttStatusUpdate(); // return status JSON via MQTT
  }
  else if (strTopic == (mqttCommandTopic + "/lcdupdate") || strTopic == (mqttGroupCommandTopic + "/lcdupdate"))
  { // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' == nextionStartOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
    if (strPayload == "")
    {
      nextionStartOtaDownload(lcdFirmwareUrl);
    }
    else
    {
      nextionStartOtaDownload(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/espupdate") || strTopic == (mqttGroupCommandTopic + "/espupdate"))
  { // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' == espStartOta("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
    if (strPayload == "")
    {
      espStartOta(espFirmwareUrl);
    }
    else
    {
      espStartOta(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/reboot") || strTopic == (mqttGroupCommandTopic + "/reboot"))
  { // '[...]/device/command/reboot' == reboot microcontroller)
    debugPrintln(F("MQTT: Rebooting device"));
    espReset();
  }
  else if (strTopic == (mqttCommandTopic + "/lcdreboot") || strTopic == (mqttGroupCommandTopic + "/lcdreboot"))
  { // '[...]/device/command/lcdreboot' == reboot LCD panel)
    debugPrintln(F("MQTT: Rebooting LCD"));
    nextionReset();
  }
  else if (strTopic == (mqttCommandTopic + "/factoryreset") || strTopic == (mqttGroupCommandTopic + "/factoryreset"))
  { // '[...]/device/command/factoryreset' == clear all saved settings)
    configClearSaved();
  }
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == ""))
  { // '[...]/device/command/p[1].b[4].txt' -m '' == nextionGetAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    nextionGetAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic) && (strPayload == ""))
  { // '[...]/group/command/p[1].b[4].txt' -m '' == nextionGetAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    nextionGetAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttCommandTopic))
  { // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' == nextionSetAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    nextionSetAttr(subTopic, strPayload);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic))
  { // '[...]/group/command/p[1].b[4].txt' -m '"Lights On"' == nextionSetAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    nextionSetAttr(subTopic, strPayload);
  }
  else if (strTopic == mqttLightBrightCommandTopic)
  { // change the brightness from the light topic
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    nextionSetAttr("dim", String(panelDim));
    nextionSendCmd("dims=dim");
    mqttClient.publish(mqttLightBrightStateTopic, strPayload);
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "OFF")
  { // set the panel dim OFF from the light topic, saving current dim level first
    nextionSendCmd("dims=dim");
    nextionSetAttr("dim", "0");
    mqttClient.publish(mqttLightStateTopic, "OFF");
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "ON")
  { // set the panel dim ON from the light topic, restoring saved dim level
    nextionSendCmd("dim=dims");
    mqttClient.publish(mqttLightStateTopic, "ON");
  }
  else if (strTopic == mqttStatusTopic && strPayload == "OFF")
  { // catch a dangling LWT from a previous connection if it appears
    mqttClient.publish(mqttStatusTopic, "ON");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttStatusUpdate()
{ // Periodically publish a JSON string indicating system status
  String mqttStatusPayload = "{";
  mqttStatusPayload += String(F("\"status\":\"available\","));
  mqttStatusPayload += String(F("\"espVersion\":")) + String(haspVersion) + String(F(","));
  if (updateEspAvailable)
  {
    mqttStatusPayload += String(F("\"updateEspAvailable\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"updateEspAvailable\":false,"));
  }
  if (lcdConnected)
  {
    mqttStatusPayload += String(F("\"lcdConnected\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"lcdConnected\":false,"));
  }
  mqttStatusPayload += String(F("\"lcdVersion\":\"")) + String(lcdVersion) + String(F("\","));
  if (updateLcdAvailable)
  {
    mqttStatusPayload += String(F("\"updateLcdAvailable\":true,"));
  }
  else
  {
    mqttStatusPayload += String(F("\"updateLcdAvailable\":false,"));
  }
  mqttStatusPayload += String(F("\"espUptime\":")) + String(long(millis() / 1000)) + String(F(","));
  mqttStatusPayload += String(F("\"signalStrength\":")) + String(WiFi.RSSI()) + String(F(","));
  mqttStatusPayload += String(F("\"haspIP\":\"")) + WiFi.localIP().toString() + String(F("\","));
  mqttStatusPayload += String(F("\"heapFree\":")) + String(ESP.getFreeHeap()) + String(F(","));
  mqttStatusPayload += String(F("\"heapFragmentation\":")) + String(ESP.getHeapFragmentation()) + String(F(","));
  mqttStatusPayload += String(F("\"espCore\":\"")) + String(ESP.getCoreVersion()) + String(F("\""));
  mqttStatusPayload += "}";

  mqttClient.publish(mqttSensorTopic, mqttStatusPayload);
  mqttClient.publish(mqttStatusTopic, "ON", true, 1);
  debugPrintln(String(F("MQTT: status update: ")) + String(mqttStatusPayload));
  debugPrintln(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [ON]");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool nextionHandleInput()
{ // Handle incoming serial data from the Nextion panel
  // This will collect serial data from the panel and place it into the global buffer
  // nextionReturnBuffer[nextionReturnIndex]
  // Return: true if we've received a string of 3 consecutive 0xFF values
  // Return: false otherwise
  bool nextionCommandComplete = false;
  static int nextionTermByteCnt = 0;   // counter for our 3 consecutive 0xFFs
  static String hmiDebug = "HMI IN: "; // assemble a string for debug output

  if (Serial.available())
  {
    lcdConnected = true;
    byte nextionCommandByte = Serial.read();
    hmiDebug += (" 0x" + String(nextionCommandByte, HEX));
    // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
    if (nextionCommandByte == 0xFF)
    {
      nextionTermByteCnt++;
      if (nextionTermByteCnt >= 3)
      { // We have received a complete command
        nextionCommandComplete = true;
        nextionTermByteCnt = 0; // reset counter
      }
    }
    else
    {
      nextionTermByteCnt = 0; // reset counter if a non-term byte was encountered
    }
    nextionReturnBuffer[nextionReturnIndex] = nextionCommandByte;
    nextionReturnIndex++;
  }
  if (nextionCommandComplete)
  {
    debugPrintln(hmiDebug);
    hmiDebug = "HMI IN: ";
  }
  return nextionCommandComplete;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionProcessInput()
{ // Process incoming serial commands from the Nextion panel
  // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
  // tl;dr, command byte, command data, 0xFF 0xFF 0xFF

  if (nextionReturnBuffer[0] == 0x65)
  { // Handle incoming touch command
    // 0x65+Page ID+Component ID+TouchEvent+End
    // Return this data when the touch event created by the user is pressed.
    // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
    // Example: 0x65 0x00 0x02 0x01 0xFF 0xFF 0xFF
    // Meaning: Touch Event, Page 0, Object 2, Press
    String nextionPage = String(nextionReturnBuffer[1]);
    String nextionButtonID = String(nextionReturnBuffer[2]);
    byte nextionButtonAction = nextionReturnBuffer[3];

    if (nextionButtonAction == 0x01)
    {
      debugPrintln(String(F("HMI IN: [Button ON] 'p[")) + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln(String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'ON'");
      mqttClient.publish(mqttButtonTopic, "ON");
      String mqttButtonJSONEvent = String(F("{\"event\":\"p[")) + String(nextionPage) + String(F("].b[")) + String(nextionButtonID) + String(F("]\", \"value\":\"ON\"}"));
      mqttClient.publish(mqttStateJSONTopic, mqttButtonJSONEvent);
    }
    if (nextionButtonAction == 0x00)
    {
      debugPrintln(String(F("HMI IN: [Button OFF] 'p[")) + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln(String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'OFF'");
      mqttClient.publish(mqttButtonTopic, "OFF");
      // Now see if this object has a .val that might have been updated.  Works for sliders,
      // two-state buttons, etc, throws a 0x1A error for normal buttons which we'll catch and ignore
      mqttGetSubtopic = "/p[" + nextionPage + "].b[" + nextionButtonID + "].val";
      mqttGetSubtopicJSON = "p[" + nextionPage + "].b[" + nextionButtonID + "].val";
      nextionGetAttr("p[" + nextionPage + "].b[" + nextionButtonID + "].val");
    }
  }
  else if (nextionReturnBuffer[0] == 0x66)
  { // Handle incoming "sendme" page number
    // 0x66+PageNum+End
    // Example: 0x66 0x02 0xFF 0xFF 0xFF
    // Meaning: page 2
    String nextionPage = String(nextionReturnBuffer[1]);
    debugPrintln(String(F("HMI IN: [sendme Page] '")) + nextionPage + "'");
    if ((nextionActivePage != nextionPage.toInt()) && ((nextionPage != "0") || nextionReportPage0))
    { // If we have a new page AND ( (it's not "0") OR (we've set the flag to report 0 anyway) )
      nextionActivePage = nextionPage.toInt();
      String mqttPageTopic = mqttStateTopic + "/page";
      debugPrintln(String(F("MQTT OUT: '")) + mqttPageTopic + "' : '" + nextionPage + "'");
      mqttClient.publish(mqttPageTopic, nextionPage);
    }
  }
  else if (nextionReturnBuffer[0] == 0x67)
  { // Handle touch coordinate data
    // 0X67+Coordinate X High+Coordinate X Low+Coordinate Y High+Coordinate Y Low+TouchEvent+End
    // Example: 0X67 0X00 0X7A 0X00 0X1E 0X01 0XFF 0XFF 0XFF
    // Meaning: Coordinate (122,30), Touch Event: Press
    // issue Nextion command "sendxy=1" to enable this output
    uint16_t xCoord = nextionReturnBuffer[1];
    xCoord = xCoord * 256 + nextionReturnBuffer[2];
    uint16_t yCoord = nextionReturnBuffer[3];
    yCoord = yCoord * 256 + nextionReturnBuffer[4];
    String xyCoord = String(xCoord) + ',' + String(yCoord);
    byte nextionTouchAction = nextionReturnBuffer[5];
    if (nextionTouchAction == 0x01)
    {
      debugPrintln(String(F("HMI IN: [Touch ON] '")) + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOn";
      debugPrintln(String(F("MQTT OUT: '")) + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic, xyCoord);
    }
    else if (nextionTouchAction == 0x00)
    {
      debugPrintln(String(F("HMI IN: [Touch OFF] '")) + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOff";
      debugPrintln(String(F("MQTT OUT: '")) + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic, xyCoord);
    }
  }
  else if (nextionReturnBuffer[0] == 0x70)
  { // Handle get string return
    // 0x70+ASCII string+End
    // Example: 0x70 0x41 0x42 0x43 0x44 0x31 0x32 0x33 0x34 0xFF 0xFF 0xFF
    // Meaning: String data, ABCD1234
    String getString;
    for (int i = 1; i < nextionReturnIndex - 3; i++)
    { // convert the payload into a string
      getString += (char)nextionReturnBuffer[i];
    }
    debugPrintln(String(F("HMI IN: [String Return] '")) + getString + "'");
    if (mqttGetSubtopic == "")
    { // If there's no outstanding request for a value, publish to mqttStateTopic
      debugPrintln(String(F("MQTT OUT: '")) + mqttStateTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttStateTopic, getString);
    }
    else
    { // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      debugPrintln(String(F("MQTT OUT: '")) + mqttReturnTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttReturnTopic, getString);
      mqttGetSubtopic = "";
    }
  }
  else if (nextionReturnBuffer[0] == 0x71)
  { // Handle get int return
    // 0x71+byte1+byte2+byte3+byte4+End (4 byte little endian)
    // Example: 0x71 0x7B 0x00 0x00 0x00 0xFF 0xFF 0xFF
    // Meaning: Integer data, 123
    unsigned long getInt = nextionReturnBuffer[4];
    getInt = getInt * 256 + nextionReturnBuffer[3];
    getInt = getInt * 256 + nextionReturnBuffer[2];
    getInt = getInt * 256 + nextionReturnBuffer[1];
    String getString = String(getInt);
    debugPrintln(String(F("HMI IN: [Int Return] '")) + getString + "'");

    if (lcdVersionQueryFlag)
    {
      lcdVersion = getInt;
      lcdVersionQueryFlag = false;
      debugPrintln(String(F("HMI IN: lcdVersion '")) + String(lcdVersion) + "'");
    }
    else if (mqttGetSubtopic == "")
    {
      mqttClient.publish(mqttStateTopic, getString);
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else
    {
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      mqttClient.publish(mqttReturnTopic, getString);
      String mqttButtonJSONEvent = String(F("{\"event\":\"")) + mqttGetSubtopicJSON + String(F("\", \"value\":")) + getString + String(F("}"));
      mqttClient.publish(mqttStateJSONTopic, mqttButtonJSONEvent);
      mqttGetSubtopic = "";
    }
  }
  else if (nextionReturnBuffer[0] == 0x63 && nextionReturnBuffer[1] == 0x6f && nextionReturnBuffer[2] == 0x6d && nextionReturnBuffer[3] == 0x6f && nextionReturnBuffer[4] == 0x6b)
  { // Catch 'comok' response to 'connect' command: https://www.itead.cc/blog/nextion-hmi-upload-protocol
    String comokField;
    uint8_t comokFieldCount = 0;
    byte comokFieldSeperator = 0x2c; // ","

    for (uint8_t i = 0; i <= nextionReturnIndex; i++)
    { // cycle through each byte looking for our field seperator
      if (nextionReturnBuffer[i] == comokFieldSeperator)
      { // Found the end of a field, so do something with it.  Maybe.
        if (comokFieldCount == 2)
        {
          nextionModel = comokField;
          debugPrintln(String(F("HMI IN: nextionModel: ")) + nextionModel);
        }
        comokFieldCount++;
        comokField = "";
      }
      else
      {
        comokField += String(char(nextionReturnBuffer[i]));
      }
    }
  }

  else if (nextionReturnBuffer[0] == 0x1A)
  { // Catch 0x1A error, possibly from .val query against things that might not support that request
    // 0x1A+End
    // ERROR: Variable name invalid
    // We'll be triggering this a lot due to requesting .val on every component that sends us a Touch Off
    // Just reset mqttGetSubtopic and move on with life.
    mqttGetSubtopic = "";
  }
  nextionReturnIndex = 0; // Done handling the buffer, reset index back to 0
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionSetAttr(String hmiAttribute, String hmiValue)
{ // Set the value of a Nextion component attribute
  Serial1.print(hmiAttribute);
  Serial1.print("=");
  Serial1.print(utf8ascii(hmiValue));
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  debugPrintln(String(F("HMI OUT: '")) + hmiAttribute + "=" + hmiValue + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionGetAttr(String hmiAttribute)
{ // Get the value of a Nextion component attribute
  // This will only send the command to the panel requesting the attribute, the actual
  // return of that value will be handled by nextionProcessInput and placed into mqttGetSubtopic
  Serial1.print("get " + hmiAttribute);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  debugPrintln(String(F("HMI OUT: 'get ")) + hmiAttribute + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionSendCmd(String nextionCmd)
{ // Send a raw command to the Nextion panel
  Serial1.print(utf8ascii(nextionCmd));
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  debugPrintln(String(F("HMI OUT: ")) + nextionCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionParseJson(String &strPayload)
{ // Parse an incoming JSON array into individual Nextion commands
  if (strPayload.endsWith(",]"))
  { // Trailing null array elements are an artifact of older Home Assistant automations and need to
    // be removed before parsing by ArduinoJSON 6+
    strPayload.remove(strPayload.length() - 2, 2);
    strPayload.concat("]");
  }
  DynamicJsonDocument nextionCommands(mqttMaxPacketSize + 1024);
  DeserializationError jsonError = deserializeJson(nextionCommands, strPayload);
  if (jsonError)
  { // Couldn't parse incoming JSON command
    debugPrintln(String(F("MQTT: [ERROR] Failed to parse incoming JSON command with error: ")) + String(jsonError.c_str()));
  }
  else
  {
    deserializeJson(nextionCommands, strPayload);
    for (uint8_t i = 0; i < nextionCommands.size(); i++)
    {
      nextionSendCmd(nextionCommands[i]);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionStartOtaDownload(String otaUrl)
{ // Upload firmware to the Nextion LCD via HTTP download
  // based in large part on code posted by indev2 here:
  // http://support.iteadstudio.com/support/discussions/topics/11000007686/page/2

  uint32_t lcdOtaFileSize = 0;
  String lcdOtaNextionCmd;
  uint32_t lcdOtaChunkCounter = 0;
  uint16_t lcdOtaPartNum = 0;
  uint32_t lcdOtaTransferred = 0;
  uint8_t lcdOtaPercentComplete = 0;
  const uint32_t lcdOtaTimeout = 30000; // timeout for receiving new data in milliseconds
  static uint32_t lcdOtaTimer = 0;      // timer for upload timeout

  debugPrintln(String(F("LCD OTA: Attempting firmware download from: ")) + otaUrl);
  WiFiClient lcdOtaWifi;
  HTTPClient lcdOtaHttp;
  lcdOtaHttp.begin(lcdOtaWifi, otaUrl);
  int lcdOtaHttpReturn = lcdOtaHttp.GET();
  if (lcdOtaHttpReturn > 0)
  { // HTTP header has been sent and Server response header has been handled
    debugPrintln(String(F("LCD OTA: HTTP GET return code:")) + String(lcdOtaHttpReturn));
    if (lcdOtaHttpReturn == HTTP_CODE_OK)
    {                                                 // file found at server
      int32_t lcdOtaRemaining = lcdOtaHttp.getSize(); // get length of document (is -1 when Server sends no Content-Length header)
      lcdOtaFileSize = lcdOtaRemaining;
      static uint16_t lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
      static const uint16_t lcdOtaBufferSize = 1024; // upload data buffer before sending to UART
      static uint8_t lcdOtaBuffer[lcdOtaBufferSize] = {};

      debugPrintln(String(F("LCD OTA: File found at Server. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));

      WiFiUDP::stopAll(); // Keep mDNS responder and MQTT traffic from breaking things
      if (mqttClient.connected())
      {
        debugPrintln(F("LCD OTA: LCD firmware upload starting, closing MQTT connection."));
        mqttClient.publish(mqttStatusTopic, "OFF");
        mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}");
        mqttClient.disconnect();
      }

      WiFiClient *stream = lcdOtaHttp.getStreamPtr();      // get tcp stream
      Serial1.write(nextionSuffix, sizeof(nextionSuffix)); // Send empty command
      Serial1.flush();
      nextionHandleInput();
      String lcdOtaNextionCmd = "whmi-wri " + String(lcdOtaFileSize) + ",115200,0";
      debugPrintln(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
      Serial1.print(lcdOtaNextionCmd);
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();

      if (nextionOtaResponse())
      {
        debugPrintln(F("LCD OTA: LCD upload command accepted."));
      }
      else
      {
        debugPrintln(F("LCD OTA: LCD upload command FAILED.  Restarting device."));
        espReset();
      }
      debugPrintln(F("LCD OTA: Starting update"));
      lcdOtaTimer = millis();
      while (lcdOtaHttp.connected() && (lcdOtaRemaining > 0 || lcdOtaRemaining == -1))
      {                                                // Write incoming data to panel as it arrives
        uint16_t lcdOtaHttpSize = stream->available(); // get available data size

        if (lcdOtaHttpSize)
        {
          uint16_t lcdOtaChunkSize = 0;
          if ((lcdOtaHttpSize <= lcdOtaBufferSize) && (lcdOtaHttpSize <= (4096 - lcdOtaChunkCounter)))
          {
            lcdOtaChunkSize = lcdOtaHttpSize;
          }
          else if ((lcdOtaBufferSize <= lcdOtaHttpSize) && (lcdOtaBufferSize <= (4096 - lcdOtaChunkCounter)))
          {
            lcdOtaChunkSize = lcdOtaBufferSize;
          }
          else
          {
            lcdOtaChunkSize = 4096 - lcdOtaChunkCounter;
          }
          stream->readBytes(lcdOtaBuffer, lcdOtaChunkSize);
          Serial1.flush();                              // make sure any previous writes the UART have completed
          Serial1.write(lcdOtaBuffer, lcdOtaChunkSize); // now send buffer to the UART
          lcdOtaChunkCounter += lcdOtaChunkSize;
          if (lcdOtaChunkCounter >= 4096)
          {
            Serial1.flush();
            lcdOtaPartNum++;
            lcdOtaTransferred += lcdOtaChunkCounter;
            lcdOtaPercentComplete = (lcdOtaTransferred * 100) / lcdOtaFileSize;
            lcdOtaChunkCounter = 0;
            if (nextionOtaResponse())
            { // We've completed a chunk
              debugPrintln(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" OK, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
              lcdOtaTimer = millis();
            }
            else
            {
              debugPrintln(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" FAILED, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
              debugPrintln(F("LCD OTA: failure"));
              delay(2000); // extra delay while the LCD does its thing
              espReset();
            }
          }
          else
          {
            delay(20);
          }
          if (lcdOtaRemaining > 0)
          {
            lcdOtaRemaining -= lcdOtaChunkSize;
          }
        }
        delay(10);
        if ((lcdOtaTimer > 0) && ((millis() - lcdOtaTimer) > lcdOtaTimeout))
        { // Our timer expired so reset
          debugPrintln(F("LCD OTA: ERROR: LCD upload timeout.  Restarting."));
          espReset();
        }
      }
      lcdOtaPartNum++;
      lcdOtaTransferred += lcdOtaChunkCounter;
      if ((lcdOtaTransferred == lcdOtaFileSize) && nextionOtaResponse())
      {
        debugPrintln(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: Failure"));
        espReset();
      }
    }
  }
  else
  {
    debugPrintln(String(F("LCD OTA: HTTP GET failed, error code ")) + lcdOtaHttp.errorToString(lcdOtaHttpReturn));
    espReset();
  }
  lcdOtaHttp.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool nextionOtaResponse()
{ // Monitor the serial port for a 0x05 response within our timeout

  unsigned long nextionCommandTimeout = 2000;   // timeout for receiving termination string in milliseconds
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout
  bool otaSuccessVal = false;
  while ((millis() - nextionCommandTimer) < nextionCommandTimeout)
  {
    if (Serial.available())
    {
      byte inByte = Serial.read();
      if (inByte == 0x5)
      {
        otaSuccessVal = true;
        break;
      }
      else
      {
        // debugPrintln(String(inByte, HEX));
      }
    }
    else
    {
      delay(1);
    }
  }
  // delay(50);
  return otaSuccessVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionConnect()
{
  if ((millis() - nextionCheckTimer) >= nextionCheckInterval)
  {
    static unsigned int nextionRetryCount = 0;
    if ((nextionModel.length() == 0) && (nextionRetryCount < (nextionRetryMax - 2)))
    { // Try issuing the "connect" command a few times
      debugPrintln(F("HMI: sending Nextion connect request"));
      nextionSendCmd("connect");
      nextionRetryCount++;
      nextionCheckTimer = millis();
    }
    else if ((nextionModel.length() == 0) && (nextionRetryCount < nextionRetryMax))
    { // If we still don't have model info, try to change nextion serial speed from 9600 to 115200
      nextionSetSpeed();
      nextionRetryCount++;
      debugPrintln(F("HMI: sending Nextion serial speed 115200 request"));
      nextionCheckTimer = millis();
    }
    else if ((lcdVersion < 1) && (nextionRetryCount <= nextionRetryMax))
    {
      if (nextionModel.length() == 0)
      { // one last hail mary, maybe the serial speed is set correctly now
        nextionSendCmd("connect");
      }
      nextionSendCmd("get " + lcdVersionQuery);
      lcdVersionQueryFlag = true;
      nextionRetryCount++;
      debugPrintln(F("HMI: sending Nextion version query"));
      nextionCheckTimer = millis();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionSetSpeed()
{
  debugPrintln(F("HMI: No Nextion response, attempting 9600bps connection"));
  Serial1.begin(9600);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.print("bauds=115200");
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  Serial1.begin(115200);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void nextionReset()
{
  debugPrintln(F("HMI: Rebooting LCD"));
  digitalWrite(nextionResetPin, LOW);
  Serial1.print("rest");
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  delay(100);
  digitalWrite(nextionResetPin, HIGH);

  unsigned long lcdResetTimer = millis();
  const unsigned long lcdResetTimeout = 5000;

  lcdConnected = false;
  while (!lcdConnected && (millis() < (lcdResetTimer + lcdResetTimeout)))
  {
    nextionHandleInput();
  }
  if (lcdConnected)
  {
    debugPrintln(F("HMI: Rebooting LCD completed"));
    if (nextionActivePage)
    {
      nextionSendCmd("page " + String(nextionActivePage));
    }
  }
  else
  {
    debugPrintln(F("ERROR: Rebooting LCD completed, but LCD is not responding."));
  }
  mqttClient.publish(mqttStatusTopic, "OFF");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiSetup()
{ // Connect to WiFi
  nextionSendCmd("page 0");
  nextionSetAttr("p[0].b[1].txt", "\"WiFi Connecting\"");

  // Read our MAC address and save it to espMac
  WiFi.macAddress(espMac);
  // Assign our hostname before connecting to WiFi
  WiFi.hostname(haspNode);
  // Tell WiFi to autoreconnect if connection has dropped
  WiFi.setAutoReconnect(true);

  if (String(wifiSSID) == "")
  { // If the sketch has no defined a static wifiSSID to connect to,
    // use WiFiManager to collect required information from the user.

    // id/name, placeholder/prompt, default value, length, extra tags
    WiFiManagerParameter custom_haspNodeHeader("<br/><br/><b>HASP Node Name</b>");
    WiFiManagerParameter custom_haspNode("haspNode", "HASP Node (required. lowercase letters, numbers, and _ only)", haspNode, 15, " maxlength=15 required pattern='[a-z0-9_]*'");
    WiFiManagerParameter custom_groupName("groupName", "Group Name (required)", groupName, 15, " maxlength=15 required");
    WiFiManagerParameter custom_mqttHeader("<br/><br/><b>MQTT Broker</b>");
    WiFiManagerParameter custom_mqttServer("mqttServer", "MQTT Server", mqttServer, 63, " maxlength=39");
    WiFiManagerParameter custom_mqttPort("mqttPort", "MQTT Port", mqttPort, 5, " maxlength=5 type='number'");
    WiFiManagerParameter custom_mqttUser("mqttUser", "MQTT User", mqttUser, 31, " maxlength=31");
    WiFiManagerParameter custom_mqttPassword("mqttPassword", "MQTT Password", mqttPassword, 31, " maxlength=31 type='password'");
    WiFiManagerParameter custom_configHeader("<br/><br/><b>Admin access</b>");
    WiFiManagerParameter custom_configUser("configUser", "Config User", configUser, 15, " maxlength=31'");
    WiFiManagerParameter custom_configPassword("configPassword", "Config Password", configPassword, 31, " maxlength=31 type='password'");

    // WiFiManager local initialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    wifiManager.setSaveConfigCallback(configSaveCallback); // set config save notify callback
    wifiManager.setCustomHeadElement(HASP_STYLE);          // add custom style

    // Add all your parameters here
    wifiManager.addParameter(&custom_haspNodeHeader);
    wifiManager.addParameter(&custom_haspNode);
    wifiManager.addParameter(&custom_groupName);
    wifiManager.addParameter(&custom_mqttHeader);
    wifiManager.addParameter(&custom_mqttServer);
    wifiManager.addParameter(&custom_mqttPort);
    wifiManager.addParameter(&custom_mqttUser);
    wifiManager.addParameter(&custom_mqttPassword);
    wifiManager.addParameter(&custom_configHeader);
    wifiManager.addParameter(&custom_configUser);
    wifiManager.addParameter(&custom_configPassword);

    // Timeout config portal after connectTimeout seconds, useful if configured wifi network was temporarily unavailable
    wifiManager.setTimeout(connectTimeout);

    wifiManager.setAPCallback(espWifiConfigCallback);

    // Construct AP name
    char espMac5[1];
    sprintf(espMac5, "%02x", espMac[5]);
    String strWifiConfigAP = String(haspNode).substring(0, 9) + "-" + String(espMac5);
    strWifiConfigAP.toCharArray(wifiConfigAP, (strWifiConfigAP.length() + 1));
    // Construct a WiFi SSID password using bytes [3] and [4] of our MAC
    char espMac34[2];
    sprintf(espMac34, "%02x%02x", espMac[3], espMac[4]);
    String strConfigPass = "hasp" + String(espMac34);
    strConfigPass.toCharArray(wifiConfigPass, 9);

    // Fetches SSID and pass from EEPROM and tries to connect
    // If it does not connect it starts an access point with the specified name
    // and goes into a blocking loop awaiting configuration.
    if (!wifiManager.autoConnect(wifiConfigAP, wifiConfigPass))
    { // Reset and try again
      debugPrintln(F("WIFI: Failed to connect and hit timeout"));
      espReset();
    }

    // Read updated parameters
    strcpy(mqttServer, custom_mqttServer.getValue());
    strcpy(mqttPort, custom_mqttPort.getValue());
    strcpy(mqttUser, custom_mqttUser.getValue());
    strcpy(mqttPassword, custom_mqttPassword.getValue());
    strcpy(haspNode, custom_haspNode.getValue());
    strcpy(groupName, custom_groupName.getValue());
    strcpy(configUser, custom_configUser.getValue());
    strcpy(configPassword, custom_configPassword.getValue());

    if (shouldSaveConfig)
    { // Save the custom parameters to FS
      configSave();
    }
  }
  else
  { // wifiSSID has been defined, so attempt to connect to it forever
    debugPrintln(String(F("Connecting to WiFi network: ")) + String(wifiSSID));
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPass);

    unsigned long wifiReconnectTimer = millis();
    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
      if (millis() >= (wifiReconnectTimer + (connectTimeout * 1000)))
      { // If we've been trying to reconnect for connectTimeout seconds, reboot and try again
        debugPrintln(F("WIFI: Failed to connect and hit timeout"));
        espReset();
      }
    }
  }
  // If you get here you have connected to WiFi
  nextionSetAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\"");
  debugPrintln(String(F("WIFI: Connected successfully and assigned IP: ")) + WiFi.localIP().toString());
  if (nextionActivePage)
  {
    nextionSendCmd("page " + String(nextionActivePage));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiReconnect()
{ // Existing WiFi connection dropped, try to reconnect
  debugPrintln(F("Reconnecting to WiFi network..."));
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPass);

  unsigned long wifiReconnectTimer = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    if (millis() >= (wifiReconnectTimer + (reConnectTimeout * 1000)))
    { // If we've been trying to reconnect for reConnectTimeout seconds, reboot and try again
      debugPrintln(F("WIFI: Failed to reconnect and hit timeout"));
      espReset();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espWifiConfigCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debugPrintln(F("WIFI: Failed to connect to assigned AP, entering config mode"));
  while (millis() < 800)
  { // for factory-reset system this will be called before display is responsive. give it a second.
    delay(10);
  }
  nextionSendCmd("page 0");
  nextionSetAttr("p[0].b[1].txt", "\"Configure HASP:\\rAP:" + String(wifiConfigAP) + "\\rPass:" + String(wifiConfigPass) + "\\r\\r\\r\\r\\r\\r\\rWeb:192.168.4.1\"");
  nextionSetAttr("p[0].b[3].txt", "\"WIFI:S:" + String(wifiConfigAP) + ";T:WPA;P:" + String(wifiConfigPass) + ";;\"");
  nextionSendCmd("vis 3,1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espSetupOta()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(haspNode);
  ArduinoOTA.setPassword(configPassword);

  ArduinoOTA.onStart([]() {
    debugPrintln(F("ESP OTA: update start"));
    nextionSendCmd("page 0");
    nextionSetAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
  });
  ArduinoOTA.onEnd([]() {
    nextionSendCmd("page 0");
    debugPrintln(F("ESP OTA: update complete"));
    nextionSetAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
    espReset();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    nextionSetAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    debugPrintln(String(F("ESP OTA: ERROR code ")) + String(error));
    if (error == OTA_AUTH_ERROR)
      debugPrintln(F("ESP OTA: ERROR - Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)
      debugPrintln(F("ESP OTA: ERROR - Begin Failed"));
    else if (error == OTA_CONNECT_ERROR)
      debugPrintln(F("ESP OTA: ERROR - Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR)
      debugPrintln(F("ESP OTA: ERROR - Receive Failed"));
    else if (error == OTA_END_ERROR)
      debugPrintln(F("ESP OTA: ERROR - End Failed"));
    nextionSetAttr("p[0].b[1].txt", "\"ESP OTA FAILED\"");
    delay(5000);
    nextionSendCmd("page " + String(nextionActivePage));
  });
  ArduinoOTA.begin();
  debugPrintln(F("ESP OTA: Over the Air firmware update ready"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espStartOta(String espOtaUrl)
{ // Update ESP firmware from HTTP
  nextionSendCmd("page 0");
  nextionSetAttr("p[0].b[1].txt", "\"HTTP update\\rstarting...\"");
  WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

  t_httpUpdate_return returnCode = ESPhttpUpdate.update(wifiClient, espOtaUrl);
  switch (returnCode)
  {
  case HTTP_UPDATE_FAILED:
    debugPrintln("ESPFW: HTTP_UPDATE_FAILED error " + String(ESPhttpUpdate.getLastError()) + " " + ESPhttpUpdate.getLastErrorString());
    nextionSetAttr("p[0].b[1].txt", "\"HTTP Update\\rFAILED\"");
    break;

  case HTTP_UPDATE_NO_UPDATES:
    debugPrintln(F("ESPFW: HTTP_UPDATE_NO_UPDATES"));
    nextionSetAttr("p[0].b[1].txt", "\"HTTP Update\\rNo update\"");
    break;

  case HTTP_UPDATE_OK:
    debugPrintln(F("ESPFW: HTTP_UPDATE_OK"));
    nextionSetAttr("p[0].b[1].txt", "\"HTTP Update\\rcomplete!\\r\\rRestarting.\"");
    espReset();
  }
  delay(5000);
  nextionSendCmd("page " + String(nextionActivePage));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void espReset()
{
  debugPrintln(F("RESET: HASP reset"));
  if (mqttClient.connected())
  {
    mqttClient.publish(mqttStatusTopic, "OFF");
    mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}");
    mqttClient.disconnect();
  }
  nextionReset();
  ESP.reset();
  delay(5000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configRead()
{ // Read saved config.json from SPIFFS
  debugPrintln(F("SPIFFS: mounting SPIFFS"));
  if (SPIFFS.begin())
  {
    if (SPIFFS.exists("/config.json"))
    { // File exists, reading and loading
      debugPrintln(F("SPIFFS: reading /config.json"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        size_t configFileSize = configFile.size(); // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[configFileSize]);
        configFile.readBytes(buf.get(), configFileSize);

        DynamicJsonDocument configJson(1024);
        DeserializationError jsonError = deserializeJson(configJson, buf.get());
        if (jsonError)
        { // Couldn't parse the saved config
          debugPrintln(String(F("SPIFFS: [ERROR] Failed to parse /config.json: ")) + String(jsonError.c_str()));
        }
        else
        {
          if (!configJson["mqttServer"].isNull())
          {
            strcpy(mqttServer, configJson["mqttServer"]);
          }
          if (!configJson["mqttPort"].isNull())
          {
            strcpy(mqttPort, configJson["mqttPort"]);
          }
          if (!configJson["mqttUser"].isNull())
          {
            strcpy(mqttUser, configJson["mqttUser"]);
          }
          if (!configJson["mqttPassword"].isNull())
          {
            strcpy(mqttPassword, configJson["mqttPassword"]);
          }
          if (!configJson["haspNode"].isNull())
          {
            strcpy(haspNode, configJson["haspNode"]);
          }
          if (!configJson["groupName"].isNull())
          {
            strcpy(groupName, configJson["groupName"]);
          }
          if (!configJson["configUser"].isNull())
          {
            strcpy(configUser, configJson["configUser"]);
          }
          if (!configJson["configPassword"].isNull())
          {
            strcpy(configPassword, configJson["configPassword"]);
          }
          if (!configJson["motionPinConfig"].isNull())
          {
            strcpy(motionPinConfig, configJson["motionPinConfig"]);
          }
          if (!configJson["debugSerialEnabled"].isNull())
          {
            if (configJson["debugSerialEnabled"])
            {
              debugSerialEnabled = true;
            }
            else
            {
              debugSerialEnabled = false;
            }
          }
          if (!configJson["debugTelnetEnabled"].isNull())
          {
            if (configJson["debugTelnetEnabled"])
            {
              debugTelnetEnabled = true;
            }
            else
            {
              debugTelnetEnabled = false;
            }
          }
          if (!configJson["mdnsEnabled"].isNull())
          {
            if (configJson["mdnsEnabled"])
            {
              mdnsEnabled = true;
            }
            else
            {
              mdnsEnabled = false;
            }
          }
          String configJsonStr;
          serializeJson(configJson, configJsonStr);
          debugPrintln(String(F("SPIFFS: parsed json:")) + configJsonStr);
        }
      }
      else
      {
        debugPrintln(F("SPIFFS: [ERROR] Failed to read /config.json"));
      }
    }
    else
    {
      debugPrintln(F("SPIFFS: [WARNING] /config.json not found, will be created on first config save"));
    }
  }
  else
  {
    debugPrintln(F("SPIFFS: [ERROR] Failed to mount FS"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configSaveCallback()
{ // Callback notifying us of the need to save config
  debugPrintln(F("SPIFFS: Configuration changed, flagging for save"));
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configSave()
{ // Save the custom parameters to config.json
  nextionSetAttr("p[0].b[1].txt", "\"Saving\\rconfig\"");
  debugPrintln(F("SPIFFS: Saving config"));
  DynamicJsonDocument jsonConfigValues(1024);
  jsonConfigValues["mqttServer"] = mqttServer;
  jsonConfigValues["mqttPort"] = mqttPort;
  jsonConfigValues["mqttUser"] = mqttUser;
  jsonConfigValues["mqttPassword"] = mqttPassword;
  jsonConfigValues["haspNode"] = haspNode;
  jsonConfigValues["groupName"] = groupName;
  jsonConfigValues["configUser"] = configUser;
  jsonConfigValues["configPassword"] = configPassword;
  jsonConfigValues["motionPinConfig"] = motionPinConfig;
  jsonConfigValues["debugSerialEnabled"] = debugSerialEnabled;
  jsonConfigValues["debugTelnetEnabled"] = debugTelnetEnabled;
  jsonConfigValues["mdnsEnabled"] = mdnsEnabled;

  debugPrintln(String(F("SPIFFS: mqttServer = ")) + String(mqttServer));
  debugPrintln(String(F("SPIFFS: mqttPort = ")) + String(mqttPort));
  debugPrintln(String(F("SPIFFS: mqttUser = ")) + String(mqttUser));
  debugPrintln(String(F("SPIFFS: mqttPassword = ")) + String(mqttPassword));
  debugPrintln(String(F("SPIFFS: haspNode = ")) + String(haspNode));
  debugPrintln(String(F("SPIFFS: groupName = ")) + String(groupName));
  debugPrintln(String(F("SPIFFS: configUser = ")) + String(configUser));
  debugPrintln(String(F("SPIFFS: configPassword = ")) + String(configPassword));
  debugPrintln(String(F("SPIFFS: motionPinConfig = ")) + String(motionPinConfig));
  debugPrintln(String(F("SPIFFS: debugSerialEnabled = ")) + String(debugSerialEnabled));
  debugPrintln(String(F("SPIFFS: debugTelnetEnabled = ")) + String(debugTelnetEnabled));
  debugPrintln(String(F("SPIFFS: mdnsEnabled = ")) + String(mdnsEnabled));

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debugPrintln(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    serializeJson(jsonConfigValues, configFile);
    configFile.close();
  }
  shouldSaveConfig = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void configClearSaved()
{ // Clear out all local storage
  nextionSendCmd("page 0");
  nextionSetAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
  debugPrintln(F("RESET: Formatting SPIFFS"));
  SPIFFS.format();
  debugPrintln(F("RESET: Clearing WiFiManager settings..."));
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  EEPROM.begin(512);
  debugPrintln(F("Clearing EEPROM..."));
  for (uint16_t i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  nextionSetAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debugPrintln(F("RESET: Rebooting device"));
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleNotFound()
{ // webServer 404
  debugPrintln(String(F("HTTP: Sending 404 to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = "File Not Found\n\n";
  httpMessage += "URI: ";
  httpMessage += webServer.uri();
  httpMessage += "\nMethod: ";
  httpMessage += (webServer.method() == HTTP_GET) ? "GET" : "POST";
  httpMessage += "\nArguments: ";
  httpMessage += webServer.args();
  httpMessage += "\n";
  for (uint8_t i = 0; i < webServer.args(); i++)
  {
    httpMessage += " " + webServer.argName(i) + ": " + webServer.arg(i) + "\n";
  }
  webServer.send(404, "text/plain", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleRoot()
{ // http://plate01/
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending root page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode);
  httpMessage += String(F("</h1>"));

  httpMessage += String(F("<form method='POST' action='saveConfig'>"));
  httpMessage += String(F("<b>WiFi SSID</b> <i><small>(required)</small></i><input id='wifiSSID' required name='wifiSSID' maxlength=32 placeholder='WiFi SSID' value='")) + String(WiFi.SSID()) + "'>";
  httpMessage += String(F("<br/><b>WiFi Password</b> <i><small>(required)</small></i><input id='wifiPass' required name='wifiPass' type='password' maxlength=64 placeholder='WiFi Password' value='")) + String("********") + "'>";
  httpMessage += String(F("<br/><br/><b>HASP Node Name</b> <i><small>(required. lowercase letters, numbers, and _ only)</small></i><input id='haspNode' required name='haspNode' maxlength=15 placeholder='HASP Node Name' pattern='[a-z0-9_]*' value='")) + String(haspNode) + "'>";
  httpMessage += String(F("<br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='groupName' required name='groupName' maxlength=15 placeholder='Group Name' value='")) + String(groupName) + "'>";
  httpMessage += String(F("<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(mqttServer) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(mqttPort) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(mqttUser) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword' value='"));
  if (strlen(mqttPassword) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><br/><b>HASP Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(configUser) + "'>";
  httpMessage += String(F("<br/><b>HASP Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='"));
  if (strlen(configPassword) != 0)
  {
    httpMessage += String("********");
  }
  httpMessage += String(F("'><br/><hr><b>Motion Sensor Pin:&nbsp;</b><select id='motionPinConfig' name='motionPinConfig'>"));
  httpMessage += String(F("<option value='0'"));
  if (!motionPin)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">disabled/not installed</option><option value='D0'"));
  if (motionPin == D0)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">D0</option><option value='D1'"));
  if (motionPin == D1)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">D1</option><option value='D2'"));
  if (motionPin == D2)
  {
    httpMessage += String(F(" selected"));
  }
  httpMessage += String(F(">D2</option></select>"));

  httpMessage += String(F("<br/><b>Serial debug output enabled:</b><input id='debugSerialEnabled' name='debugSerialEnabled' type='checkbox'"));
  if (debugSerialEnabled)
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><b>Telnet debug output enabled:</b><input id='debugTelnetEnabled' name='debugTelnetEnabled' type='checkbox'"));
  if (debugTelnetEnabled)
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><b>mDNS enabled:</b><input id='mdnsEnabled' name='mdnsEnabled' type='checkbox'"));
  if (mdnsEnabled)
  {
    httpMessage += String(F(" checked='checked'"));
  }
  httpMessage += String(F("><br/><hr><button type='submit'>save settings</button></form>"));

  if (updateEspAvailable)
  {
    httpMessage += String(F("<br/><hr><font color='green'><center><h3>HASP Update available!</h3></center></font>"));
    httpMessage += String(F("<form method='get' action='espfirmware'>"));
    httpMessage += String(F("<input id='espFirmwareURL' type='hidden' name='espFirmware' value='")) + espFirmwareUrl + "'>";
    httpMessage += String(F("<button type='submit'>update HASP to v")) + String(updateEspAvailableVersion) + String(F("</button></form>"));
  }

  httpMessage += String(F("<hr><form method='get' action='firmware'>"));
  httpMessage += String(F("<button type='submit'>update firmware</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='reboot'>"));
  httpMessage += String(F("<button type='submit'>reboot device</button></form>"));

  httpMessage += String(F("<hr><form method='get' action='resetConfig'>"));
  httpMessage += String(F("<button type='submit'>factory reset settings</button></form>"));

  httpMessage += String(F("<hr><b>MQTT Status: </b>"));
  if (mqttClient.connected())
  { // Check MQTT connection
    httpMessage += String(F("Connected"));
  }
  else
  {
    httpMessage += String(F("<font color='red'><b>Disconnected</b></font>, return code: ")) + String(mqttClient.returnCode());
  }
  httpMessage += String(F("<br/><b>MQTT ClientID: </b>")) + String(mqttClientId);
  httpMessage += String(F("<br/><b>HASP Version: </b>")) + String(haspVersion);
  httpMessage += String(F("<br/><b>LCD Model: </b>")) + String(nextionModel);
  httpMessage += String(F("<br/><b>LCD Version: </b>")) + String(lcdVersion);
  httpMessage += String(F("<br/><b>LCD Active Page: </b>")) + String(nextionActivePage);
  httpMessage += String(F("<br/><b>CPU Frequency: </b>")) + String(ESP.getCpuFreqMHz()) + String(F("MHz"));
  httpMessage += String(F("<br/><b>Sketch Size: </b>")) + String(ESP.getSketchSize()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Free Sketch Space: </b>")) + String(ESP.getFreeSketchSpace()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Heap Free: </b>")) + String(ESP.getFreeHeap());
  httpMessage += String(F("<br/><b>Heap Fragmentation: </b>")) + String(ESP.getHeapFragmentation());
  httpMessage += String(F("<br/><b>ESP core version: </b>")) + String(ESP.getCoreVersion());
  httpMessage += String(F("<br/><b>IP Address: </b>")) + String(WiFi.localIP().toString());
  httpMessage += String(F("<br/><b>Signal Strength: </b>")) + String(WiFi.RSSI());
  httpMessage += String(F("<br/><b>Uptime: </b>")) + String(long(millis() / 1000));
  httpMessage += String(F("<br/><b>Last reset: </b>")) + String(ESP.getResetInfo());

  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleSaveConfig()
{ // http://plate01/saveConfig
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /saveConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);

  bool shouldSaveWifi = false;
  // Check required values
  if (webServer.arg("wifiSSID") != "" && webServer.arg("wifiSSID") != String(WiFi.SSID()))
  { // Handle WiFi update
    shouldSaveConfig = true;
    shouldSaveWifi = true;
    webServer.arg("wifiSSID").toCharArray(wifiSSID, 32);
    if (webServer.arg("wifiPass") != String("********"))
    {
      webServer.arg("wifiPass").toCharArray(wifiPass, 64);
    }
  }
  if (webServer.arg("mqttServer") != "" && webServer.arg("mqttServer") != String(mqttServer))
  { // Handle mqttServer
    shouldSaveConfig = true;
    webServer.arg("mqttServer").toCharArray(mqttServer, 64);
  }
  if (webServer.arg("mqttPort") != "" && webServer.arg("mqttPort") != String(mqttPort))
  { // Handle mqttPort
    shouldSaveConfig = true;
    webServer.arg("mqttPort").toCharArray(mqttPort, 6);
  }
  if (webServer.arg("haspNode") != "" && webServer.arg("haspNode") != String(haspNode))
  { // Handle haspNode
    shouldSaveConfig = true;
    String lowerHaspNode = webServer.arg("haspNode");
    lowerHaspNode.toLowerCase();
    lowerHaspNode.toCharArray(haspNode, 16);
  }
  if (webServer.arg("groupName") != "" && webServer.arg("groupName") != String(groupName))
  { // Handle groupName
    shouldSaveConfig = true;
    webServer.arg("groupName").toCharArray(groupName, 16);
  }
  // Check optional values
  if (webServer.arg("mqttUser") != String(mqttUser))
  { // Handle mqttUser
    shouldSaveConfig = true;
    webServer.arg("mqttUser").toCharArray(mqttUser, 32);
  }
  if (webServer.arg("mqttPassword") != String("********"))
  { // Handle mqttPassword
    shouldSaveConfig = true;
    webServer.arg("mqttPassword").toCharArray(mqttPassword, 32);
  }
  if (webServer.arg("configUser") != String(configUser))
  { // Handle configUser
    shouldSaveConfig = true;
    webServer.arg("configUser").toCharArray(configUser, 32);
  }
  if (webServer.arg("configPassword") != String("********"))
  { // Handle configPassword
    shouldSaveConfig = true;
    webServer.arg("configPassword").toCharArray(configPassword, 32);
  }
  if (webServer.arg("motionPinConfig") != String(motionPinConfig))
  { // Handle motionPinConfig
    shouldSaveConfig = true;
    webServer.arg("motionPinConfig").toCharArray(motionPinConfig, 3);
  }
  if ((webServer.arg("debugSerialEnabled") == String("on")) && !debugSerialEnabled)
  { // debugSerialEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    debugSerialEnabled = true;
  }
  else if ((webServer.arg("debugSerialEnabled") == String("")) && debugSerialEnabled)
  { // debugSerialEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    debugSerialEnabled = false;
  }
  if ((webServer.arg("debugTelnetEnabled") == String("on")) && !debugTelnetEnabled)
  { // debugTelnetEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    debugTelnetEnabled = true;
  }
  else if ((webServer.arg("debugTelnetEnabled") == String("")) && debugTelnetEnabled)
  { // debugTelnetEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    debugTelnetEnabled = false;
  }
  if ((webServer.arg("mdnsEnabled") == String("on")) && !mdnsEnabled)
  { // mdnsEnabled was disabled but should now be enabled
    shouldSaveConfig = true;
    mdnsEnabled = true;
  }
  else if ((webServer.arg("mdnsEnabled") == String("")) && mdnsEnabled)
  { // mdnsEnabled was enabled but should now be disabled
    shouldSaveConfig = true;
    mdnsEnabled = false;
  }

  if (shouldSaveConfig)
  { // Config updated, notify user and trigger write to SPIFFS
    httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
    httpMessage += FPSTR(HTTP_HEAD_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
    httpMessage += String(F("<br/>Saving updated configuration values and restarting device"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);

    configSave();
    if (shouldSaveWifi)
    {
      debugPrintln(String(F("CONFIG: Attempting connection to SSID: ")) + webServer.arg("wifiSSID"));
      espWifiSetup();
    }
    espReset();
  }
  else
  { // No change found, notify user and link back to config page
    httpMessage += String(F("<meta http-equiv='refresh' content='3;url=/' />"));
    httpMessage += FPSTR(HTTP_HEAD_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
    httpMessage += String(F("<br/>No changes found, returning to <a href='/'>home page</a>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleResetConfig()
{ // http://plate01/resetConfig
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /resetConfig page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", String(haspNode));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEAD_END);

  if (webServer.arg("confirm") == "yes")
  { // User has confirmed, so reset everything
    httpMessage += String(F("<h1>"));
    httpMessage += String(haspNode);
    httpMessage += String(F("</h1><b>Resetting all saved settings and restarting device into WiFi AP mode</b>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
    delay(1000);
    configClearSaved();
  }
  else
  {
    httpMessage += String(F("<h1>Warning</h1><b>This process will reset all settings to the default values and restart the device.  You may need to connect to the WiFi AP displayed on the panel to re-configure the device before accessing it again."));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='resetConfig'>"));
    httpMessage += String(F("<br/><br/><button type='submit' name='confirm' value='yes'>reset all settings</button></form>"));
    httpMessage += String(F("<br/><hr><br/><form method='get' action='/'>"));
    httpMessage += String(F("<button type='submit'>return home</button></form>"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleFirmware()
{ // http://plate01/firmware
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /firmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" firmware</h1>"));

  // Display main firmware page
  // HTTPS Disabled pending resolution of issue: https://github.com/esp8266/Arduino/issues/4696
  // Until then, using a proxy host at http://haswitchplate.com to deliver unsecured firmware images from GitHub
  httpMessage += String(F("<form method='get' action='/espfirmware'>"));
  if (updateEspAvailable)
  {
    httpMessage += String(F("<font color='green'><b>HASP ESP8266 update available!</b></font>"));
  }
  httpMessage += String(F("<br/><b>Update ESP8266 from URL</b><small><i> http only</i></small>"));
  httpMessage += String(F("<br/><input id='espFirmwareURL' name='espFirmware' value='")) + espFirmwareUrl + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>Update ESP from URL</button></form>"));

  httpMessage += String(F("<br/><form method='POST' action='/update' enctype='multipart/form-data'>"));
  httpMessage += String(F("<b>Update ESP8266 from file</b><input type='file' id='espSelect' name='espSelect' accept='.bin'>"));
  httpMessage += String(F("<br/><br/><button type='submit' id='espUploadSubmit' onclick='ackEspUploadSubmit()'>Update ESP from file</button></form>"));

  httpMessage += String(F("<br/><br/><hr><h1>WARNING!</h1>"));
  httpMessage += String(F("<b>Nextion LCD firmware updates can be risky.</b> If interrupted, the HASP will need to be manually power cycled which might mean a trip to the breaker box. "));
  httpMessage += String(F("After a power cycle, the LCD will display an error message until a successful firmware update has completed.<br/>"));

  httpMessage += String(F("<br/><hr><form method='get' action='lcddownload'>"));
  if (updateLcdAvailable)
  {
    httpMessage += String(F("<font color='green'><b>HASP LCD update available!</b></font>"));
  }
  httpMessage += String(F("<br/><b>Update Nextion LCD from URL</b><small><i> http only</i></small>"));
  httpMessage += String(F("<br/><input id='lcdFirmware' name='lcdFirmware' value='")) + lcdFirmwareUrl + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>Update LCD from URL</button></form>"));

  httpMessage += String(F("<br/><form method='POST' action='/lcdupload' enctype='multipart/form-data'>"));
  httpMessage += String(F("<br/><b>Update Nextion LCD from file</b><input type='file' id='lcdSelect' name='files[]' accept='.tft'/>"));
  httpMessage += String(F("<br/><br/><button type='submit' id='lcdUploadSubmit' onclick='ackLcdUploadSubmit()'>Update LCD from file</button></form>"));

  // Javascript to collect the filesize of the LCD upload and send it to /tftFileSize
  httpMessage += String(F("<script>function handleLcdFileSelect(evt) {"));
  httpMessage += String(F("var uploadFile = evt.target.files[0];"));
  httpMessage += String(F("document.getElementById('lcdUploadSubmit').innerHTML = 'Upload LCD firmware ' + uploadFile.name;"));
  httpMessage += String(F("var tftFileSize = '/tftFileSize?tftFileSize=' + uploadFile.size;"));
  httpMessage += String(F("var xhttp = new XMLHttpRequest();xhttp.open('GET', tftFileSize, true);xhttp.send();}"));
  httpMessage += String(F("function ackLcdUploadSubmit() {document.getElementById('lcdUploadSubmit').innerHTML = 'Uploading LCD firmware...';}"));
  httpMessage += String(F("function handleEspFileSelect(evt) {var uploadFile = evt.target.files[0];document.getElementById('espUploadSubmit').innerHTML = 'Upload ESP firmware ' + uploadFile.name;}"));
  httpMessage += String(F("function ackEspUploadSubmit() {document.getElementById('espUploadSubmit').innerHTML = 'Uploading ESP firmware...';}"));
  httpMessage += String(F("document.getElementById('lcdSelect').addEventListener('change', handleLcdFileSelect, false);"));
  httpMessage += String(F("document.getElementById('espSelect').addEventListener('change', handleEspFileSelect, false);</script>"));

  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleEspFirmware()
{ // http://plate01/espfirmware
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  debugPrintln(String(F("HTTP: Sending /espfirmware page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " ESP update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='60;url=/' />"));
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " ESP update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating ESP firmware from: " + String(webServer.arg("espFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  debugPrintln("ESPFW: Attempting ESP firmware update from: " + String(webServer.arg("espFirmware")));
  espStartOta(webServer.arg("espFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpload()
{ // http://plate01/lcdupload
  // Upload firmware to the Nextion LCD via HTTP upload

  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  static uint32_t lcdOtaTransferred = 0;
  static uint32_t lcdOtaRemaining;
  static uint16_t lcdOtaParts;
  const uint32_t lcdOtaTimeout = 30000; // timeout for receiving new data in milliseconds
  static uint32_t lcdOtaTimer = 0;      // timer for upload timeout

  HTTPUpload &upload = webServer.upload();

  if (tftFileSize == 0)
  {
    debugPrintln(String(F("LCD OTA: FAILED, no filesize sent.")));
    String httpMessage = FPSTR(HTTP_HEAD);
    httpMessage.replace("{v}", (String(haspNode) + " LCD update"));
    httpMessage += FPSTR(HTTP_SCRIPT);
    httpMessage += FPSTR(HTTP_STYLE);
    httpMessage += String(HASP_STYLE);
    httpMessage += String(F("<meta http-equiv='refresh' content='5;url=/' />"));
    httpMessage += FPSTR(HTTP_HEAD_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + " LCD update FAILED</h1>";
    httpMessage += String(F("No update file size reported.  You must use a modern browser with Javascript enabled."));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
  }
  else if ((lcdOtaTimer > 0) && ((millis() - lcdOtaTimer) > lcdOtaTimeout))
  { // Our timer expired so reset
    debugPrintln(F("LCD OTA: ERROR: LCD upload timeout.  Restarting."));
    espReset();
  }
  else if (upload.status == UPLOAD_FILE_START)
  {
    WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

    debugPrintln(String(F("LCD OTA: Attempting firmware upload")));
    debugPrintln(String(F("LCD OTA: upload.filename: ")) + String(upload.filename));
    debugPrintln(String(F("LCD OTA: TFTfileSize: ")) + String(tftFileSize));

    lcdOtaRemaining = tftFileSize;
    lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
    debugPrintln(String(F("LCD OTA: File upload beginning. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));

    Serial1.write(nextionSuffix, sizeof(nextionSuffix)); // Send empty command to LCD
    Serial1.flush();
    nextionHandleInput();

    String lcdOtaNextionCmd = "whmi-wri " + String(tftFileSize) + ",115200,0";
    debugPrintln(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
    Serial1.print(lcdOtaNextionCmd);
    Serial1.write(nextionSuffix, sizeof(nextionSuffix));
    Serial1.flush();

    if (nextionOtaResponse())
    {
      debugPrintln(F("LCD OTA: LCD upload command accepted"));
    }
    else
    {
      debugPrintln(F("LCD OTA: LCD upload command FAILED."));
      espReset();
    }
    lcdOtaTimer = millis();
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  { // Handle upload data
    static int lcdOtaChunkCounter = 0;
    static uint16_t lcdOtaPartNum = 0;
    static int lcdOtaPercentComplete = 0;
    static const uint16_t lcdOtaBufferSize = 1024; // upload data buffer before sending to UART
    static uint8_t lcdOtaBuffer[lcdOtaBufferSize] = {};
    uint16_t lcdOtaUploadIndex = 0;
    int32_t lcdOtaPacketRemaining = upload.currentSize;

    while (lcdOtaPacketRemaining > 0)
    { // Write incoming data to panel as it arrives
      // determine chunk size as lowest value of lcdOtaPacketRemaining, lcdOtaBufferSize, or 4096 - lcdOtaChunkCounter
      uint16_t lcdOtaChunkSize = 0;
      if ((lcdOtaPacketRemaining <= lcdOtaBufferSize) && (lcdOtaPacketRemaining <= (4096 - lcdOtaChunkCounter)))
      {
        lcdOtaChunkSize = lcdOtaPacketRemaining;
      }
      else if ((lcdOtaBufferSize <= lcdOtaPacketRemaining) && (lcdOtaBufferSize <= (4096 - lcdOtaChunkCounter)))
      {
        lcdOtaChunkSize = lcdOtaBufferSize;
      }
      else
      {
        lcdOtaChunkSize = 4096 - lcdOtaChunkCounter;
      }

      for (uint16_t i = 0; i < lcdOtaChunkSize; i++)
      { // Load up the UART buffer
        lcdOtaBuffer[i] = upload.buf[lcdOtaUploadIndex];
        lcdOtaUploadIndex++;
      }
      Serial1.flush();                              // Clear out current UART buffer
      Serial1.write(lcdOtaBuffer, lcdOtaChunkSize); // And send the most recent data
      lcdOtaChunkCounter += lcdOtaChunkSize;
      lcdOtaTransferred += lcdOtaChunkSize;
      if (lcdOtaChunkCounter >= 4096)
      {
        Serial1.flush();
        lcdOtaPartNum++;
        lcdOtaPercentComplete = (lcdOtaTransferred * 100) / tftFileSize;
        lcdOtaChunkCounter = 0;
        if (nextionOtaResponse())
        {
          debugPrintln(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" OK, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
        }
        else
        {
          debugPrintln(String(F("LCD OTA: Part ")) + String(lcdOtaPartNum) + String(F(" FAILED, ")) + String(lcdOtaPercentComplete) + String(F("% complete")));
        }
      }
      else
      {
        delay(10);
      }
      if (lcdOtaRemaining > 0)
      {
        lcdOtaRemaining -= lcdOtaChunkSize;
      }
      if (lcdOtaPacketRemaining > 0)
      {
        lcdOtaPacketRemaining -= lcdOtaChunkSize;
      }
    }

    if (lcdOtaTransferred >= tftFileSize)
    {
      if (nextionOtaResponse())
      {
        debugPrintln(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        webServer.sendHeader("Location", "/lcdOtaSuccess");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: Failure"));
        webServer.sendHeader("Location", "/lcdOtaFailure");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 1000)
        { // extra 1sec delay for client to grab failure page
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
    }
    lcdOtaTimer = millis();
  }
  else if (upload.status == UPLOAD_FILE_END)
  { // Upload completed
    if (lcdOtaTransferred >= tftFileSize)
    {
      if (nextionOtaResponse())
      { // YAY WE DID IT
        debugPrintln(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        webServer.sendHeader("Location", "/lcdOtaSuccess");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 5000)
        { // extra 5sec delay while the LCD handles any local firmware updates from new versions of code sent to it
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: Failure"));
        webServer.sendHeader("Location", "/lcdOtaFailure");
        webServer.send(303);
        uint32_t lcdOtaDelay = millis();
        while ((millis() - lcdOtaDelay) < 1000)
        { // extra 1sec delay for client to grab failure page
          webServer.handleClient();
          delay(1);
        }
        espReset();
      }
    }
  }
  else if (upload.status == UPLOAD_FILE_ABORTED)
  { // Something went kablooey
    debugPrintln(F("LCD OTA: ERROR: upload.status returned: UPLOAD_FILE_ABORTED"));
    debugPrintln(F("LCD OTA: Failure"));
    webServer.sendHeader("Location", "/lcdOtaFailure");
    webServer.send(303);
    uint32_t lcdOtaDelay = millis();
    while ((millis() - lcdOtaDelay) < 1000)
    { // extra 1sec delay for client to grab failure page
      webServer.handleClient();
      delay(1);
    }
    espReset();
  }
  else
  { // Something went weird, we should never get here...
    debugPrintln(String(F("LCD OTA: upload.status returned: ")) + String(upload.status));
    debugPrintln(F("LCD OTA: Failure"));
    webServer.sendHeader("Location", "/lcdOtaFailure");
    webServer.send(303);
    uint32_t lcdOtaDelay = millis();
    while ((millis() - lcdOtaDelay) < 1000)
    { // extra 1sec delay for client to grab failure page
      webServer.handleClient();
      delay(1);
    }
    espReset();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpdateSuccess()
{ // http://plate01/lcdOtaSuccess
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /lcdOtaSuccess page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update success"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" LCD update success</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to apply changes..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdUpdateFailure()
{ // http://plate01/lcdOtaFailure
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /lcdOtaFailure page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update failed"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F(" LCD update failed :(</h1>"));
  httpMessage += String(F("Restarting HASwitchPlate to reset device..."));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLcdDownload()
{ // http://plate01/lcddownload
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /lcddownload page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " LCD update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " LCD update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating LCD firmware from: " + String(webServer.arg("lcdFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  nextionStartOtaDownload(webServer.arg("lcdFirmware"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleTftFileSize()
{ // http://plate01/tftFileSize
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /tftFileSize page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " TFT Filesize"));
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  tftFileSize = webServer.arg("tftFileSize").toInt();
  debugPrintln(String(F("WEB: tftFileSize: ")) + String(tftFileSize));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleReboot()
{ // http://plate01/reboot
  if (configPassword[0] != '\0')
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }
  debugPrintln(String(F("HTTP: Sending /reboot page to client connected from: ")) + webServer.client().remoteIP().toString());
  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " HASP reboot"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += String(F("<meta http-equiv='refresh' content='10;url=/' />"));
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
  httpMessage += String(F("<br/>Rebooting device"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
  debugPrintln(F("RESET: Rebooting device"));
  nextionSendCmd("page 0");
  nextionSetAttr("p[0].b[1].txt", "\"Rebooting...\"");
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool updateCheck()
{ // firmware update check
  HTTPClient updateClient;
  debugPrintln(String(F("UPDATE: Checking update URL: ")) + String(UPDATE_URL));
  String updatePayload;
  updateClient.begin(wifiClient, UPDATE_URL);
  int httpCode = updateClient.GET(); // start connection and send HTTP header

  if (httpCode > 0)
  { // httpCode will be negative on error
    if (httpCode == HTTP_CODE_OK)
    { // file found at server
      updatePayload = updateClient.getString();
    }
  }
  else
  {
    debugPrintln(String(F("UPDATE: Update check failed: ")) + updateClient.errorToString(httpCode));
    return false;
  }
  updateClient.end();

  DynamicJsonDocument updateJson(2048);
  DeserializationError jsonError = deserializeJson(updateJson, updatePayload);

  if (jsonError)
  { // Couldn't parse the returned JSON, so bail
    debugPrintln(String(F("UPDATE: JSON parsing failed: ")) + String(jsonError.c_str()));
    return false;
  }
  else
  {
    if (!updateJson["d1_mini"]["version"].isNull())
    {
      updateEspAvailableVersion = updateJson["d1_mini"]["version"].as<float>();
      debugPrintln(String(F("UPDATE: updateEspAvailableVersion: ")) + String(updateEspAvailableVersion));
      espFirmwareUrl = updateJson["d1_mini"]["firmware"].as<String>();
      if (updateEspAvailableVersion > haspVersion)
      {
        updateEspAvailable = true;
        debugPrintln(String(F("UPDATE: New ESP version available: ")) + String(updateEspAvailableVersion));
      }
    }
    if (nextionModel && !updateJson[nextionModel]["version"].isNull())
    {
      updateLcdAvailableVersion = updateJson[nextionModel]["version"].as<int>();
      debugPrintln(String(F("UPDATE: updateLcdAvailableVersion: ")) + String(updateLcdAvailableVersion));
      lcdFirmwareUrl = updateJson[nextionModel]["firmware"].as<String>();
      if (updateLcdAvailableVersion > lcdVersion)
      {
        updateLcdAvailable = true;
        debugPrintln(String(F("UPDATE: New LCD version available: ")) + String(updateLcdAvailableVersion));
      }
    }
    debugPrintln(F("UPDATE: Update check completed"));
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionSetup()
{
  if (strcmp(motionPinConfig, "D0") == 0)
  {
    motionEnabled = true;
    motionPin = D0;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(motionPinConfig, "D1") == 0)
  {
    motionEnabled = true;
    motionPin = D1;
    pinMode(motionPin, INPUT);
  }
  else if (strcmp(motionPinConfig, "D2") == 0)
  {
    motionEnabled = true;
    motionPin = D2;
    pinMode(motionPin, INPUT);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void motionUpdate()
{
  static unsigned long motionLatchTimer = 0;         // Timer for motion sensor latch
  static unsigned long motionBufferTimer = millis(); // Timer for motion sensor buffer
  static bool motionActiveBuffer = motionActive;
  bool motionRead = digitalRead(motionPin);

  if (motionRead != motionActiveBuffer)
  { // if we've changed state
    motionBufferTimer = millis();
    motionActiveBuffer = motionRead;
  }
  else if (millis() > (motionBufferTimer + motionBufferTimeout))
  {
    if ((motionActiveBuffer && !motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqttClient.publish(mqttMotionStateTopic, "ON");
      motionActive = motionActiveBuffer;
      debugPrintln("MOTION: Active");
    }
    else if ((!motionActiveBuffer && motionActive) && (millis() > (motionLatchTimer + motionLatchTimeout)))
    {
      motionLatchTimer = millis();
      mqttClient.publish(mqttMotionStateTopic, "OFF");
      motionActive = motionActiveBuffer;
      debugPrintln("MOTION: Inactive");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void handleTelnetClient()
{ // Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
  static unsigned long telnetInputIndex = 0;
  if (telnetServer.hasClient())
  { // client is connected
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
      {
        telnetClient.stop(); // client disconnected
      }
      telnetClient = telnetServer.available(); // ready for new client
      telnetInputIndex = 0;                    // reset input buffer index
    }
    else
    {
      telnetServer.available().stop(); // have client, block new connections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available())
  { // client input processing
    static char telnetInputBuffer[telnetInputMax];

    if (telnetClient.available())
    {
      char telnetInputByte = telnetClient.read(); // Read client byte
      // debugPrintln(String("telnet in: 0x") + String(telnetInputByte, HEX));
      if (telnetInputByte == 5)
      { // If the telnet client sent a bunch of control commands on connection (which end in ENQUIRY/0x05), ignore them and restart the buffer
        telnetInputIndex = 0;
      }
      else if (telnetInputByte == 13)
      { // telnet line endings should be CRLF: https://tools.ietf.org/html/rfc5198#appendix-C
        // If we get a CR just ignore it
      }
      else if (telnetInputByte == 10)
      {                                          // We've caught a LF (DEC 10), send buffer contents to the Nextion
        telnetInputBuffer[telnetInputIndex] = 0; // null terminate our char array
        nextionSendCmd(String(telnetInputBuffer));
        telnetInputIndex = 0;
      }
      else if (telnetInputIndex < telnetInputMax)
      { // If we have room left in our buffer add the current byte
        telnetInputBuffer[telnetInputIndex] = telnetInputByte;
        telnetInputIndex++;
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void debugPrintln(String debugText)
{ // Debug output line of text to our debug targets
  String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
  Serial.println(debugTimeText);
  if (debugSerialEnabled)
  {
    SoftwareSerial debugSerial(SW_SERIAL_UNUSED_PIN, 1); // 17==nc for RX, 1==TX pin
    debugSerial.begin(115200);
    debugSerial.println(debugTimeText);
    debugSerial.flush();
  }
  if (debugTelnetEnabled)
  {
    if (telnetClient.connected())
    {
      telnetClient.println(debugTimeText);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void debugPrint(String debugText)
{ // Debug output single character to our debug targets (DON'T USE THIS!)
  // Try to avoid using this function if at all possible.  When connected to telnet, printing each
  // character requires a full TCP round-trip + acknowledgement back and execution halts while this
  // happens.  Far better to put everything into a line and send it all out in one packet using
  // debugPrintln.
  if (debugSerialEnabled)
    Serial.print(debugText);
  {
    SoftwareSerial debugSerial(SW_SERIAL_UNUSED_PIN, 1); // 17==nc for RX, 1==TX pin
    debugSerial.begin(115200);
    debugSerial.print(debugText);
    debugSerial.flush();
  }
  if (debugTelnetEnabled)
  {
    if (telnetClient.connected())
    {
      telnetClient.print(debugText);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// UTF8-Decoder: convert UTF8-string to extended ASCII http://playground.arduino.cc/main/Utf8ascii
// Workaround for issue here: https://github.com/home-assistant/home-assistant/issues/9528
// Nextion claims that "Unicode and UTF will not be among the supported encodings", so this should
// be safe to run against all attribute values coming in.
static byte c1; // Last character buffer
byte utf8ascii(byte ascii)
{ // Convert a single Character from UTF8 to Extended ASCII. Return "0" if a byte has to be ignored.
  if (ascii < 128)
  { // Standard ASCII-set 0..0x7F handling
    c1 = 0;
    return (ascii);
  }
  // get previous input
  byte last = c1; // get last char
  c1 = ascii;     // remember actual character
  switch (last)
  { // conversion depending on first UTF8-character
  case 0xC2:
    return (ascii);
    break;
  case 0xC3:
    return (ascii | 0xC0);
    break;
  case 0x82:
    if (ascii == 0xAC)
      return (0x80); // special case Euro-symbol
  }
  return (0); // otherwise: return zero, if character has to be ignored
}

String utf8ascii(String s)
{ // convert String object from UTF8 String to Extended ASCII
  String r = "";
  char c;
  for (uint16_t i = 0; i < s.length(); i++)
  {
    c = utf8ascii(s.charAt(i));
    if (c != 0)
      r += c;
  }
  return r;
}

void utf8ascii(char *s)
{ // In Place conversion UTF8-string to Extended ASCII (ASCII is shorter!)
  uint16_t k = 0;
  char c;
  for (uint16_t i = 0; i < strlen(s); i++)
  {
    c = utf8ascii(s[i]);
    if (c != 0)
      s[k++] = c;
  }
  s[k] = 0;
}