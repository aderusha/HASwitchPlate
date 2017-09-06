////////////////////////////////////////////////////////////////////////////////////////////////////
// Modify these values for your environment
const char* ssid = "YOUR_WIFI_NETWORK";  // Your WiFi network name
const char* password = "YOUR_WIFI_PASSWORD";  // Your WiFi network password
const char* ota_password = "YOUR_OTA_PASSWORD";  // OTA update password
const char* mqtt_server = "YOUR_MQTT_BROKER_IP";  // Your MQTT server IP address
const char* mqtt_user = ""; // mqtt username, set to "" for no user
const char* mqtt_password = ""; // mqtt password, set to "" for no password
const String mqtt_node = "HASwitchPlate"; // Your unique hostname for this device
const String mqtt_discovery_prefix = "homeassistant"; // Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <SoftwareSerial.h>

// MQTT topic strings
const String mqtt_subscription = mqtt_discovery_prefix + "/haswitchplate/" + mqtt_node + "/#";
const String mqtt_attr_topic = mqtt_discovery_prefix + "/haswitchplate/" + mqtt_node + "/nextionattr";
const String mqtt_cmd_topic = mqtt_discovery_prefix + "/haswitchplate/" + mqtt_node + "/nextioncmd";

// Home Assistant MQTT Discovery, see https://home-assistant.io/docs/mqtt/discovery/
// We'll create one binary_sensor device to track MQTT connectivity
const String mqtt_disco_binary_state_topic = mqtt_discovery_prefix + "/binary_sensor/" + mqtt_node + "/state";
const String mqtt_disco_binary_config_topic = mqtt_discovery_prefix + "/binary_sensor/" + mqtt_node + "/config";
// And one light device to set dim values on the panel
const String mqtt_disco_light_cmd_topic = mqtt_discovery_prefix + "/light/" + mqtt_node + "/switch";
const String mqtt_disco_light_state_topic = mqtt_discovery_prefix + "/light/" + mqtt_node + "/status";
const String mqtt_disco_light_brightcmd_topic = mqtt_discovery_prefix + "/light/" + mqtt_node + "/brightness/set";
const String mqtt_disco_light_brightstate_topic = mqtt_discovery_prefix + "/light/" + mqtt_node + "/brightness";
const String mqtt_disco_light_config_topic = mqtt_discovery_prefix + "/light/" + mqtt_node + "/config";
const String mqtt_disco_light_subscription = mqtt_discovery_prefix + "/light/" + mqtt_node + "/#";

// The strings below will spill over the PubSubClient default packet length of 128 bytes.
// You'll need to modify the file PubSubClient.h and change the following line to 512:
// #define MQTT_MAX_PACKET_SIZE 128
const String mqtt_disco_binary_config_payload = "{\"name\": \"" + mqtt_node + "\", \"device_class\": \"connectivity\", \"state_topic\": \"" + mqtt_disco_binary_state_topic + "\"}";
const String mqtt_disco_light_config_payload = "{\"name\": \"" + mqtt_node + "\", \"command_topic\": \"" + mqtt_disco_light_cmd_topic + "\", \"state_topic\": \"" + mqtt_disco_light_state_topic + "\", \"brightness_command_topic\": \"" + mqtt_disco_light_brightcmd_topic + "\", \"brightness_state_topic\": \"" + mqtt_disco_light_brightstate_topic + "\"}";

WiFiClient espClient;
PubSubClient client(espClient);

// SoftwareSerial RX, TX - connect to Nextion TX, RX
SoftwareSerial HMISerial(D1, D2); // RX, TX

////////////////////////////////////////////////////////////////////////////////////////////////////
// System setup
void setup() {
  Serial.begin(115200);  // Debug serial
  HMISerial.begin(115200); // HMI serial
  Serial.println("\n\nHardware initialized, starting program load");
  Serial.println("\nmqtt_disco_light_config_payload: " + mqtt_disco_light_config_payload);
  // initialize display
  initializeNextion();

  // Start up networking
  setup_wifi();

  // Start up MQTT
  client.setServer(mqtt_server, 1883);
  client.setCallback(mqtt_callback);

  // Start up OTA
  setup_OTA();

  // flip the device over to page 1
  // sendNextionCmd("page p1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Main execution loop
void loop() {
  // check wifi connection
  if (!client.connected()) {
    reconnect();
  }

  // Process user input from HMI
  if (HMISerial.available() > 0) {
    processNextionInput();
  }

  // MQTT client loop
  client.loop();

  // OTA loop
  ArduinoOTA.handle();
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands from MQTT
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  // attribute topic: homeassistant/haswitchplate/devicename/nextionattr/p0.b1.txt
  // payload: "Lights On"
  // topicProperty: p0.b1.txt

  String strTopic = topic;
  // convert the payload into a string
  String strPayload;
  for (int i = 0; i < length; i++) {
    strPayload += (char)payload[i];
  }

  Serial.println("MQTT message: [" + strTopic + "] : [" + strPayload + "]");
  // set an attribute
  if (strTopic.startsWith(mqtt_attr_topic)) {
    String topicProperty = strTopic.substring(mqtt_attr_topic.length() + 1);
    setNextionAttr(topicProperty, strPayload);
  }
  // execute a command
  else if (strTopic == mqtt_cmd_topic) {
    sendNextionCmd(strPayload);
  }
  // change the brightness from the light topic
  else if (strTopic == mqtt_disco_light_brightcmd_topic) {
    int panelDim = map(strPayload.toInt(), 0, 255, 0, 100);
    setNextionAttr("dim", String(panelDim));
    sendNextionCmd("dims=dim");
    client.publish(mqtt_disco_light_brightstate_topic.c_str(), strPayload.c_str(), true);
  }
  // set the panel dim OFF from the light topic, saving current dim level first
  else if (strTopic == mqtt_disco_light_cmd_topic && strPayload == "OFF") {
    sendNextionCmd("dims=dim");
    setNextionAttr("dim", "0");
    client.publish(mqtt_disco_light_state_topic.c_str(), "OFF", true);
  }
  // set the panel dim ON from the light topic, restoring saved dim level
  else if (strTopic == mqtt_disco_light_cmd_topic && strPayload == "ON") {
    sendNextionCmd("dim=dims");
    client.publish(mqtt_disco_light_state_topic.c_str(), "ON", true);
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Set the value of a Nextion component attribute
void setNextionAttr (String hmi_attribute, String hmi_value) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  HMISerial.print(hmi_attribute);
  HMISerial.print("=");
  HMISerial.print(hmi_value);
  HMISerial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.println("HMI command: " + hmi_attribute + "=" + hmi_value);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send a raw command to the Nextion panel
void sendNextionCmd (String nexcmd) {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  HMISerial.print(nexcmd);
  HMISerial.write(nextionSuffix, sizeof(nextionSuffix));
  Serial.println("HMI command: " + nexcmd);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Send initialization commands to the HMI device at startup
void initializeNextion() {
  byte nextionSuffix[] = {0xFF, 0xFF, 0xFF};
  HMISerial.write(nextionSuffix, sizeof(nextionSuffix));
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Handle incoming commands from the Nextion device
// Command reference: https://www.itead.cc/wiki/Nextion_Instruction_Set#Format_of_Device_Return_Data
// tl;dr, command byte, command data, 0xFF 0xFF 0xFF
// Create a buffer for our command, waiting for those 0xFFs, then process the command once we get
// three in a row
void processNextionInput() {
  byte nextionBuffer[20];  // bin to hold our incoming command
  int nextionCommandByteCnt = 0;  // Counter for incoming command buffer
  bool nextionCommandIncoming = true; // we'll flip this to false when we receive 3 consecutive 0xFFs
  int nextionCommandTimeout = 100; // timeout for receiving termination string in milliseconds
  int nextionTermByteCnt = 0;
  unsigned long nextionCommandTimer = millis(); // record current time for our timeout

  //  Serial.print("In: ");
  // Load the nextionBuffer until we receive a termination command or we hit our timeout
  while (nextionCommandIncoming && ((millis() - nextionCommandTimer) < nextionCommandTimeout)) {
    byte nextionCommandByte = HMISerial.read();
    //    Serial.print("[");
    //    Serial.print(nextionCommandByte, HEX);
    //    Serial.print("] ");

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
    nextionBuffer[nextionCommandByteCnt] = nextionCommandByte;
    nextionCommandByteCnt++;
  }
  //Serial.println(" [stop]");

  // Handle incoming touch command
  // 0X65+Page ID+Component ID+TouchEvent+End
  // Return this data when the touch event created by the user is pressed.
  // Definition of TouchEvent: Press Event 0x01, Release Event 0X00
  // Example: 0X65 0X00 0X02 0X01 0XFF 0XFF 0XFF
  // Meaning: Touch Event, Page 0, Button 2, Press
  if (nextionBuffer[0] == 0x65) {
    String nextionPage = String(nextionBuffer[1]);
    String nextionButtonID = String(nextionBuffer[2]);
    byte nextionButtonAction = nextionBuffer[3];

    // There's kind of a logic problem here, in that the messages below will be published
    // and then received by our subscription, thus sending a command to the Nextion that
    // it won't understand (p1.b2=ON).  Fortunately it'll just ignore that and move on
    // with life.

    // on page 1 button 2 press, publish mqtt_attr_topic/p1.b2: "ON"
    if (nextionButtonAction == 0x01) {
      String mqtt_btntopic = mqtt_attr_topic + "/p" + nextionPage + ".b" + nextionButtonID;
      client.publish(mqtt_btntopic.c_str(), "ON");
    }

    // on page 1 button 2 release, publish mqtt_attr_topic/p1.b2: "OFF"
    if (nextionButtonAction == 0x00) {
      String mqtt_btntopic = mqtt_attr_topic + "/p" + nextionPage + ".b" + nextionButtonID;
      client.publish(mqtt_btntopic.c_str(), "OFF");
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// (mostly) boilerplate WiFi connection from library examples
void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  setNextionAttr("p0.t0.txt", "\"WiFi Connecting\"");
  sendNextionCmd("page 0");
  Serial.print("Connecting to WiFi network: ");
  Serial.print(ssid);
  WiFi.hostname(mqtt_node.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.print("\nWiFi connected succesfully and assigned IP: ");
  Serial.println(WiFi.localIP());
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// (mostly) boilerplate MQTT connection from library examples
void reconnect() {
  // Loop until we're reconnected to MQTT
  while (!client.connected()) {
    setNextionAttr("p0.t0.txt", "\"MQTT Connecting\"");
    sendNextionCmd("page 0");
    Serial.print("Attempting MQTT connection to broker: ");
    Serial.println(mqtt_server);
    // Attempt to connect to broker, setting last will and testament
    if (client.connect(mqtt_node.c_str(), mqtt_user, mqtt_password, mqtt_disco_binary_state_topic.c_str(), 1, 1, "OFF")) {
      Serial.println("MQTT connected");
      client.subscribe(mqtt_subscription.c_str());
      client.subscribe(mqtt_disco_light_subscription.c_str());
      Serial.println("MQTT discovery config: [" + mqtt_disco_binary_config_topic + "] : [" + mqtt_disco_binary_config_payload + "]");
      Serial.println("MQTT discovery state: [" + mqtt_disco_binary_state_topic + "] : [ON]");
      client.publish(mqtt_disco_binary_config_topic.c_str(), mqtt_disco_binary_config_payload.c_str(), true);
      client.publish(mqtt_disco_binary_state_topic.c_str(), "ON", true);
      Serial.println("MQTT discovery config: [" + mqtt_disco_light_config_topic + "] : [" + mqtt_disco_light_config_topic + "]");
      Serial.println("MQTT discovery state: [" + mqtt_disco_light_state_topic + "] : [ON]");
      client.publish(mqtt_disco_light_config_topic.c_str(), mqtt_disco_light_config_payload.c_str(), true);
      client.publish(mqtt_disco_light_state_topic.c_str(), "ON", true);
    }
    else {
      Serial.print("MQTT connection failed, rc=");
      Serial.print(client.state());
      Serial.println(".  Trying again in 5 seconds.");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  sendNextionCmd("page 1");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// (mostly) boilerplate OTA setup from library examples
void setup_OTA() {
  // Start up OTA

  // ArduinoOTA.setPort(8266); // Port defaults to 8266
  ArduinoOTA.setHostname(mqtt_node.c_str()); // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword(ota_password); // No authentication by default

  ArduinoOTA.onStart([]() {
    Serial.println("OTA update start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nOTA update complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OTA firmware update ready");
}
