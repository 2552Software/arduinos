#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <esp_event.h>
#include <WiFi.h>

#include "state.h"

void State::setup(){
  Log.notice(F("mounting file system"));
  // Next lines have to be done ONLY ONCE!!!!!When SPIFFS is formatted ONCE you can comment these lines out!!
  //Serial.println("Please wait 30 secs for SPIFFS to be formatted");
  //SPIFFS.format();
  //sleep(30*1000);
  
  if (SPIFFS.begin()) { // often used
    //clean FS, for testing
    //Serial.println("SPIFFS remove ...");
    //SPIFFS.remove(CONFIG_FILE);
    Log.notice(F("mounted file system bytes: %d/%d"), SPIFFS.usedBytes(), SPIFFS.totalBytes());
    get();
  }
  else {
     Log.error("no SPIFFS maybe you need to call SPIFFS.format one time");
  }

}

void State::powerSleep(){ // sleep when its ok to save power
    if (sleepTimeS) {
      Log.notice("power sleep for %d seconds", sleepTimeS);
      esp_sleep_enable_timer_wakeup(sleepTimeS*1000000); //10 seconds
      int ret = esp_light_sleep_start();
      Log.notice("light_sleep: %d", ret);
     // ESP.deepSleep(20e6); // 20e6 is 20 microseconds
      //ESP.deepSleep(sleepTimeS * 1000000);//ESP32 sleep 10s by default
      Log.notice("sleep over");
    }
}

void State::echo(){
  Log.notice(F("name %s, ssid: %s pwd %s: other: %s, sleep time in seconds %d"), name, ssid, password, other, sleepTimeS);
}

void State::setDefault(){
  Log.notice(F("set default State"));
  snprintf(ssid, sizeof(ssid), SSID);   
  snprintf(password, sizeof(password), PWD);   
  uint64_t chipid = ESP.getEfuseMac();
  snprintf(name, sizeof(name), "cam1-%X%X",(uint16_t)(chipid>>32), (uint32_t)chipid);   
  other[0] = '\0';
  sleepTimeS=10;
  priority = 1; // each esp can have a start priority defined by initial sleep 
  echo();
}

// todo bugbug mqtt can send us these parameters too once we get security going
void State::set(){
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    File configFile = SPIFFS.open(defaultConfig, "w");
    if (!configFile) {
      Log.error("failed to open config file for writing");
    }
    else {
      json["name"] = name;
      json["ssid"] = ssid;
      json["pwd"] = password;
      json["other"] = other;
      json["priority"] = priority;
      json["sleepTimeS"] = sleepTimeS;
      Log.notice("config.json contents");
      echo();
      json.prettyPrintTo(Serial);
      json.printTo(configFile);
      configFile.close();
    }
}
void State::get(){
  //read configuration from FS json
  if (SPIFFS.exists(defaultConfig)) {
    //file exists, reading and loading
    Log.notice("reading config file");
    File configFile = SPIFFS.open(defaultConfig, "r");
    if (configFile) {
      Log.notice("opened config file");
      size_t size = configFile.size();
      // Allocate a buffer to store contents of the file. crash if no memory
      std::unique_ptr<char[]> buf(new char[size]);
      configFile.readBytes(buf.get(), size);
      DynamicJsonBuffer jsonBuffer(JSON_OBJECT_SIZE(1) + MAX_PWD_LEN);
      JsonObject& json = jsonBuffer.parseObject(buf.get());
      json.printTo(Serial);
      if (json.success()) {
        Log.notice("parsed json A OK");
        snprintf(ssid, sizeof(ssid), json["ssid"]);   
        snprintf(password, sizeof(password), json["pwd"]);   
        snprintf(other, sizeof(other), json["other"]);   
        snprintf(name, sizeof(name), json["name"]);   
        priority = atoi(json["priority"]);
        sleepTimeS = json["sleepTimes"];
        echo();
      } 
      else {
        Log.error("failed to load json config");
      }
    }
    else {
      Log.error("internal error");
    }
  }
  else {
    Log.notice("no config file, set defaults");
    setDefault();
  }

  echo();
}
