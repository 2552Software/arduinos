#ifndef _STATE_
#define _STATE_

#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <SPIFFS.h>

// saved configuration etc, can be set via mqtt, can be unique for each device. Must be set each time a new OS is installed as data is stored on the device for things like pwd so pwd is never 
// stored in open source
static const int MAX_PWD_LEN = 16;
static const int MAX_SSID_LEN = 16;
static const char* SSID = "SAM-Home"; // create a SSID of WIFI1 outside of this env. to use 3rd party wifi like mobile access point or such. If no such SSID is created we will create one here
static const char* PWD = NULL; // never set here, set in the run time that stores config, todo bugbug make api to change

class State {
public:
	State() { setDefault(); }
	void setup();
	void powerSleep(); // sleep when its ok to save power
	char name[MAX_SSID_LEN];  // everyone needs a unique name, defaults to esp id
	char ssid[MAX_SSID_LEN];
	char password[MAX_PWD_LEN];
	char other[16];
	uint32_t priority;
	int sleepTimeS;
	void set();
	void get();
	void setDefault();
	void echo();
private:
	const char *defaultConfig = "/config.json";
};

#endif
