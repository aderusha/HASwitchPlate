////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify these values for your environment
const char* wifiSSID = "YOUR_WIFI_NETWORK";  // your WiFi network name
const char* wifiPassword = "YOUR_WIFI_PASSWORD";  // your WiFi network password
const char* otaPassword = "YOUR_OTA_PASSWORD";  // set to "" to disable OTA updates
const char* mqttServer = "YOUR_MQTT_BROKER_IP";  // your MQTT broker IP address
const char* mqttUser = ""; // mqtt broker username, set to "" for no user
const char* mqttPassword = ""; // mqtt broker password, set to "" for no password
const String mqttNode = "HASwitchPlate"; // your unique hostname for this device
const String mqttDiscoveryPrefix = "homeassistant"; // Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
////////////////////////////////////////////////////////////////////////////////////////////////////

// Serial debug output control, comment out to disable
#define DEBUGSERIAL
// Open a super-insecure (but read-only) telnet debug port
#define DEBUGTELNET

// OK embedded guys don't read anything below because thar be strings errywhere (HEY I HAVE THE RAM OK)

// MQTT topic string definitions
const String mqttStateTopic = mqttDiscoveryPrefix + "/haswitchplate/" + mqttNode + "/state";
const String mqttCommandTopic = mqttDiscoveryPrefix + "/haswitchplate/" + mqttNode + "/command";
const String mqttSubscription = mqttCommandTopic + "/#";

// Variable to hold topic string for incoming commands requesting state
String mqttGetSubtopic;

// Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
// We'll create one binary_sensor device to track MQTT connectivity
const String mqttDiscoBinaryStateTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/state";
const String mqttDiscoBinaryConfigTopic = mqttDiscoveryPrefix + "/binary_sensor/" + mqttNode + "/config";
// And one light device to set dim values on the panel
const String mqttDiscoLightSwitchCommandTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/switch";
const String mqttDiscoLightSwitchStateTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/status";
const String mqttDiscoLightBrightCommandTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/brightness/set";
const String mqttDiscoLightBrightStateTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/brightness";
const String mqttDiscoLightConfigTopic = mqttDiscoveryPrefix + "/light/" + mqttNode + "/config";
const String mqttDiscoLightSubscription = mqttDiscoveryPrefix + "/light/" + mqttNode + "/#";

// The strings below will spill over the PubSubClient_MAX_PACKET_SIZE 128
const String mqttDiscoBinaryConfigPayload = "{\"name\": \"" + mqttNode + "\", \"device_class\": \"connectivity\", \"state_topic\": \"" + mqttDiscoBinaryStateTopic + "\"}";
const String mqttDiscoLightConfigPayload = "{\"name\": \"" + mqttNode + "\", \"command_topic\": \"" + mqttDiscoLightSwitchCommandTopic + "\", \"state_topic\": \"" + mqttDiscoLightSwitchStateTopic + "\", \"brightness_command_topic\": \"" + mqttDiscoLightBrightCommandTopic + "\", \"brightness_state_topic\": \"" + mqttDiscoLightBrightStateTopic + "\"}";

// global var to pass around data coming from the panel
byte nextionReturnBuffer[100];
int nextionReturnIndex = 0;

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

#ifdef DEBUGTELNET
WiFiServer telnetServer(23);
WiFiClient telnetClient;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// System setup
void setup() {
  // Serial - LCD RX (after swap), debug TX
  // Serial1 - LCD TX, no RX
  Serial.begin(115200);
  Serial1.begin(115200);

  debugPrintln("\r\nHardware initialized, starting program load");

  // initialize display
  initializeNextion();

  // Start up networking
  setupWifi();

  // Create server and assign callbacks for MQTT
  mqttClient.setServer(mqttServer, 1883);
  mqttClient.setCallback(mqtt_callback);

  // Start up OTA
  if (otaPassword[0]) {
    setupOTA();
  }

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
// Main execution loop
void loop() {
  // Process user input from HMI
  if (Serial.available() > 0) {
    processNextionInput();
  }

  // check WiFi connection
  if (WiFi.status() != WL_CONNECTED) {
    setNextionAttr("p[0].b[1].txt", "\"WiFi connecting\"");
    setupWifi();
    setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\"");
  }

  // check MQTT connection
  if (!mqttClient.connected()) {
    mqttConnect();
  }

  // MQTT client loop
  mqttClient.loop();

  // OTA loop
  if (otaPassword[0]) {
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
// Handle incoming commands from MQTT
void mqtt_callback(char* topic, byte* payload, unsigned int payloadLength) {
  // strTopic: homeassistant/haswitchplate/devicename/command/p[1].b[4].txt
  // payload: "Lights On"
  // subTopic: p[1].b[4].txt

  String strTopic = topic;
  // convert the payload into a string
  String strPayload;
  for (int i = 0; i < payloadLength; i++) {
    strPayload += (char)payload[i];
  }

  // Incoming Namespace:
  // '[...]/device/command' ''  : undefined
  // '[...]/device/command' 'page 1'  : sendNextionCmd("page 1")
  // '[...]/device/command/page' '1'  : sendNextionCmd("page 1")
  // '[...]/device/command/p[1].b[4].txt' '' : getNextionAttr("p[1].b[4].txt")
  // '[...]/device/command/p[1].b[4].txt' '"Lights On"' : setNextionAttr("p[1].b[4].txt", "\"Lights On\"")

  debugPrintln("MQTT IN:  '" + strTopic + "' : '" + strPayload + "'");

  // '[...]/device/command' ''  : undefined
  if ((strTopic == mqttCommandTopic) && (strPayload == "")) {
    // currently undefined
  }

  // '[...]/device/command' 'page 1'  : sendNextionCmd("page 1")
  else if ((strTopic == mqttCommandTopic)) {
    sendNextionCmd(strPayload);
  }

  // '[...]/device/command/page' '1'  : sendNextionCmd("page 1")
  else if ((strTopic == (mqttCommandTopic + "/page"))) {
    sendNextionCmd("page " + strPayload);
  }

  // '[...]/device/command/update' 'http://192.168.0.10:8123/local/HASwitchPlate.tft' : start LCD update
  else if ((strTopic == (mqttCommandTopic + "/update")) && (strPayload != "")) {
    startNextionOTA(strPayload);
  }

  // '[...]/device/command/p[1].b[4].txt' '' : getNextionAttr("p[1].b[4].txt")
  else if (strTopic.startsWith(mqttCommandTopic) && (strPayload == "")) {
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    mqttGetSubtopic = "/" + subTopic;
    getNextionAttr(subTopic, strPayload);
  }

  // '[...]/device/command/p[1].b[4].txt' '"Lights On"' : setNextionAttr("p[1].b[4].txt", "\"Lights On\"")
  else if (strTopic.startsWith(mqttCommandTopic)) {
    String subTopic = strTopic.substring(mqttCommandTopic.length() + 1);
    setNextionAttr(subTopic, strPayload);
  }

  // change the brightness from the light topic
  else if (strTopic == mqttDiscoLightBrightCommandTopic) {
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    setNextionAttr("dim", String(panelDim));
    sendNextionCmd("dims=dim");
    mqttClient.publish(mqttDiscoLightBrightStateTopic.c_str(), strPayload.c_str(), true);
  }
  // set the panel dim OFF from the light topic, saving current dim level first
  else if (strTopic == mqttDiscoLightSwitchCommandTopic && strPayload == "OFF") {
    sendNextionCmd("dims=dim");
    setNextionAttr("dim", "0");
    mqttClient.publish(mqttDiscoLightSwitchStateTopic.c_str(), "OFF", true);
  }
  // set the panel dim ON from the light topic, restoring saved dim level
  else if (strTopic == mqttDiscoLightSwitchCommandTopic && strPayload == "ON") {
    sendNextionCmd("dim=dims");
    mqttClient.publish(mqttDiscoLightSwitchStateTopic.c_str(), "ON", true);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the value of a Nextion component attribute
void setNextionAttr (String hmi_attribute, String hmi_value) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print(hmi_attribute);
  Serial1.print("=");
  Serial1.print(hmi_value);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln("HMI OUT:  '" + hmi_attribute + "=" + hmi_value + "'");
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Get the value of a Nextion component attribute
void getNextionAttr (String hmi_attribute, String hmi_value) {
  // This will only send the command to the panel requesting the attribute, the actual
  // return of that value will be handled by processNextionInput and placed into mqttGetSubtopic
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print("get " + hmi_attribute);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln("HMI OUT:  'get " + hmi_attribute + "'");
  delay(1);
  processNextionInput();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send a raw command to the Nextion panel
void sendNextionCmd (String nexcmd) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print(nexcmd);
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  debugPrintln("HMI OUT:  " + nexcmd);
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send initialization commands to the HMI device at startup
void initializeNextion() {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  Serial1.print("rest");
  Serial1.write(nextionSuffix, sizeof(nextionSuffix));
  Serial1.flush();
  delay(1);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands from the Nextion device
void processNextionInput() {
  // Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
  // tl;dr, command byte, command data, 0xFF 0xFF 0xFF

  // Call handleNextionInput() to load nextionReturnBuffer[] and set nextionReturnIndex
  handleNextionInput();

  // Handle incoming touch command
  // 0x65+Page ID+Component ID+TouchEvent+End
  // Return this data when the touch event created by the user is pressed.
  // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
  // Example: 0x65 0x00 0x02 0x01 0xFF 0xFF 0xFF
  // Meaning: Touch Event, Page 0, Object 2, Press
  if (nextionReturnBuffer[0] == 0x65) {
    String nextionPage = String(nextionReturnBuffer[1]);
    String nextionButtonID = String(nextionReturnBuffer[2]);
    byte nextionButtonAction = nextionReturnBuffer[3];

    if (nextionButtonAction == 0x01) {
      debugPrintln("HMI IN:   [Button ON] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'ON'");
      mqttClient.publish(mqttButtonTopic.c_str(), "ON");
    }
    if (nextionButtonAction == 0x00) {
      debugPrintln("HMI IN:   [Button OFF] 'p[" + nextionPage + "].b[" + nextionButtonID + "]'");
      String mqttButtonTopic = mqttStateTopic + "/p[" + nextionPage + "].b[" + nextionButtonID + "]";
      debugPrintln("MQTT OUT: '" + mqttButtonTopic + "' : 'OFF'");
      mqttClient.publish(mqttButtonTopic.c_str(), "OFF");
    }
  }
  // Handle incoming "sendme" page number
  // 0x66+PageNum+End
  // Example: 0x66 0x02 0xFF 0xFF 0xFF
  // Meaning: page 2
  else if (nextionReturnBuffer[0] == 0x66) {
    String nextionPage = String(nextionReturnBuffer[1]);
    debugPrintln("HMI IN:   [sendme Page] '" + nextionPage + "'");
    String mqttPageTopic = mqttStateTopic + "/page";
    debugPrintln("MQTT OUT: '" + mqttPageTopic + "' : '" + nextionPage + "'");
    mqttClient.publish(mqttPageTopic.c_str(), nextionPage.c_str());
  }
  // Handle touch coordinate data
  // 0X67+Coordinate X High+Coordinate X Low+Coordinate Y High+Coordinate Y Low+TouchEvent+End
  // Example: 0X67 0X00 0X7A 0X00 0X1E 0X01 0XFF 0XFF 0XFF
  // Meaning: Coordinate (122,30), Touch Event: Press
  // issue Nextion command "sendxy=1" to enable this output
  else if (nextionReturnBuffer[0] == 0x67) {
    uint16_t xCoord = nextionReturnBuffer[1];
    xCoord = xCoord * 256 + nextionReturnBuffer[2];
    uint16_t yCoord = nextionReturnBuffer[3];
    yCoord = yCoord * 256 + nextionReturnBuffer[4];
    String xyCoord = String(xCoord) + ',' + String(yCoord);
    byte nextionTouchAction = nextionReturnBuffer[5];
    if (nextionTouchAction == 0x01) {
      debugPrintln("HMI IN:   [Touch ON] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOn";
      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic.c_str(), xyCoord.c_str());
    }
    else if (nextionTouchAction == 0x00) {
      debugPrintln("HMI IN:   [Touch OFF] '" + xyCoord + "'");
      String mqttTouchTopic = mqttStateTopic + "/touchOff";
      debugPrintln("MQTT OUT: '" + mqttTouchTopic + "' : '" + xyCoord + "'");
      mqttClient.publish(mqttTouchTopic.c_str(), xyCoord.c_str());
    }
  }
  // Handle get string return
  // 0x70+ASCII string+End
  // Example: 0x70 0x41 0x42 0x43 0x44 0x31 0x32 0x33 0x34 0xFF 0xFF 0xFF
  // Meaning: String data, ABCD1234
  else if (nextionReturnBuffer[0] == 0x70) {
    String getString;
    // convert the payload into a string
    for (int i = 1; i < nextionReturnIndex - 3; i++) {
      getString += (char)nextionReturnBuffer[i];
    }
    debugPrintln("HMI IN:   [String Return] '" + getString + "'");
    // If there's no outstanding request for a value, publish to mqttStateTopic
    if (mqttGetSubtopic == "") {
      debugPrintln("MQTT OUT: '" + mqttStateTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttStateTopic.c_str(), getString.c_str());
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else {
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      debugPrintln("MQTT OUT: '" + mqttReturnTopic + "' : '" + getString + "]");
      mqttClient.publish(mqttReturnTopic.c_str(), getString.c_str());
      mqttGetSubtopic = "";
    }
  }
  // Handle get int return
  // 0x71+byte1+byte2+byte3+byte4+End (4 byte little endian)
  // Example: 0x71 0x7B 0x00 0x00 0x00 0xFF 0xFF 0xFF
  // Meaning: Integer data, 123
  else if (nextionReturnBuffer[0] == 0x71) {
    unsigned long getInt = nextionReturnBuffer[4];
    getInt = getInt * 256 + nextionReturnBuffer[3];
    getInt = getInt * 256 + nextionReturnBuffer[2];
    getInt = getInt * 256 + nextionReturnBuffer[1];
    String getString = String(getInt);
    debugPrintln("HMI IN:   [Int Return] '" + getString + "'");
    // If there's no outstanding request for a value, published to mqttStateTopic
    if (mqttGetSubtopic == "") {
      mqttClient.publish(mqttStateTopic.c_str(), getString.c_str());
    }
    // Otherwise, publish the to saved mqttGetSubtopic and then reset mqttGetSubtopic
    else {
      String mqttReturnTopic = mqttStateTopic + mqttGetSubtopic;
      mqttClient.publish(mqttReturnTopic.c_str(), getString.c_str());
      mqttGetSubtopic = "";
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Connect to WiFi
void setupWifi() {
  setNextionAttr("p[0].b[1].txt", "\"WiFi connecting\"");
  sendNextionCmd("page 0");
  debugPrintln("Connecting to WiFi network: " + String(wifiSSID));
  WiFi.hostname(mqttNode.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    debugPrint(".");
  }
  debugPrintln("\r\nWiFi connected succesfully and assigned IP: " + WiFi.localIP().toString());
  setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\"");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// MQTT connection and subscriptions
void mqttConnect() {
  // Loop until we're reconnected to MQTT
  while (!mqttClient.connected()) {
    sendNextionCmd("page 0");
    setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connecting\"");
    debugPrintln("Attempting MQTT connection to broker: " + String(mqttServer));
    // Attempt to connect to broker, setting last will and testament
    if (mqttClient.connect(mqttNode.c_str(), mqttUser, mqttPassword, mqttDiscoBinaryStateTopic.c_str(), 1, 1, "OFF")) {
      mqttClient.subscribe(mqttSubscription.c_str());
      mqttClient.subscribe(mqttDiscoLightSubscription.c_str());
      debugPrintln("MQTT discovery config: [" + mqttDiscoBinaryConfigTopic + "] : [" + mqttDiscoBinaryConfigPayload + "]");
      debugPrintln("MQTT discovery state: [" + mqttDiscoBinaryStateTopic + "] : [ON]");
      mqttClient.publish(mqttDiscoBinaryConfigTopic.c_str(), mqttDiscoBinaryConfigPayload.c_str(), true);
      mqttClient.publish(mqttDiscoBinaryStateTopic.c_str(), "ON", true);
      debugPrintln("MQTT discovery config: [" + mqttDiscoLightConfigTopic + "] : [" + mqttDiscoLightConfigTopic + "]");
      debugPrintln("MQTT discovery state: [" + mqttDiscoLightSwitchStateTopic + "] : [ON]");
      mqttClient.publish(mqttDiscoLightConfigTopic.c_str(), mqttDiscoLightConfigPayload.c_str(), true);
      mqttClient.publish(mqttDiscoLightSwitchStateTopic.c_str(), "ON", true);
      setNextionAttr("p[0].b[1].txt", "\"WiFi connected\\r" + WiFi.localIP().toString() + "\\rMQTT Connected\\r" + String(mqttServer) + "\"");
      debugPrintln("MQTT connected");
    }
    else {
      debugPrintln("MQTT connection failed, rc=" + String(mqttClient.state()) + ".  Trying again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// (mostly) boilerplate OTA setup from library examples
void setupOTA() {
  // Start up OTA
  // ArduinoOTA.setPort(8266); // Port defaults to 8266
  ArduinoOTA.setHostname(mqttNode.c_str()); // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword(otaPassword); // No authentication by default

  ArduinoOTA.onStart([]() {
    debugPrintln("OTA update start");
  });
  ArduinoOTA.onEnd([]() {
    debugPrintln("\nOTA update complete");
  });
  ArduinoOTA.onProgress([](unsigned int 
, unsigned int total) {
    //Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    debugPrint("OTA Error: " + String(error));
    if (error == OTA_AUTH_ERROR) debugPrintln(" Auth Failed");
    else if (error == OTA_BEGIN_ERROR) debugPrintln(" Begin Failed");
    else if (error == OTA_CONNECT_ERROR) debugPrintln(" Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) debugPrintln(" Receive Failed");
    else if (error == OTA_END_ERROR) debugPrintln(" End Failed");
  });
  ArduinoOTA.begin();
  debugPrintln("OTA firmware update ready");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Upload firmware to the Nextion LCD
void startNextionOTA (String otaURL) {
  // based in large part on code posted by indev2 here:
  // http://support.iteadstudio.com/support/discussions/topics/11000007686/page/2
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  int nextionResponseTimeout = 500; // timeout for receiving ack string in milliseconds
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
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    debugPrintln("LCD OTA: HTTP GET return code:" + String(httpCode));
    if (httpCode == HTTP_CODE_OK) { // file found at server
      // get length of document (is -1 when Server sends no Content-Length header)
      int len = http.getSize();
      FileSize = len;
      int Parts = (len / 4096) + 1;
      debugPrintln("LCD OTA: File found at Server. Size " + String(len) + " bytes in " + String(Parts) + " 4k chunks.");
      // create buffer for read
      uint8_t buff[128] = {};
      // get tcp stream
      WiFiClient * stream = http.getStreamPtr();

      debugPrintln("LCD OTA: Issuing NULL command");
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      handleNextionInput();

      String nexcmd = "whmi-wri " + String(FileSize) + ",115200,0";
      debugPrintln("LCD OTA: Sending LCD upload command: " + nexcmd);
      Serial1.print(nexcmd);
      Serial1.write(nextionSuffix, sizeof(nextionSuffix));
      Serial1.flush();

      if (otaReturnSuccess()) {
        debugPrintln("LCD OTA: LCD upload command accepted");
      }
      else {
        debugPrintln("LCD OTA: LCD upload command FAILED.");
        return;
      }
      debugPrintln("LCD OTA: Starting update");
      while (http.connected() && (len > 0 || len == -1)) {
        // get available data size
        size_t size = stream->available();
        if (size) {
          // read up to 128 bytes
          int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
          // write it to Serial
          Serial1.write(buff, c);
          Serial1.flush();
          count += c;
          if (count == 4096) {
            nextionResponseTimer = millis();
            partNum ++;
            total += count;
            pCent = (total * 100) / FileSize;
            count = 0;
            if (otaReturnSuccess()) {
              debugPrintln("LCD OTA: Part " + String(partNum) + " OK, " + String(pCent) + "% complete");
            }
            else {
              debugPrintln("LCD OTA: Part " + String(partNum) + " FAILED, " + String(pCent) + "% complete");
            }
          }
          if (len > 0) {
            len -= c;
          }
        }
        //delay(1);
      }
      partNum++;
      //delay (250);
      total += count;
      if ((total == FileSize) && otaReturnSuccess()) {
        debugPrintln("LCD OTA: success, wrote " + String(total) + " of " + String(FileSize) + " bytes.");
      }
      else {
        debugPrintln("LCD OTA: failure");
      }
    }
  }
  else {
    debugPrintln("LCD OTA: HTTP GET failed, error code " + http.errorToString(httpCode));
  }
  http.end();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming serial data from the Nextion panel
void handleNextionInput() {
  // This will collect serial data from the panel and place it into the global buffer
  // nextionReturnBuffer[nextionReturnIndex] until it finds a string of 3 consecutive
  // 0xFF values or runs into the timeout
  bool nextionCommandIncoming = true; // we'll flip this to false when we receive 3 consecutive 0xFFs
  int nextionCommandTimeout = 1000; // timeout for receiving termination string in milliseconds
  int nextionTermByteCnt = 0; // counter for our 3 consecutive 0xFFs
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout
  nextionReturnIndex = 0; // reset the global nextionReturnIndex back to zero
  String hmiDebug = "HMI IN: "; // assemble a string for debug output
  // Load the nextionBuffer until we receive a termination command or we hit our timeout
  while (nextionCommandIncoming && ((millis() - nextionCommandTimer) < nextionCommandTimeout)) {
    if (Serial.available()) {
      byte nextionCommandByte = Serial.read();
      delay(1);
      hmiDebug += (" 0x" + String(nextionCommandByte, HEX));
      // check to see if we have one of 3 consecutive 0xFF which indicates the end of a command
      if (nextionCommandByte == 0xFF) {
        nextionTermByteCnt++;
        if (nextionTermByteCnt >= 3) {
          nextionCommandIncoming = false;
        }
      }
      else {
        nextionTermByteCnt = 0;  // reset counter if a non-term byte was encountered
      }
      nextionReturnBuffer[nextionReturnIndex] = nextionCommandByte;
      nextionReturnIndex++;
    }
    else {
      delay (1);
    }
  }
  debugPrintln(hmiDebug);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Monitor the serial port for a 0x05 response within our timeout
bool otaReturnSuccess() {
  int nextionCommandTimeout = 1000; // timeout for receiving termination string in milliseconds
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout
  bool otaSuccessVal = false;
  while ((millis() - nextionCommandTimer) < nextionCommandTimeout) {
    if (Serial.available()) {
      byte inByte = Serial.read();
      if (inByte == 0x5) {
        otaSuccessVal = true;
        break;
      }
    }
    else {
      delay (1);
    }
  }
  return otaSuccessVal;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Basic telnet client handling code from: https://gist.github.com/tablatronix/4793677ca748f5f584c95ec4a2b10303
#ifdef DEBUGTELNET
void handleTelnetClient() {
  if (telnetServer.hasClient()) {
    // client is connected
    if (!telnetClient || !telnetClient.connected()) {
      if (telnetClient) telnetClient.stop();   // client disconnected
      telnetClient = telnetServer.available(); // ready for new client
    } else {
      telnetServer.available().stop();  // have client, block new conections
    }
  }
  // Handle client input from telnet connection.
  if (telnetClient && telnetClient.connected() && telnetClient.available()) {
    // client input processing
    while (telnetClient.available()) {
      // Read data from telnet just to clear out the buffer
      telnetClient.read();
    }
  }
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug output line of text to our debug targets
void debugPrintln (String debugText) {
#ifdef DEBUGSERIAL
  Serial.println(debugText);
  Serial.flush();
#endif
#ifdef DEBUGTELNET
  if (telnetClient.connected()) {
    debugText += "\r\n";
    const size_t len = debugText.length();
    const char* buffer = debugText.c_str();
    //telnetClient.println(debugText);
    telnetClient.write(buffer, len);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Debug output single character to our debug targets (DON'T USE THIS!)
void debugPrint (String debugText) {
  // Try to avoid using this function if at all possible.  When connected to telnet, printing each
  // character requires a full TCP round-trip + acknowledgement back and execution halts while this
  // happens.  Far better to put everything into a line and send it all out in one packet using
  // debugPrintln.
#ifdef DEBUGSERIAL
  Serial.print(debugText);
  Serial.flush();
#endif
#ifdef DEBUGTELNET
  if (telnetClient.connected()) {
    const size_t len = debugText.length();
    const char* buffer = debugText.c_str();
    //telnetClient.println(debugText);
    telnetClient.write(buffer, len);
  }
#endif
}
