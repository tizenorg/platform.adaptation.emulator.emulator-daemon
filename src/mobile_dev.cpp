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

static int battery_level = 50;

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

#define PATH_BATTERY_CAPACITY       "sys/class/power_supply/battery/capacity"
#define PATH_BATTERY_CHARGER_ON     "/sys/devices/platform/jack/charger_online"
#define PATH_BATTERY_CHARGE_FULL    "/sys/class/power_supply/battery/charge_full"
#define PATH_BATTERY_CHARGE_NOW     "/sys/class/power_supply/battery/charge_now"

int parse_batterylevel_data(int len, char *buffer)
{
    int len1=0, id = 0, ret = 0;
    char tmpbuf[255];
    int level = 0, charger = 0, charger_online = 0, charge_full = 0;
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

    id = atoi(tmpbuf);
    if(id == 1) // level
    {
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        level = atoi(tmpbuf);
        battery_level = level;

        if(level == 100)
        {
            charger = 0;
        }
        else
        {
            charger = 1;
        }

        fd = fopen(PATH_BATTERY_CAPACITY, "w");
        if(!fd)
        {
            LOGERR("fopen fail");
            return -1;
        }
        fprintf(fd, "%d", level);
        fclose(fd);

        fd = fopen(PATH_BATTERY_CHARGER_ON, "r");
        if(!fd)
        {
            LOGERR("fopen fail");
            return -1;
        }
        ret = fscanf(fd, "%d", &charger_online);
        fclose(fd);
        if (ret < 0)
        {
            LOGERR("failed to get charger_online value");
            return -1;
        }

        LOGDEBUG("charge_online: %d", charger_online);

        if(charger_online == 1 && level == 100)
        {
            charge_full = 1;
        }
        else
        {
            charge_full = 0;
        }
        LOGDEBUG("charge_full: %d", charge_full);

        fd = fopen(PATH_BATTERY_CHARGE_FULL, "w");
        if(!fd)
        {
            LOGERR("charge_full fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charge_full);
        fclose(fd);

        if(charger_online == 1)
        {
            fd = fopen(PATH_BATTERY_CHARGE_NOW, "w");
            if(!fd)
            {
                LOGERR("charge_now fopen fail");
                return -1;
            }
            fprintf(fd, "%d", charger);
            fclose(fd);
        }

        // because time based polling
        systemcall("/usr/bin/sys_event device_charge_chgdet");
    }
    else if(id == 2)
    {
        /* second data */
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        charger = atoi(tmpbuf);
        fd = fopen(PATH_BATTERY_CHARGER_ON, "w");
        if(!fd)
        {
            LOGERR("charger_online fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charger);
        fclose(fd);

        fd = fopen(PATH_BATTERY_CHARGE_FULL, "w");
        if(!fd)
        {
            LOGERR("charge_full fopen fail");
            return -1;
        }

        if(battery_level == 100 && charger == 1)
        {
            fprintf(fd, "%d", 1);   // charge full
            charger = 0;
        }
        else
        {
            fprintf(fd, "%d", 0);
        }
        fclose(fd);

        systemcall("/usr/bin/sys_event device_charge_chgdet");

        fd = fopen(PATH_BATTERY_CHARGE_NOW, "w");
        if(!fd)
        {
            LOGERR("charge_now fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charger);
        fclose(fd);

        // because time based polling
        systemcall("/usr/bin/sys_event device_ta_chgdet");
    }

    return 0;
}

int parse_rssi_data(int len, char *buffer)
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

    sprintf(command, "vconftool set -t int memory/telephony/rssi %d -i -f", x);
    systemcall(command);

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
    systemcall("/usr/bin/sys_event device_earjack_chgdet");

    return 0;
}

#define PATH_JACK_USB               "/sys/devices/platform/jack/usb_online"
int parse_usb_data(int len, char *buffer)
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

    fd = fopen(PATH_JACK_USB, "w");
    if(!fd)
    {
        LOGERR("usb_online fopen fail");
        return -1;
    }
    fprintf(fd, "%d", x);
    fclose(fd);

    // because time based polling
    systemcall("/usr/bin/sys_event device_usb_chgdet");
    return 0;
}

void device_parser(char *buffer)
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
        ret = parse_batterylevel_data(len, buffer);
        if(ret < 0)
            LOGERR("batterylevel parse error!");
        break;
    case RSSI:
        ret = parse_rssi_data(len, buffer);
        if(ret < 0)
            LOGERR("rssi parse error!");
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

static void* setting_device(void* data)
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

        if (pthread_create(&tid[2], NULL, setting_device, (void*)param) != 0)
        {
            LOGERR("sensor pthread create fail!");
            return;
        }

    }
    else
    {
        if (ijcmd->data != NULL && strlen(ijcmd->data) > 0) {
            device_parser(ijcmd->data);
        }
    }
}

