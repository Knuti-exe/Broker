#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <esp_system.h>


#define charger 4
#define charging_led 8

// WiFi
static const char *ssid = "TP-Link_5235";
static const char *passwd = "MaDaPi16";
// MQTT
static const char *mqtt_server = "192.168.0.100";
static const int mqtt_port = 1883;
static const char *mqtt_user = "custom";
static const char *mqtt_passwd = "broker#123";
static const char *mqtt_bat_topic = "broker/battery";
static const char *mqtt_charg_topic = "broker/charger";
// Telnet
static const int telnet_port = 23;

static bool OTAUpdate = false;
static bool last_charger_state = true;
static int64_t last_recon_time = 0;

// Font colors
const static char* clr_charger_on = "\033[32mON\033[0m";
const static char* clr_charger_off = "\033[33mOFF\033[0m";
const static char* CLR_RST = "\033[0m";
const static char* CLR_RED = "\033[31m";
const static char* CLR_GRN = "\033[32m";
const static char* CLR_YLW = "\033[33m";

static WiFiServer telnetServer(telnet_port);
static WiFiClient telnet;

static WiFiClient client;
static PubSubClient mqttClient(client);

void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
const char* getMQTTState(int state);
const char* getResetReason();
void Change_state(bool charge=true);

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, passwd);

  pinMode(charging_led, OUTPUT);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Serial.print(".");
    digitalWrite(charging_led, !digitalRead(charging_led));
  }
  Serial.println(WiFi.localIP());
  
  digitalWrite(charging_led, HIGH);   // reverse logic for builtin led
  
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

  
  Change_state(last_charger_state);
  
}

void loop() {
  ArduinoOTA.handle();

  if (!OTAUpdate) {

    int64_t now = esp_timer_get_time();

    if (!mqttClient.connected() && now - last_recon_time > 5000000) { // 5 sec
      reconnect();
      last_recon_time = esp_timer_get_time();
    }

    mqttClient.loop();


    if ((!telnet || !telnet.connected())) {
      
      if (telnet) telnet.stop();

      telnet.available();

      telnet = telnetServer.accept();
      
      if (telnet) {
        
        telnet.print("INFO: \tTelnet client have just connected!\n\r");
        Serial.print("Telnet client have just connected!\n\r");
        telnet.print("...\n\r");

        telnet.printf("%sWARNING%s: \tLast reset reason: %s%s%s\n\r",
          CLR_YLW, CLR_RST, CLR_YLW, getResetReason(), CLR_RST);

        telnet.printf("INFO: \tLast charger state: %s\n\r", 
          (last_charger_state) ? clr_charger_on : clr_charger_off);
    
      }
    }

    while (telnet && telnet.available()) telnet.read();
    

    vTaskDelay(pdMS_TO_TICKS(100));
  }


}

void reconnect() {

  bool connected = false;

  Serial.print("Reconnecting with MQTT broker...\n");
  telnet.printf("\r%sReconnecting with MQTT broker...%s\n\r", CLR_YLW, CLR_RST);
   
  if (mqttClient.connect("Charger-c3", mqtt_user, mqtt_passwd)) {
    Serial.print("Connected.\n\n");
    telnet.print("Connected.\n\n\r");
    
    connected = true;
    
    mqttClient.subscribe(mqtt_bat_topic);
    
  } else {
    
    Serial.printf("%sError: \t%s%s\n", 
      CLR_RED, getMQTTState(mqttClient.state()), CLR_RST);

    telnet.printf("%sError: \t%s%s\n\r", 
      CLR_RED, getMQTTState(mqttClient.state()), CLR_RST);
   
  }
  
  
  if (!connected) {
    Serial.print("Trying again later...\n");
    telnet.print("Trying again later...\n\r");
  }  
  
}

void callback(char *topic, byte *payload, unsigned int length) {

  String msg;

  for (uint i=0; i<length; i++) {
    msg += (char)payload[i];
  }


  if (String(topic) == String(mqtt_bat_topic)) {

    if (msg.toInt() >= 80 ) {

      Change_state(false);

      mqttClient.publish(mqtt_charg_topic, "off", true);

    } else if (msg.toInt() <= 30 ) { 

      Change_state(true);

      mqttClient.publish(mqtt_charg_topic, "on", true);
      
    } else if (msg.toInt() >30 && msg.toInt() < 80){

      telnet.printf("\tINFO: \tCHARGER : STAYING AT %s STATE\n\r", 
        (last_charger_state) ? clr_charger_on : clr_charger_off);

      mqttClient.publish(mqtt_charg_topic, (last_charger_state) ? "on" : "off", true);
    
    } else {
      telnet.printf("\r%sERROR%s: Received message has wrong value : %ld, in str : %s\n\r", 
        CLR_RED, CLR_RST, msg.toInt(), msg.c_str());
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

void Change_state(bool charge) {
  if (charge) {
    
    digitalWrite(charger, HIGH);
    digitalWrite(charging_led, LOW);

    last_charger_state = true;

    if (telnet.connected()) telnet.printf("\tINFO: CHARGER: %s\n\r", clr_charger_on);

  } else {

    digitalWrite(charger, LOW);
    digitalWrite(charging_led, HIGH);

    last_charger_state = false;

    if (telnet.connected()) telnet.printf("\tINFO: CHARGER: %s\n\r", clr_charger_off);
    
  }
}