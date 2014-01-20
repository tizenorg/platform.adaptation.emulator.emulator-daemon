/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * SooYoung Ha <yoosah.ha@samsnung.com>
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

#include "emuld.h"
#include "emuld_common.h"
#include <vconf/vconf.h>
#include <vconf/vconf-keys.h>
#include "log.h"

static int inline get_message(char* message, int status, int buf_len, bool is_evdi)
{
    if (is_evdi) {
        sprintf(message, "%d", status);
        return strlen(message);
    } else {
        // int to byte
        message[3] = (char) (status & 0xff);
        message[2] = (char) (status >> 8 & 0xff);
        message[1] = (char) (status >> 16 & 0xff);
        message[0] = (char) (status >> 24 & 0xff);
        message[4] = '\0';
    }

    return 4;
}

static int inline get_status(const char* filename)
{
    int ret;
    int status = 0;
    FILE* fd = fopen(filename, "r");
    if(!fd)
        return -1;

    ret = fscanf(fd, "%d", &status);
    fclose(fd);

    if (ret < 0) {
        return ret;
    }

    return status;
}

static int inline get_file_status(char* msg, const char* filename, int buf_len, bool is_evdi)
{
    int status = get_status(filename);
    if (status < 0)
        return status;
    return get_message(msg, status, buf_len, is_evdi);
}

static int inline get_vconf_status(char* msg, const char* key, int buf_len, bool is_evdi)
{
    int status;
    int ret = vconf_get_int(key, &status);
    if (ret != 0) {
        //LOG("cannot get vconf key - %s", key);
        return -1;
    }

    return get_message(msg, status, buf_len, is_evdi);
}

char* __tmpalloc(const int size)
{
    char* message = (char*)malloc(sizeof(char) * size);
    memset(message, 0, sizeof(char) * size);
    return message;
}

void __tmpfree(char* message)
{
    if (message) {
        free(message);
        message = 0;
    }
}

char* get_usb_status(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_file_status(message, "/sys/devices/platform/jack/usb_online", 5, is_evdi);
    if (length < 0){
        //LOG("get usb status error - %d", length);
        length = 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = USB_STATUS;

    return message;
}

char* get_earjack_status(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_file_status(message, "/sys/devices/platform/jack/earjack_online", 5, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = EARJACK_STATUS;

    return message;
}

char* get_rssi_level(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_vconf_status(message, "memory/telephony/rssi", 5, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = RSSI_LEVEL;

    return message;
}

char* get_battery_level(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_file_status(message, "/sys/class/power_supply/battery/capacity", 5, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = BATTERY_LEVEL;

    return message;
}

char* get_battery_charger(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_file_status(message, "/sys/class/power_supply/battery/charge_now", 5, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = BATTERY_CHARGER;

    return message;
}

char* get_proximity_status(void* p, bool is_evdi)
{
    char* message = __tmpalloc(5);
    int length = get_file_status(message, PATH_SENSOR_PROXI_VO, 5, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = PROXI_VALUE;

    return message;
}

char* get_light_level(void* p, bool is_evdi)
{
    char* message = __tmpalloc(6);
    int length = get_file_status(message, PATH_SENSOR_LIGHT_ADC, 6, is_evdi);
    if (length < 0){
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = LIGHT_VALUE;

    return message;
}

char* get_acceleration_value(void* p, bool is_evdi)
{
    FILE* fd = fopen(PATH_SENSOR_ACCEL_XYZ, "r");
    if(!fd)
    {
        return 0;
    }

    char* message = __tmpalloc(128);

    //fscanf(fd, "%d, %d, %d", message);
    if (!fgets(message, 128, fd))
        LOG("fgets failure");

    fclose(fd);

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = strlen(message);
    packet->group  = STATUS;
    packet->action = ACCEL_VALUE;

    return message;
}

char* get_gyroscope_value(void* p, bool is_evdi)
{
    int x, y, z;
    int ret;

    FILE* fd = fopen(PATH_SENSOR_GYRO_X_RAW, "r");
    if(!fd)
    {
        return 0;
    }
    ret = fscanf(fd, "%d", &x);
    fclose(fd);

    fd = fopen(PATH_SENSOR_GYRO_Y_RAW, "r");
    if(!fd)
    {
        return 0;
    }
    ret = fscanf(fd, "%d", &y);
    fclose(fd);

    fd = fopen(PATH_SENSOR_GYRO_Z_RAW, "r");
    if(!fd)
    {
        return 0;
    }
    ret = fscanf(fd, "%d", &z);
    fclose(fd);

    char* message = __tmpalloc(128);

    ret = sprintf(message, "%d, %d, %d", x, y, z);
    if (ret < 0) {
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = strlen(message);
    packet->group  = STATUS;
    packet->action = GYRO_VALUE;

    return message;
}

char* get_magnetic_value(void* p, bool is_evdi)
{
    FILE* fd = fopen(PATH_SENSOR_GEO_TESLA, "r");
    if(!fd)
    {
        return 0;
    }

    char* message = __tmpalloc(128);
    if (!fgets(message, 128, fd))
    {
        LOG("fgets failure");
    }
    fclose(fd);

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = strlen(message);
    packet->group  = STATUS;
    packet->action = MAG_VALUE;
    return message;
}

char* get_location_status(void* p, bool is_evdi)
{
    int mode;
    int ret = vconf_get_int("db/location/replay/ReplayMode", &mode);
    if (ret != 0) {
        return 0;
    }

    char* message = 0;

    if (mode == 0)
    { // STOP
        message = (char*)malloc(5);
        memset(message, 0, 5);

        ret = sprintf(message, "%d", mode);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    }
    else if (mode == 1)
    { // NMEA MODE(LOG MODE)
        char* temp = 0;
        temp = (char*) vconf_get_str("db/location/replay/FileName");
        if (temp == 0) {
            //free(temp);
            return 0;
        }

        message = (char*)malloc(256);
        memset(message, 0, 256);
        ret = sprintf(message, "%d,%s", mode, temp);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    } else if (mode == 2) { // MANUAL MODE
        double latitude;
        double logitude;
        ret = vconf_get_dbl("db/location/replay/ManualLatitude", &latitude);
        if (ret != 0) {
            return 0;
        }
        ret = vconf_get_dbl("db/location/replay/ManualLongitude", &logitude);
        if (ret != 0) {
            return 0;
        }
        message = (char*)malloc(128);
        memset(message, 0, 128);
        ret = sprintf(message, "%d,%f,%f", mode, latitude, logitude);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = strlen(message);
    packet->group  = STATUS;
    packet->action = LOCATION_STATUS;

    return message;
}

char* get_nfc_status(void* p, bool is_evdi)
{
    int ret;
    FILE* fd = fopen(PATH_NFC_DATA, "r");
    if(!fd)
    {
        return 0;
    }

    char* message = __tmpalloc(5000);
    ret = fscanf(fd, "%s\n", message);
    fclose(fd);
    if (ret < 0)
    {
        __tmpfree(message);
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = strlen(message);
    packet->group  = STATUS;
    packet->action = NFC_STATUS;

    return message;
}


