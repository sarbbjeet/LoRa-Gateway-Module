#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ArduinoJson.h>  //5.7.0 version
#include <Ticker.h>       //Ticker Library

//lora library
#include <SPI.h>  // include libraries
#include <LoRa.h>

Ticker blinker;

//variables for LoRa transeiver
String outgoing;  // outgoing message
String incoming = "";
byte msgCount = 0;         // count of outgoing messages
byte localAddress = 0xAA;  // address of this device
byte destination = 0xBB;   // destination to send to
byte deviceModel = 0;      //default device id
long lastSendTime = 0;     // last send time
int interval = 2000;       // interval between sends



/********************MQTT broker config****************************/
const char* mqtt_server = "allmotorsltd.co.uk";
String user_id = "7890";  //farmer id

String inTopic = "/inTopic/" + user_id;    //-->subscribe
String outTopic = "/outTopic/" + user_id;  //-->publish

//MQTT broker authentication.
#define AIO_SERVERPORT 1883  // use 8883 for SSL
#define AIO_USERNAME "sarb" //username 
#define AIO_KEY "password" //password


//interrupt pin to change wifi mode reset
#define mode_reset D3  //D3 pin of node mcu
#define notifyPin D4   //D4 pin

//pin to control LoRa transeiver
#define csPin D0     // D1 LoRa radio chip select
#define resetPin D1  // D0 LoRa radio reset
#define irqPin D2    // D2 change for your board; must be a hardware interrupt pin


//Variables for esp wifi config
String esid = "", epass = "";
uint8_t wifi_mode = 0;    //0  for access point and 1 for station mode
uint8_t ledBlinkTag = 0;  //tag to handle blinking 0--> fast, 1 -->slow, 2--> steady

//declare methods
void readEEPROM(void);
ICACHE_RAM_ATTR void changeWifiMode();
// void handle_conf(void);

//Establishing Local server at port 80 whenever required
ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
unsigned long lastMsg1 = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
void setup_wifi() {
  if (wifi_mode == 1) {
    // We start by connecting to a WiFi network
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(esid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(esid, epass);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      ledBlinkTag = 0;  //blink fast

      Serial.print(".");
      if (wifi_mode == 0)
        break;
    }
    randomSeed(micros());
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    return;
  } else {
    //create access point
    WiFi.softAP("agri_wifi", "");
    Serial.println("softap");
    delay(10);
    server.handleClient();
    ledBlinkTag = 0;  //blink fast
  }
}

//received data from mqtt broker
void callback(char* topic, byte* payload, unsigned int length) {
  //convert to json
  StaticJsonDocument<200> newBuffer;
  // // Deserialize the JSON document
  DeserializationError error = deserializeJson(newBuffer, (char*)payload);
  // // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }
  //processing received mqtt packet
  byte gateway = newBuffer["gateway"];
  byte node = newBuffer["node"];
  byte deviceM = newBuffer["deviceModel"];
  //read relay data
  if (gateway != localAddress)  //gateway address not matched
    return;
  if (deviceM != 0 && deviceM != 1)  //just 2 device model is configured
    return;

  byte relays[deviceM == 0 ? 1 : 2];  //0 --> 1relay,  1-->  2relays
  for (byte r = 0; r < sizeof(relays) / sizeof(relays[0]); r++) {
    String key = "relay" + String(r);
    String receivedStr = String(newBuffer["data"][key]);
    if (receivedStr != "null") {
      byte* bytes = ((byte *)receivedStr.c_str()); 
      byte relay=bytes[0]-48;  
      if (relay == 1 || relay == 0) relays[r] = relay;
      else relays[r] = 2;  //no data
    } else  relays[r] = 2;  //no data
  }
  //send to LoRa Node
  LoRa.beginPacket();  // start packet
  LoRa.write(gateway);
  LoRa.write(node);
  LoRa.write(deviceM);
  LoRa.write(deviceM == 0 ? 1 : 2);  //message length
  for (byte _r = 0; _r < sizeof(relays) / sizeof(relays[0]); _r++)
    LoRa.write(relays[_r]);
  LoRa.endPacket();  // finish packet and send it
  LoRa.receive();    // go back into receive mode

  Serial.print("gateway=");
  Serial.println(String(newBuffer["gateway"]));
}

void reconnect() {
  // Loop until we're reconnected
  // while (!client.connected()) {
  Serial.print("Attempting MQTT connection...");
  // Create a random client ID
  String clientId = "ESP8266Client-";
  clientId += String(random(0xffff), HEX);
  // Attempt to connect
  if (client.connect(clientId.c_str(), AIO_USERNAME, AIO_KEY)) {
    Serial.println("connected");
    // ... and resubscribe
    client.subscribe(inTopic.c_str());
  } else {
    Serial.print("failed, rc=");
    Serial.print(client.state());

    Serial.println(" try again in 5 seconds");
    // Wait 5 seconds before retrying
    delay(5000);
    ledBlinkTag = 1;  //blink slow when broker error
  }
}

//led notification
void notifyLed() {
  if (ledBlinkTag == 0)  //fast blinking
  {
    digitalWrite(notifyPin, !digitalRead(notifyPin));
    Serial.println("fast blinking");
  } else if (ledBlinkTag == 1) {  //bliking slow
    unsigned long now = millis();
    if (now - lastMsg1 > 1500) {
      lastMsg1 = now;
      Serial.println("slow blinking");
      digitalWrite(notifyPin, !digitalRead(notifyPin));
      // digitalWrite(BUILTIN_LED, !(digitalRead(BUILTIN_LED)));
    }
  }
}
void setup() {
  pinMode(notifyPin, OUTPUT);  // Initialize the BUILTIN_LED pin as an output
  //define interrupt pin
  pinMode(mode_reset, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(mode_reset), changeWifiMode, RISING);  //interrupt for wifi mode reset
  Serial.begin(115200);

  //LoRa module config ...
  LoRa.setPins(csPin, resetPin, irqPin);  // set CS, reset, IRQ pin
  if (!LoRa.begin(868E6)) {               // initialize ratio at 915 MHz
    Serial.println("LoRa init failed. Check your connections.");
    while (true)
      ;  // if failed, do nothing
  }
  LoRa.onReceive(onReceive);
  LoRa.receive();
  Serial.println("LoRa init succeeded.");


  //notification led timer logic Initialize Ticker every 0.5s
  blinker.attach(0.5, notifyLed);  //Use attach_ms if you need time in ms

  EEPROM.begin(512);  //Initialasing EEPROM
  delay(10);
  readEEPROM();
  Serial.println("Disconnecting previously connected WiFi");
  WiFi.disconnect();
  delay(10);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  server.on("/", httpCallback);  //web server listener to get ssid and password (as a POST request)
  Serial.println("softap");
  server.begin();  //start server
  Serial.println("HTTP server started");
}
void loop() {
  if (client.connected()) {
    ledBlinkTag = 2;               //stop blinking led
    digitalWrite(notifyPin, LOW);  // turn on
    client.loop();
  } else {
    reconnect();
    setup_wifi();  //monitor wifi connection
  }
  //onReceive(LoRa.parsePacket());
}

//get ssid, password and farmerId from  http POST request
void httpCallback() {
  String data = server.arg("plain");
  StaticJsonDocument<200> newBuffer;
  // Deserialize the JSON document
  DeserializationError error = deserializeJson(newBuffer, data);
  // Test if parsing succeeds.
  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(500, "text/html", "error");
    return;
  }
  esid = String(newBuffer['ssid']);
  epass = String(newBuffer['pass']);
  user_id = String(newBuffer['user_id']);  //farmer id
  server.send(200, "text/html", "ok");


  // StaticJsonBuffer<200> newBuffer;
  // JsonObject& jObject = newBuffer.parseObject(data);
  // esid =String(jObject["ssid"]);
  // epass = String(jObject["pass"]);
  // server.send(200, "text/html", "ok");

  //change wifi mode to web station
  wifi_mode = 1;
  EEPROM.write(96, wifi_mode);
  delay(10);
  EEPROM.commit();  //Store data to EEPROM
}


//wifi mode reset pin interrupt
ICACHE_RAM_ATTR void changeWifiMode() {
  wifi_mode = 0;  //change wifi mode  to access point
  Serial.println("wifi mode change to access point");
  EEPROM.write(96, wifi_mode);
  delay(10);
  EEPROM.commit();  //Store data to EEPROM
  WiFi.disconnect();
}


void readEEPROM(void) {
  esid = "";
  epass = "";
  user_id = "";
  for (int i = 0; i < 32; ++i) {
    esid += char(EEPROM.read(i));
  }
  Serial.println();
  Serial.print("SSID ->: ");
  Serial.println(esid);
  Serial.println("Reading EEPROM ssid and pass");

  for (int i = 32; i < 96; ++i) {
    epass += char(EEPROM.read(i));
  }
  Serial.print("PASS ->: ");
  Serial.println(epass);

  //read wifi mode
  wifi_mode = EEPROM.read(96);
  delay(5);
  if (wifi_mode != 0 && wifi_mode != 1) {
    wifi_mode = 0;
    EEPROM.write(96, wifi_mode);
    delay(10);
    EEPROM.commit();  //Store data to EEPROM
    return;
  }
  //user_id
  for (int i = 97; i < 130; ++i) {
    user_id += char(EEPROM.read(i));
  }
}

String arraysToJsonbuffer(float* sensors, byte numberOfSensors, byte* relays, byte numberOfRelays) {
  //create json object
  StaticJsonDocument<200> jsonBuffer;
  String myOutput = "";
  //add value
  jsonBuffer["gateway"] = localAddress;
  jsonBuffer["node"] = destination;
  jsonBuffer["deviceModel"] = deviceModel;

  JsonObject data = jsonBuffer.createNestedObject("data");  //creata data object
  for (byte s = 0; s < numberOfSensors; s++) {
    String key = "sensor" + String(s);
    data[key] = sensors[s];
  }
  for (byte r = 0; r < numberOfRelays; r++) {
    String key = "relay" + String(r);
    data[key] = relays[r];
  }
  //serializeJson(jsonBuffer, Serial);
  serializeJson(jsonBuffer, myOutput);
  return myOutput;
}

//publish json data to broker
void publishToBroker(String dataPacket) {
  //deviceModel= 0  --> 2 sensors and 1 relay (i prefix for sensors and o prefix for relay. For example-> "i89.56,i23,o1")
  //deviceModel =1. --> 4 sensors and 2 relay (dataPacket example --> "i23.4,i78.00,i32.21,i-90.78,o1,o0")
  if (client.connected()) {
    //split dataPacket to get sensors data and convert string to float and byte
    byte numberOfSensors = 2;  //default
    byte numberOfRelays = 1;
    if (deviceModel == 0) {
      numberOfSensors = 2;
      numberOfRelays = 1;
    } else {
      numberOfSensors = 4;
      numberOfRelays = 2;
    }

    float sensors[numberOfSensors];
    byte relays[numberOfRelays];
    //sensors
    for (byte s = 0; s < numberOfSensors; s++) {
      String t_packet = splitStr(dataPacket, ',', s);
      if (t_packet != "" && t_packet.startsWith("i"))
        sensors[s] = t_packet.substring(1).toFloat();
      else sensors[s] = 255;  //error
    }
    //relays
    for (byte r = numberOfSensors; r < numberOfSensors + numberOfRelays; r++) {
      String t_packet = splitStr(dataPacket, ',', r);
      if (t_packet != "" && t_packet.startsWith("o")) {
        if (t_packet.substring(1, 2) == "1") relays[r - numberOfSensors] = 1;
        else if (t_packet.substring(1, 2) == "0") relays[r - numberOfSensors] = 0;
        else relays[r - numberOfSensors] = 2;  //not found or undefined
      } else relays[r - numberOfSensors] = 2;
    }
    //publish
    client.publish(outTopic.c_str(), arraysToJsonbuffer(sensors, numberOfSensors, relays, numberOfRelays).c_str());
    // arraysToJsonbuffer(sensors,numberOfSensors, relays, numberOfRelays);
  }
}

//LoRa receive event listener callback
void onReceive(int packetSize) {
  if (packetSize == 0) return;  // if there's no packet, return
  // read packet header bytes:
  int recipient = LoRa.read();        // recipient address
  byte sender = LoRa.read();          // sender address
  deviceModel = LoRa.read();          //device id of the sender device
  byte incomingLength = LoRa.read();  // incoming msg length
  incoming = "";
  // if the recipient isn't this device or broadcast,
  if (recipient != localAddress) {
    Serial.println("This message is not for me.");
    return;  // skip rest of function
  }
  while (LoRa.available()) {
    incoming += (char)LoRa.read();
  }

  if (incomingLength != incoming.length()) {  // check length for error
    Serial.println("error: message length does not match length");
    return;  // skip rest of function
  }

  if (deviceModel != 0 && deviceModel != 1) {  //check for device model 0 and 1
    Serial.println("error: data received by unknown device model");
    return;
  }

  //start publishing data to broker
  publishToBroker(incoming);
  Serial.println("Message: " + incoming);
  Serial.println("RSSI: " + String(LoRa.packetRssi()));
  Serial.println("Snr: " + String(LoRa.packetSnr()));
  Serial.println();
}

//method to split string
String splitStr(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = { 0, -1 };
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}