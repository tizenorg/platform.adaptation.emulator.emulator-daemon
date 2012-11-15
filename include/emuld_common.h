#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

// define group id
// return value to the injector
#define STATUS 15

// define action id
#define BATTERY_LEVEL 100
#define BATTERY_CHARGER 101
#define USB_STATUS 102
#define EARJACK_STATUS 103
#define RSSI_LEVEL 104

#define ACCEL_VALUE 110
#define GYRO_VALUE 111
#define MAG_VALUE 112
#define LIGHT_VALUE 113
#define PROXI_VALUE 114
#define MOTION_VALUE 115

#define LOCATION_STATUS 120

struct LXT_MESSAGE// lxt_message
{
	unsigned short length;
	unsigned char group;
	unsigned char action;
	void *data; 
};

typedef struct LXT_MESSAGE LXT_MESSAGE;

// Device
char* get_battery_level(void* );
char* get_battery_charger(void* );
char* get_usb_status(void* );
char* get_earjack_status(void* );
char* get_rssi_level(void* );

// Sensor
char* get_proximity_status(void* );
char* get_light_level(void* );
char* get_acceleration_value(void* );
char* get_gyroscope_value(void* );
char* get_magnetic_value(void* );

// Location
char* get_location_status(void* );
