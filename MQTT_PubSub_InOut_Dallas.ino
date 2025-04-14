#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <arduino-timer.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include "certificate.h"

#define CERT mqtt_broker_cert
#define MSG_BUFFER_SIZE (50)

// Data wire is conntec to the Arduino digital pin 4
#define ONE_WIRE_BUS 4

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature sensor 
DallasTemperature sensors(&oneWire);
float fTempF = 32.0;
float fTempC =  0.0;
//--------------------------------------
// config (edit here before compiling)
//--------------------------------------
//#define MQTT_TLS // uncomment this define to enable TLS transport
//#define MQTT_TLS_VERIFY // uncomment this define to enable broker certificate verification
const char* ssid = "Eckenberger_92";;
const char* password = "marlene!";
const char* mqtt_server = "rpi5-1"; // eg. your-demo.cedalo.cloud or 192.168.1.11
const uint16_t mqtt_server_port = 1883; // or 8883 most common for tls transport
const char* mqttUser = "matthias";
const char* mqttPassword = "marlene";
const char* mqttTopicOut = "climate/dallas1";
const char* mqttTopicIn = "steuerkanal";
auto timer = timer_create_default(); // create a timer with default settings

//--------------------------------------
// globals
//--------------------------------------
#ifdef MQTT_TLS
  WiFiClientSecure wifiClient;
#else
  WiFiClient wifiClient;
#endif
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
PubSubClient mqttClient(wifiClient);
uint32_t chipid = ESP.getChipId();

//--------------------------------------
// function setup_wifi called once
//--------------------------------------
void setup_wifi() {

  delay(10);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();

  #ifdef MQTT_TLS
    #ifdef MQTT_TLS_VERIFY
      X509List *cert = new X509List(CERT);
      wifiClient.setTrustAnchors(cert);
    #else
      wifiClient.setInsecure();
    #endif
  #endif

  Serial.println("WiFi connected");
}

//--------------------------------------
// function callback called everytime 
// if a mqtt message arrives from the broker
//--------------------------------------
void callback(char* topic, byte* payload, unsigned int length) {
  String sPayload = "";
  Serial.println();
  Serial.print("Message arrived on topic: '");
  Serial.print(topic);
  Serial.print("' with payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
    sPayload = sPayload + (char)payload[i];
  }
  Serial.print(" - " + sPayload);
  Serial.println();
  // here react on the message
  /*
  timeClient.update();
  String myCurrentTime = timeClient.getFormattedTime();
  mqttClient.publish(mqttTopicIn,("Meassage received: " + sPayload + " at: " + myCurrentTime).c_str());
  Serial.println("Meassage received: " + sPayload + " at: " + myCurrentTime); */
}
//--------------------------------------
// function connect called to (re)connect
// to the broker
//--------------------------------------
void connect() {
  while (!mqttClient.connected()) {
    String mqttClientId = (String)chipid;
    Serial.print("Attempting MQTT connection with clientId: " + mqttClientId + "...");
    if (mqttClient.connect(mqttClientId.c_str(), mqttUser, mqttPassword)) {
      Serial.println("connected");
      mqttClient.subscribe(mqttTopicIn,1); // topic, QoS
      Serial.println("subscribed to topic: " + (String)mqttTopicIn);
    } else {
      Serial.print("failed, error_code=");
      Serial.print(mqttClient.state());
      Serial.println(" will try again in 5 seconds");
      delay(5000);
    }
  }
}

//--------------------------------------
// main arduino setup fuction called once
//--------------------------------------
void setup() {
  Serial.begin(9600);
  setup_wifi();
  mqttClient.setServer(mqtt_server, mqtt_server_port);
  mqttClient.setCallback(callback);
  timer.every(60000, publish);
}
//--------------------------------------
// publishing procedure
//--------------------------------------
bool publish(void *) {
  // Call sensors.requestTemperatures() to issue a global temperature and Requests to all devices on the bus
  sensors.begin();
  sensors.requestTemperatures();   
  fTempC = sensors.getTempCByIndex(0);
  Serial.println();
  Serial.print("Celsius temperature: ");
  // Why "byIndex"? You can have more than one IC on the same bus. 0 refers to the first IC on the wire
  Serial.print(fTempC); 
  Serial.print(" - Fahrenheit temperature: ");
  fTempF = sensors.getTempFByIndex(0);
  Serial.println(fTempF);
  // now publish results
  Serial.println("publishing:");
  timeClient.update();
  String myFormatedTime = timeClient.getFormattedTime();
  String myUnixTime = (String)timeClient.getEpochTime() + "000000000"; // 19 digits, nano-seconds
  String sLineProtocol = "weatherdata,mcu_id=" + (String)chipid + " temperature=" + (String)fTempC + " " + myUnixTime;
  mqttClient.publish(mqttTopicOut,sLineProtocol.c_str(),false); // topic, message, retain
  Serial.println(sLineProtocol);
  return true; // repeat? true
}
//--------------------------------------
// main arduino loop fuction called periodically
//--------------------------------------
void loop() {
  Serial.print(".");
  delay(5000);
  if (!mqttClient.connected()) {
    connect();
  }
  // check the message queue
  mqttClient.loop();
  // tick the timer
  timer.tick(); 
}
