/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: 
 * Sungmin Ha <sungmin82.ha@samsung.com>
 * YeongKyoon Lee <yeongkyoon.lee@samsung.com>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * Contributors:
 * - S-Core Co., Ltd
 *
 */

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

#define NFC_STATUS 121

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

// NFC
char* get_nfc_status(void* );
