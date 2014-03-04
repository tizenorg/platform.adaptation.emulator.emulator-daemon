/* 
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
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
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pthread.h>
#include <stdarg.h>

#define SENSOR_DEBUG

#include "emuld.h"
#include "emuld_proc.h"

static int battery_level = 50;
pthread_t d_tid[16];

struct appdata
{
    void* data;
};
struct appdata ad;

typedef struct
{
    int len;
    int repeatCnt;
    int fileCnt;
    char *buffer;
} FileInput_args;

typedef struct
{
    char filename[256];
} FileInput_files;

enum sensor_type{
    MOTION = 6,
    USBKEYBOARD = 7,
    BATTERYLEVEL = 8,
    EARJACK = 9,
    USB = 10,
    RSSI = 11,
    FILE_ACCEL = 14,
    FILE_MAGNETIC = 15,
    FILE_GYRO = 16
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

#define PATH_SENSOR_ACCEL_XYZ       "/sys/devices/virtual/sensor/accel/xyz"
#define PATH_SENSOR_PROXI_VO        "/sys/devices/virtual/sensor/proxi/vo"
#define PATH_SENSOR_LIGHT_ADC       "/sys/devices/virtual/sensor/light/adc"
#define PATH_SENSOR_LIGHT_LEVEL     "/sys/devices/virtual/sensor/light/level"
#define PATH_SENSOR_GEO_RAW         "/sys/devices/virtual/sensor/geo/raw"
#define PATH_SENSOR_GEO_TESLA       "/sys/devices/virtual/sensor/geo/tesla"
#define PATH_SENSOR_GYRO_X_RAW      "/sys/devices/virtual/sensor/gyro/gyro_x_raw"
#define PATH_SENSOR_GYRO_Y_RAW      "/sys/devices/virtual/sensor/gyro/gyro_y_raw"
#define PATH_SENSOR_GYRO_Z_RAW      "/sys/devices/virtual/sensor/gyro/gyro_z_raw"

int check_nodes();

int parse_motion_data(int len, char *buffer);
int parse_usbkeyboard_data(int len, char *buffer);
int parse_batterylevel_data(int len, char *buffer);
int parse_earjack_data(int len, char *buffer);
int parse_usb_data(int len, char *buffer);
int parse_rssi_data(int len, char *buffer);

static void system_msg(const char* msg)
{
    int ret = system(msg);
    if (ret == -1) {
        LOGERR("system command is failed: %s", msg);
    }
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
        system_msg(command);
    //  memset(command, '\0', sizeof(command));
    //  sprintf(command, "vconftool set -t int memory/private/sensor/800004 %d -i -f", SENSOR_MOTION_DOUBLETAP_NONE);
    //  system_msg(command);
        break;
    case 2: // shake start
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_DETECTED);
        system_msg(command);
        memset(command, '\0', sizeof(command));
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_CONTINUING);
        system_msg(command);
        break;
    case 3: // shake stop
        sprintf(command, "vconftool set -t int memory/private/sensor/800002 %d -i -f", SENSOR_MOTION_SHAKE_FINISHED);
        system_msg(command);
        break;
    case 4: // snap x+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_X);
        system_msg(command);
        break;
    case 5: // snap x-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_X);
        system_msg(command);
        break;
    case 6: // snap y+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_Y);
        system_msg(command);
        break;
    case 7: // snap y-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_Y);
        system_msg(command);
        break;
    case 8: // snap z+
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_Z);
        system_msg(command);
        break;
    case 9: // snap z-
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_Z);
        system_msg(command);
        break;
    case 10: // snap left
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_NEGATIVE_X);
        system_msg(command);
        break;
    case 11: // snap right
        sprintf(command, "vconftool set -t int memory/private/sensor/800001 %d -i -f", SENSOR_MOTION_SNAP_POSITIVE_X);
        system_msg(command);
        break;
    case 12: // move to call (direct call)
        sprintf(command, "vconftool set -t int memory/private/sensor/800020 %d -i -f", SENSOR_MOTION_MOVE_MOVETOCALL);
        system_msg(command);
        break;
    default:
        LOGERR("not supported activity");
        break;
    }

    return 0;
}

int parse_usbkeyboard_data(int len, char *buffer)
{
    int len1=0;
    char tmpbuf[255];
    int x;

    LOGDEBUG("read data: %s", buffer);

    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    x = atoi(tmpbuf);

    if(x == 1)
    {
        system_msg("udevadm trigger --subsystem-match=input --sysname-match=event3 --action=add");
    }
    else if(x == 0)
    {
        system_msg("udevadm trigger --subsystem-match=input --sysname-match=event3 --action=remove");
    }
    else
        assert(0);

    return 0;
}

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

        fd = fopen("/sys/class/power_supply/battery/capacity", "w");
        if(!fd)
        {
            LOGERR("fopen fail");
            return -1;
        }
        fprintf(fd, "%d", level);
        fclose(fd);

        fd = fopen("/sys/devices/platform/jack/charger_online", "r");
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

        fd = fopen("/sys/class/power_supply/battery/charge_full", "w");
        if(!fd)
        {
            LOGERR("charge_full fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charge_full);
        fclose(fd);

        if(charger_online == 1)
        {
            fd = fopen("/sys/class/power_supply/battery/charge_now", "w");
            if(!fd)
            {
                LOGERR("charge_now fopen fail");
                return -1;
            }
            fprintf(fd, "%d", charger);
            fclose(fd);
        }

        // because time based polling
        system_msg("/usr/bin/sys_event device_charge_chgdet");
    }
    else if(id == 2)
    {
        /* second data */
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        charger = atoi(tmpbuf);
        fd = fopen("/sys/devices/platform/jack/charger_online", "w");
        if(!fd)
        {
            LOGERR("charger_online fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charger);
        fclose(fd);

        fd = fopen("/sys/class/power_supply/battery/charge_full", "w");
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

        system_msg("/usr/bin/sys_event device_charge_chgdet");

        fd = fopen("/sys/class/power_supply/battery/charge_now", "w");
        if(!fd)
        {
            LOGERR("charge_now fopen fail");
            return -1;
        }
        fprintf(fd, "%d", charger);
        fclose(fd);

        // because time based polling
        system_msg("/usr/bin/sys_event device_ta_chgdet");
    }

    return 0;
}

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

    fd = fopen("/sys/devices/platform/jack/earjack_online", "w");
    if(!fd)
    {
        LOGERR("earjack_online fopen fail");
        return -1;
    }
    fprintf(fd, "%d", x);
    fclose(fd);

    // because time based polling
    system_msg("/usr/bin/sys_event device_earjack_chgdet");

    return 0;
}

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

    fd = fopen("/sys/devices/platform/jack/usb_online", "w");
    if(!fd)
    {
        LOGERR("usb_online fopen fail");
        return -1;
    }
    fprintf(fd, "%d", x);
    fclose(fd);

    // because time based polling
    system_msg("/usr/bin/sys_event device_usb_chgdet");
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
    system_msg(command);

    return 0;
}

void* file_input_accel(void* param)
{
    FILE* srcFD;
    FILE* dstFD;

    int len1 = 0, fileCnt = 0, repeatCnt = 0, prevTime = 0, nextTime = 0, sleepTime = 0, x, y, z;
    int waitCnt = 0;
    double g = 9.80665;

    char tmpbuf[255];
    char token[] = ",";
    char* ret = NULL;
    char command[255];
    char lineData[1024];

    FileInput_args* args = (FileInput_args*)param;
    FileInput_files fname[args->fileCnt];
    memset(fname, '\0', sizeof(fname));

    pthread_detach(pthread_self());

    LOGINFO("file_input_accel start");

    // save file names
    for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
    {
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(args->buffer+args->len, 0x0a, tmpbuf);
        args->len += len1;

        strcpy(fname[fileCnt].filename, tmpbuf);
        LOGINFO("saved file name: %s", fname[fileCnt].filename);
    }

    // play files
    for(repeatCnt = 0; repeatCnt < args->repeatCnt; repeatCnt++)
    {
        for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
        {
            memset(command, '\0', sizeof(command));
            sprintf(command, "/tmp/accel/InputFiles/%s", fname[fileCnt].filename);
            command[strlen(command) - 1] = 0x00;    // erase '\n' for fopen
            LOGINFO("fopen command: %s", command);

            waitCnt = 0;
            while(access(command, F_OK) != 0 && waitCnt < 3)
            {
                usleep(10000);
                waitCnt++;
            }

            srcFD = fopen(command, "r");
            if(!srcFD)
            {
                LOGINFO("fopen fail");
                pthread_exit((void *) 0);
            }

            prevTime = 0;
            nextTime = 0;

            memset(lineData, '\0', sizeof(lineData));
            while(fgets(lineData, 1024, srcFD) != NULL)
            {
                ret = strtok(lineData, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    nextTime = prevTime + 1;
                }
                else
                    nextTime = atoi(ret);

                sleepTime = (nextTime - prevTime) * 10000;
                if(sleepTime < 0)
                {
                    sleepTime = 10000;
                    nextTime = prevTime + 1;
                }

                usleep(sleepTime);  // convert millisecond
                prevTime = nextTime;

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("x data is NULL");
                    x = 0;
                }
                else
                    x = (int)(atof(ret) * g * -100000);

                if (x > 1961330)
                    x = 1961330;
                if (x < -1961330)
                    x = -1961330;
                LOGINFO("x: %d", x);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("y data is NULL");
                    y = 0;
                }
                else
                    y = (int)(atof(ret) * g * -100000);

                if (y > 1961330)
                    y = 1961330;
                if (y < -1961330)
                    y = -1961330;
                LOGINFO("y: %d", y);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    z = 0;
                }
                else
                    z = (int)(atof(ret) * g * -100000);

                if (z > 1961330)
                    z = 1961330;
                if (z < -1961330)
                    z = -1961330;
                LOGINFO("z: %d", z);

                dstFD = fopen("/opt/sensor/accel/xyz", "w");
                if(!dstFD)
                {
                    LOGINFO("fopen fail");
                    pthread_exit((void *) 0);
                }
                fprintf(dstFD, "%d, %d, %d",x, y, z);
                fclose(dstFD);
            }

            fclose(srcFD);
        }
    }

    LOGINFO("thread exit");
    system_msg("rm -rf /tmp/accel/InputFiles/*");

    pthread_exit((void *) 0);
}

int parse_file_accel_data(int len, char *buffer)
{
    int len1=0, repeat = -1;
    char tmpbuf[255];

    LOGDEBUG("read data: %s", buffer);

    // read start/stop
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    repeat = atoi(tmpbuf);

    pthread_cancel(d_tid[0]);

    if(repeat == 0) // stop
    {
        system_msg("rm -rf /tmp/accel/InputFiles/*");
    }
    else            // start
    {
        // read file count
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        FileInput_args args;
        args.len = len;
        args.repeatCnt = repeat;
        args.fileCnt = atoi(tmpbuf);
        args.buffer = buffer;
        if(pthread_create(&d_tid[0], NULL, file_input_accel, &args) != 0) {
            LOGERR("pthread create fail!");
        }
    }

    return 0;
}

void* file_input_magnetic(void* param)
{
    FILE* srcFD;
    FILE* dstFD;

    int len1 = 0, fileCnt = 0, repeatCnt = 0, prevTime = 0, nextTime = 0, sleepTime = 0, x, y, z;
    int waitCnt = 0;

    char tmpbuf[255];
    char token[] = ",";
    char* ret = NULL;
    char command[255];
    char lineData[1024];
    FileInput_args* args = (FileInput_args*) param;
    FileInput_files fname[args->fileCnt];
    memset(fname, '\0', sizeof(fname));

    pthread_detach(pthread_self());

    LOGINFO("file_input_magnetic start");

    // save file names
    for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
    {
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(args->buffer+args->len, 0x0a, tmpbuf);
        args->len += len1;

        strcpy(fname[fileCnt].filename, tmpbuf);
        LOGINFO("saved file name: %s", fname[fileCnt].filename);
    }

    // play files
    for(repeatCnt = 0; repeatCnt < args->repeatCnt; repeatCnt++)
    {
        for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
        {
            memset(command, '\0', sizeof(command));
            sprintf(command, "/tmp/geo/InputFiles/%s", fname[fileCnt].filename);
            command[strlen(command) - 1] = 0x00;    // erase '\n' for fopen
            LOGINFO("fopen command: %s", command);

            waitCnt = 0;
            while(access(command, F_OK) != 0 && waitCnt < 3)
            {
                usleep(10000);
                waitCnt++;
            }

            srcFD = fopen(command, "r");
            if(!srcFD)
            {
                LOGERR("fopen fail");
                pthread_exit((void *) 0);
            }

            prevTime = 0;
            nextTime = 0;

            memset(lineData, '\0', sizeof(lineData));
            while(fgets(lineData, 1024, srcFD) != NULL)
            {
                ret = strtok(lineData, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    nextTime = prevTime + 1;
                }
                else
                    nextTime = atoi(ret);

                sleepTime = (nextTime - prevTime) * 10000;
                if(sleepTime < 0)
                {
                    sleepTime = 10000;
                    nextTime = prevTime + 1;
                }

                usleep(sleepTime);  // convert millisecond
                prevTime = nextTime;

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("x data is NULL");
                    x = 0;
                }
                else
                    x = atoi(ret);

                if (x > 2000)
                    x = 2000;
                if (x < -2000)
                    x = -2000;
                LOGINFO("x: %d", x);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("y data is NULL");
                    y = 0;
                }
                else
                    y = atoi(ret);

                if (y > 2000)
                    y = 2000;
                if (y < -2000)
                    y = -2000;
                LOGINFO("y: %d", y);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    z = 0;
                }
                else
                    z = atoi(ret);

                if (z > 2000)
                    z = 2000;
                if (z < -2000)
                    z = -2000;
                LOGINFO("z: %d", z);

                dstFD = fopen(PATH_SENSOR_GEO_TESLA, "w");
                if(!dstFD)
                {
                    LOGINFO("fopen fail");
                    pthread_exit((void *) 0);
                }
                fprintf(dstFD, "%d %d %d",x, y, z);
                fclose(dstFD);
            }

            fclose(srcFD);
        }
    }

    LOGINFO("thread exit");
    system_msg("rm -rf /tmp/geo/InputFiles/*");

    pthread_exit((void *) 0);
}

int parse_file_magnetic_data(int len, char *buffer)
{
    int len1=0, repeat = -1;
    char tmpbuf[255];

    LOGDEBUG("read data: %s", buffer);

    // read start/stop
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    repeat = atoi(tmpbuf);

    pthread_cancel(d_tid[1]);

    if(repeat == 0) // stop
    {
        system_msg("rm -rf /tmp/geo/InputFiles/*");
    }
    else            // start
    {
        // read file count
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        FileInput_args args;
        args.len = len;
        args.repeatCnt = repeat;
        args.fileCnt = atoi(tmpbuf);
        args.buffer = buffer;
        if(pthread_create(&d_tid[1], NULL, file_input_magnetic, &args) != 0)
                LOGINFO("pthread create fail!");
    }

    return 0;
}

void* file_input_gyro(void* param)
{
    FILE* srcFD;
    FILE* dstFD;

    int len1 = 0, fileCnt = 0, repeatCnt = 0, prevTime = 0, nextTime = 0, sleepTime = 0, x, y, z;
    int waitCnt = 0;

    char tmpbuf[255];
    char token[] = ",";
    char* ret = NULL;
    char command[255];
    char lineData[1024];
    FileInput_args* args = (FileInput_args*) param;
    FileInput_files fname[args->fileCnt];
    memset(fname, '\0', sizeof(fname));

    pthread_detach(pthread_self());

    LOGINFO("file_input_gyro start");

    // save file names
    for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
    {
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(args->buffer+args->len, 0x0a, tmpbuf);
        args->len += len1;

        strcpy(fname[fileCnt].filename, tmpbuf);
        LOGINFO("saved file name: %s", fname[fileCnt].filename);
    }

    // play files
    for(repeatCnt = 0; repeatCnt < args->repeatCnt; repeatCnt++)
    {
        for(fileCnt = 0; fileCnt < args->fileCnt; fileCnt++)
        {
            memset(command, '\0', sizeof(command));
            sprintf(command, "/tmp/gyro/InputFiles/%s", fname[fileCnt].filename);
            command[strlen(command) - 1] = 0x00;    // erase '\n' for fopen
            LOGINFO("fopen command: %s", command);

            waitCnt = 0;
            while(access(command, F_OK) != 0 && waitCnt < 3)
            {
                usleep(10000);
                waitCnt++;
            }

            srcFD = fopen(command, "r");
            if(!srcFD)
            {
                LOGINFO("fopen fail");
                pthread_exit((void *) 0);
            }

            prevTime = 0;
            nextTime = 0;

            memset(lineData, '\0', sizeof(lineData));
            while(fgets(lineData, 1024, srcFD) != NULL)
            {
                ret = strtok(lineData, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    nextTime = prevTime + 1;
                }
                else
                    nextTime = atoi(ret);

                sleepTime = (nextTime - prevTime) * 10000;
                if(sleepTime < 0)
                {
                    sleepTime = 10000;
                    nextTime = prevTime + 1;
                }

                usleep(sleepTime);  // convert millisecond
                prevTime = nextTime;

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("x data is NULL");
                    x = 0;
                }
                else
                    x = (int)(atof(ret) * 1000)/17.50;

                if (x > 571)
                    x = 571;
                if (x < -571)
                    x = -571;
                LOGINFO("x: %d", x);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("y data is NULL");
                    y = 0;
                }
                else
                    y = (int)(atof(ret) * 1000)/17.50;

                if (y > 571)
                    y = 571;
                if (y < -571)
                    y = -571;
                LOGINFO("y: %d", y);

                ret = strtok(NULL, token);
                if(!ret)
                {
                    LOGINFO("data is NULL");
                    z = 0;
                }
                else
                    z = (int)(atof(ret) * 1000)/17.50;

                if (z > 571)
                    z = 571;
                if (z < -571)
                    z = -571;
                LOGINFO("z: %d", z);

                dstFD = fopen("/opt/sensor/gyro/gyro_x_raw", "w");
                if(!dstFD)
                {
                    LOGINFO("fopen fail");
                    pthread_exit((void *) 0);
                }
                fprintf(dstFD, "%d",x);
                fclose(dstFD);

                dstFD = fopen("/opt/sensor/gyro/gyro_y_raw", "w");
                if(!dstFD)
                {
                    LOGINFO("fopen fail");
                    pthread_exit((void *) 0);
                }
                fprintf(dstFD, "%d",y);
                fclose(dstFD);

                dstFD = fopen("/opt/sensor/gyro/gyro_z_raw", "w");
                if(!dstFD)
                {
                    LOGINFO("fopen fail");
                    pthread_exit((void *) 0);
                }
                fprintf(dstFD, "%d",z);
                fclose(dstFD);
            }
            fclose(srcFD);
        }
    }

    LOGINFO("thread exit");
    system_msg("rm -rf /tmp/gyro/InputFiles/*");

    pthread_exit((void *) 0);
}

int parse_file_gyro_data(int len, char *buffer)
{
    int len1=0, repeat = -1;
    char tmpbuf[255];

    LOGDEBUG("read data: %s", buffer);

    // read start/stop
    memset(tmpbuf, '\0', sizeof(tmpbuf));
    len1 = parse_val(buffer+len, 0x0a, tmpbuf);
    len += len1;

    repeat = atoi(tmpbuf);

    pthread_cancel(d_tid[2]);

    if(repeat == 0) // stop
    {
        system_msg("rm -rf /tmp/gyro/InputFiles/*");
    }
    else            // start
    {
        // read file count
        memset(tmpbuf, '\0', sizeof(tmpbuf));
        len1 = parse_val(buffer+len, 0x0a, tmpbuf);
        len += len1;

        FileInput_args args;
        args.len = len;
        args.repeatCnt = repeat;
        args.fileCnt = atoi(tmpbuf);
        args.buffer = buffer;
        if(pthread_create(&d_tid[2], NULL, file_input_gyro, &args) != 0)
                LOGINFO("pthread create fail!");
    }

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
    case USBKEYBOARD:
        ret = parse_usbkeyboard_data(len, buffer);
        if(ret < 0)
            LOGERR("usbkeyboard parse error!");
        break;
    case BATTERYLEVEL:
        ret = parse_batterylevel_data(len, buffer);
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
    case FILE_ACCEL:
        ret = parse_file_accel_data(len, buffer);
        if(ret < 0)
            LOGERR("file_accel parse error!");
        break;
    case FILE_MAGNETIC:
        ret = parse_file_magnetic_data(len, buffer);
        if(ret < 0)
            LOGERR("file_magnetic parse error!");
        break;
    case FILE_GYRO:
        ret = parse_file_gyro_data(len, buffer);
        if(ret < 0)
            LOGERR("file_gyro parse error!");
        break;
    default:
        break;
    }
}
