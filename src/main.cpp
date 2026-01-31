#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>

#define charger 33

// WiFi
const char *ssid = "*******";
const char *passwd = "*******";
// MQTT
const char *mqtt_server = "192.168.0.100";
const int mqtt_port = 1883;
const char *mqtt_user = "xiao0";
const char *mqtt_passwd = "broker#1234";
const char *mqtt_bat_topic = "broker/battery";
const char *mqtt_charg_topic = "broker/charger";
// Telnet
const int telnet_port = 23;

bool charging = false;
bool state_change = false;
bool telnet_connected = false;
long long int last_time = 0;
long long int last_recon_time = 0;
long long int last_check = 0;

WiFiServer telnetServer(telnet_port);
WiFiClient telnetClient;

WiFiClient client;
PubSubClient mqttClient(client);

void reconnect();
void callback(char *topic, byte *payload, unsigned int length);


void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, passwd);
  // TODO : wdrozyc strone WWW logowania do sieci WIFI 

  pinMode(2, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
    digitalWrite(2, !digitalRead(2));
  }
  Serial.println(WiFi.localIP());
  digitalWrite(2, LOW);
  
  telnetServer.begin();
  
  pinMode(charger, OUTPUT);

  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();

  if (!mqttClient.connected() && millis() - last_recon_time > 5000) {
    reconnect();
    last_recon_time = millis();
  }

  mqttClient.loop();


  if ((!telnetClient || !telnetClient.connected()) && !telnet_connected) {
    
    telnetClient = telnetServer.accept();
      
    if (telnetClient) {
      
      telnetClient.println("Telnet client have just connected!");
      Serial.println("Telnet client have just connected!");
      telnetClient.println("...\r\n");
      telnet_connected = true;
      
    }
  }

  if (state_change) {

    mqttClient.publish(mqtt_charg_topic, charging ? "on" : "off", true);

    state_change = false;
  }

  if (millis() - last_check > 2000) {
    telnetClient.printf("\rcharger read: %i\n", digitalRead(charger));
    
    last_check = millis();

  }
}

void reconnect() {

  bool connected = false;

  Serial.println("Reconnecting with MQTT broker...");
  telnetClient.println("\rReconnecting with MQTT broker...");

  for (int i=0; i<5; i++) {
    if (mqttClient.connect("XIAO0", mqtt_user, mqtt_passwd)) {
      Serial.println("Connected.");
      telnetClient.println("Connected.");

      mqttClient.subscribe(mqtt_bat_topic);
      
    } else {
      
      Serial.printf("Error : %i", mqttClient.state());
      telnetClient.printf("Error : %i", mqttClient.state());

      
    }
  }
  
  if (!connected) {
    Serial.println("Trying again later...");
    telnetClient.println("Trying again later...");
  }  
  
}

void callback(char *topic, byte *payload, unsigned int length) {

  String msg;

  for (int i=0; i<length; i++) {
    msg += (char)payload[i];
  }

  telnetClient.printf("\rGot msg: %s, %s", topic, msg.c_str());


  if (String(topic) == String(mqtt_bat_topic)) {

    if (msg.toInt() >= 80 ) {

      digitalWrite(charger, LOW);
      charging = false;

      telnetClient.println("NOT Charging...");
      

    } else if (msg.toInt() < 80 ) { // TODO

      digitalWrite(charger, HIGH);
      charging = true;

      telnetClient.println("Charging...");


    } else {
      telnetClient.printf("ERROR: Received message has wrong value : %i, in str : %s", 
        msg.toInt(), msg.c_str());

      

    }
  }

  state_change = true;

}
