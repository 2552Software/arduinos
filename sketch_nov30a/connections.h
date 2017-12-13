#ifndef _CONNECTIONS_
#define _CONNECTIONS_

#include <PubSubClient.h>
#include "state.h"

class Connections {
public:
  Connections(){isSoftAP = false;}
  void setup(char *ssid, char *pwd=nullptr);
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
private:
  char ssid[MAX_SSID_LEN];
  char password[MAX_PWD_LEN];
  static void WiFiEvent(WiFiEvent_t event);
  bool isSoftAP;
  void connect();
  uint8_t waitForResult(int connectTimeout);
  static void  input(char* topic, byte* payload, unsigned int length);
} ;
extern Connections connections;
#endif
