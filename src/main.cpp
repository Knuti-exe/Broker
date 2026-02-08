#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <esp_system.h>


#define charger 4
#define led_builtin 8

// WiFi
static const char *ssid = "***********";
static const char *passwd = "********";
// MQTT
static const char *mqtt_server = "192.168.0.100";
static const int mqtt_port = 1883;
static const char *mqtt_user = "*******";
static const char *mqtt_passwd = "********";
static const char *mqtt_bat_topic = "broker/battery";
static const char *mqtt_charg_topic = "broker/charger";
// Telnet
static const int telnet_port = 23;

static bool telnet_connected = false;
static bool OTAUpdate = false;
static int64_t last_recon_time = 0;

static WiFiServer telnetServer(telnet_port);
static WiFiClient telnet;

static WiFiClient client;
static PubSubClient mqttClient(client);

void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
const char* getMQTTState(int state);
const char* getResetReason();


void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, passwd);

  pinMode(led_builtin, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Serial.print(".");
    digitalWrite(led_builtin, !digitalRead(led_builtin));
  }
  Serial.println(WiFi.localIP());
  
  digitalWrite(led_builtin, HIGH);   // reverse logic for builtin led
  
  telnetServer.begin();
  
  pinMode(charger, OUTPUT);

  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);        // light-sleep mode

  ArduinoOTA.begin();

  ArduinoOTA.onStart([]() {
    OTAUpdate = true;
  });

  ArduinoOTA.onEnd([]() {
    OTAUpdate = false;
  });

}

void loop() {
  ArduinoOTA.handle();

  int64_t now = esp_timer_get_time();

  if (!mqttClient.connected() && now - last_recon_time > 5000000) { // 5 sec
    reconnect();
    last_recon_time = esp_timer_get_time();
  }

  mqttClient.loop();


  if ((!telnet || !telnet.connected()) && !telnet_connected) {
    
    telnet = telnetServer.accept();
      
    if (telnet) {
      
      telnet.println("INFO: \tTelnet client have just connected!\r");
      Serial.println("Telnet client have just connected!");
      telnet.println("...\r\n");
      telnet_connected = true;

      telnet.printf("WARNING: \tLast reset reason: %s\n\r", getResetReason());

      
    }
  }

  vTaskDelay(pdMS_TO_TICKS(100));
}

void reconnect() {

  bool connected = false;

  Serial.println("Reconnecting with MQTT broker...");
  telnet.println("\rReconnecting with MQTT broker...\r");
   
  if (mqttClient.connect("Charger-c3", mqtt_user, mqtt_passwd)) {
    Serial.println("Connected.");
    telnet.println("Connected.\n\r");
    
    connected = true;
    
    mqttClient.subscribe(mqtt_bat_topic);
    
  } else {
    
    Serial.printf("Error: \t%s\n", getMQTTState(mqttClient.state()));
    telnet.printf("Error: \t%s\n\r", getMQTTState(mqttClient.state()));
   
  }
  
  
  if (!connected) {
    Serial.println("Trying again later...");
    telnet.println("Trying again later...\r");
  }  
  
}

void callback(char *topic, byte *payload, unsigned int length) {

  String msg;

  for (uint i=0; i<length; i++) {
    msg += (char)payload[i];
  }

  telnet.printf("\rGot msg: %s, %s", topic, msg.c_str());  // useless


  if (String(topic) == String(mqtt_bat_topic)) {

    if (msg.toInt() >= 80 ) {

      digitalWrite(charger, LOW);
      digitalWrite(led_builtin, HIGH); 

      mqttClient.publish(mqtt_charg_topic, "off", true);

      telnet.print("\tINFO: \tCHARGER : OFF\n\r");
      

    } else if (msg.toInt() <= 30 ) { 

      digitalWrite(charger, HIGH);
      digitalWrite(led_builtin, LOW);

      mqttClient.publish(mqtt_charg_topic, "on", true);


      telnet.print("\tINFO: \tCHARGER : ON\n\r");
      


    } else {
      telnet.printf("ERROR: Received message has wrong value : %ld, in str : %s\n", 
        msg.toInt(), msg.c_str());
    }
  }

}

const char* getMQTTState(int state) {
    switch (state) {
        case -4: return "MQTT_CONNECTION_TIMEOUT";
        case -3: return "MQTT_CONNECTION_LOST";
        case -2: return "MQTT_CONNECT_FAILED";
        case -1: return "MQTT_DISCONNECTED";
        case  0: return "MQTT_CONNECTED";
        case  1: return "MQTT_CONNECT_BAD_PROTOCOL";
        case  2: return "MQTT_CONNECT_BAD_CLIENT_ID";
        case  3: return "MQTT_CONNECT_UNAVAILABLE";
        case  4: return "MQTT_CONNECT_BAD_CREDENTIALS";
        case  5: return "MQTT_CONNECT_UNAUTHORIZED";
        default: return "Unknown type";
    }
}

const char* getResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "Power on reset";
        case ESP_RST_SW:      return "Software reset";
        case ESP_RST_PANIC:   return "Exception / Panic reset";
        case ESP_RST_INT_WDT: return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:return "Task Watchdog";
        case ESP_RST_BROWNOUT:return "Voltage dip";
        default:              return "Other";
    }
}