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
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software
// and associated documentation files (the "Software"), to deal in the Software without restriction,
// including without limitation the rights to use, copy, modify, merge, publish, distribute, 
// sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or
// substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
// NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
////////////////////////////////////////////////////////////////////////////////////////////////////

// Optional: Assign default values here.  These may be overwritten by saved values from config.json
char mqttServer[40] = "";
char mqttPort[6] = "1883";
char mqttUser[32] = "";
char mqttPassword[32] = "";
char haspNode[16] = "plate01";
char otaPassword[32] = "OTA_Update_Password";
////////////////////////////////////////////////////////////////////////////////////////////////////

// Serial debug output control, comment out to disable
#define DEBUGSERIAL
// Open a super-insecure (but read-only) telnet debug port
#define DEBUGTELNET

#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <MQTT.h>

#ifdef DEBUGTELNET
WiFiServer telnetServer(23);
WiFiClient telnetClient;
#endif
WiFiClient wifiClient;
MQTTClient mqttClient(256);

const char* haspVersion = "0.2.0";  // Current HASP software release version
byte nextionReturnBuffer[100];      // Byte array to pass around data coming from the panel
uint8_t nextionReturnIndex = 0;     // Index for nextionReturnBuffer
uint8_t nextionActivePage = 0;      // Track active LCD page
char wifiConfigPass[9];             // AP config password, always 8 chars + NUL
char wifiConfigAP[19];              // AP config SSID, haspNode + 3 chars
bool shouldSaveConfig = false;      // Flag to save json config to SPIFFS
byte espMac[6];                     // Byte array to store our MAC address
String mqttGetSubtopic;             // MQTT subtopic for incoming commands requesting .val
String mqttStateTopic;              // MQTT topic for outgoing panel interactions
String mqttCommandTopic;            // MQTT topic for incoming panel commands
String mqttBinarySensorStateTopic;  // MQTT topic for publishing device connectivity state
String mqttLightCommandTopic;       // MQTT topic for incoming panel backlight on/off commands
String mqttLightStateTopic;         // MQTT topic for outgoing panel backlight on/off state
String mqttLightBrightCommandTopic; // MQTT topic for incoming panel backlight dimmer commands
String mqttLightBrightStateTopic;   // MQTT topic for outgoing panel backlight dimmer state

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{ // System setup

  Serial.begin(115200);  // Serial - LCD RX (after swap), debug TX
  Serial1.begin(115200); // Serial1 - LCD TX, no RX
  debugPrintln("\r\nHardware initialized, starting program load");

  // If this ESP8266 has saved WiFi creds, you'll need to uncomment
  // this command, flash to the device, let it run once, then
  // comment it out again to clear out any previously saved config
  // clearSavedConfig();

  initializeNextion(); // initialize display

  readSavedConfig(); // Check filesystem for a saved config.json
  setupWifi();       // Start up networking

  if (otaPassword[0])
  { // Start up OTA
    setupOTA();
  }

  // Create server and assign callbacks for MQTT
  //mqttClient.begin(mqttServer, mqttPort, wifiClient);
  mqttClient.begin(mqttServer, atoi(mqttPort), wifiClient);
  mqttClient.onMessage(mqtt_callback);
  mqttConnect();

#ifdef DEBUGTELNET
  // Setup telnet server for remote debug output
  telnetServer.begin();
  telnetServer.setNoDelay(true);
  debugPrintln("telnetClient debug server enabled at telnet:" + WiFi.localIP().toString());
#endif

  debugPrintln("Initialization complete, swapping debug serial output to D8\r\n");
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

  // MQTT client loop
  mqttClient.loop();

  if (otaPassword[0])
  { // OTA loop
    ArduinoOTA.handle();
  }

#ifdef DEBUGTELNET
  // telnetClient loop
  handleTelnetClient();
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Functions

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

  debugPrintln("MQTT IN:  '" + strTopic + "' : '" + strPayload + "'");

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
void setNextionAttr(String hmi_attribute, String hmi_value)
{ // Set the value of a Nextion component attribute
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print(hmi_attribute);
  Serial1.print("=");
  Serial1.print(hmi_value);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln("HMI OUT:  '" + hmi_attribute + "=" + hmi_value + "'");
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
  debugPrintln("HMI OUT:  'get " + hmi_attribute + "'");
  processNextionInput();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void sendNextionCmd(String nexcmd)
{ // Send a raw command to the Nextion panel
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print(nexcmd);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln("HMI OUT:  " + nexcmd);
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
      debugPrintln("HMI IN:   [Button ON] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'ON'");
      mqttClient.publish(mqttButtonTopic, "ON");
    }
    if (nextionButtonAction == 0x00)
    {
      debugPrintln("HMI IN:   [Button OFF] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'OFF'");
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
    debugPrintln("HMI IN:   [sendme Page] '" + nextionPage + "'");
    if (nextionActivePage != nextionPage.toInt())
    {
      nextionActivePage = nextionPage.toInt();
      String mqttPageTopic = mqttStateTopic + "/page";
      debugPrintln("MQTT OUT: '" + mqttPageTopic + "' : '" + nextionPage + "'");
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
      debugPrintln("HMI IN:   [Touch ON] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOn";
      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic, xyCoord);
    }
    else if (nextionTouchAction == 0x00)
    {
      debugPrintln("HMI IN:   [Touch OFF] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOff";
      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
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
    debugPrintln("HMI IN:   [String Return] '" + getString + "'");
    if (mqttGetSubtopic == "")
    { // If there's no outstanding request for a value, publish to mqttStateTopic
      debugPrintln("MQTT OUT: '" + mqttStateTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttStateTopic, getString);
    }
    else
    { // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      debugPrintln("MQTT OUT: '" + mqttReturnTopic + "' : '" + getString + "]");
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
    debugPrintln("HMI IN:   [Int Return] '" + getString + "'");
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
void setupWifi()
{ // Connect to WiFi
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"WiFi connecting\"");

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqttServer("server", "mqtt server", mqttServer, 40);
  WiFiManagerParameter custom_mqttPort("port", "mqtt port", mqttPort, 6);
  WiFiManagerParameter custom_mqttUser("user", "mqtt user", mqttUser, 32);
  WiFiManagerParameter custom_mqttPassword("password", "mqtt password", mqttPassword, 32);
  WiFiManagerParameter custom_haspNode("node", "HASP node", haspNode, 16);
  WiFiManagerParameter custom_otaPassword("password", "OTA password", otaPassword, 32);

  // WiFiManager local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  // Set client static ip
  // wifiManager.setSTAStaticIPConfig(IPAddress(10, 0, 1, 99), IPAddress(10, 0, 1, 1), IPAddress(255, 255, 255, 0));

  // Add all your parameters here
  wifiManager.addParameter(&custom_mqttServer);
  wifiManager.addParameter(&custom_mqttPort);
  wifiManager.addParameter(&custom_mqttUser);
  wifiManager.addParameter(&custom_mqttPassword);
  wifiManager.addParameter(&custom_haspNode);
  wifiManager.addParameter(&custom_otaPassword);

  // Set minimum quality of signal so it ignores AP's under that quality. Defaults to 8%
  //wifiManager.setMinimumSignalQuality();

  // Timeout until configuration portal gets turned off
  wifiManager.setTimeout(180);

  wifiManager.setAPCallback(wifiConfigCallback);

  // Read our MAC address and save it to espMac
  WiFi.macAddress(espMac);

  // Construct AP name as haspNode-byte[5] of mac
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
    debugPrintln("WIFI: Failed to connect and hit timeout");
    delay(3000);
    ESP.reset();
    delay(5000);
  }

  // If you get here you have connected to the WiFi
  debugPrintln("WIFI: Connected succesfully and assigned IP: " + WiFi.localIP().toString());
  String nextionPageRestore = "page " + String(nextionActivePage);
  sendNextionCmd(nextionPageRestore);

  // Read updated parameters
  strcpy(mqttServer, custom_mqttServer.getValue());
  strcpy(mqttPort, custom_mqttPort.getValue());
  strcpy(mqttUser, custom_mqttUser.getValue());
  strcpy(mqttPassword, custom_mqttPassword.getValue());
  strcpy(haspNode, custom_haspNode.getValue());
  strcpy(otaPassword, custom_otaPassword.getValue());

  if (shouldSaveConfig)
  { // Save the custom parameters to FS
    saveUpdatedConfig();
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void wifiConfigCallback(WiFiManager *myWiFiManager)
{ // Notify the user that we're entering config mode
  debugPrintln("WIFI: Failed to connect to assigned AP, entering config mode");
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Configure HASP\\r\\rAP:" + String(wifiConfigAP) + "\\rPass:" + String(wifiConfigPass) + "\"");
}

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
    String mqttClientId = String(haspNode) + "-" + String(espMac[0], HEX) + String(espMac[1], HEX) + String(espMac[2], HEX) + String(espMac[3], HEX) + String(espMac[4], HEX) + String(espMac[5], HEX);
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connecting\"");
    debugPrintln("Attempting MQTT connection to broker " + String(mqttServer) + " as clientID " + mqttClientId);

    // Set keepAlive, cleanSession, timeout
    mqttClient.setOptions(5, true, 5000);

    // declare LWT
    mqttClient.setWill(mqttBinarySensorStateTopic.c_str(), "OFF");

    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword))
    { // Attempt to connect to broker, setting last will and testament
      // Subscribe to our incoming topics
      if (mqttClient.subscribe(mqttCommandSubscription))
      {
        debugPrintln("MQTT: subscribed to " + mqttCommandSubscription);
      }
      if (mqttClient.subscribe(mqttLightSubscription))
      {
        debugPrintln("MQTT: subscribed to " + mqttLightSubscription);
      }
      if (mqttClient.subscribe(mqttLightBrightSubscription))
      {
        debugPrintln("MQTT: subscribed to " + mqttLightSubscription);
      }

      debugPrintln("MQTT: binary_sensor state: [" + mqttBinarySensorStateTopic + "] : [ON]");
      mqttClient.publish(mqttBinarySensorStateTopic, "ON", true, 1);

      // Update panel with MQTT status
      setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connected\\r" + String(mqttServer) + "\"");
      debugPrintln("MQTT: connected");
      String nextionPageRestore = "page " + String(nextionActivePage);
      sendNextionCmd(nextionPageRestore);
    }
    else
    { // Wait 15 seconds before retrying
      debugPrintln("MQTT connection failed, rc=" + String(mqttClient.returnCode()) + ".  Trying again in 15 seconds.");
      delay(1000);
      unsigned long mqttReconnectTimeout = 15000;  // timeout for MQTT reconnect
      unsigned long mqttReconnectTimer = millis(); // record current time for our timeout
      while ((millis() - mqttReconnectTimer) < mqttReconnectTimeout)
      { // Handle OTA while we're waiting for MQTT to reconnect
        yield();
        if (otaPassword[0])
        {
          ArduinoOTA.handle();
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void setupOTA()
{ // (mostly) boilerplate OTA setup from library examples

  ArduinoOTA.setHostname(haspNode);
  ArduinoOTA.setPassword(otaPassword);

  ArduinoOTA.onStart([]() {
    debugPrintln("ESP OTA:  update start");
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\"");
  });
  ArduinoOTA.onEnd([]() {
    debugPrintln("ESP OTA:  update complete");
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rComplete!\"");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    setNextionAttr("p[0].b[1].txt", "\"ESP OTA Update\\rProgress: " + String(progress / (total / 100)) + "%\"");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    debugPrintln("ESP OTA:  ERROR code " + String(error));
    if (error == OTA_AUTH_ERROR)
      debugPrintln("ESP OTA:  ERROR - Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      debugPrintln("ESP OTA:  ERROR - Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      debugPrintln("ESP OTA:  ERROR - Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      debugPrintln("ESP OTA:  ERROR - Receive Failed");
    else if (error == OTA_END_ERROR)
      debugPrintln("ESP OTA:  ERROR - End Failed");
  });
  ArduinoOTA.begin();
  debugPrintln("ESP OTA:  Over the Air firmware update ready");
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

  debugPrintln("LCD OTA: Attempting firmware download from:" + otaURL);
  HTTPClient http;
  http.begin(otaURL);
  int httpCode = http.GET();
  if (httpCode > 0)
  { // HTTP header has been send and Server response header has been handled
    debugPrintln("LCD OTA: HTTP GET return code:" + String(httpCode));
    if (httpCode == HTTP_CODE_OK)
    { // file found at server
      // get length of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      FileSize = len;
      int Parts = (len / 4096) + 1;
      debugPrintln("LCD OTA: File found at Server. Size " + String(len) + " bytes in " + String(Parts) + " 4k chunks.");
      // create buffer for read
      uint8_t buff[128] = {};
      // get tcp stream
      WiFiClient *stream = http.getStreamPtr();

      debugPrintln("LCD OTA: Issuing NULL command");
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      handleNextionInput();

      String nexcmd = "whmi-wri " + String(FileSize) + ",115200,0";
      debugPrintln("LCD OTA: Sending LCD upload command: " + nexcmd);
      Serial1.print(nexcmd);
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();

      if (otaReturnSuccess())
      {
        debugPrintln("LCD OTA: LCD upload command accepted");
      }
      else
      {
        debugPrintln("LCD OTA: LCD upload command FAILED.");
        return;
      }
      debugPrintln("LCD OTA: Starting update");
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
              debugPrintln("LCD OTA: Part " + String(partNum) + " OK, " + String(pCent) + "% complete");
            }
            else
            {
              debugPrintln("LCD OTA: Part " + String(partNum) + " FAILED, " + String(pCent) + "% complete");
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
        debugPrintln("LCD OTA: success, wrote " + String(total) + " of " + String(FileSize) + " bytes.");
        delay(2000);
        ESP.reset();
      }
      else
      {
        debugPrintln("LCD OTA: failure");
      }
    }
  }
  else
  {
    debugPrintln("LCD OTA: HTTP GET failed, error code " + http.errorToString(httpCode));
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
void readSavedConfig()
{ // Read saved config.json from SPIFFS

  // Read configuration from FS json
  debugPrintln("SPIFFS: mounting FS...");

  if (SPIFFS.begin())
  {
    debugPrintln("SPIFFS: mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      // File exists, reading and loading
      debugPrintln("SPIFFS: reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        debugPrintln("SPIFFS: opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());
        //json.printTo(Serial);
        if (json.success())
        {
          debugPrintln("SPIFFS: parsed json");
          strcpy(mqttServer, json["mqttServer"]);
          strcpy(mqttPort, json["mqttPort"]);
          strcpy(mqttUser, json["mqttUser"]);
          strcpy(mqttPassword, json["mqttPassword"]);
          strcpy(haspNode, json["haspNode"]);
          strcpy(otaPassword, json["otaPassword"]);
        }
        else
        {
          debugPrintln("SPIFFS: [ERROR] Failed to load json config");
        }
      }
    }
  }
  else
  {
    debugPrintln("SPIFFS: [ERROR] Failed to mount FS");
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void saveConfigCallback()
{ // Callback notifying us of the need to save config
  debugPrintln("SPIFFS: Configuration changed, flagging for save");
  shouldSaveConfig = true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void saveUpdatedConfig()
{ // Save the custom parameters to config.json
  debugPrintln("SPIFFS: Saving config");
  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();
  json["mqttServer"] = mqttServer;
  json["mqttPort"] = mqttPort;
  json["mqttUser"] = mqttUser;
  json["mqttPassword"] = mqttPassword;
  json["haspNode"] = haspNode;
  json["otaPassword"] = otaPassword;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    debugPrintln("SPIFFS: Failed to open config file for writing");
  }

  // json.printTo(Serial);
  json.printTo(configFile);
  configFile.close();
  //end save
}

////////////////////////////////////////////////////////////////////////////////////////////////////
void clearSavedConfig()
{ // Clear out all local storage
  sendNextionCmd("page 0");
  setNextionAttr("p[0].b[1].txt", "\"Resetting\\rsystem...\"");
  debugPrintln("RESET: Formatting SPIFFS");
  SPIFFS.format();
  debugPrintln("RESET: Clearing WiFiManager settings...");
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  setNextionAttr("p[0].b[1].txt", "\"Rebooting\\rsystem...\"");
  debugPrintln("RESET: Rebooting device");
  ESP.reset();
}

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
