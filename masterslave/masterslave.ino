#include <Wire.h>
#define SDA_PIN 21
#define SCL_PIN 22

#define SDA_VIDEO_PIN 18
#define SCL_VIDEO_PIN 19
//TwoWire Wire2(1);

void setup()
{
  Serial.begin(9600);
  Serial.print("\nSign on SDA is "); 
  Serial.print(SDA_PIN); 
  Serial.print(", SCL is "); 
  Serial.println(SCL_PIN); 
  
  Wire.begin(SDA_PIN, SCL_PIN);
}

  byte x = 0;

void loop()
{

  Wire.beginTransmission(8); // transmit to device #8, 8 is always an ESP32
  Wire.write(x);              // sends one byte
  Wire.endTransmission();    // stop transmitting

  x++;
  delay(10);
  Serial.flush();
  Wire.requestFrom(8, 1);   

  while (Wire.available()) { // slave may send less than requested
    byte b = Wire.read(); // receive a byte as character
    Serial.print(b);      
  }
}


/*

#define SDA_VIDEO_PIN 18
#define SCL_VIDEO_PIN 19
TwoWire Wire2(1);
void setup()
{
  Serial.begin(9600);
  Serial.println("Sign on\n");
  Wire2.begin(SDA_VIDEO_PIN, SCL_VIDEO_PIN);

  Serial.println("\nI2C Scanner");
}


void loop()
{
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  nDevices = 0;
  for(address = 1; address < 127; address++ ) 
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire2.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print("I2C device found at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.print(address,HEX);
      Serial.println("  !");

      nDevices++;
    }
    else if (error==4) 
    {
      Serial.print("Unknown error at address 0x");
      if (address<16) 
        Serial.print("0");
      Serial.println(address,HEX);
    }    
  }
  if (nDevices == 0)
    Serial.println("No I2C devices found\n");
  else
    Serial.println("done\n");

  delay(5000);           // wait 5 seconds for next scan
}
*/
