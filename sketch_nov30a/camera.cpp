#define ARDUINOJSON_ENABLE_PROGMEM 0
#include <ArduinoJson.h>
#include <ArduinoLog.h>
#include <esp_event.h>
#include <WiFi.h>
#include <SPI.h>

#include "camera.h"

// work for ours?
void set_Compress_quality(uint8_t quality)  {
  switch(quality)    {
    case high_quality:
      myCAM.wrSensorReg16_8(0x4407, 0x02);
      break;
    case default_quality:
      myCAM.wrSensorReg16_8(0x4407, 0x04);
      break;
    case low_quality:
      myCAM.wrSensorReg16_8(0x4407, 0x08);
      break;
  }
}
// some sort of magic from  https://github.com/ArduCAM/Arduino/issues/77
void shutterSpeed(){
  delay(1000);
  uint8_t R4; //Register 4: bits 0 and 1 correspond to bits 0 and 1 of exposure multiplier
  uint8_t R10; //Register 10: the 8 bits correspond to bits 2-9 of exposure multiplier
  uint8_t R13; // Register 13: bit 0 switches auto exposure control on(1)/off(0); bit 2 switches auto gain control on(1)/off(0)
  uint8_t R45; // Register 45: bits 0-5 correspond to bits 10-15 of exposure multiplier
  myCAM.wrSensorReg8_8(0xFF, 0x01);
  myCAM.rdSensorReg8_8(0x13,&R13);
  myCAM.rdSensorReg8_8(0x04,&R4);
  myCAM.rdSensorReg8_8(0x10,&R10);
  myCAM.rdSensorReg8_8(0x45,&R45);
  
  //uint16_t Exp[]={50,500,5000,50000}; //Exposure time in us (micro seconds) one millionth of a second.
  uint16_t Exp[]={5,50,500,5000}; //Exposure time in us (micro seconds) one millionth of a second.
  for (int j = 0; j<4; j++){
    uint16_t a=round(Exp[j]/53.39316); //DEC number corresponding to exposure time
    // a = 1 << 15;
    uint16_t r4shift=14;
    uint16_t r10leftshift=6;
    uint16_t r10rightshift=8;
    uint16_t r45leftshift=10;
    uint16_t b = a<<r4shift;
    uint8_t r4 = b >> r4shift; //Extracts bits 0 and 1
    uint8_t r10= (a << r10leftshift) >> r10rightshift; // Extracts bits 2-9
    uint8_t r45= a >> r45leftshift; // Extracts bits 10-15
    
    for (int i=0; i<2; i++){
      bitWrite(R4,i,bitRead(r4,i));
    }
   
    for (int i=0; i<8; i++){
     bitWrite(R10,i,bitRead(r10,i));
    }
    
    for (int i=0; i<6; i++){
      bitWrite(R45,i,bitRead(r45,i));
    }
    
    bitClear(R13,0);
    // bitClear(R13,2);
    
    myCAM.wrSensorReg8_8(0x13,(int) R13);
    myCAM.wrSensorReg8_8(0x04,(int) R4);
    myCAM.wrSensorReg8_8(0x10,(int) R10);
    myCAM.wrSensorReg8_8(0x45,(int) R45);
    
    uint8_t new13, new4, new10, new45;
    
    myCAM.rdSensorReg8_8(0x13,&new13);
    myCAM.rdSensorReg8_8(0x04,&new4);
    myCAM.rdSensorReg8_8(0x10,&new10);
    myCAM.rdSensorReg8_8(0x45,&new45);
  }
}

void setMode(int);

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

#if defined (OV2640_MINI_2MP) || defined (OV2640_CAM)
  while(1){
    //Check if the camera module type is OV2640
    myCAM.wrSensorReg8_8(0xff, 0x01);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_HIGH, &vid);
    myCAM.rdSensorReg8_8(OV2640_CHIPID_LOW, &pid);
    if ((vid != 0x26 ) && (( pid != 0x41 ) || ( pid != 0x42 ))){
      Log.trace(F("damn it ACK CMD Can't find OV2640 module! vid == %x, pid == %x"), vid, pid);
      delay(1000);
      continue;
    }
    else{
      Log.trace(F("ACK CMD OV2640 detected, how lucky you are"));
      return true;
    } 
  }
  // 5* not tested/dev'd yet but I assume it will need the above loop
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
    myCAM.OV2640_set_JPEG_size(OV2640_1024x768); //OV2640_320x240 OV2640_800x600 OV2640_640x480 OV2640_1024x768 OV2640_1600x1200
    connections.trace("init OV2640_640x480");
#elif defined (OV5640_MINI_5MP_PLUS) || defined (OV5640_CAM)
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5640_set_JPEG_size(OV5640_320x240);
    communication.trace("init OV5640_320x240");
#elif defined (OV5642_MINI_5MP_PLUS) || defined (OV5642_MINI_5MP) ||(defined (OV5642_CAM))
    myCAM.write_reg(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);   //VSYNC is active HIGH
    myCAM.OV5642_set_JPEG_size(OV5642_320x240);  
    connections.trace("init OV5642_320x240");
#endif
//When you do operate on camera, you should use 
myCAM.clear_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);
myCAM.set_bit(ARDUCHIP_GPIO,GPIO_PWDN_MASK);
//only set in day light shutterSpeed();
//set_Compress_quality(low_quality); // send high quality? 
myCAM.OV2640_set_Special_effects(BW);
//myCAM.OV2640_set_Brightness(Brightness2); 
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
    uint8_t byte = myCAM.read_reg(ARDUCHIP_TEST1);
    if(myCAM.read_reg(ARDUCHIP_TEST1) != 0x55){
      char text[62];
      snprintf(text, sizeof(text), "SPI interface Error! %x", byte); 
      connections.error(text);
      delay(200);
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
// grab this one too https://github.com/ArduCAM/Arduino/issues/77
void setMode(int val){
  
  return;
  /* great examples here...
  int temp = 0x84;
  Serial.println(val);
  switch (val)
  {
    case 0:
      myCAM.OV2640_set_JPEG_size(OV2640_160x120);
      delay(1000);
      Serial.println(F("ACK CMD switch to OV2640_160x120"));
    break;
    case 1:
      myCAM.OV2640_set_JPEG_size(OV2640_176x144);delay(1000);
      Serial.println(F("ACK CMD switch to OV2640_176x144"));
    break;
    case 2: 
      myCAM.OV2640_set_JPEG_size(OV2640_320x240);delay(1000);
      Serial.println(F("ACK CMD switch to OV2640_320x240"));
    break;
    case 3:
    myCAM.OV2640_set_JPEG_size(OV2640_352x288);delay(1000);
    Serial.println(F("ACK CMD switch to OV2640_352x288"));
    break;
    case 4:
      myCAM.OV2640_set_JPEG_size(OV2640_640x480);delay(1000);
      Serial.println(F("ACK CMD switch to OV2640_640x480"));
    break;
    case 5:
    myCAM.OV2640_set_JPEG_size(OV2640_800x600);delay(1000);
    Serial.println(F("ACK CMD switch to OV2640_800x600"));
    break;
    case 6:
     myCAM.OV2640_set_JPEG_size(OV2640_1024x768);delay(1000);
     Serial.println(F("ACK CMD switch to OV2640_1024x768"));
    break;
    case 7:
    myCAM.OV2640_set_JPEG_size(OV2640_1280x1024);delay(1000);
    Serial.println(F("ACK CMD switch to OV2640_1280x1024"));
    break;
    case 8:
    myCAM.OV2640_set_JPEG_size(OV2640_1600x1200);delay(1000);
    Serial.println(F("ACK CMD switch to OV2640_1600x1200"));
    break;
    case 31:
    myCAM.set_format(BMP);
    myCAM.InitCAM();
    #if !(defined (OV2640_MINI_2MP))        
    myCAM.clear_bit(ARDUCHIP_TIM, VSYNC_LEVEL_MASK);
    #endif
    myCAM.wrSensorReg16_8(0x3818, 0x81);
    myCAM.wrSensorReg16_8(0x3621, 0xA7);
    break;
    case 40:
    myCAM.OV2640_set_Light_Mode(Auto);
    Serial.println(F("ACK CMD Set to Auto"));break;
     case 41:
    myCAM.OV2640_set_Light_Mode(Sunny);
    Serial.println(F("ACK CMD Set to Sunny"));break;
     case 42:
    myCAM.OV2640_set_Light_Mode(Cloudy);
    Serial.println(F("ACK CMD Set to Cloudy"));break;
     case 43:
    myCAM.OV2640_set_Light_Mode(Office);
    Serial.println(F("ACK CMD Set to Office"));break;
   case 44:
    myCAM.OV2640_set_Light_Mode(Home);
   Serial.println(F("ACK CMD Set to Home"));break;
   case 45:
    myCAM.OV2640_set_Color_Saturation(Saturation2); 
     Serial.println(F("ACK CMD Set to Saturation+2"));break;
   case 46:
     myCAM.OV2640_set_Color_Saturation(Saturation1);
     Serial.println(F("ACK CMD Set to Saturation+1"));break;
   case 47:
    myCAM.OV2640_set_Color_Saturation(Saturation0); 
     Serial.println(F("ACK CMD Set to Saturation+0"));break;
    case 48:
    myCAM. OV2640_set_Color_Saturation(Saturation_1);
     Serial.println(F("ACK CMD Set to Saturation-1"));break;
    case 49:
     myCAM.OV2640_set_Color_Saturation(Saturation_2);
     Serial.println(F("ACK CMD Set to Saturation-2"));break; 
   case 51:
    myCAM.OV2640_set_Brightness(Brightness2); 
     Serial.println(F("ACK CMD Set to Brightness+2"));break;
   case 52:
     myCAM.OV2640_set_Brightness(Brightness1); 
     Serial.println(F("ACK CMD Set to Brightness+1"));break;
   case 53:
    myCAM.OV2640_set_Brightness(Brightness0); 
     Serial.println(F("ACK CMD Set to Brightness+0"));break;
    case 54:
    myCAM. OV2640_set_Brightness(Brightness_1); temp = 0xff;
     Serial.println(F("ACK CMD Set to Brightness-1"));break;
    case 55:
     myCAM.OV2640_set_Brightness(Brightness_2); temp = 0xff;
     Serial.println(F("ACK CMD Set to Brightness-2"));break; 
    case 56:
      myCAM.OV2640_set_Contrast(Contrast2);temp = 0xff;
     Serial.println(F("ACK CMD Set to Contrast+2"));break; 
    case 57:
      myCAM.OV2640_set_Contrast(Contrast1);temp = 0xff;
     Serial.println(F("ACK CMD Set to Contrast+1"));break;
     case 58:
      myCAM.OV2640_set_Contrast(Contrast0);temp = 0xff;
     Serial.println(F("ACK CMD Set to Contrast+0"));break;
    case 59:
      myCAM.OV2640_set_Contrast(Contrast_1);temp = 0xff;
     Serial.println(F("ACK CMD Set to Contrast-1"));break;
   case 60:
      myCAM.OV2640_set_Contrast(Contrast_2);temp = 0xff;
     Serial.println(F("ACK CMD Set to Contrast-2"));break;
   case 61:
    myCAM.OV2640_set_Special_effects(Antique);temp = 0xff;
    Serial.println(F("ACK CMD Set to Antique"));break;
   case 62:
    myCAM.OV2640_set_Special_effects(Bluish);temp = 0xff;
    Serial.println(F("ACK CMD Set to Bluish"));break;
   case 63:
    myCAM.OV2640_set_Special_effects(Greenish);temp = 0xff;
    Serial.println(F("ACK CMD Set to Greenish"));break;  
   case 64:
    myCAM.OV2640_set_Special_effects(Reddish);temp = 0xff;
    Serial.println(F("ACK CMD Set to Reddish"));break;  
   case 65:
    myCAM.OV2640_set_Special_effects(BW);temp = 0xff;
    Serial.println(F("ACK CMD Set to BW"));break; 
  case 66:
    myCAM.OV2640_set_Special_effects(Negative);temp = 0xff;
    Serial.println(F("ACK CMD Set to Negative"));break; 
  case 67:
    myCAM.OV2640_set_Special_effects(BWnegative);temp = 0xff;
    Serial.println(F("ACK CMD Set to BWnegative"));break;   
   case 68:
    myCAM.OV2640_set_Special_effects(Normal);temp = 0xff;
    Serial.println(F("ACK CMD Set to Normal"));break;     
  }
  */
}
//send via mqtt, mqtt server will turn to B&W and see if there are changes ie motion, of so it will send it on
void Camera::captureAndSend(const char * name, const char * filename, Connections&connections){
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
  char topic[sizeof(State::name)+20];
  snprintf(topic, sizeof(topic), "%s.%s", name, filename);
  char path[sizeof(State::name)*2]; //bugbug todo get these sizes right
  snprintf(path, sizeof(path), "%s.jpg", filename);

  // let reader know we are coming so they can start saving data
  DynamicJsonBuffer jsonBuffer;
  JsonObject& JSONencoder = jsonBuffer.createObject();
  JSONencoder["path"] = path; // also name of topic with data in messages further on
  JSONencoder["datatopic"] = topic;
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
        //datacam1 each device needs its own subscription put in state etc, like use name
        connections.sendToMQTT("datacam1", buf, i+1); // will need to namic topic like "Cam1" kind of things once working bugbug
        // now send notice
        // send MQTT start xfer message
        DynamicJsonBuffer jsonBuffer;
        JsonObject& JSONencoder = jsonBuffer.createObject();
        JSONencoder["path"] = path; // also name of topic with data
        JSONencoder["len"] = total; // server can compare len with actual data lenght to make sure data was not lost
        JSONencoder["datatopic"] = topic;
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
          connections.sendToMQTT("datacam1", buf, i+1);
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
    delayMicroseconds(15);
  } 
  delete buf;
}
Camera camera = Camera();

