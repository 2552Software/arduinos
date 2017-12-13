#ifndef _CONNECTIONS_
#define _CONNECTIONS_

#include <PubSubClient.h>
#include "state.h"

class Connections {
public:
  Connections(State*instate){isSoftAP = false;state = instate;}
  void setup();
  void loop();
  bool sendToMQTT(const char*topic, JsonObject& root);
  bool sendToMQTT(const char*topic, const char*msg);
  bool sendToMQTT(const char*topic, const uint8_t * payload, unsigned int plength);
  static const int MQTTport = 1883;
  // ip types vary by api expecations bugbug todo get all these in a class home
  const char* ipServer = "192.168.88.100";// for now assume this, 100-200 are servers
  WiFiClient wifiClient;
  PubSubClient mqttClient;
  //char blynk_token[33] = "YOUR_BLYNK_TOKEN";//todo bugbug
  void makeAP(char *ssid, char*pwd);
  void connectMQTT();
  void trace(const char*msg);
  State*state=nullptr;
private:
  static void WiFiEvent(WiFiEvent_t event);
  bool isSoftAP;
  void connect(); // call inside sends so we can optimzie as needed
  uint8_t waitForResult(int connectTimeout);
  static void  input(char* topic, byte* payload, unsigned int length);
} ;

#endif
