#ifndef _CAMERA_
#define _CAMERA_

#define OV2640_MINI_2MP 
#include <ArduCAM.h>
#include "memorysaver.h" 

#include "connections.h"

// this mod requires some arudcam stuff
//This can work on OV2640_MINI_2MP/OV5640_MINI_5MP_PLUS/OV5642_MINI_5MP_PLUS/OV5642_MINI_5MP_PLUS/
#if !(defined (OV2640_MINI_2MP)||defined (OV5640_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || defined (OV2640_CAM) || defined (OV5640_CAM) || defined (OV5642_CAM))
#error Please select the hardware platform and camera module in the ../libraries/ArduCAM/memorysaver.h file
#endif

// set GPIO16 as the slave select :
static const int CS = 17; // unique to each setup possibly, bugbug make this run time data stored in config? bugbug to do

#ifndef ISMAIN
extern ArduCAM myCAM;
#else
#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
ArduCAM myCAM(OV2640, CS);
#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
ArduCAM myCAM(OV5640, CS); //todo use the better camera in doors or in general? or when wifi? todo bugbug
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP)  ||(defined (OV5642_CAM))
ArduCAM myCAM(OV5642, CS);
#endif
#endif



class Camera {
public:

  void setup();
  void captureAndSend(const char * path, Connections&connections);
  void turnOff(){
      Log.trace(F("cam off"));
      digitalWrite(CAM_POWER_ON, LOW);//camera power off
  }
  void turnOn(){
     Log.trace(F("cam on"));
     digitalWrite(CAM_POWER_ON, HIGH);
  }
private:
  static const uint8_t D10 = 4;
  const int CAM_POWER_ON = D10;
  void capture();
  bool findCamera();
  void initCam();
};


#endif
