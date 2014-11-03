/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
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
#include <vconf/vconf.h>
#include <vconf/vconf-keys.h>

#include "emuld.h"
#include "mobile.h"

#define STATUS              15
#define RSSI_LEVEL          104

enum sensor_type{
    MOTION = 6,
    USBKEYBOARD = 7,
    BATTERYLEVEL = 8,
    EARJACK = 9,
    USB = 10,
    RSSI = 11,
};

enum motion_doubletap{
    SENSOR_MOTION_DOUBLETAP_NONE = 0,
    SENSOR_MOTION_DOUBLETAP_DETECTION = 1
};

enum motion_shake{
    SENSOR_MOTION_SHAKE_NONE = 0,
    SENSOR_MOTION_SHAKE_DETECTED = 1,
    SENSOR_MOTION_SHAKE_CONTINUING  = 2,
    SENSOR_MOTION_SHAKE_FINISHED = 3,
    SENSOR_MOTION_SHAKE_BREAK = 4
};

enum motion_snap{
    SENSOR_MOTION_SNAP_NONE = 0,
    SENSOR_MOTION_SNAP_NEGATIVE_X = 1,
    SENSOR_MOTION_SNAP_POSITIVE_X = 2,
    SENSOR_MOTION_SNAP_NEGATIVE_Y = 3,
    SENSOR_MOTION_SNAP_POSITIVE_Y = 4,
    SENSOR_MOTION_SNAP_NEGATIVE_Z = 5,
    SENSOR_MOTION_SNAP_POSITIVE_Z = 6,
    SENSOR_MOTION_SNAP_LEFT = SENSOR_MOTION_SNAP_NEGATIVE_X,
    SENSOR_MOTION_SNAP_RIGHT = SENSOR_MOTION_SNAP_POSITIVE_X
};

enum motion_move{
    SENSOR_MOTION_MOVE_NONE = 0,
    SENSOR_MOTION_MOVE_MOVETOCALL = 1
};

static void system_cmd(const char* msg)
{
    int ret = system(msg);
    if (ret == -1) {
        LOGERR("system command is failed: %s", msg);
    }
}

#define DBUS_SEND_CMD   "dbus-send --system --type=method_call --print-reply --reply-timeout=120000 --dest=org.tizen.system.deviced /Org/Tizen/System/DeviceD/SysNoti org.tizen.system.deviced.SysNoti."
static void dbus_send(const char* device, const char* option)
{
    const char* dbus_send_cmd = DBUS_SEND_CMD;
    char* cmd;

    if (device == NULL || option == NULL)
        return;

    cmd = (char*)malloc(512);
    if (cmd == NULL)
        return;

    memset(cmd, 0, 512);

    sprintf(cmd, "%s%s string:\"%s\" %s", dbus_send_cmd, device, device, option);

    system_cmd(cmd);
    LOGINFO("dbus_send: %s", cmd);

    free(cmd);
}

#define POWER_SUPPLY    "power_supply"
#define FULL            "Full"
#define CHARGING        "Charging"
#define DISCHARGING     "Discharging"
static void dbus_send_power_supply(int capacity, int charger)
{
    const char* power_device = POWER_SUPPLY;
    char state [16];
    char option [128];
    memset(state, 0, 16);
    memset(option, 0, 128);

    if (capacity == 100 && charger == 1) {
		memcpy(state, FULL, 4);
    } else if (charger == 1) {
		memcpy(state, CHARGING, 8);
    } else {
		memcpy(state, DISCHARGING, 11);
    }

    sprintf(option, "int32:5 string:\"%d\" string:\"%s\" string:\"Good\" string:\"%d\" string:\"1\"",
            capacity, state, (charger + 1));

    dbus_send(power_device, option);
}

#define DEVICE_CHANGED      "device_changed"

static void dbus_send_usb(int on)
{
    const char* usb_device = DEVICE_CHANGED;
    char option [128];
    memset(option, 0, 128);

    sprintf(option, "int32:2 string:\"usb\" string:\"%d\"", on);

    dbus_send(usb_device, option);
}

static void dbus_send_earjack(int on)
{
	const char* earjack_device = DEVICE_CHANGED;
    char option [128];
    memset(option, 0, 128);

    sprintf(option, "int32:2 string:\"earjack\" string:\"%d\"", on);

    dbus_send(earjack_device, option);
}

int parse_motion_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;
    char command[128];
    memset(command, '\0', sizeof(command));

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

    switch(x)
    {
    case 1: // double tap
        sprintf(command, "vconftool set -t int memory/private/sensor/800004 %d -i -f", SENSOR_MOTION_DOUBLETAP_DETECTION);
        systemcall(command);
    //  memset(command, '\0', sizeof(command));
    //  sprintf(command, "vconftool set -t int memory/private/sensor/800004 %d -i -f", SENSOR_MOTION_DOUBLETAP_NONE);
    //  systemcall(command);
        break;
    case 2: // shake start
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_DETECTED);
        systemcall(command);
        memset(command, '\0', sizeof(command));
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_CONTINUING);
        systemcall(command);
        break;
    case 3: // shake stop
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_FINISHED);
        systemcall(command);
        break;
    case 4: // snap x+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_X);
        systemcall(command);
        break;
    case 5: // snap x-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_X);
        systemcall(command);
        break;
    case 6: // snap y+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_Y);
        systemcall(command);
        break;
    case 7: // snap y-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_Y);
        systemcall(command);
        break;
    case 8: // snap z+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_Z);
        systemcall(command);
        break;
    case 9: // snap z-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_Z);
        systemcall(command);
        break;
    case 10: // snap left
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_X);
        systemcall(command);
        break;
    case 11: // snap right
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_X);
        systemcall(command);
        break;
    case 12: // move to call (direct call)
        sprintf(command, "vconftool set -t int memory/private/sensor/800020 %d -i -f", SENSOR_MOTION_MOVE_MOVETOCALL);
        systemcall(command);
        break;
    default:
        LOGERR("not supported activity");
        break;
    }

    return 0;
}

#define FILE_BATTERY_CAPACITY "/sys/class/power_supply/battery/capacity"
#define FILE_BATTERY_CHARGER_ONLINE "/sys/devices/platform/jack/charger_online"
#define FILE_BATTERY_CHARGE_FULL "/sys/class/power_supply/battery/charge_full"
#define FILE_BATTERY_CHARGE_NOW "/sys/class/power_supply/battery/charge_now"

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

#define PATH_JACK_EARJACK           "/sys/devices/platform/jack/earjack_online"
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

    // because time based polling
	//FIXME: change to dbus
    //system_cmd("/usr/bin/sys_event device_earjack_chgdet");
	dbus_send_earjack(x);
    return 0;
}

#define FILE_USB_ONLINE "/sys/devices/platform/jack/usb_online"
int parse_usb_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;

    #ifdef SENSOR_DEBUG
    LOG("read data: %s", buffer);
    #endif
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


int parse_rssi_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;
    char command[128];
    memset(command, '\0', sizeof(command));

    LOGINFO("read data: %s", buffer);

    // read param count
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    /* first data */
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    x = atoi(tmpbuf);

    sprintf(command, "vconftool set -t int memory/telephony/rssi %d -i -f", x);
    systemcall(command);

    return 0;
}

void setting_sensor(char *buffer)
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
    case MOTION:
        ret = parse_motion_data(len, buffer);
        if(ret < 0)
            LOGERR("motion parse error!");
        break;
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
    case RSSI:
        ret = parse_rssi_data(len, buffer);
        if(ret < 0)
            LOGERR("rssi parse error!");
        break;
    default:
        break;
    }
}


static int inline get_vconf_status(char* msg, const char* key, int buf_len)
{
    int status;
    int ret = vconf_get_int(key, &status);
    if (ret != 0) {
        LOGERR("cannot get vconf key - %s", key);
        return -1;
    }

    sprintf(msg, "%d", status);
    return strlen(msg);
}

char* __tmpalloc(const int size)
{
    char* message = (char*)malloc(sizeof(char) * size);
    memset(message, 0, sizeof(char) * size);
    return message;
}

char* get_rssi_level(void* p)
{
    char* message = __tmpalloc(5);
    int length = get_vconf_status(message, "memory/telephony/rssi", 5);
    if (length < 0){
        return 0;
    }

    LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
    memset(packet, 0, sizeof(LXT_MESSAGE));
    packet->length = length;
    packet->group  = STATUS;
    packet->action = RSSI_LEVEL;

    return message;
}

static void* getting_sensor(void* data)
{
    pthread_detach(pthread_self());

    setting_device_param* param = (setting_device_param*) data;

    if (!param)
        return 0;

    char* msg = 0;
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));

    switch(param->ActionID)
    {
        case RSSI_LEVEL:
            msg = get_rssi_level((void*)packet);
            if (msg == 0) {
                LOGERR("failed getting rssi level");
            }
            break;
        default:
            LOGERR("Wrong action ID. %d", param->ActionID);
        break;
    }

	if (msg == 0)
	{
		LOGDEBUG("send error message to injector");
		memset(packet, 0, sizeof(LXT_MESSAGE));
		packet->length = 0;
		packet->group = STATUS;
		packet->action = param->ActionID;
	}
	else
	{
		LOGDEBUG("send data to injector");
	}

	const int tmplen = HEADER_SIZE + packet->length;
	char* tmp = (char*) malloc(tmplen);
	if (tmp)
	{
		memcpy(tmp, packet, HEADER_SIZE);
		if (packet->length > 0)
			memcpy(tmp + HEADER_SIZE, msg, packet->length);

		ijmsg_send_to_evdi(g_fd[fdtype_device], param->type_cmd, (const char*) tmp, tmplen);

		free(tmp);
    }

    if(msg != 0)
    {
        free(msg);
        msg = 0;
    }
    if (packet)
    {
        free(packet);
        packet = NULL;
    }

    if (param)
        delete param;

    pthread_exit((void *) 0);
}

void msgproc_sensor(const int sockfd, ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_sensor");

    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return;

        memset(param, 0, sizeof(*param));

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[TID_SENSOR], NULL, getting_sensor, (void*)param) != 0)
        {
            LOGERR("sensor pthread create fail!");
            return;
        }

    }
    else
    {
        if (ijcmd->data != NULL && strlen(ijcmd->data) > 0) {
            setting_sensor(ijcmd->data);
        }
    }
}

