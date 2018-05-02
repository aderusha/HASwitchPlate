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
const char *wifiSSID = ""; // Leave unset for wireless autoconfig.
const char *wifiPass = ""; // YOU PROBABLY SHOULD NOT CHANGE THIS!
////////////////////////////////////////////////////////////////////////////////////////////////////
char mqttServer[64] = ""; // These may be overwritten by values saved from the web interface
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
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

#ifdef DEBUGTELNET
WiFiServer telnetServer(23);
WiFiClient telnetClient;
#endif
WiFiClient wifiClient;
MQTTClient mqttClient(256);
ESP8266WebServer webServer(80);
ESP8266HTTPUpdateServer httpOTAUpdate;

const char *haspVersion = "0.24";   // Current HASP software release version
byte nextionReturnBuffer[100];      // Byte array to pass around data coming from the panel
uint8_t nextionReturnIndex = 0;     // Index for nextionReturnBuffer
uint8_t nextionActivePage = 0;      // Track active LCD page
char wifiConfigPass[9];             // AP config password, always 8 chars + NUL
char wifiConfigAP[19];              // AP config SSID, haspNode + 3 chars
bool shouldSaveConfig = false;      // Flag to save json config to SPIFFS
bool suppressPageReport = false;    // Flag to temporarily supporess page change reports
byte espMac[6];                     // Byte array to store our MAC address
String mqttClientId;                // Auto-generated MQTT ClientID
String mqttGetSubtopic;             // MQTT subtopic for incoming commands requesting .val
String mqttStateTopic;              // MQTT topic for outgoing panel interactions
String mqttCommandTopic;            // MQTT topic for incoming panel commands
String mqttBinarySensorStateTopic;  // MQTT topic for publishing device connectivity state
String mqttLightCommandTopic;       // MQTT topic for incoming panel backlight on/off commands
String mqttLightStateTopic;         // MQTT topic for outgoing panel backlight on/off state
String mqttLightBrightCommandTopic; // MQTT topic for incoming panel backlight dimmer commands
String mqttLightBrightStateTopic;   // MQTT topic for outgoing panel backlight dimmer state

// Additional CSS style to match Hass theme
const char HASP_STYLE[] = "<style>button{background-color:#03A9F4;}</style>";
// Default link to compiled Arduino firmware
const char ESPFIRMWARE_URL[] = "http://haswitchplate.com/HASwitchPlate.ino.d1_mini.bin";

// Default link to compiled Nextion firmware
const char LCDFIRMWARE_URL[] = "http://haswitchplate.com/HASwitchPlate.tft";

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup
  delay(1000);
  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  debugPrintln(F("\r\nSYSTEM: Hardware initialized, starting program load"));

  // Uncomment for testing, clears all saved settings on each boot
  // clearSavedConfig();

  initializeNextion(); // initialize display

  readSavedConfig(); // Check filesystem for a saved config.json
  setupWifi();       // Start up networking

  if (configPassword)
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
  webServer.on("/espfirmware", webHandleESPFirmware);
  webServer.on("/lcdfirmware", webHandleLCDFirmware);
  webServer.on("/reboot", webHandleReboot);
  webServer.onNotFound(webHandleNotFound);
  webServer.begin();
  debugPrintln(String(F("HTTP: Server started @ http://")) + WiFi.localIP().toString());

  setupOTA();

  // Create server and assign callbacks for MQTT
  mqttClient.begin(mqttServer, atoi(mqttPort), wifiClient);
  mqttClient.onMessage(mqtt_callback);
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

  if (WiFi.status() != WL_CONNECTED)
  { // Check WiFi connection
    setNextionAttr("p[0].b[1].txt", "\"WiFi connecting\"");
    setupWifi();
    setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\"");
  }

  if (!mqttClient.connected())
  { // Check MQTT connection
    mqttConnect();
  }

  mqttClient.loop();        // MQTT client loop
  ArduinoOTA.handle();      // Arduino OTA loop
  webServer.handleClient(); // webServer loop

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
  // MQTT topic string definitions
  mqttStateTopic = "hasp/" + String(haspNode) + "/state";
  mqttCommandTopic = "hasp/" + String(haspNode) + "/command";
  mqttBinarySensorStateTopic = "hasp/" + String(haspNode) + "/status";
  mqttLightCommandTopic = "hasp/" + String(haspNode) + "/light/switch";
  mqttLightStateTopic = "hasp/" + String(haspNode) + "/light/status";
  mqttLightBrightCommandTopic = "hasp/" + String(haspNode) + "/brightness/set";
  mqttLightBrightStateTopic = "hasp/" + String(haspNode) + "/brightness/status";

  const String mqttCommandSubscription = mqttCommandTopic + "/#";
  const String mqttLightSubscription = "hasp/" + String(haspNode) + "/light/#";
  const String mqttLightBrightSubscription = "hasp/" + String(haspNode) + "/brightness/#";

  // Loop until we're reconnected to MQTT
  while (!mqttClient.connected())
  {
    // Generate an MQTT client ID as haspNode + our MAC address
    mqttClientId = String(haspNode) + "-" + String(espMac[0], HEX) + String(espMac[1], HEX) + String(espMac[2], HEX) + String(espMac[3], HEX) + String(espMac[4], HEX) + String(espMac[5], HEX);
    suppressPageReport = true;
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connecting\"");
    debugPrintln(String(F("MQTT: Attempting connection to broker ")) + String(mqttServer) + " as clientID " + mqttClientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(5, true, 5000);

    // declare LWT
    mqttClient.setWill(mqttBinarySensorStateTopic.c_str(), "OFF");

    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(mqttCommandSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttCommandSubscription);
      }
      if (mqttClient.subscribe(mqttLightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttLightBrightSubscription))
      {
        debugPrintln(String(F("MQTT: subscribed to ")) + mqttLightSubscription);
      }

      debugPrintln(String(F("MQTT: binary_sensor state: [")) + mqttBinarySensorStateTopic + "] : [ON]");
      mqttClient.publish(mqttBinarySensorStateTopic, "ON", true, 1);

      // Update panel with MQTT status
      setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connected\\r" + String(mqttServer) + "\"");
      debugPrintln(F("MQTT: connected"));
      String nextionPageRestore = "page " + String(nextionActivePage);
      sendNextionCmd(nextionPageRestore);
      suppressPageReport = true;
    }
    else
    { // Wait 15 seconds before retrying
      debugPrintln(String(F("MQTT connection failed, rc=")) + String(mqttClient.returnCode()) + String(F(".  Trying again in 15 seconds.")));
      delay(1000);
      unsigned long mqttReconnectTimeout = 15000;  // timeout for MQTT reconnect
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
void mqtt_callback(String &strTopic, String &strPayload)
{ // Handle incoming commands from MQTT

  // strTopic: homeassistant/haswitchplate/devicename/command/p[1].b[4].txt
  // strPayload: "Lights On"
  // subTopic: p[1].b[4].txt

  // Incoming Namespace:
  // '[...]/device/command' -m '' = undefined
  // '[...]/device/command' -m 'dim 50' = sendNextionCmd("dim 50")
  // '[...]/device/command/page' -m '1' = sendNextionCmd("page 1")
  // '[...]/device/command/update' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = startNextionOTA("http://192.168.0.10/local/HASwitchPlate.tft")
  // '[...]/device/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")

  debugPrintln(String(F("MQTT IN:  '")) + strTopic + "' : '" + strPayload + "'");

  if ((strTopic == mqttCommandTopic) && (strPayload == ""))
  { // '[...]/device/command' -m '' = undefined
    // currently undefined
  }
  else if ((strTopic == mqttCommandTopic))
  { // '[...]/device/command' -m 'dim 50' = sendNextionCmd("dim 50")
    sendNextionCmd(strPayload);
  }
  else if ((strTopic == (mqttCommandTopic + "/page")))
  { // '[...]/device/command/page' -m '1' = sendNextionCmd("page 1")
    if (nextionActivePage != strPayload.toInt())
    { // Hass likes to send duplicate responses to things like page requests and there are no plans to fix that behavior, so try and track it locally
      nextionActivePage = strPayload.toInt();
      sendNextionCmd("page " + strPayload);
    }
  }
  else if ((strTopic == (mqttCommandTopic + "/update")) && (strPayload != ""))
  { // '[...]/device/command/update' -m 'http://192.168.0.10/local/HASwitchPlate.tft' = startNextionOTA("http://192.168.0.10/local/HASwitchPlate.tft")
    startNextionOTA(strPayload);
  }
  else if ((strTopic == (mqttCommandTopic + "/reboot")))
  { // '[...]/device/command/reboot' = reboot microcontroller)
    debugPrintln(F("MQTT: Rebooting device"));
    delay(2000);
    ESP.reset();
    delay(5000);
  }
  else if ((strTopic == (mqttCommandTopic + "/factoryreset")))
  { // '[...]/device/command/factoryreset' = clear all saved settings)
    clearSavedConfig();
  }
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == ""))
  { // '[...]/device/command/p[1].b[4].txt' -m '' = getNextionAttr("p[1].b[4].txt")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    getNextionAttr(subTopic);
  }
  else if (strTopic.startsWith(mqttCommandTopic))
  { // '[...]/device/command/p[1].b[4].txt' -m '"Lights On"' = setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    setNextionAttr(subTopic, strPayload);
  }
  else if (strTopic == mqttLightBrightCommandTopic)
  { // change the brightness from the light topic
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    setNextionAttr("dim", String(panelDim));
    sendNextionCmd("dims=dim");
    mqttClient.publish(mqttLightBrightStateTopic, strPayload, true, 1);
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "OFF")
  { // set the panel dim OFF from the light topic, saving current dim level first
    sendNextionCmd("dims=dim");
    setNextionAttr("dim", "0");
    mqttClient.publish(mqttLightStateTopic, "OFF", true, 1);
  }
  else if (strTopic == mqttLightCommandTopic && strPayload == "ON")
  { // set the panel dim ON from the light topic, restoring saved dim level
    sendNextionCmd("dim=dims");
    mqttClient.publish(mqttLightStateTopic, "ON", true, 1);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void initializeNextion()
{ // Send initialization commands to the Nextion panel at startup
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print("rest");
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void handleNextionInput()
{ // Handle incoming serial data from the Nextion panel
  // This will collect serial data from the panel and place it into the global buffer
  // nextionReturnBuffer[nextionReturnIndex] until it finds a string of 3 consecutive
  // 0xFF values or runs into the timeout
  bool nextionCommandIncoming = true;           // we'll flip this to false when we receive 3 consecutive 0xFFs
  int nextionCommandTimeout = 1000;             // timeout for receiving termination string in milliseconds
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
      debugPrintln(String(F("HMI IN:   [Button ON] 'p[")) + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln(String(F("MQTT OUT: '")) + mqttButtonTopic + "' : 'ON'");
      mqttClient.publish(mqttButtonTopic, "ON");
    }
    if (nextionButtonAction == 0x00)
    {
      debugPrintln(String(F("HMI IN:   [Button OFF] 'p[")) + nextionPage + "].b[" + nextionButtonID + "]'");
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
    debugPrintln(String(F("HMI IN:   [sendme Page] '")) + nextionPage + "'");
    if ((nextionActivePage != nextionPage.toInt()) && !suppressPageReport)
    { // If we have a new page, and we haven't set the flag to temporarily stop reporting page changes
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
      debugPrintln(String(F("HMI IN:   [Touch ON] '")) + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOn";
      debugPrintln(String(F("MQTT OUT: '")) + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic, xyCoord);
    }
    else if (nextionTouchAction == 0x00)
    {
      debugPrintln(String(F("HMI IN:   [Touch OFF] '")) + xyCoord + "'");
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
    debugPrintln(String(F("HMI IN:   [String Return] '")) + getString + "'");
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
    debugPrintln(String(F("HMI IN:   [Int Return] '")) + getString + "'");
    // If there's no outstanding request for a value, publish to mqttStateTopic
    if (mqttGetSubtopic == "")
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
  if (nextionReturnBuffer[0] == 0x1A)
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
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
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
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print("get " + hmi_attribute);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln(String(F("HMI OUT:  'get ")) + hmi_attribute + "'");
  processNextionInput();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void sendNextionCmd(String nexcmd)
{ // Send a raw command to the Nextion panel
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print(nexcmd);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln(String(F("HMI OUT:  ")) + nexcmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setupWifi()
{ // Connect to WiFi
  suppressPageReport = true;
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"WiFi connecting\"");

  // Read our MAC address and save it to espMac
  WiFi.macAddress(espMac);
  // Assign our hostname before connecting to WiFi
  WiFi.hostname(haspNode);

  if (String(wifiSSID) == "")
  { // If the sketch has no defined a static wifiSSID to connect to,
    // use WiFiManager to collect required information from the user.

    // id/name, placeholder/prompt, default value, length, extra tags
    WiFiManagerParameter custom_haspNodeHeader("<b>HASP Node Name</b>");
    WiFiManagerParameter custom_haspNode("haspNode", "HASP Node (required)", haspNode, 15, " maxlength=15 required");
    WiFiManagerParameter custom_mqttHeader("<br/><hr><b>MQTT Broker</b>");
    WiFiManagerParameter custom_mqttServer("mqttServer", "MQTT Server", mqttServer, 63, " maxlength=39");
    WiFiManagerParameter custom_mqttPort("mqttPort", "MQTT Port", mqttPort, 5, " maxlength=5 type='number'");
    WiFiManagerParameter custom_mqttUser("mqttUser", "MQTT User", mqttUser, 31, " maxlength=31");
    WiFiManagerParameter custom_mqttPassword("mqttPassword", "MQTT Password", mqttPassword, 31, " maxlength=31 type='password'");
    WiFiManagerParameter custom_configHeader("<br/><hr><b>Admin access</b>");
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
    String strWifiConfigAP = String(haspNode) + "-" + String(espMac[5], HEX);
    strWifiConfigAP.toCharArray(wifiConfigAP, (strWifiConfigAP.length() + 1));
    // Construct a WiFi SSID password using bytes [3] and [4] of our MAC
    String strConfigPass = "hasp" + String(espMac[3], HEX) + String(espMac[4], HEX);
    strConfigPass.toCharArray(wifiConfigPass, 9);

    // Fetches SSID and pass from EEPROM and tries to connect
    // If it does not connect it starts an access point with the specified name
    // and goes into a blocking loop awaiting configuration.
    if (!wifiManager.autoConnect(wifiConfigAP, wifiConfigPass))
    { // Reset and try again
      debugPrintln(F("WIFI: Failed to connect and hit timeout"));
      delay(2000);
      ESP.reset();
      delay(5000);
    }

    // Read updated parameters
    strcpy(mqttServer, custom_mqttServer.getValue());
    strcpy(mqttPort, custom_mqttPort.getValue());
    strcpy(mqttUser, custom_mqttUser.getValue());
    strcpy(mqttPassword, custom_mqttPassword.getValue());
    strcpy(haspNode, custom_haspNode.getValue());
    strcpy(configUser, custom_configUser.getValue());
    strcpy(configPassword, custom_configPassword.getValue());

    if (shouldSaveConfig)
    { // Save the custom parameters to FS
      saveUpdatedConfig();
    }
  }
  else
  { // wifiSSID has been defined in the sketch, so attempt to connect to it forever
    debugPrintln(String(F("Connecting to WiFi network: ")) + String(wifiSSID));
    WiFi.mode(WIFI_STA);
    WiFi.begin(wifiSSID, wifiPass);

    while (WiFi.status() != WL_CONNECTED)
    {
      delay(500);
    }
  }
  // If you get here you have connected to WiFi
  debugPrintln(String(F("WIFI: Connected succesfully and assigned IP: ")) + WiFi.localIP().toString());
  String nextionPageRestore = "page " + String(nextionActivePage);
  sendNextionCmd(nextionPageRestore);
  suppressPageReport = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void wifiConfigCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debugPrintln(F("WIFI: Failed to connect to assigned AP, entering config mode"));
  suppressPageReport = true;
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Configure HASP\\r\\rAP:" + String(wifiConfigAP) + "\\rPass:" + String(wifiConfigPass) + "\"");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setupOTA()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(haspNode);
  ArduinoOTA.setPassword(configPassword);

  ArduinoOTA.onStart([]() {
    suppressPageReport = true;
    debugPrintln(F("ESP OTA: update start"));
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
  });
  ArduinoOTA.onEnd([]() {
    suppressPageReport = false;
    String nextionPageRestore = "page " + String(nextionActivePage);
    sendNextionCmd(nextionPageRestore);
    debugPrintln(F("ESP OTA: update complete"));
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    suppressPageReport = false;
    String nextionPageRestore = "page " + String(nextionActivePage);
    sendNextionCmd(nextionPageRestore);
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
  });
  ArduinoOTA.begin();
  debugPrintln(F("ESP OTA: Over the Air firmware update ready"));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void startNextionOTA(String otaURL)
{ // Upload firmware to the Nextion LCD
  // based in large part on code posted by indev2 here:
  // http://support.iteadstudio.com/support/discussions/topics/11000007686/page/2
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  int nextionResponseTimeout = 500;              // timeout for receiving ack string in milliseconds
  unsigned long nextionResponseTimer = millis(); // record current time for our timeout

  int FileSize = 0;
  String nexcmd;
  int count = 0;
  byte partNum = 0;
  int total = 0;
  int pCent = 0;

  debugPrintln(String(F("LCD OTA: Attempting firmware download from:")) + otaURL);
  HTTPClient http;
  http.begin(otaURL);
  int httpCode = http.GET();
  if (httpCode > 0)
  { // HTTP header has been send and Server response header has been handled
    debugPrintln(String(F("LCD OTA: HTTP GET return code:")) + String(httpCode));
    if (httpCode == HTTP_CODE_OK)
    { // file found at server
      // get length of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      FileSize = len;
      int Parts = (len / 4096) + 1;
      debugPrintln(String(F("LCD OTA: File found at Server. Size ")) + String(len) + String(F(" bytes in ")) + String(Parts) + String(F(" 4k chunks.")));
      // create buffer for read
      uint8_t buff[128] = {};
      // get tcp stream
      WiFiClient *stream = http.getStreamPtr();

      debugPrintln(F("LCD OTA: Issuing NULL command"));
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      handleNextionInput();

      String nexcmd = "whmi-wri " + String(FileSize) + ",115200,0";
      debugPrintln(String(F("LCD OTA: Sending LCD upload command: ")) + nexcmd);
      Serial1.print(nexcmd);
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();

      if (otaReturnSuccess())
      {
        debugPrintln(F("LCD OTA: LCD upload command accepted"));
      }
      else
      {
        debugPrintln(F("LCD OTA: LCD upload command FAILED."));
        return;
      }
      debugPrintln(F("LCD OTA: Starting update"));
      while (http.connected() && (len > 0 || len == -1))
      { // Write incoming data to panel as it arrives
        // get available data size
        size_t size = stream->available();
        if (size)
        {
          // read up to 128 bytes
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          // write it to panel
          Serial1.write(buff, c);
          Serial1.flush();
          count += c;
          if (count == 4096)
          {
            nextionResponseTimer = millis();
            partNum++;
            total += count;
            pCent = (total * 100) / FileSize;
            count = 0;
            if (otaReturnSuccess())
            {
              debugPrintln(String(F("LCD OTA: Part ")) + String(partNum) + String(F(" OK, ")) + String(pCent) + String(F("% complete")));
            }
            else
            {
              debugPrintln(String(F("LCD OTA: Part ")) + String(partNum) + String(F(" FAILED, ")) + String(pCent) + String(F("% complete")));
            }
          }
          if (len > 0)
          {
            len -= c;
          }
        }
      }
      partNum++;
      total += count;
      if ((total == FileSize) && otaReturnSuccess())
      {
        debugPrintln(String(F("LCD OTA: success, wrote ")) + String(total) + " of " + String(FileSize) + " bytes.");
        delay(2000);
        ESP.reset();
        delay(5000);
      }
      else
      {
        debugPrintln(F("LCD OTA: failure"));
      }
    }
  }
  else
  {
    debugPrintln(String(F("LCD OTA: HTTP GET failed, error code ")) + http.errorToString(httpCode));
  }
  http.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
bool otaReturnSuccess()
{ // Monitor the serial port for a 0x05 response within our timeout

  int nextionCommandTimeout = 1000;             // timeout for receiving termination string in milliseconds
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
        DynamicJsonBuffer jsonBuffer(256);
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        if (json.success())
        {
          if (json.containsKey("mqttServer"))
          {
            strcpy(mqttServer, json["mqttServer"]);
          }
          if (json.containsKey("mqttPort"))
          {
            strcpy(mqttPort, json["mqttPort"]);
          }
          if (json.containsKey("mqttUser"))
          {
            strcpy(mqttUser, json["mqttUser"]);
          }
          if (json.containsKey("mqttPassword"))
          {
            strcpy(mqttPassword, json["mqttPassword"]);
          }
          if (json.containsKey("haspNode"))
          {
            strcpy(haspNode, json["haspNode"]);
          }
          if (json.containsKey("configUser"))
          {
            strcpy(configUser, json["configUser"]);
          }
          if (json.containsKey("configPassword"))
          {
            strcpy(configPassword, json["configPassword"]);
          }
          String jsonStr;
          json.printTo(jsonStr);
          debugPrintln(String(F("SPIFFS: parsed json:")) + jsonStr);
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
  suppressPageReport = true;
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
  debugPrintln(F("RESET: Formatting SPIFFS"));
  SPIFFS.format();
  debugPrintln(F("RESET: Clearing WiFiManager settings..."));
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  EEPROM.begin(512);
  debugPrintln(F("Clearing EEPROM..."));
  for (int i = 0; i < EEPROM.length(); i++)
  {
    EEPROM.write(i, 0);
  }
  setNextionAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debugPrintln(F("RESET: Rebooting device"));
  delay(2000);
  sendNextionCmd("rest");
  delay(100);
  ESP.reset();
  delay(5000);
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
  if (String(configPassword) != "")
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
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode);
  httpMessage += String(F("</h1>"));

  httpMessage += String(F("<form method='get' action='saveConfig'>"));
  httpMessage += String(F("<b>HASP Node Name</b> <i><small>(required)</small></i><input id='haspNode' required name='haspNode' maxlength=15 placeholder='HASP Node Name' value='")) + String(haspNode) + "'>";
  httpMessage += String(F("<br/><br/><hr><b>MQTT Broker</b> <i><small>(required)</small></i><input id='mqttServer' required name='mqttServer' maxlength=63 placeholder='mqttServer' value='")) + String(mqttServer) + "'>";
  httpMessage += String(F("<br/><b>MQTT Port</b> <i><small>(required)</small></i><input id='mqttPort' required name='mqttPort' type='number' maxlength=5 placeholder='mqttPort' value='")) + String(mqttPort) + "'>";
  httpMessage += String(F("<br/><b>MQTT User</b> <i><small>(optional)</small></i><input id='mqttUser' name='mqttUser' maxlength=31 placeholder='mqttUser' value='")) + String(mqttUser) + "'>";
  httpMessage += String(F("<br/><b>MQTT Password</b> <i><small>(optional)</small></i><input id='mqttPassword' name='mqttPassword' type='password' maxlength=31 placeholder='mqttPassword'>"));
  httpMessage += String(F("<br/><br/><hr><b>HASP Admin Username</b> <i><small>(optional)</small></i><input id='configUser' name='configUser' maxlength=31 placeholder='Admin User' value='")) + String(configUser) + "'>";
  httpMessage += String(F("<br/><b>HASP Admin Password</b> <i><small>(optional)</small></i><input id='configPassword' name='configPassword' type='password' maxlength=31 placeholder='Admin User Password' value='")) + String(configPassword) + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>save settings</button></form>"));

  httpMessage += String(F("<br/><hr><br/><form method='get' action='firmware'>"));
  httpMessage += String(F("<button type='submit'>update firmware</button></form>"));

  httpMessage += String(F("<br/><hr><br/><form method='get' action='reboot'>"));
  httpMessage += String(F("<button type='submit'>reboot device</button></form>"));

  httpMessage += String(F("<br/><hr><br/><form method='get' action='resetConfig'>"));
  httpMessage += String(F("<button type='submit'>factory reset settings</button></form>"));

  httpMessage += String(F("<br/><hr><br/><b>MQTT Status: </b>"));
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
  httpMessage += String(F("<br/><b>LCD Active Page: </b>")) + String(nextionActivePage);
  httpMessage += String(F("<br/><b>CPU Frequency: </b>")) + String(ESP.getCpuFreqMHz()) + String(F("MHz"));
  httpMessage += String(F("<br/><b>Sketch Size: </b>")) + String(ESP.getSketchSize()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Free Sketch Space: </b>")) + String(ESP.getFreeSketchSpace()) + String(F(" bytes"));
  httpMessage += String(F("<br/><b>Heap Free: </b>")) + String(ESP.getFreeHeap());
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleSaveConfig()
{ // http://plate01/saveConfig
  if (String(configPassword) != "")
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

  // Check required values
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
    debugPrintln(F("RESET: Rebooting device"));
    delay(2000);
    ESP.reset();
    delay(5000);
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
  if (String(configPassword) != "")
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
  if (String(configPassword) != "")
  { //Request HTTP auth if configPassword is set
    if (!webServer.authenticate(configUser, configPassword))
    {
      return webServer.requestAuthentication();
    }
  }

  String httpMessage = FPSTR(HTTP_HEAD);
  httpMessage.replace("{v}", (String(haspNode) + " update"));
  httpMessage += FPSTR(HTTP_SCRIPT);
  httpMessage += FPSTR(HTTP_STYLE);
  httpMessage += String(HASP_STYLE);
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " update";
  httpMessage += String(F("</h1>"));

  // Display main firmware page
  // HTTPS Disabled pending resolution of issue: https://github.com/esp8266/Arduino/issues/4696
  // Until then, using a proxy host at http://haswitchplate.com to deliver unsecured firmware images from GitHub
  httpMessage += String(F("<form method='get' action='espfirmware'>"));
  httpMessage += String(F("<br/><b>Update ESP8266 from URL</b><small><i> http only</i></small><input id='espFirmwareURL' name='espFirmware' value='")) + String(ESPFIRMWARE_URL) + "'>";
  httpMessage += String(F("<br/><br/><button type='submit'>Update ESP from URL</button></form>"));

  httpMessage += String(F("<br/><hr><form method='POST' action='/update' enctype='multipart/form-data'>"));
  httpMessage += String(F("<br/><b>Update ESP8266 from file</b><input name='update' type='file'>"));
  httpMessage += String(F("<br/><br/><button type='submit'>Update ESP from file</button></form>"));

  // httpMessage += String(F("<br/><hr><form method='get' action='lcdfirmware'>"));
  // httpMessage += String(F("<br/><b>Update LCD from URL</b><input id='lcdFirmware' name='lcdFirmware' value='")) + String(LCDFIRMWARE_URL) + "'>";
  // httpMessage += String(F("<br/><br/><button type='submit'>Update LCD from URL</button></form>"));

  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleESPFirmware()
{ // http://plate01/espfirmware
  if (String(configPassword) != "")
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
  httpMessage += FPSTR(HTTP_HEAD_END);
  httpMessage += String(F("<h1>"));
  httpMessage += String(haspNode) + " ESP update";
  httpMessage += String(F("</h1>"));
  httpMessage += "<br/>Updating ESP firmware from: " + String(webServer.arg("espFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);

  debugPrintln("ESPFW: Attempting ESP firmware update from: " + String(webServer.arg("espFirmware")));
  suppressPageReport = true;
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"HTTP update\\rstarting...\"");
  String nextionPageRestore = "page " + String(nextionActivePage);

  t_httpUpdate_return returnCode = ESPhttpUpdate.update(webServer.arg("espFirmware"), haspVersion);
  switch (returnCode)
  {
  case HTTP_UPDATE_FAILED:
    debugPrintln("ESPFW: HTTP_UPDATE_FAILED error " + String(ESPhttpUpdate.getLastError()) + " " + ESPhttpUpdate.getLastErrorString());
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rFAILED\"");
    delay(5000);
    suppressPageReport = false;
    sendNextionCmd(nextionPageRestore);
    break;

  case HTTP_UPDATE_NO_UPDATES:
    debugPrintln(F("ESPFW: HTTP_UPDATE_NO_UPDATES"));
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rNo update\"");
    delay(5000);
    suppressPageReport = false;
    sendNextionCmd(nextionPageRestore);
    break;

  case HTTP_UPDATE_OK:
    debugPrintln(F("ESPFW: HTTP_UPDATE_OK"));
    setNextionAttr("p[0].b[1].txt", "\"HTTP Update\\rcomplete!\\r\\rRestarting.\"");
    delay(2000);
    ESP.reset();
    delay(5000);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleLCDFirmware()
{ // http://plate01/lcdfirmware
  if (String(configPassword) != "")
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
  httpMessage += "<br/>lcdFirmware: " + String(webServer.arg("lcdFirmware"));
  httpMessage += FPSTR(HTTP_END);
  webServer.send(200, "text/html", httpMessage);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void webHandleReboot()
{ // http://plate01/lcdfirmware
  if (String(configPassword) != "")
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
  suppressPageReport = true;
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Rebooting...\"");

  delay(2000);
  sendNextionCmd("rest");
  delay(200);
  ESP.reset();
  delay(5000);
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
  Serial.print(debugText);
  Serial.flush();
#endif
#ifdef DEBUGTELNET
  if (telnetClient.connected())
  {
    const size_t len = debugText.length();
    const char *buffer = debugText.c_str();
    //telnetClient.println(debugText);
    telnetClient.write(buffer, len);
  }
#endif
}