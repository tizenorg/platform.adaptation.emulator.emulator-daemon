/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
 * SooYoung Ha <yoosah.ha@samsnung.com>
 * Sungmin Ha <sungmin82.ha@samsung.com>
 * Daiyoung Kim <daiyoung777.kim@samsung.com>
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


#ifndef __emuld_common_h__
#define __emuld_common_h__


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>

// define group id
// return value to the injector
#define STATUS              15

// define action id
#define BATTERY_LEVEL       100
#define BATTERY_CHARGER     101
#define USB_STATUS          102
#define EARJACK_STATUS      103
#define RSSI_LEVEL          104

#define ACCEL_VALUE         110
#define GYRO_VALUE          111
#define MAG_VALUE           112
#define LIGHT_VALUE         113
#define PROXI_VALUE         114
#define MOTION_VALUE        115

#define LOCATION_STATUS     120

#define NFC_STATUS          121

#define PATH_SENSOR_ACCEL_XYZ   "/sys/devices/virtual/sensor/accel/xyz"
#define PATH_SENSOR_PROXI_VO    "/sys/devices/virtual/sensor/proxi/vo"
#define PATH_SENSOR_LIGHT_ADC   "/sys/devices/virtual/sensor/light/adc"
#define PATH_SENSOR_GYRO_X_RAW  "/sys/devices/virtual/sensor/gyro/gyro_x_raw"
#define PATH_SENSOR_GYRO_Y_RAW  "/sys/devices/virtual/sensor/gyro/gyro_y_raw"
#define PATH_SENSOR_GYRO_Z_RAW  "/sys/devices/virtual/sensor/gyro/gyro_z_raw"
#define PATH_SENSOR_GEO_TESLA   "/sys/devices/virtual/sensor/geo/tesla"
#define PATH_NFC_DATA           "/sys/devices/virtual/network/nfc/data"

struct LXT_MESSAGE// lxt_message
{
    unsigned short length;
    unsigned char group;
    unsigned char action;
    void *data;
};

typedef struct LXT_MESSAGE LXT_MESSAGE;

// Device
char* get_battery_level(void* , bool);
char* get_battery_charger(void* , bool);
char* get_usb_status(void* , bool);
char* get_earjack_status(void* , bool);
char* get_rssi_level(void* , bool);

// Sensor
char* get_proximity_status(void* , bool);
char* get_light_level(void* , bool);
char* get_acceleration_value(void* , bool);
char* get_gyroscope_value(void* , bool);
char* get_magnetic_value(void* , bool);

// Location
char* get_location_status(void* , bool);

// NFC
char* get_nfc_status(void* , bool);

struct _auto_mutex
{
    _auto_mutex(pthread_mutex_t* t)
    {
        _mutex = t;
        pthread_mutex_lock(_mutex);

    }
    ~_auto_mutex()
    {
        pthread_mutex_unlock(_mutex);
    }

    pthread_mutex_t* _mutex;
};

#endif //
