#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <AsyncMqtt_Generic.h>

#define APSSID          "WIFI NAME"   //AP SSID
#define APPASSWORD      "WIFI PASSWORD"         //AP password
#define SERVERPORT      80         //Web server port
#define WWWUSERNAME     "admin"    // Set www user name
#define WWWPASSWORD     "admin"    // Set www user password
#define OTAUSER         "OTAadmin"    // Set OTA user
#define OTAPASSWORD     "OTApassword"    // Set OTA password
#define OTAPATH         "/firmware"// Set path for update
#define RELAYPIN        15         // GPIO12 relay pin -> GPIO15
#define LEDPIN          16         // GPIO13 GREEN LED (active low)-> GPIO16 change to wifi connect
#define BUTTONPIN       5          // GPIO0 button pin -> GPIO5
#define CLICKPIN        0           //test button
#define BUTTONTIME      0.05       // [s] time between button read
#define BUTTONON        "color: green; border: 3px #fff outset;"
#define BUTTONOFF       "color: red; border: 3px #fff outset;"
#define BUTTONNOACT     "color: black; border: 7px #fff outset;"
#define BUTTONDEBOUNCE  1 //Minimum number of seconds between a valid button press or relay switch.
#define mqtt_id         "test"
#define mqtt_topic      "MyHome/Light/test"
#define mqtt_topic_sta  "MyHome/Light/Sub/Server/State"
#define mqtt_topic_con  "MyHome/Light/Sub/Server/Connect"
#define mqtt_topic_sub  "MyHome/Light/Pub/test"
#define MQTT_HOST IPAddress(192,168,0,1)
#define MQTT_PORT 1883

long buttonPressTime = 0;
bool buttonPressed = false;
bool LongRelayMode = false;  // press long time for woring relay

volatile bool RelayState = false;     // Relay off
bool LEDState = true;                 // Green LED off
bool ButtonFlag = false;              // Does the button need to be handled on this loop
bool wifiCon = false;
int ButtonCount = 0;                  // How many cycles/checks the button has been pressed for.
String OnButt;
String OffButt;

// MQTT with async-mqtt-lib
AsyncMqttClient asyncMQTTClient;
Ticker mqttReconnectTimer;

WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;

//Setup classes needed from libraries.
MDNSResponder mdns;
Ticker buttonTick;
ESP8266WebServer server(SERVERPORT);
ESP8266HTTPUpdateServer httpUpdater;

long lastMsg = 0;
char msg[50];
int value = 0;

void setup(void) {
  //  Init
  pinMode(BUTTONPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);

  Serial.begin(115200);
  delay(5000);


  //Start wifi connection
  Serial.println("Connecting to wifi..");

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  asyncMQTTClient.onConnect(onMqttConnect);
  asyncMQTTClient.onDisconnect(onMqttDisconnect);
  asyncMQTTClient.onSubscribe(onMqttSubscribe);
  asyncMQTTClient.onUnsubscribe(onMqttUnsubscribe);
  asyncMQTTClient.onMessage(onMqttMessage);
  asyncMQTTClient.onPublish(onMqttPublish);
  asyncMQTTClient.setServer(MQTT_HOST, MQTT_PORT);

  connectToWifi();

  //Enable periodic watcher for button event handling
  buttonTick.attach(BUTTONTIME, buttonFlagSet);
}

// MQTT with async-mqtt-lib
void connectToWifi() {
  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(APSSID, APPASSWORD);
}

void connectToMqtt() {
  Serial.println("Connecting to MQTT...");
  asyncMQTTClient.connect();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event) {
  Serial.println("Connected to Wi-Fi.");
  wifiCon = true;
  setLED(false);
  //Print startup status and network information
  Serial.println("");
  Serial.print("Connected to: ");
  Serial.println(APSSID);
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("Subnet: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Device ID: ");
  Serial.println(ESP.getChipId());
  if (mdns.begin("esp8266", WiFi.localIP())) {
    Serial.println("MDNS: Responder Started");
  }

  //Setup HTTP Server Endpoints
  server.on("/", HTTP_GET, handleGET);
  server.on("/device", HTTP_POST, handleStatePOST);
  server.on("/device", HTTP_GET, handleStateGET);
  server.on("/state", HTTP_GET, RelayStateGET);
  server.on("/change", HTTP_GET, RelayChange);
  server.onNotFound(handleNotFound);
  httpUpdater.setup(&server, OTAPATH, OTAUSER, OTAPASSWORD);  //OTA Update endpoint

  //Start the web server
  server.begin();

  //Start up blink of LED signaling everything is ready. Fast Flash
  for (int i = 0; i < 10; i++) {
    setLED(!LEDState);
    delay(100);
  }
  Serial.println("Server is up.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event) {
  Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void onMqttConnect(bool sessionPresent) {
  Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);

  uint16_t packetIdSub = asyncMQTTClient.subscribe(mqtt_topic_sub, 2);
  Serial.print("Subscribing at QoS 2, packetId: ");
  Serial.println(packetIdSub);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected()) {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  String Msg = "";
  int i = 0;
  while (i < len) Msg += (char)payload[i++];

  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, Msg.c_str(), len);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    char error_put[64];
    StaticJsonDocument<64> doc;
    doc["sender"] = "self";
    doc["message"] = error.c_str();
    doc["room"] = mqtt_id;
    mqttPublish(error.c_str(), "self");
    return;
  }

  const char* sender = doc["Light"]["sender"];
  const char* message = doc["Light"]["message"];
  const char* destination = doc["Light"]["destination"];

  const char* char_message_on = "On";
  const char* char_message_off = "Off";
  const char* char_message_already_on = "already On";
  const char* char_message_already_off = "already Off";

  String message_str = message;
  String destination_str = destination;

  if (message_str.equals("ON")) {
    if (RelayState == true) {
      mqttPublish(char_message_already_on, sender);
    } else {
      setRelay(!RelayState);
      mqttPublish(char_message_on, sender);
    }
  } else if (message_str.equals("STATE")) {
    if (RelayState == true) {
      mqttPublish(char_message_on, sender);
    } else {
      mqttPublish(char_message_off, sender);
    }
  } else if (message_str.equals("OFF")){
    if (RelayState == true) {
      setRelay(!RelayState);
      mqttPublish(char_message_off, sender);
    } else {
      mqttPublish(char_message_already_off, sender);
    }
  }
  else if(message_str.equals("MODE")){
    setMode(!LongRelayMode);
  }
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void mqttPublish(const char* message,const char* sender ){
  char output[256];
  StaticJsonDocument<256> doc;

  doc["sender"] = sender;
  doc["message"] = message;
  doc["room"] = mqtt_id;

  serializeJson(doc, output);

  asyncMQTTClient.publish(mqtt_topic, 1, true, output);

  delay(100);
}

void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}
void handleGET() {
  Serial.println("Serviced Page Request");
  String buff;
  buff = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
  buff += "<html><head>\n";
  buff += "<style type=\"text/css\">\n";
  buff += "html {font-family: sans-serif;background:#f0f5f5}\n";
  buff += ".submit {width: 10%; height:5vw; font-size: 100%; font-weight: bold; border-radius: 4vw;}\n";
  buff += "@media (max-width: 1281px) {\n";
  buff += "html {font-size: 3vw; font-family: sans-serif;background:#f0f5f5}\n";
  buff += ".submit {width: 40%; height:20vw; font-size: 100%; font-weight: bold; border-radius: 15vw;}}\n";
  buff += "</style>\n";
  buff += "<meta content=\"text/html; charset=utf-8\">\n";
  buff += "<title>Mcity - Wifi Power Switch</title></head><body>\n";
  buff += "</pre>\n";
  buff += "Wifi-enabled IIoT Power Switch\n";
  buff += "<form action=\"/device\" method=\"POST\">\n";
  buff += "<h2>Device ID: " + String(ESP.getChipId()) + "</h2>\n";
  buff += "<h2>Device topic: " + String(mqtt_topic_sub) + "</h2>\n";
  buff += "<h2>Relay State: ";
  if (RelayState) {
    buff += "ON</h2>\n";
  } else {
    buff += "OFF</h2>\n";
  }
  buff += "<input type=\"hidden\" name=\"return\" value=\"TRUE\">";
  buff += "<input type=\"submit\" name=\"state\" class=\"submit\" value=\"ON\" style=\"" + OnButt + "\">\n";
  buff += "<input type=\"submit\" name=\"state\" class=\"submit\" value=\"OFF\" style=\"" + OffButt + "\">\n";
  buff += "</form></body></html>\n";
  server.send(200, "text/html", buff);

  delay(20);
}
void RelayStateGET() {
  String buff;
  if (RelayState) {
    buff = "ON\n";
  } else {
    buff = "OFF\n";
  }
  server.send(50, "text/html", buff);
}
void RelayChange() {
  setRelay(!RelayState);
  RelayStateGET();
}
void handleStatePOST() {
  /* request for www user/password from client */
  if (!server.authenticate(WWWUSERNAME, WWWPASSWORD))
    return server.requestAuthentication();
  if (server.arg("state") == "ON") setRelay(true);
  if (server.arg("state") == "OFF") setRelay(false);

  //Redirect to home page is user requests it.
  if (server.arg("return") == "TRUE") handleGET();
  else handleStateGET();
}
void handleStateGET() {
  //Serve Page
  Serial.println("Serviced API Request");

  //Print Relay state
  String buff;
  if (RelayState) {
    buff = "ON\n";
  } else {
    buff = "OFF\n";
  }

  server.send(200, "text/html", buff);

  //Quick LED Flash
  delay(20);
  //setLED(!LEDState);
}
void setRelay(bool SetRelayState) {
  //Switch the HTML for the display page
  if (SetRelayState == true) {
    OnButt = BUTTONON;
    OffButt = BUTTONNOACT;
  }
  if (SetRelayState == false) {
    OnButt = BUTTONNOACT;
    OffButt = BUTTONOFF;
  }

  //Set the relay state
  RelayState = SetRelayState;

  digitalWrite(RELAYPIN, RelayState);

  //Set the LED to opposite of the button.
  //setLED(!SetRelayState);
}
void setLED(bool SetLEDState) {
  LEDState = SetLEDState;  // set green LED
  digitalWrite(LEDPIN, LEDState);
}
void setMode(bool SetModeState){
  LongRelayMode = SetModeState;
}

void buttonFlagSet(void) {
  ButtonFlag = true;
}

void controlButton(void){
  if(LongRelayMode){
    if(digitalRead(BUTTONPIN) == false){
      if(buttonPressTime == 0){
        buttonPressTime = millis();
      }
      else if(millis() - buttonPressTime >= 2000){
        Serial.println("LongPress is working");
        if (RelayState == false) {
          mqttPublish("On","self");
        } else {
          mqttPublish("Off","self");   
        }
        setRelay(!RelayState);
        buttonPressTime = 0;
      }
    }
    else{
      buttonPressTime = 0;
    }
  }
  else{
    if (digitalRead(BUTTONPIN) == false) {
      Serial.println(RelayState);
      if (RelayState == false) {
        mqttPublish("On","self");
      } else {
        mqttPublish("Off","self");   
      }
      setRelay(!RelayState);
      delay(500);
    }
  }
}
/*
 * loop
 * System Loop
 */
void loop(void) {
  if(wifiCon){
    server.handleClient();  // Listen for HTTP request
  }
  if (ButtonFlag || buttonPressed) {
    controlButton();
  }
}
