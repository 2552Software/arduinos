#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <esp_event.h>
#include <WiFi.h>
#include <SPI.h>

#include "camera.h"

// get from the camera
void Camera::capture(){
  int total_time = 0;

  Log.trace(F("start capture"));
  myCAM.flush_fifo(); // added this
  myCAM.clear_fifo_flag();
  myCAM.start_capture();

  total_time = millis();
  while (!myCAM.get_bit(ARDUCHIP_TRIG, CAP_DONE_MASK));
  total_time = millis() - total_time;

  Log.trace(F("capture total_time used (in miliseconds): %D"), total_time);
}
 
bool  Camera::findCamera(){
  uint8_t vid, pid;

  delay(5000); // make things are warmed up

#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  //Check if the camera module type is OV2640
  myCAM.wrSensorReg8_8(0xff, 0x01);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
  if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
    Log.trace(F("OV2640 not detected, vid %x, pid %x"), vid, pid);
  }
  else {
    connections.trace("OV2640 detected");
    return true;
  }
#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
  //Check if the camera module type is OV5640
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5640_CHIPID_LOW, &pid);
  if((vid != 0x56) || (pid != 0x40)){
     Log.trace(F("OV5640 not detected"));
  }
  else {
    connections.trace("OV5640 detected");
    return true;
  }
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) || (defined (OV5642_CAM))
 //Check if the camera module type is OV5642
  myCAM.wrSensorReg16_8(0xff, 0x01);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_HIGH, &vid);
  myCAM.rdSensorReg16_8(OV5642_CHIPID_LOW, &pid);
   if((vid != 0x56) || (pid != 0x42)){
     Log.trace(F("OV5642 not detected"));
   }
   else {
    connections.trace("OV5642 detected");
    return true;
   }
#endif
  connections.error("no cam detected");
  return false;// no cam
}
void Camera::initCam(){
  Log.trace(F("init camera"));
  //Change to JPEG capture mode and initialize the camera module
  myCAM.set_format(JPEG);
  myCAM.InitCAM();
  //bugbug todo allow mqtt based set of these, then a restart if needed, store in config file
#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
    myCAM.OV2640_set_JPEG_size(OV2640_800x600); //OV2640_320x240 OV2640_800x600 OV2640_640x480 OV2640_1024x768 OV2640_1600x1200
    connections.trace("init OV2640_800x600");
#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5640_set_JPEG_size(OV5640_320x240);
    communication.trace("init OV5640_320x240");
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) ||(defined (OV5642_CAM))
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);  
    connections.trace("init OV5642_320x240");
#endif

}
// SPI must be setup
void Camera::setup(){
  //set the CS as an output:
  pinMode(CS,OUTPUT);
  pinMode(CAM_POWER_ON , OUTPUT);
  digitalWrite(CAM_POWER_ON, HIGH);

  while(1){
    //Check if the ArduCAM SPI bus is OK
    myCAM.write_reg(ARDUCHIP_TEST1, 0x55);
    if(myCAM.read_reg(ARDUCHIP_TEST1) != 0x55){
      connections.error("SPI interface Error!");
      delay(20);
      continue;
    }
    else {
      connections.trace("SPI interface to camera found");
      break;
    }
  } 
  // need to check in a loop at least for a bit
  int i = 0;
  while(1){
    Log.trace(F("try to find camera"));
    if (findCamera()) {
      break;
    }
    if (i++ > 5){
      connections.error("restart esp to try to find camera"); //bugbug this does not seem to help problem is intermitant
      esp_restart();
    }
  }

  initCam();
  delay(1000); // let things setup
}

//send via mqtt, mqtt server will turn to B&W and see if there are changes ie motion, of so it will send it on
void Camera::captureAndSend(const char * path, Connections&connections){
  // not sure about this one yet
  #define MQTT_HEADER_SIZE 32 //5 + 2+strlen(topic)

  uint32_t length = 0;
  bool is_header = false;
  
  capture();

  //OV2640_320x240 is 6149 currently at least, not too bad, 640x480 is 15365, still not too bad, 800x600 16389, OV2640_1024x768 is 32773, getting too large? will vary based on image
  // 71685 for OV2640_1600x1200. Next see how each mode looks and how fast OV2640_1600x1200 can be sent. All data sizes around power of 2 with some padding. is this consistant?
  length = myCAM.read_fifo_length();
  
  if (length >= MAX_FIFO_SIZE) {
     Log.error(F("len %d, MAX_FIFO_SIZE is %d, ignore"), length, MAX_FIFO_SIZE);
     connections.error("fifo too big");
     return;
  }
  if (length == 0 )   {
     Log.error(F("len is 0 ignore"));
     connections.error("fifo 0");
     return;
  }

  // will send file in parts
  uint8_t* buf = new uint8_t[MQTT_MAX_PACKET_SIZE];
  if (buf == nullptr){
     connections.error("no mem");
     return;
  }
  // let reader know we are coming so they can start saving data
  DynamicJsonBuffer jsonBuffer;
  JsonObject& JSONencoder = jsonBuffer.createObject();
  JSONencoder["path"] = path; // also name of topic with data
  connections.sendToMQTT("dataready", JSONencoder);

  // goto this next https://github.com/Links2004/arduinoWebSockets
  uint8_t temp = 0, temp_last = 0;
  unsigned int i = 0; // current index
  unsigned int total = 0; // total bytes sent
  myCAM.CS_LOW();
  myCAM.set_fifo_burst();
  while ( length-- )  {
    temp_last = temp;
    temp =  SPI.transfer(0x00);
    //Read JPEG data from FIFO until EOF, send send the image.  Size likely to be smaller than buffer
    if ( (temp == 0xD9) && (temp_last == 0xFF) ){ //If find the end ,break while,
        buf[i++] = temp;  //save the last  0XD9     
       //Write the remain bytes in the buffer
       // todo get the save code from before, then use that to store CAMID (already there I think) and use that as part of xfer
        myCAM.CS_HIGH();
        // send buffer via mqtt here  
        total += i+1;
        Log.trace("total %d", total);
        connections.sendToMQTT(path, buf, i+1); // will need to namic topic like "Cam1" kind of things once working bugbug
        // now send notice
        // send MQTT start xfer message
        DynamicJsonBuffer jsonBuffer;
        JsonObject& JSONencoder = jsonBuffer.createObject();
        JSONencoder["path"] = path; // also name of topic with data
        JSONencoder["len"] = total; // server can compare len with actual data lenght to make sure data was not lost
        connections.sendToMQTT("datafinal", JSONencoder);
        is_header = false;
        i = 0;
    }  
    if (is_header)    { 
       //Write image data to buffer if not full
        if (i < MQTT_MAX_PACKET_SIZE-MQTT_HEADER_SIZE){
          buf[i++] = temp;
        }
        else        {
          //Write MQTT_MAX_PACKET_SIZE bytes image data
          myCAM.CS_HIGH();
          total += i+1;
          connections.sendToMQTT(path, buf, i+1);
          i = 0; // back to start of buffer
          buf[i++] = temp;
          myCAM.CS_LOW();
          myCAM.set_fifo_burst();
        }        
    }
    else if ((temp == 0xD8) & (temp_last == 0xFF)) { // start of file
      is_header = true;
      buf[i++] = temp_last;
      buf[i++] = temp;   
    } 
  } 
  delete buf;
}
Camera camera = Camera();

