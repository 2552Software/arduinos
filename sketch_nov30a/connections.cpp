#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <esp_event.h>
#include <WiFi.h>

#include "connections.h"

// send to serial and mqtt if mqtt connected, msg must be null terminated string
void Connections::trace(const char*msg){
  sendToMQTT("trace", msg);
}
void Connections::loop(){
  if (!wifiClient.connected()) {
    connect();
  }
  mqttClient.loop();

}
// payload must be null terminated
bool Connections::sendToMQTT(const char*topic, const char*payload){
  connect(); // do as much as we can to work with bad networks
  if (!mqttClient.publish(topic, payload)) {
    Log.error("sending binary message to mqtt %s", topic);
    return false;
  }
 }

// send to mqtt, binary is only partily echoed
bool Connections::sendToMQTT(const char*topic, JsonObject& root){
  String output;
  root.printTo(output);
  Log.trace(F("sending message to MQTT, topic is %s ;"), topic);
  Log.trace(output.c_str());
  connect(); // do as much as we can to work with bad networks
  if (!mqttClient.publish(topic, output.c_str())) {
    Log.error("sending message to mqtt fails topic %s", topic);
    return false;
  }
  Log.trace("sent message to mqtt topic %s", topic);
  return true;
}
boolean Connections::sendToMQTT(const char* topic, const uint8_t * payload, unsigned int length){
  Log.trace(F("sending binary message to MQTT, topic is %s size is %d"), topic, length);
  connect(); // do as much as we can to work with bad networks
  if (!mqttClient.publish(topic, payload, length)) {
    Log.error("sending binary message to mqtt topic %s len %d", topic, length);
    return false;
  }
  Log.trace("sent binary message to mqtt just fine topic %s len %d", topic, length);
  delay(1000);
  return true;
  
}

// timeout connection attempt
uint8_t Connections::waitForResult(int connectTimeout) {
  if (connectTimeout == 0) {
    Log.trace(F("Waiting for connection result without timeout" CR));
    return WiFi.waitForConnectResult();
  } 
  else {
    Log.trace(F("Waiting for connection result with time out of %d" CR), connectTimeout);
    unsigned long start = millis();
    uint8_t status;
    while (1) {
      status = WiFi.status();
      https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/include/wl_definitions.h
      if (millis() > start + connectTimeout) {
        Log.trace(F("Connection timed out"));
        return WL_CONNECT_FAILED;
      }
      if (status == WL_CONNECTED){
        Log.notice(F("hot dog! we connected"));
        return WL_CONNECTED;
      }
      if (status == WL_CONNECT_FAILED) {
        Log.error(F("Connection failed"));
      }
      Log.trace(F("."));
      delay(200);
    }

    return status;
  }
}

// allow for reconnect, can be called often it needs to be fast
void Connections::connect(){
  if (!state) {
    Log.error("state required - abort");
    return;
  }
  //https://github.com/esp8266/Arduino/blob/master/libraries/ESP8266WiFi/src/include/wl_definitions.h
  if (!WiFi.isConnected()){
    Log.trace(F("WiFi.status() != WL_CONNECTED"));
    if (state->password[0] != '\0'){
      Log.notice("Connect to WiFi... %s %s", state->ssid, state->password);
      WiFi.begin(state->ssid, state->password);
    }
    else {
      Log.notice("Connect to WiFi... %s", state->ssid);
      WiFi.begin(state->ssid);
    }
    waitForResult(10000);
    if (!WiFi.isConnected()) {
      Log.error(F("something went horribly wrong"));
      return;
    }
  }  
  connectMQTT();
}

// reconnect to mqtt
void Connections::connectMQTT() {
  // Loop until we're reconnected
  while (!mqttClient.connected()) {
   Log.trace("Connecting to MQTT... %s", ipServer);
    // Create a random client ID bugbug todo clean this up, use process id etc
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (mqttClient.connect(clientId.c_str())) {
      Log.trace("connected");
      // Once connected, publish an announcement...
      mqttClient.publish("outTopic", "hello world");
      // ... and resubscribe
      mqttClient.subscribe("inTopic");
    } else {
      Log.notice("failed, rc=%d try again in 5 seconds", mqttClient.state());
      delay(5000);
    }
  }

}
void  Connections::input(char* topic, byte* payload, unsigned int length) {
  Log.trace("Message arrived, topic %s, len %d", topic, length);
  /* example Switch on the LED if an 1 was received as first character
   *  we would set a config item like turn camera on etc maybe reboot, maybe set in the .config file
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is acive low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
  */
}

//bugbug if an AP is needed just use one $10 esp32, mixing AP with client kind of works but its not desired as one radio is shared and it leads to problems
void Connections::makeAP(char *ssid, char*pwd){
    Log.trace("makeAP %s %s", ssid, pwd);
    if (pwd && *pwd) {
      WiFi.softAP(ssid, pwd);
    }
    else {
      WiFi.softAP(ssid);
    }
    IPAddress ip = WiFi.softAPIP();
    Log.trace("softap IP %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
    isSoftAP = true; //todo bugbug PI will need to keep trying to connect to this AP
}

void Connections::setup(){
  Log.trace(F("Connections::setup, server %s, port %d"), ipServer, MQTTport);
  //put a copy in here when ready blynk_token[33] = "YOUR_BLYNK_TOKEN";//todo bugbug
  mqttClient.setServer(ipServer, MQTTport);
  mqttClient.setCallback(input);
  mqttClient.setClient(wifiClient);
  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_STA); // any ESP can be an AP if main AP is not found.
  WiFi.setAutoReconnect(true); // let system auto reconnect for us
}


void Connections::WiFiEvent(WiFiEvent_t event) {
  //https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/include/esp32/esp_event.h
  switch (event){
  case SYSTEM_EVENT_STA_START:
    Log.notice("[WiFi-event] SYSTEM_EVENT_STA_START");
    break;
  case SYSTEM_EVENT_STA_CONNECTED:
    Log.notice("[WiFi-event] SYSTEM_EVENT_STA_CONNECTED");
    break;
  case SYSTEM_EVENT_STA_GOT_IP:
    Log.notice("[WiFi-event] SYSTEM_EVENT_STA_GOT_IP");
    WiFi.setHostname("Station_Tester_02"); //bugbug todo make this unqiue and refelective and get from config
    Log.notice("Connected, host name is %s", WiFi.getHostname()); //bugbug todo make this unqiue and refelective
    Log.notice("RSSI: %d dBm", WiFi.RSSI());
    Log.notice("BSSID: %d", *WiFi.BSSID());
    Log.notice("LocalIP: %s", WiFi.localIP().toString().c_str());
    break;
  default:
    char text[32];
    snprintf(text, 32, "[WiFi-event] event: %d", event); // just use incrementor and unique name/type bugbug todo
    Log.notice(text); //bugbug todo get this to send via mqtt
  }
}
