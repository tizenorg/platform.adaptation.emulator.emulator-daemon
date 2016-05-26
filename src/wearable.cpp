/*
 * emulator-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song <ch81.song@samsung.com>
 * Jinhyung Choi <jinh0.choi@samsnung.com>
 * DaiYoung Kim <daiyoung777.kim@samsnung.com>
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

#include <stdio.h>

#include "emuld.h"
#include "wearable.h"

enum sensor_type{
    BATTERYLEVEL = 8,
    EARJACK = 9,
    USB = 10,
};

#define POWER_SUPPLY    "power_supply"
#define FULL            "Full"
#define CHARGING        "Charging"
#define DISCHARGING     "Discharging"
static void dbus_send_power_supply(int capacity, int charger)
{
    const char* power_device = POWER_SUPPLY;
    char state [16];
    char option [DBUS_MSG_BUF_SIZE];
    memset(state, 0, sizeof(state));

    if (capacity == 100 && charger == 1) {
        memcpy(state, FULL, 4);
    } else if (charger == 1) {
        memcpy(state, CHARGING, 8);
    } else {
        memcpy(state, DISCHARGING, 11);
    }

    snprintf(option, sizeof(option), "int32:5 string:\"%d\" string:\"%s\" string:\"Good\" string:\"%d\" string:\"1\"",
            capacity, state, (charger + 1));

    dbus_send(power_device, DBUS_SEND_SYSNOTI, option);
}

static void dbus_send_usb(int on)
{
    const char* usb_device = DEVICE_CHANGED;
    char option [DBUS_MSG_BUF_SIZE];

    snprintf(option, sizeof(option), "int32:2 string:\"usb\" string:\"%d\"", on);

    dbus_send_signal(usb_device, DBUS_SEND_EXTCON, option);
}

static void dbus_send_earjack(int on)
{
    const char* earjack_device = DEVICE_CHANGED;
    char option [DBUS_MSG_BUF_SIZE];

    snprintf(option, sizeof(option), "int32:2 string:\"earjack\" string:\"%d\"", on);

    dbus_send_signal(earjack_device, DBUS_SEND_EXTCON, option);
}

static int read_from_file(const char* file_name)
{
    int ret;
    FILE* fd;
    int value;

    fd = fopen(file_name, "r");
    if(!fd)
    {
        LOGERR("fopen fail: %s", file_name);
        return -1;
    }

    ret = fscanf(fd, "%d", &value);
    fclose(fd);
    if (ret <= 0) {
        LOGERR("failed to get value");
        return -1;
    }

    return value;
}

static void write_to_file(const char* file_name, int value)
{
    FILE* fd;

    fd = fopen(file_name, "w");
    if(!fd)
    {
        LOGERR("fopen fail: %s", file_name);
        return;
    }
    fprintf(fd, "%d", value);
    fclose(fd);
}

#define FILE_BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define FILE_BATTERY_CHARGER_ONLINE "/sys/devices/platform/jack/charger_online"

int set_battery_data(void)
{
    int charger_online = 0;
    int battery_level = 0;

    battery_level = read_from_file(FILE_BATTERY_CAPACITY);
    LOGINFO("battery level: %d", battery_level);
    if (battery_level < 0)
        return -1;

    charger_online = read_from_file(FILE_BATTERY_CHARGER_ONLINE);
    LOGINFO("charge_online: %d", charger_online);
    if (charger_online < 0)
        return -1;

    dbus_send_power_supply(battery_level, charger_online);

    return 0;
}

#define PATH_JACK_EARJACK "/sys/devices/platform/jack/earjack_online"
int parse_earjack_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;
    FILE* fd;

    LOGDEBUG("read data: %s", buffer);

    // read param count
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    /* first data */
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    x = atoi(tmpbuf);

    fd = fopen(PATH_JACK_EARJACK, "w");
    if(!fd)
    {
        LOGERR("earjack_online fopen fail");
        return -1;
    }
    fprintf(fd, "%d", x);
    fclose(fd);

    dbus_send_earjack(x);
    return 0;
}

#define FILE_USB_ONLINE "/sys/devices/platform/jack/usb_online"
int parse_usb_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;

    // read param count
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    /* first data */
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    x = atoi(tmpbuf);

    write_to_file(FILE_USB_ONLINE, x);

    // because time based polling
    dbus_send_usb(x);

    return 0;
}

static void setting_sensor(char *buffer)
{
    int len = 0;
    int ret = 0;
    char tmpbuf[255];

    LOGDEBUG("read data: %s", buffer);

    // read sensor type
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len = parse_val(buffer, 0x0a, tmpbuf);

    switch(atoi(tmpbuf))
    {
    case BATTERYLEVEL:
        ret = set_battery_data();
        if(ret < 0)
            LOGERR("batterylevel parse error!");
        break;
    case EARJACK:
        ret = parse_earjack_data(len, buffer);
        if(ret < 0)
            LOGERR("earjack parse error!");
        break;
    case USB:
        ret = parse_usb_data(len, buffer);
        if(ret < 0)
            LOGERR("usb parse error!");
        break;
    default:
        break;
    }
}

bool msgproc_sensor(ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_sensor");

    if (ijcmd->msg.group != STATUS)
    {
        if (ijcmd->data != NULL && strlen(ijcmd->data) > 0) {
            setting_sensor(ijcmd->data);
        }
    }
    return true;
}

void add_vconf_map_profile(void)
{
}

void add_msg_proc_ext(void)
{
    if (!msgproc_add(DEFAULT_MSGPROC, IJTYPE_SENSOR, &msgproc_sensor, MSGPROC_PRIO_MIDDLE))
    {
        LOGWARN("Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_SENSOR);
    }
}
