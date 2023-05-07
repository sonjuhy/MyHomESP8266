#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>
#include <PubSubClient.h>
#include <SimpleTimer.h>
#include <ArduinoJson.h>

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
#define mqtt_server     "192.168.0.1"
#define mqtt_port       1883
#define mqtt_id         "test"
#define mqtt_topic      "MyHome/Light/test"
#define mqtt_topic_sta  "MyHome/Light/Sub/Server/State"
#define mqtt_topic_con  "MyHome/Light/Sub/Server/Connect"
#define mqtt_topic_sub  "MyHome/Light/Pub/test"

bool    LEDState        = true;    // Green LED off
bool    RelayState      = false;   // Relay off
bool    ButtonFlag      = false;   // Does the button need to be handled on this loop
char    ButtonCount     = 0;       // How many cycles/checks the button has been pressed for.
String  OnButt;
String  OffButt;
String  SendButt;

//Setup classes needed from libraries.
MDNSResponder mdns;
Ticker buttonTick;
Ticker tickerConnect;
ESP8266WebServer server(SERVERPORT);
ESP8266HTTPUpdateServer httpUpdater;
//mqttclient
WiFiClient espClient;
PubSubClient client(espClient);

long lastMsg = 0;
char msg[50];
int value = 0;

SimpleTimer timerCnt;
volatile bool blinkOn;
volatile unsigned long nextMil;

void setup(void){  
  //  Init
  pinMode(BUTTONPIN, INPUT);
  pinMode(LEDPIN, OUTPUT);
  pinMode(RELAYPIN, OUTPUT);
  pinMode(CLICKPIN, INPUT_PULLUP);

  attachInterrupt(BUTTONPIN, getButton, FALLING);
  
  Serial.begin(115200); 
  delay(5000);

  //Start wifi connection
  Serial.println("Connecting to wifi..");
  WiFi.begin(APSSID, APPASSWORD);
  
  //Print MAC to serial so we can use the address for auth if needed.
  printMAC();

  // Wait for connection - Slow flash
  Serial.print("Waiting on Connection ...");
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(LEDPIN, LOW);
    delay(500);
    Serial.print(".");
    //Serial.println(WiFi.status());
    digitalWrite(LEDPIN, HIGH);
    delay(500);
  }
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
  server.on("/state",HTTP_GET,RelayStateGET);
  server.on("/change",HTTP_GET,RelayChange);
  server.onNotFound(handleNotFound);
  httpUpdater.setup(&server, OTAPATH, OTAUSER, OTAPASSWORD); //OTA Update endpoint
  
  //Start the web server
  server.begin();

  client.setServer(mqtt_server,mqtt_port);
  client.setCallback(callback);
  reconnect();

 
  //Start up blink of LED signaling everything is ready. Fast Flash
  for (int i = 0; i < 10; i++) {
    setLED(!LEDState);
    delay(100);
  }
  Serial.println("Server is up.");

  //Enable periodic watcher for button event handling
  buttonTick.attach(BUTTONTIME, buttonFlagSet);
}
//mqtt
void mqtt_publish(char* message,const char* sender){
  if(!client.connected()){
    Serial.println("Mqtt reconnect");
    reconnect();
  }
  client.loop();
  
  char output[256];
  StaticJsonDocument<256> doc;

  doc["sender"] = sender;
  doc["message"] = message;
  doc["room"] = mqtt_id;

  serializeJson(doc, output);

  client.publish(mqtt_topic, output);  
  Serial.println("Mqtt Send");
  delay(100);
}
void callback(char* topic, byte* payload, unsigned int length) {
  String Msg = "";
  int i=0;
  Serial.println("Mqtt Message Arrived here");
  while (i<length) Msg += (char)payload[i++];
  
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, Msg.c_str(), length);
  
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.c_str());
    char error_put[64];
    StaticJsonDocument<64> doc;
    doc["sender"] = "self";
    doc["message"] = error.c_str();
    doc["room"] = mqtt_id;
    client.publish(mqtt_topic, error_put);
    return;
  }

  const char* sender = doc["Light"]["sender"];
  const char* message = doc["Light"]["message"];
  const char* destination = doc["Light"]["destination"];
  String message_str = message;
  String destination_str = destination;
  
  if(message_str.equals("ON")){
    if(RelayState == true){
      mqtt_publish("already On", sender);
    }
    else{
      setRelay(!RelayState);
      mqtt_publish("On", sender);
    }
  }
  else if(message_str.equals("STATE")){
    if(RelayState == true){
      mqtt_publish("On", sender);
    }
    else{
      mqtt_publish("Off", sender);
    }
  }
  else{
    if(message_str.equals("OFF")){
      if(RelayState == true){
        setRelay(!RelayState);
        mqtt_publish("Off", sender);
      }
      else{
        mqtt_publish("already Off", sender);
      }
    }
  }
} 
void reconnect() {
  if(client.connected()){
    Serial.println("Already connected on mqtt server");
    client.publish(mqtt_topic_con, "test button is already conneted on mqtt server");
  }
  else{
    // Loop until we're reconnected
    Serial.println("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(mqtt_id)) { //change to ClientID
      Serial.println("connected");
       
      // ... and resubscribe
      client.subscribe(mqtt_topic_sub);
 
      // Once connected, publish an announcement...
      client.publish(mqtt_topic_con, "{\"sender\":\"self\",\"message\":\"reconneted\",\"room\":\"test\"}");
       
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
    }
  }
}
/*
 * printMAC
 * Print the device MAC address to the serial port.
 */
void printMAC(void) {
  byte mac[6];
  WiFi.macAddress(mac);
  Serial.print("MAC: ");
  Serial.print(mac[0],HEX);
  Serial.print(":");
  Serial.print(mac[1],HEX);
  Serial.print(":");
  Serial.print(mac[2],HEX);
  Serial.print(":");
  Serial.print(mac[3],HEX);
  Serial.print(":");
  Serial.print(mac[4],HEX);
  Serial.print(":");
  Serial.println(mac[5],HEX);
}

/* 
 *  handleNotFound
 *  Return a 404 error on not found page.
 */
void handleNotFound() {
  server.send(404, "text/plain", "404: Not found");
}

/* 
 *  handleMainPage - GET
 *  Return Text for main page on GET
 */
void handleGET() {
  //Quick LED Flash
  //setLED(!LEDState);

  //Serve Page
  Serial.println("Serviced Page Request");
  String  buff;
  buff  = "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n";
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
  buff += "<input type=\"submit\" name=\"state\" class=\"submit\" value=\"SEND\" style=\"" + SendButt + "\">\n";
  buff += "</form></body></html>\n";
  server.send(200, "text/html", buff);

  //Quick LED Flash
  delay(20);
  //setLED(!LEDState);
}
void RelayStateGET(){
    String  buff;
  if (RelayState) {
    buff = "ON\n";
    mqtt_publish("test On message","test");
  } else {
    buff = "OFF\n";
    mqtt_publish("test Off message","test");
  }
  server.send(50, "text/html", buff);
}
void RelayChange(){
  setRelay(!RelayState);
  RelayStateGET();
}
/* 
 *  handleStatePOST
 *  Modify state on POST
 */
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

/* 
 *  handleStateGET
 *  Print state on GET
 */
void handleStateGET() {    
  //Serve Page
  Serial.println("Serviced API Request");

  //Print Relay state
  String  buff;
  if (RelayState) {
    buff = "ON\n";
    mqtt_publish("test On message","test");
  } else {
    buff = "OFF\n";
    mqtt_publish("test Off message","test");
  }

  server.send(200, "text/html", buff);
  
  //Quick LED Flash
  delay(20);
  //setLED(!LEDState);
}

/* 
 *  setRelay
 *  Sets the state of the Relay
*/
void setRelay(bool SetRelayState) {
  //Switch the HTML for the display page
  if (SetRelayState == true) {
    OnButt  = BUTTONON;
    OffButt = BUTTONNOACT;
  }
  if (SetRelayState == false) {
    OnButt = BUTTONNOACT;
    OffButt  = BUTTONOFF;
  }

  //Set the relay state
  RelayState = SetRelayState;
  
  digitalWrite(RELAYPIN, RelayState);

  //Set the LED to opposite of the button.
  //setLED(!SetRelayState);
}

/*
 * setLED
 * Sets the state of the LED
 */
void setLED(bool SetLEDState) {
  LEDState = SetLEDState;     // set green LED
  digitalWrite(LEDPIN, LEDState);
}

/*
 * ButtonFlagSet
 * Sets a variable so that on next loop, the button state is handled.
 */
void buttonFlagSet(void) {
  ButtonFlag = true;
}

/* Read and handle button Press*/
ICACHE_RAM_ATTR void getButton(void) {
  // short press butoon to change state of relay
  if (digitalRead(BUTTONPIN) == false ) {
    ++ButtonCount;
    }
  if (digitalRead(BUTTONPIN) == false && ButtonCount > 1 && ButtonCount < 12 ) {
    setRelay(!RelayState); // change relay
      if(RelayState==true){
        client.publish(mqtt_topic,"{\"sender\":\"self\",\"message\":\"On\",\"room\":\"test\"}"); 
      }
      else{
        client.publish(mqtt_topic,"{\"sender\":\"self\",\"message\":\"Off\",\"room\":\"test\"}");
      }
      ButtonCount = 0;
      delay(500);
  }
  /* long press button restart */
  if (ButtonCount > 12) {
    setLED(!LEDState);
    buttonTick.detach();    // Stop Tickers
    /* Wait for release button */
    while (!digitalRead(BUTTONPIN)) yield();
    delay(100);
    ESP.restart();
  }
  if (digitalRead(BUTTONPIN) == true) ButtonCount = 0;
  ButtonFlag = false;
}
 
/*
 * loop
 * System Loop
 */
void loop(void){
  server.handleClient();           // Listen for HTTP request
  if(!client.connected()) reconnect();
  client.loop();
} 
