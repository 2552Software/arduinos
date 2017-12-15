 //https://arduinojson.org/faq/esp32/

/* core camera, uses MQTT to send data in small chuncks.  
 *  need to figure license and copy right etc yet
 */
 // tested cam
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>

#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <esp_event.h>

#if !(defined ESP32 )
#error Please select the some sort of ESP32 board in the Tools/Board
#endif

class myLog : public Logging{
  // figure how to use this then send over mqtt, will be cool
};

#define ISMAIN
#include "state.h"
#include "connections.h"
#include "camera.h"


//Version 2,set GPIO0 as the slave select :

//https://github.com/thijse/Arduino-Log/blob/master/ArduinoLog.h
int logLevel = LOG_LEVEL_ERROR; //LOG_LEVEL_VERBOSE; // 0-LOG_LEVEL_VERBOSE, will be very slow unless 0 (none) or 1 (LOG_LEVEL_FATAL)

//todo bugbug move to a logging class and put a better time date stamp
void printTimestamp(Print* _logOutput) {
  char c[12];
  int m = sprintf(c, "%10lu ", millis());
  _logOutput->print(c);
}

void printNewline(Print* _logOutput) {
  _logOutput->print('\n');
}
void setup(){
  Serial.begin(115200);

  Log.begin(logLevel, &Serial);
  Log.setPrefix(printTimestamp); 
  Log.setSuffix(printNewline); 
  Log.notice(F("ArduCAM Start!"));

  Wire.begin();
  SPI.begin();
  state.setup();
  connections.setup(state);
  camera.setup();

/* figure this mqtt logging out
  char text[32];
  snprintf(text, 32, "device %s setup", "find this"); 
  connections.trace(text); // not sure but maybe something is wrong
  */
/*

  // put all other code we can below camera setup to give it time to setup
  
  // stagger the starts to make sure the highest priority is most likely to become AP etc
  if (state.priority != 1){
    delay(1000*state.priority);
  }
  */
}

void loop(){


  connections.loop();
  
  char buf[128];//bugbug get better sizing
  snprintf(buf, sizeof(buf), "hearbeat %lu", state.count); // just use incrementor and unique name/type bugbug todo
  connections.trace(buf);

  // for at least now make name just a number as assume elsehwere json accounts for a more unique name
  snprintf(buf, sizeof(buf), "%lu", state.count++); // just use incrementor and unique name/type bugbug todo
  state.set(); // count needs to be saved

  
  camera.captureAndSend(state.name, buf, connections);
  // figure out sleep next bugbug to do
  /*
  state.powerSleep();
  camera.turnOff();
  camera.turnOn(); // do we need this? bugbug todo do we have to wait for start up here? 1 second? what?
  */
}







