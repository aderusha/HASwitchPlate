////////////////////////////////////////////////////////////////////////////////////////////////////
//           _____ _____ _____ _____
//          |  |  |  _  |   __|  _  |
//          |     |     |__   |   __|
//          |__|__|__|__|_____|__|
//        Home Automation Switch Plate
// https://github.com/aderusha/HASwitchPlate
//
// Copyright (c) 2018 Allen Derusha allen@derusha.org
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
char wifiSSID[32] = ""; // Leave unset for wireless autoconfig.
char wifiPass[64] = ""; // Note that these values will be lost if auto-update is used,
                        // and that's probably OK because they will be saved in EEPROM.

////////////////////////////////////////////////////////////////////////////////////////////////////
char mqttServer[64] = ""; // These defaults may be overwritten with values saved by the web interface
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
char groupName[16] = "plates";
char configUser[32] = "admin";
char configPassword[32] = "";
////////////////////////////////////////////////////////////////////////////////////////////////////

// Serial debug output control, comment out to disable
#define DEBUGSERIAL
// Open a super-insecure (but read-only) telnet debug port
// #define DEBUGTELNET

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

#ifdef DEBUGTELNET
WiFiServer telnetServer(23);
WiFiClient telnetClient;
#endif
WiFiClient wifiClient;
MQTTClient mqttClient(256);
ESP8266WebServer webServer(80);
ESP8266HTTPUpdateServer httpOTAUpdate;

const float haspVersion = 0.291;                 // Current HASP software release version
byte nextionReturnBuffer[128];                   // Byte array to pass around data coming from the panel
uint8_t nextionReturnIndex = 0;                  // Index for nextionReturnBuffer
uint8_t nextionActivePage = 0;                   // Track active LCD page
char wifiConfigPass[9];                          // AP config password, always 8 chars + NUL
char wifiConfigAP[19];                           // AP config SSID, haspNode + 3 chars
bool shouldSaveConfig = false;                   // Flag to save json config to SPIFFS
bool nextionReportPage0 = false;                 // If false, don't report page 0 sendme
const long updateCheckInterval = 43200000;       // Time in msec between update checks (12 hours)
bool startupDelayFlag = true;                    // These four values are used for housekeeping tasks
const long startupDelayTimeBegin = 30000;        // after initial startup.  We'll wait 30 seconds for
const long startupDelayTimeEnd = 45000;          // everything to settle and then attempt to collect
const long startupDelayRetryInterval = 1000;     // information from attached devices and then
long startupDelayRetryTimer = 0;                 // collect update information from the web
long updateCheckTimer = 0;                       // Timer for update check
bool updateEspAvailable = false;                 // Flag for update check to report new ESP FW version
float updateEspAvailableVersion;                 // Float to hold the new ESP FW version number
bool updateLcdAvailable = false;                 // Flag for update check to report new LCD FW version
unsigned long lcdVersion = 0;                    // Int to hold current LCD FW version number
unsigned long updateLcdAvailableVersion;         // Int to hold the new LCD FW version number
bool lcdVersionQueryFlag = false;                // Flag to set if we've queried lcdVersion
const String lcdVersionQuery = "p[0].b[2].val";  // Object ID for lcdVersion in HMI
const long statusUpdateInterval = 300000;        // Time in msec between publishing MQTT status updates (5 minutes)
long statusUpdateTimer = 0;                      // Timer for update check
byte espMac[6];                                  // Byte array to store our MAC address
String mqttClientId;                             // Auto-generated MQTT ClientID
String mqttGetSubtopic;                          // MQTT subtopic for incoming commands requesting .val
String mqttStateTopic;                           // MQTT topic for outgoing panel interactions
String mqttCommandTopic;                         // MQTT topic for incoming panel commands
String mqttGroupCommandTopic;                    // MQTT topic for incoming group panel commands
String mqttStatusTopic;                          // MQTT topic for publishing device connectivity state
String mqttSensorTopic;                          // MQTT topic for publishing device information in JSON format
String mqttLightCommandTopic;                    // MQTT topic for incoming panel backlight on/off commands
String mqttLightStateTopic;                      // MQTT topic for outgoing panel backlight on/off state
String mqttLightBrightCommandTopic;              // MQTT topic for incoming panel backlight dimmer commands
String mqttLightBrightStateTopic;                // MQTT topic for outgoing panel backlight dimmer state
String nextionModel;                             // Record reported model number of LCD panel
const byte nextionSuffix[] = {0xFF, 0xFF, 0xFF}; // Standard suffix for Nextion commands
long tftFileSize = 0;                            // Filesize for TFT firmware upload

// Additional CSS style to match Hass theme
const char HASP_STYLE[] = "<style>button{background-color:#03A9F4;}body{width:60%;margin:auto;}</style>";
// URL for auto-update "version.json"
const char UPDATE_URL[] = "http://haswitchplate.com/update/version.json";
// Default link to compiled Arduino firmware image
String espFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.ino.d1_mini.bin";
// Default link to compiled Nextion firmware images
String lcdFirmwareUrl = "http://haswitchplate.com/update/HASwitchPlate.tft";

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup

  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX

  debugPrintln(F("\r\nSYSTEM: Hardware initialized, starting program load"));

  sendNextionCmd("rest"); // reset the LCD

  // Uncomment for testing, clears all saved settings on each boot
  // clearSavedConfig();

  readSavedConfig(); // Check filesystem for a saved config.json
  setupWifi();       // Start up networking

  if (configPassword[0] != '\0')
  {
    httpOTAUpdate.setup(&webServer, "/update", configUser, configPassword);
  }
  else
  {
    httpOTAUpdate.setup(&webServer, "/update");
  }
  webServer.on("/", webHandleRoot);
  webServer.on("/saveConfig", webHandleSaveConfig);
  webServer.on("/resetConfig", webHandleResetConfig);
  webServer.on("/firmware", webHandleFirmware);
  webServer.on("/espfirmware", webHandleEspFirmware);
  webServer.on("/lcdupload", HTTP_POST, []() { webServer.send(200, "text/plain", ""); }, webHandleLcdUpload);
  webServer.on("/tftFileSize", webHandleTftFileSize);
  webServer.on("/lcddownload", webHandleLcdDownload);
  webServer.on("/reboot", webHandleReboot);
  webServer.onNotFound(webHandleNotFound);
  webServer.begin();
  debugPrintln(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());

  setupOta();

  // Create server and assign callbacks for MQTT
  mqttClient.begin(mqttServer, atoi(mqttPort), wifiClient);
  mqttClient.onMessage(mqttCallback);
  mqttConnect();

#ifdef DEBUGTELNET
  // Setup telnet server for remote debug output
  telnetServer.setNoDelay(true);
  telnetServer.begin();
  debugPrintln(String(F("TELNET: debug server enabled at telnet:")) + WiFi.localIP().toString());
#endif

  debugPrintln(F("SYSTEM: Init complete, swapping debug serial output to D8\r\n"));
  Serial.swap();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop()
{ // Main execution loop

  if (Serial.available() > 0)
  { // Process user input from HMI
    processNextionInput();
  }

  while ((WiFi.status() != WL_CONNECTED) || (WiFi.localIP().toString() == "0.0.0.0"))
  { // Check WiFi is connected and that we have a valid IP, retry until we do.
    if (WiFi.status() == WL_CONNECTED)
    { // If we're currently connected, disconnect so we can try again
      WiFi.disconnect();
    }
    setupWifi();
  }

  if (!mqttClient.connected())
  { // Check MQTT connection
    mqttConnect();
  }

  mqttClient.loop();        // MQTT client loop
  ArduinoOTA.handle();      // Arduino OTA loop
  webServer.handleClient(); // webServer loop

  if ((millis() >= (startupDelayTimeBegin - 5000)) && (millis() <= startupDelayTimeEnd) && ((millis() - startupDelayRetryTimer) >= startupDelayRetryInterval) && startupDelayFlag && nextionModel == "")
  { // Begin request for LCD information 5 seconds early if we haven't collected it yet
    sendNextionCmd("get " + lcdVersionQuery);
    lcdVersionQueryFlag = true;
    startupDelayRetryTimer = millis();
    sendNextionCmd("connect");
  }

  if ((millis() - statusUpdateTimer) >= statusUpdateInterval)
  { // Run periodic status update
    statusUpdateTimer = millis();
    mqttStatusUpdate();
  }

  if (((millis() - updateCheckTimer) >= updateCheckInterval) || ((millis() >= startupDelayTimeBegin) && (millis() <= startupDelayTimeEnd) && startupDelayFlag))
  { // Run periodic update check
    updateCheckTimer = millis();
    if (updateCheck())
    { // If updateCheck hasn't returned an error, clear the startupDelay flag and update MQTT status
      startupDelayFlag = false;
      debugPrintln(F("UPDATE: Update check completed"));
      mqttStatusUpdate();
    }
  }

#ifdef DEBUGTELNET
  // telnetClient loop
  handleTelnetClient();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

////////////////////////////////////////////////////////////////////////////////////////////////////
void mqttConnect()
{ // MQTT connection and subscriptions

  // Check to see if we have a broker configured and notify the user if not
  if (mqttServer[0] == 0)
  {
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"WiFi Connected!\\rConfigure MQTT:\\r" + WiFi.localIP().toString() + "\\r\\r\\r\\r\\r\\r\\r \"");
    setNextionAttr("p[0].b[3].txt", "\"http://" + WiFi.localIP().toString() + "\"");
    sendNextionCmd("vis 3,1");
    while (mqttServer[0] == 0)
    { // Handle HTTP and OTA while we're waiting for MQTT to be configured
      yield();
      webServer.handleClient();
      ArduinoOTA.handle();
    }
  }
  // MQTT topic string definitions
  mqttStateTopic = "hasp/" + String(haspNode) + "/state";
  mqttCommandTopic = "hasp/" + String(haspNode) + "/command";
  mqttGroupCommandTopic = "hasp/" + String(groupName) + "/command";
  mqttStatusTopic = "hasp/" + String(haspNode) + "/status";
  mqttSensorTopic = "hasp/" + String(haspNode) + "/sensor";
  mqttLightCommandTopic = "hasp/" + String(haspNode) + "/light/switch";
  mqttLightStateTopic = "hasp/" + String(haspNode) + "/light/status";
  mqttLightBrightCommandTopic = "hasp/" + String(haspNode) + "/brightness/set";
  mqttLightBrightStateTopic = "hasp/" + String(haspNode) + "/brightness/status";

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
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\\rMQTT Connecting\"");
    debugPrintln(String(F("MQTT: Attempting connection to broker ")) + String(mqttServer) + " as clientID " + mqttClientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(5, true, 5000);

    // declare LWT
    mqttClient.setWill(mqttStatusTopic.c_str(), "OFF");

    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(mqttCommandTopic))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttCommandTopic);
      }
      if (mqttClient.subscribe(mqttGroupCommandTopic))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttGroupCommandTopic);
      }
      if (mqttClient.subscribe(mqttCommandSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttCommandSubscription);
      }
      if (mqttClient.subscribe(mqttGroupCommandSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttGroupCommandSubscription);
      }
      if (mqttClient.subscribe(mqttStatusTopic))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttStatusTopic);
      }
      if (mqttClient.subscribe(mqttLightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttLightBrightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }

      debugPrintln(String(F("MQTT: binary_sensor state: [")) + mqttStatusTopic + "] : [ON]");
      mqttClient.publish(mqttStatusTopic, "ON", true, 1);

      mqttReconnectCount = 0;

      // Update panel with MQTT status
      setNextionAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\\rMQTT Connected:\\r" + String(mqttServer) + "\"");
      debugPrintln(F("MQTT: connected"));
      if (nextionActivePage)
      {
        sendNextionCmd("page " + String(nextionActivePage));
      }
    }
    else
    { // Retry until we give up and restart
      mqttReconnectCount++;
      if (mqttReconnectCount > 5)
      {
        debugPrintln(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Restarting device.")));
        espReset();
      }
      debugPrintln(String(F("MQTT connection attempt ")) + String(mqttReconnectCount) + String(F(" failed with rc ")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 10 seconds.")));
      unsigned long mqttReconnectTimeout = 10000;  // timeout for MQTT reconnect
      unsigned long mqttReconnectTimer = millis(); // record current time for our timeout
      while ((millis() - mqttReconnectTimer) < mqttReconnectTimeout)
      { // Handle HTTP and OTA while we're waiting for MQTT to reconnect
        yield();
        webServer.handleClient();
        ArduinoOTA.handle();
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

  // Incoming Namespace:
  // '[...]/device/command' -m '' = undefined
  // '[...]/group/command' -m '' = undefined
  // '[...]/device/command' -m 'dim 50' = sendNextionCmd("dim 50")
  // '[...]/device/command/page' -m '1' = sendNextionCmd("page 1")
  // '[...]/group/command/page' -m '1' = sendNextionCmd("page 1")
  // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = startNextionOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/group/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = startNextionOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/lcdupdate' -m '' = startNextionOtaDownload("lcdFirmwareUrl")
  // '[...]/group/command/lcdupdate' -m '' = startNextionOtaDownload("lcdFirmwareUrl")
  // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' = startEspOTA("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
  // '[...]/group/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' = startEspOTA("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
  // '[...]/device/command/espupdate' -m '' = startEspOTA("espFirmwareUrl")
  // '[...]/group/command/espupdate' -m '' = startEspOTA("espFirmwareUrl")
  // '[...]/device/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
  // '[...]/group/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
  // '[...]/group/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")

  debugPrintln(String(F("MQTT IN:  '")) + strTopic + "' : '" + strPayload + "'");

  if (((strTopic == mqttCommandTopic) || (strTopic == mqttGroupCommandTopic)) && (strPayload == ""))
  { // '[...]/device/command' -m '' = undefined
    // '[...]/group/command' -m '' = undefined
    // currently undefined
  }
  else if (strTopic == mqttCommandTopic || strTopic == mqttGroupCommandTopic)
  { // '[...]/device/command' -m 'dim 50' == sendNextionCmd("dim 50")
    // '[...]/group/command' -m 'dim 50' == sendNextionCmd("dim 50")
    sendNextionCmd(strPayload);
  }
  else if (strTopic == (mqttCommandTopic + "/page") || strTopic == (mqttGroupCommandTopic + "/page"))
  { // '[...]/device/command/page' -m '1' == sendNextionCmd("page 1")
    // '[...]/group/command/page' -m '1' == sendNextionCmd("page 1")
    if (nextionActivePage != strPayload.toInt())
    { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
      nextionActivePage = strPayload.toInt();
      sendNextionCmd("page " + strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/statusupdate") || strTopic == (mqttGroupCommandTopic + "/statusupdate"))
  { // '[...]/device/command/statusupdate' == mqttStatusUpdate()
    // '[...]/group/command/statusupdate' == mqttStatusUpdate()
    mqttStatusUpdate(); // return status JSON via MQTT;
  }
  else if (strTopic == (mqttCommandTopic + "/lcdupdate") || strTopic == (mqttGroupCommandTopic + "/lcdupdate"))
  { // '[...]/device/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' == startNextionOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
    // '[...]/group/command/lcdupdate' -m 'http://192.168.0.10/local/HASwitchPlate.tft' == startNextionOtaDownload("http://192.168.0.10/local/HASwitchPlate.tft")
    if (strPayload == "")
    {
      startNextionOtaDownload(lcdFirmwareUrl);
    }
    else
    {
      startNextionOtaDownload(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/espupdate") || strTopic == (mqttGroupCommandTopic + "/espupdate"))
  { // '[...]/device/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' == startEspOTA("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
    // '[...]/group/command/espupdate' -m 'http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin' == startEspOTA("http://192.168.0.10/local/HASwitchPlate.ino.d1_mini.bin")
    if (strPayload == "")
    {
      startEspOTA(espFirmwareUrl);
    }
    else
    {
      startEspOTA(strPayload);
    }
  }
  else if (strTopic == (mqttCommandTopic + "/reboot") || strTopic == (mqttGroupCommandTopic + "/reboot"))
  { // '[...]/device/command/reboot' == reboot microcontroller)
    // '[...]/group/command/reboot' == reboot microcontroller)
    debugPrintln(F("MQTT: Rebooting device"));
    espReset();
  }
  else if (strTopic == (mqttCommandTopic + "/factoryreset") || strTopic == (mqttGroupCommandTopic + "/factoryreset"))
  { // '[...]/device/command/factoryreset' == clear all saved settings)
    // '[...]/group/command/factoryreset' == clear all saved settings)
    clearSavedConfig();
  }
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == ""))
  { // '[...]/device/command/p[1].b[4].txt' -m '' == getNextionAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    getNextionAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic) && (strPayload == ""))
  { // '[...]/group/command/p[1].b[4].txt' -m '' == getNextionAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    getNextionAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttCommandTopic))
  { // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' == setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    setNextionAttr(subTopic, strPayload);
  }
  else if (strTopic.startsWith(mqttGroupCommandTopic))
  { // '[...]/group/command/p[1].b[4].txt' -m '"Lights On"' == setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttGroupCommandTopic.length() + 1);
    setNextionAttr(subTopic, strPayload);
  }
  else if (strTopic == mqttLightBrightCommandTopic)
  { // change the brightness from the light topic
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    setNextionAttr("dim", String(panelDim));
    sendNextionCmd("dims=dim");
    mqttClient.publish(mqttLightBrightStateTopic, strPayload);
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "OFF")
  { // set the panel dim OFF from the light topic, saving current dim level first
    sendNextionCmd("dims=dim");
    setNextionAttr("dim", "0");
    mqttClient.publish(mqttLightStateTopic, "OFF");
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "ON")
  { // set the panel dim ON from the light topic, restoring saved dim level
    sendNextionCmd("dim=dims");
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
  DynamicJsonBuffer updateBuffer(200);
  JsonObject &statusUpdate = updateBuffer.createObject();
  statusUpdate["status"] = "available";
  statusUpdate["espVersion"] = haspVersion;
  statusUpdate["updateEspAvailable"] = updateEspAvailable;
  statusUpdate["lcdVersion"] = lcdVersion;
  statusUpdate["updateLcdAvailable"] = updateLcdAvailable;
  statusUpdate["espUptime"] = long(millis() / 1000);
  statusUpdate["signalStrength"] = WiFi.RSSI();
  statusUpdate["haspIP"] = WiFi.localIP().toString();
  String mqttStatusPayload;
  statusUpdate.printTo(mqttStatusPayload);
  mqttClient.publish(mqttSensorTopic, mqttStatusPayload);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void handleNextionInput()
{ // Handle incoming serial data from the Nextion panel
  // This will collect serial data from the panel and place it into the global buffer
  // nextionReturnBuffer[nextionReturnIndex] until it finds a string of 3 consecutive
  // 0xFF values or runs into the timeout
  bool nextionCommandIncoming = true;           // we'll flip this to false when we receive 3 consecutive 0xFFs
  unsigned long nextionCommandTimeout = 1000;   // timeout for receiving termination string in milliseconds
  int nextionTermByteCnt = 0;                   // counter for our 3 consecutive 0xFFs
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout
  nextionReturnIndex = 0;                       // reset the global nextionReturnIndex back to zero
  String hmiDebug = "HMI IN: ";                 // assemble a string for debug output
  // Load the nextionBuffer until we receive a termination command or we hit our timeout
  while (nextionCommandIncoming && ((millis() - nextionCommandTimer) < nextionCommandTimeout))
  {
    if (Serial.available())
    {
      byte nextionCommandByte = Serial.read();
      hmiDebug += (" 0x" + String(nextionCommandByte, HEX));
      // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
      if (nextionCommandByte == 0xFF)
      {
        nextionTermByteCnt++;
        if (nextionTermByteCnt >= 3)
        {
          nextionCommandIncoming = false;
        }
      }
      else
      {
        nextionTermByteCnt = 0; // reset counter if a non-term byte was encountered
      }
      nextionReturnBuffer[nextionReturnIndex] = nextionCommandByte;
      nextionReturnIndex++;
    }
    else
    { // If there's a pause in the action, check on our buddy MQTT
      yield();
      mqttClient.loop();
    }
  }
  debugPrintln(hmiDebug);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void processNextionInput()
{ // Handle incoming commands from the Nextion device
  // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
  // tl;dr, command byte, command data, 0xFF 0xFF 0xFF

  // Call handleNextionInput() to load nextionReturnBuffer[] and set nextionReturnIndex
  handleNextionInput();

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
    }
    if (nextionButtonAction == 0x00)
    {
      debugPrintln(String(F("HMI IN: [Button OFF] 'p[")) + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln(String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'OFF'");
      mqttClient.publish(mqttButtonTopic, "OFF");
      // Now see if this object has a .val that might have been updated.
      // works for sliders, two-state buttons, etc, throws a 0x1A error for normal buttons
      // which we'll catch and ignore
      mqttGetSubtopic = "/p[" + nextionPage + "].b[" + nextionButtonID + "].val";
      getNextionAttr("p[" + nextionPage + "].b[" + nextionButtonID + "].val");
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
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setNextionAttr(String hmi_attribute, String hmi_value)
{ // Set the value of a Nextion component attribute
  Serial1.print(hmi_attribute);
  Serial1.print("=");
  Serial1.print(hmi_value);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln(String(F("HMI OUT:  '")) + hmi_attribute + "=" + hmi_value + "'");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void getNextionAttr(String hmi_attribute)
{ // Get the value of a Nextion component attribute
  // This will only send the command to the panel requesting the attribute, the actual
  // return of that value will be handled by processNextionInput and placed into mqttGetSubtopic
  Serial1.print("get " + hmi_attribute);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln(String(F("HMI OUT:  'get ")) + hmi_attribute + "'");
  processNextionInput();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void sendNextionCmd(String nextionCmd)
{ // Send a raw command to the Nextion panel
  Serial1.print(nextionCmd);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln(String(F("HMI OUT:  ")) + nextionCmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setupWifi()
{ // Connect to WiFi
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"WiFi Connecting\"");

  // Read our MAC address and save it to espMac
  WiFi.macAddress(espMac);
  // Assign our hostname before connecting to WiFi
  WiFi.hostname(haspNode);

  if (String(wifiSSID) == "")
  { // If the sketch has no defined a static wifiSSID to connect to,
    // use WiFiManager to collect required information from the user.

    // id/name, placeholder/prompt, default value, length, extra tags
    WiFiManagerParameter custom_haspNodeHeader("<br/><br/><b>HASP Node Name</b>");
    WiFiManagerParameter custom_haspNode("haspNode", "HASP Node (required)", haspNode, 15, " maxlength=15 required");
    WiFiManagerParameter custom_groupName("groupName", "Group Name (required)", groupName, 15, " maxlength=15 required");
    WiFiManagerParameter custom_mqttHeader("<br/><br/><b>MQTT Broker</b>");
    WiFiManagerParameter custom_mqttServer("mqttServer", "MQTT Server", mqttServer, 63, " maxlength=39");
    WiFiManagerParameter custom_mqttPort("mqttPort", "MQTT Port", mqttPort, 5, " maxlength=5 type='number'");
    WiFiManagerParameter custom_mqttUser("mqttUser", "MQTT User", mqttUser, 31, " maxlength=31");
    WiFiManagerParameter custom_mqttPassword("mqttPassword", "MQTT Password", mqttPassword, 31, " maxlength=31 type='password'");
    WiFiManagerParameter custom_configHeader("<br/><br/><b>Admin access</b>");
    WiFiManagerParameter custom_configUser("configUser", "Config User", configUser, 15, " maxlength=31'");
    WiFiManagerParameter custom_configPassword("configPassword", "Config Password", configPassword, 31, " maxlength=31 type='password'");

    // WiFiManager local intialization. Once its business is done, there is no need to keep it around
    WiFiManager wifiManager;

    // set config save notify callback
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    wifiManager.setCustomHeadElement(HASP_STYLE);

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

    // Timeout until configuration portal gets turned off
    wifiManager.setTimeout(180);

    wifiManager.setAPCallback(wifiConfigCallback);

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
      saveUpdatedConfig();
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
      if (millis() >= (wifiReconnectTimer + 180000))
      { // If we've been trying to reconnect for 3 minutes, reboot and try again
        debugPrintln(F("WIFI: Failed to connect and hit timeout"));
        espReset();
      }
    }
  }
  // If you get here you have connected to WiFi
  setNextionAttr("p[0].b[1].txt", "\"WiFi Connected:\\r" + WiFi.localIP().toString() + "\"");
  debugPrintln(String(F("WIFI: Connected succesfully and assigned IP: ")) + WiFi.localIP().toString());
  if (nextionActivePage)
  {
    sendNextionCmd("page " + String(nextionActivePage));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void wifiConfigCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debugPrintln(F("WIFI: Failed to connect to assigned AP, entering config mode"));
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Configure HASP:\\rAP:" + String(wifiConfigAP) + "\\rPass:" + String(wifiConfigPass) + "\\r\\r\\r\\r\\r\\r\\rWeb:192.168.4.1\"");
  setNextionAttr("p[0].b[3].txt", "\"WIFI:S:" + String(wifiConfigAP) + ";T:WPA;P:" + String(wifiConfigPass) + ";;\"");
  sendNextionCmd("vis 3,1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setupOta()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(haspNode);
  ArduinoOTA.setPassword(configPassword);

  ArduinoOTA.onStart([]() {
    debugPrintln(F("ESP OTA: update start"));
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
  });
  ArduinoOTA.onEnd([]() {
    sendNextionCmd("page 0");
    debugPrintln(F("ESP OTA: update complete"));
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
    espReset();
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
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
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA FAILED\"");
    delay(5000);
    sendNextionCmd("page " + String(nextionActivePage));
  });
  ArduinoOTA.begin();
  debugPrintln(F("ESP OTA: Over the Air firmware update ready"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void startNextionOtaDownload(String otaUrl)
{ // Upload firmware to the Nextion LCD via HTTP download
  // based in large part on code posted by indev2 here:
  // http://support.iteadstudio.com/support/discussions/topics/11000007686/page/2

  int lcdOtaFileSize = 0;
  String lcdOtaNextionCmd;
  int lcdOtaChunkCounter = 0;
  uint16_t lcdOtaPartNum = 0;
  int lcdOtaTransferred = 0;
  int lcdOtaPercentComplete = 0;

  debugPrintln(String(F("LCD OTA: Attempting firmware download from:")) + otaUrl);
  HTTPClient lcdOtaHttp;
  lcdOtaHttp.begin(otaUrl);
  int lcdOtaHttpReturn = lcdOtaHttp.GET();
  if (lcdOtaHttpReturn > 0)
  { // HTTP header has been sent and Server response header has been handled
    debugPrintln(String(F("LCD OTA: HTTP GET return code:")) + String(lcdOtaHttpReturn));
    if (lcdOtaHttpReturn == HTTP_CODE_OK)
    { // file found at server
      // get length of document (is -1 when Server sends no Content-Length header)
      int lcdOtaRemaining = lcdOtaHttp.getSize();
      lcdOtaFileSize = lcdOtaRemaining;
      int lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
      uint8_t lcdOtaBuffer[128] = {}; // max size of ESP8266 UART buffer

      debugPrintln(String(F("LCD OTA: File found at Server. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));

      WiFiUDP::stopAll(); // Keep mDNS responder and MQTT traffic from breaking things
      if (mqttClient.connected())
      {
        debugPrintln(F("LCD OTA: LCD firmware upload starting, closing MQTT connection."));
        mqttClient.publish(mqttStatusTopic, "OFF");
        mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}");
        mqttClient.disconnect();
      }

      // get tcp stream
      WiFiClient *stream = lcdOtaHttp.getStreamPtr();
      // Send empty command
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();
      handleNextionInput();

      String lcdOtaNextionCmd = "whmi-wri " + String(lcdOtaFileSize) + ",115200,0";
      debugPrintln(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
      Serial1.print(lcdOtaNextionCmd);
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();

      if (otaReturnSuccess())
      {
        debugPrintln(F("LCD OTA: LCD upload command accepted."));
      }
      else
      {
        debugPrintln(F("LCD OTA: LCD upload command FAILED.  Restarting device."));
        ESP.reset();
        delay(5000);
      }
      debugPrintln(F("LCD OTA: Starting update"));
      while (lcdOtaHttp.connected() && (lcdOtaRemaining > 0 || lcdOtaRemaining == -1))
      { // Write incoming data to panel as it arrives
        // get available data size
        size_t lcdOtaHttpSize = stream->available();
        if (lcdOtaHttpSize)
        {
          // read up to 128 bytes
          int lcdOtaChunkSize = stream->readBytes(lcdOtaBuffer, ((lcdOtaHttpSize > sizeof(lcdOtaBuffer)) ? sizeof(lcdOtaBuffer) : lcdOtaHttpSize));
          // write it to panel
          Serial1.flush();
          Serial1.write(lcdOtaBuffer, lcdOtaChunkSize);
          lcdOtaChunkCounter += lcdOtaChunkSize;
          if (lcdOtaChunkCounter >= 4096)
          {
            Serial1.flush();
            lcdOtaPartNum++;
            lcdOtaTransferred += lcdOtaChunkCounter;
            lcdOtaPercentComplete = (lcdOtaTransferred * 100) / lcdOtaFileSize;
            lcdOtaChunkCounter = 0;
            if (otaReturnSuccess())
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
            delay(20);
          }
          if (lcdOtaRemaining > 0)
          {
            lcdOtaRemaining -= lcdOtaChunkSize;
          }
        }
      }
      lcdOtaPartNum++;
      lcdOtaTransferred += lcdOtaChunkCounter;
      if ((lcdOtaTransferred == lcdOtaFileSize) && otaReturnSuccess())
      {
        debugPrintln(String(F("LCD OTA: success, wrote ")) + String(lcdOtaTransferred) + " of " + String(lcdOtaFileSize) + " bytes.");
        delay(2000); // extra delay while the LCD does its thing
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: failure"));
        delay(2000); // extra delay while the LCD does its thing
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
bool otaReturnSuccess()
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
    }
    else
    {
      delay(1);
    }
  }
  delay(10);
  return otaSuccessVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void readSavedConfig()
{ // Read saved config.json from SPIFFS

  // Read configuration from FS json
  debugPrintln(F("SPIFFS: mounting FS..."));

  if (SPIFFS.begin())
  {
    debugPrintln(F("SPIFFS: mounted file system"));
    if (SPIFFS.exists("/config.json"))
    {
      // File exists, reading and loading
      debugPrintln(F("SPIFFS: reading config file"));
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        debugPrintln(F("SPIFFS: opened config file"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer configJsonBuffer(256);
        JsonObject &configJson = configJsonBuffer.parseObject(buf.get());
        if (configJson.success())
        {
          if (configJson["mqttServer"].success())
          {
            strcpy(mqttServer, configJson["mqttServer"]);
          }
          if (configJson["mqttPort"].success())
          {
            strcpy(mqttPort, configJson["mqttPort"]);
          }
          if (configJson["mqttUser"].success())
          {
            strcpy(mqttUser, configJson["mqttUser"]);
          }
          if (configJson["mqttPassword"].success())
          {
            strcpy(mqttPassword, configJson["mqttPassword"]);
          }
          if (configJson["haspNode"].success())
          {
            strcpy(haspNode, configJson["haspNode"]);
          }
          if (configJson["groupName"].success())
          {
            strcpy(groupName, configJson["groupName"]);
          }
          if (configJson["configUser"].success())
          {
            strcpy(configUser, configJson["configUser"]);
          }
          if (configJson["configPassword"].success())
          {
            strcpy(configPassword, configJson["configPassword"]);
          }
          String configJsonStr;
          configJson.printTo(configJsonStr);
          debugPrintln(String(F("SPIFFS: parsed json:")) + configJsonStr);
        }
        else
        {
          debugPrintln(F("SPIFFS: [ERROR] Failed to load json config"));
        }
      }
    }
  }
  else
  {
    debugPrintln(F("SPIFFS: [ERROR] Failed to mount FS"));
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void saveConfigCallback()
{ // Callback notifying us of the need to save config
  debugPrintln(F("SPIFFS: Configuration changed, flagging for save"));
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void saveUpdatedConfig()
{ // Save the custom parameters to config.json
  debugPrintln(F("SPIFFS: Saving config"));
  DynamicJsonBuffer jsonBuffer(256);
  JsonObject &json = jsonBuffer.createObject();
  json["mqttServer"] = mqttServer;
  json["mqttPort"] = mqttPort;
  json["mqttUser"] = mqttUser;
  json["mqttPassword"] = mqttPassword;
  json["haspNode"] = haspNode;
  json["groupName"] = groupName;
  json["configUser"] = configUser;
  json["configPassword"] = configPassword;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debugPrintln(F("SPIFFS: Failed to open config file for writing"));
  }
  else
  {
    json.printTo(configFile);
    configFile.close();
  }
  shouldSaveConfig = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clearSavedConfig()
{ // Clear out all local storage
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
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
  setNextionAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debugPrintln(F("RESET: Rebooting device"));
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleNotFound()
{ // webSevrer 404
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
  debugPrintln(httpMessage);
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

  // If we haven't collected the Nextion model, try now
  if (nextionModel == "")
  {
    sendNextionCmd("connect");
    delay(10);
    if (Serial.available() > 0)
    { // Process user input from HMI
      processNextionInput();
    }
  }
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
  httpMessage += String(F("<br/><br/><b>HASP Node Name</b> <i><small>(required)</small></i><input id='haspNode' required name='haspNode' maxlength=15 placeholder='HASP Node Name' value='")) + String(haspNode) + "'>";
  httpMessage += String(F("<br/><br/><b>Group Name</b> <i><small>(required)</small></i><input id='groupName' required name='groupName' maxlength=15 placeholder='Group Name' value='")) + String(groupName) + "'>";
  httpMessage += String(F("<br/><br/><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(mqttServer) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(mqttPort) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(mqttUser) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword'>"));
  httpMessage += String(F("<br/><br/><b>HASP Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(configUser) + "'>";
  httpMessage += String(F("<br/><b>HASP Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='")) + String(configPassword) + "'>";
  httpMessage += String(F("<br/><hr><button type='submit'>save settings</button></form>"));

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
    webServer.arg("wifiPass").toCharArray(wifiPass, 64);
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
    webServer.arg("haspNode").toCharArray(haspNode, 16);
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
  if (webServer.arg("mqttPassword") != String(mqttPassword))
  { // Handle mqttPassword
    shouldSaveConfig = true;
    webServer.arg("mqttPassword").toCharArray(mqttPassword, 32);
  }
  if (webServer.arg("configUser") != String(configUser))
  { // Handle configUser
    shouldSaveConfig = true;
    webServer.arg("configUser").toCharArray(configUser, 32);
  }
  if (webServer.arg("configPassword") != String(configPassword))
  { // Handle configPassword
    shouldSaveConfig = true;
    webServer.arg("configPassword").toCharArray(configPassword, 32);
  }

  if (shouldSaveConfig)
  { // Config updated, notify user and trigger write to SPIFFS
    httpMessage += String(F("<meta http-equiv='refresh' content='15;url=/' />"));
    httpMessage += FPSTR(HTTP_HEAD_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + String(F("</h1>"));
    httpMessage += String(F("<br/>Saving updated configuration values and restarting device"));
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);
    saveUpdatedConfig();
    if (shouldSaveWifi)
    {
      debugPrintln(String(F("CONFIG: Attempting connection to SSID: ")) + webServer.arg("wifiSSID"));
      setupWifi();
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
    clearSavedConfig();
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

  // If we haven't collected the Nextion model, try now
  if (nextionModel == "")
  {
    sendNextionCmd("connect");
    delay(10);
    if (Serial.available() > 0)
    { // Process user input from HMI
      processNextionInput();
    }
  }

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
  startEspOTA(webServer.arg("espFirmware"));
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

  static int lcdOtaTransferred = 0;
  static int lcdOtaRemaining;
  static int lcdOtaParts;

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
  else if (upload.status == UPLOAD_FILE_START)
  {
    String httpMessage = FPSTR(HTTP_HEAD);
    httpMessage.replace("{v}", (String(haspNode) + " LCD update"));
    httpMessage += FPSTR(HTTP_SCRIPT);
    httpMessage += FPSTR(HTTP_STYLE);
    httpMessage += String(HASP_STYLE);
    httpMessage += String(F("<meta http-equiv='refresh' content='30;url=/' />"));
    httpMessage += FPSTR(HTTP_HEAD_END);
    httpMessage += String(F("<h1>")) + String(haspNode) + " LCD update</h1>";
    httpMessage += String(F("Completing LCD firmware update from: ")) + String(upload.filename);
    httpMessage += FPSTR(HTTP_END);
    webServer.send(200, "text/html", httpMessage);

    WiFiUDP::stopAll(); // Keep mDNS responder and MQTT traffic from breaking things
    if (mqttClient.connected())
    {
      debugPrintln(F("LCD OTA: LCD firmware upload starting, closing MQTT connection."));
      mqttClient.publish(mqttStatusTopic, "OFF");
      mqttClient.publish(mqttSensorTopic, "{\"status\": \"unavailable\"}");
      mqttClient.disconnect();
    }

    debugPrintln(String(F("LCD OTA: Attempting firmware upload")));
    debugPrintln(String(F("LCD OTA: upload.filename: ")) + String(upload.filename));
    debugPrintln(String(F("LCD OTA: TFTfileSize: ")) + String(tftFileSize));

    lcdOtaRemaining = tftFileSize;
    lcdOtaParts = (lcdOtaRemaining / 4096) + 1;
    debugPrintln(String(F("LCD OTA: File upload beginning. Size ")) + String(lcdOtaRemaining) + String(F(" bytes in ")) + String(lcdOtaParts) + String(F(" 4k chunks.")));
    // Send empty command to LCD
    Serial1.write(nextionSuffix, sizeof(nextionSuffix));
    Serial1.flush();
    handleNextionInput();

    String lcdOtaNextionCmd = "whmi-wri " + String(tftFileSize) + ",115200,0";
    debugPrintln(String(F("LCD OTA: Sending LCD upload command: ")) + lcdOtaNextionCmd);
    Serial1.print(lcdOtaNextionCmd);
    Serial1.write(nextionSuffix, sizeof(nextionSuffix));
    Serial1.flush();

    if (otaReturnSuccess())
    {
      debugPrintln(F("LCD OTA: LCD upload command accepted."));
    }
    else
    {
      debugPrintln(F("LCD OTA: LCD upload command FAILED."));
      ESP.reset();
      delay(5000);
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE)
  {
    static int lcdOtaChunkCounter = 0;
    static uint16_t lcdOtaPartNum = 0;
    static int lcdOtaPercentComplete = 0;
    uint8_t lcdOtaBuffer[128] = {}; // max size of ESP8266 UART buffer
    uint16_t lcdOtaUploadIndex = 0;
    size_t lcdOtaPacketRemaining = upload.currentSize;
    while (lcdOtaPacketRemaining > 0)
    { // Write incoming data to panel as it arrives
      // read up to 128 bytes or whatever is left of the current packet
      size_t lcdOtaChunkSize = ((lcdOtaPacketRemaining > sizeof(lcdOtaBuffer)) ? sizeof(lcdOtaBuffer) : lcdOtaPacketRemaining);
      for (uint8_t i = 0; i < lcdOtaChunkSize; i++)
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
        if (otaReturnSuccess())
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
        delay(20);
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
      if (otaReturnSuccess())
      {
        debugPrintln(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        delay(5000); // extra delay while the LCD does its thing
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: Failure"));
        espReset();
      }
    }
  }
  else if (upload.status == UPLOAD_FILE_END)
  {
    if (lcdOtaTransferred >= tftFileSize)
    {
      if (otaReturnSuccess())
      {
        debugPrintln(String(F("LCD OTA: Success, wrote ")) + String(lcdOtaTransferred) + " of " + String(tftFileSize) + " bytes.");
        delay(5000); // extra delay while the LCD does its thing
        espReset();
      }
      else
      {
        debugPrintln(F("LCD OTA: Failure"));
        espReset();
      }
    }
  }
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

  debugPrintln("LCDFW: Attempting LCD firmware update from: " + String(webServer.arg("lcdFirmware")));
  startNextionOtaDownload(webServer.arg("lcdFirmware"));
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
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Rebooting...\"");
  espReset();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool updateCheck()
{ // firmware update check
  HTTPClient updateClient;
  debugPrintln(String(F("UPDATE: Checking update URL: ")) + String(UPDATE_URL));
  String updatePayload;
  updateClient.begin(UPDATE_URL);

  // start connection and send HTTP header
  int httpCode = updateClient.GET();

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      updatePayload = updateClient.getString();
    }
  }
  else
  {
    debugPrintln(String(F("UPDATE: Update check failed: ")) + updateClient.errorToString(httpCode));
    return false;
  }
  updateClient.end();

  DynamicJsonBuffer updateJsonBuffer(sizeof(updatePayload));

  // Parse JSON object
  JsonObject &updateJson = updateJsonBuffer.parseObject(updatePayload);
  if (!updateJson.success())
  {
    debugPrintln(F("UPDATE: JSON parsing failed"));
    return false;
  }
  else
  {
    if (updateJson["d1_mini"]["version"].success())
    {
      updateEspAvailableVersion = updateJson["d1_mini"]["version"].as<float>();
      espFirmwareUrl = updateJson["d1_mini"]["firmware"].as<String>();
      if (updateEspAvailableVersion > haspVersion)
      {
        updateEspAvailable = true;
        debugPrintln(String(F("UPDATE: New ESP version available: ")) + String(updateEspAvailableVersion));
      }
    }
    if (nextionModel && updateJson[nextionModel]["version"].success())
    {
      updateLcdAvailableVersion = updateJson[nextionModel]["version"].as<int>();
      lcdFirmwareUrl = updateJson[nextionModel]["firmware"].as<String>();
      if (updateLcdAvailableVersion > lcdVersion)
      {
        updateLcdAvailable = true;
        debugPrintln(String(F("UPDATE: New LCD version available: ")) + String(updateLcdAvailableVersion));
      }
    }
  }
  return true;
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
  delay(2000);
  Serial1.print("rest");
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  ESP.reset();
  delay(5000);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void startEspOTA(String espOtaUrl)
{ // Update ESP firmware from HTTP
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"HTTP update\\rstarting...\"");
  WiFiUDP::stopAll(); // Keep mDNS responder from breaking things

  t_httpUpdate_return returnCode = ESPhttpUpdate.update(espOtaUrl);
  switch (returnCode)
  {
  case HTTP_UPDATE_FAILED:
    debugPrintln("ESPFW: HTTP_UPDATE_FAILED error " + String(ESPhttpUpdate.getLastError()) + " " + ESPhttpUpdate.getLastErrorString());
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rFAILED\"");
    break;

  case HTTP_UPDATE_NO_UPDATES:
    debugPrintln(F("ESPFW: HTTP_UPDATE_NO_UPDATES"));
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rNo update\"");
    break;

  case HTTP_UPDATE_OK:
    debugPrintln(F("ESPFW: HTTP_UPDATE_OK"));
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rcomplete!\\r\\rRestarting.\"");
    espReset();
  }
  delay(5000);
  sendNextionCmd("page " + String(nextionActivePage));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef DEBUGTELNET
void handleTelnetClient()
{ // Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
  if (telnetServer.hasClient())
  {
    // client is connected
    if (!telnetClient || !telnetClient.connected())
    {
      if (telnetClient)
        telnetClient.stop();                   // client disconnected
      telnetClient = telnetServer.available(); // ready for new client
    }
    else
    {
      telnetServer.available().stop(); // have client, block new conections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available())
  {
    // client input processing
    while (telnetClient.available())
    {
      // Read data from telnet just to clear out the buffer
      telnetClient.read();
    }
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
void debugPrintln(String debugText)
{ // Debug output line of text to our debug targets
  String debugTimeText = "[+" + String(float(millis()) / 1000, 3) + "s] " + debugText;
#ifdef DEBUGSERIAL
  SoftwareSerial debugSerial = SoftwareSerial(17, 1); // 17==nc for RX, 1==TX pin
  debugSerial.begin(115200);
  debugSerial.println(debugTimeText);
  debugSerial.flush();
  Serial.println(debugTimeText);
  Serial.flush();
#endif
#ifdef DEBUGTELNET
  if (telnetClient.connected())
  {
    debugTimeText += "\r\n";
    const size_t len = debugTimeText.length();
    const char *buffer = debugTimeText.c_str();
    telnetClient.write(buffer, len);
    handleTelnetClient();
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void debugPrint(String debugText)
{ // Debug output single character to our debug targets (DON'T USE THIS!)
// Try to avoid using this function if at all possible.  When connected to telnet, printing each
// character requires a full TCP round-trip + acknowledgement back and execution halts while this
// happens.  Far better to put everything into a line and send it all out in one packet using
// debugPrintln.
#ifdef DEBUGSERIAL
  SoftwareSerial debugSerial = SoftwareSerial(17, 1); // 17==nc for RX, 1==TX pin
  debugSerial.begin(115200);
  debugSerial.print(debugText);
  debugSerial.flush();
  Serial.print(debugText);
  Serial.flush();
#endif
#ifdef DEBUGTELNET
  if (telnetClient.connected())
  {
    const size_t len = debugText.length();
    const char *buffer = debugText.c_str();
    telnetClient.write(buffer, len);
    handleTelnetClient();
  }
#endif
}