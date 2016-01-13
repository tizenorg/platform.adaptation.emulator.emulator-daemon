/*
 * emulator-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song <ch81.song@samsung.com>
 * Jinhyung Choi <jinh0.choi@samsnung.com>
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

#include <stdio.h>
#include "emuld.h"

#define LOCATION_STATUS     120

/*
 * Location function
 */
static char* get_location_status(void* p)
{
    int mode;
    int ret = vconf_get_int(VCONF_REPLAYMODE, &mode);
    if (ret != 0) {
        return 0;
    }

    char* message = 0;

    if (mode == 0)
    { // STOP
        message = (char*)malloc(5);
        memset(message, 0, 5);

        ret = snprintf(message, 5, "%d", mode);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    }
    else if (mode == 1)
    { // NMEA MODE(LOG MODE)
        char* temp = 0;
        temp = (char*) vconf_get_str(VCONF_FILENAME);
        if (temp == 0) {
            //free(temp);
            return 0;
        }

        message = (char*)malloc(256);
        memset(message, 0, 256);
        ret = snprintf(message, 256, "%d,%s", mode, temp);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    } else if (mode == 2) { // MANUAL MODE
        double latitude;
        double logitude;
        double altitude;
        double accuracy;
        ret = vconf_get_dbl(VCONF_MLATITUDE, &latitude);
        if (ret != 0) {
            return 0;
        }
        ret = vconf_get_dbl(VCONF_MLONGITUDE, &logitude);
        if (ret != 0) {
            return 0;
        }
        ret = vconf_get_dbl(VCONF_MALTITUDE, &altitude);
        if (ret != 0) {
            return 0;
        }
         ret = vconf_get_dbl(VCONF_MHACCURACY, &accuracy);
        if (ret != 0) {
            return 0;
        }

        message = (char*)malloc(128);
        memset(message, 0, 128);
        ret = snprintf(message, 128, "%d,%f,%f,%f,%f", mode, latitude, logitude, altitude, accuracy);
        if (ret < 0) {
            free(message);
            message = 0;
            return 0;
        }
    }

    if (message) {
        LXT_MESSAGE* packet = (LXT_MESSAGE*)p;
        memset(packet, 0, sizeof(LXT_MESSAGE));
        packet->length = strlen(message);
        packet->group  = STATUS;
        packet->action = LOCATION_STATUS;
        return message;
    } else {
        return NULL;
    }
}

static void* getting_location(void* data)
{
    pthread_detach(pthread_self());

    setting_device_param* param = (setting_device_param*) data;

    if (!param)
        return 0;

    char* msg = 0;
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));

    if (!package)
    {
        LOGERR("Failed to allocate packet");
        return NULL;
    }

    switch (param->ActionID)
    {
        case LOCATION_STATUS:
            msg = get_location_status((void*)packet);
            if (msg == 0) {
                LOGERR("failed getting location status");
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

    if (msg != 0)
    {
        free(msg);
        msg = 0;
    }

    free(packet);

    if (param)
        delete param;

    pthread_exit((void *) 0);
}

void setting_location(char* databuf)
{
    char *s = strchr(databuf, ',');
    int err = 0;
    char *saveptr;
    if (s == NULL) { // SET MODE
        int mode = atoi(databuf);

        // 0: STOP MODE, 1: NMEA_MODE (LOG MODE), 2: MANUAL MODE
        if (mode < 0 || mode > 2) {
            LOGERR("error(%s) : stop replay mode", databuf);
            mode = 0;
        }

        err = vconf_set_int(VCONF_REPLAYMODE, mode);
        LOGFAIL(err, "Set ReplayMode failed. mode = %d", mode);
    } else {
        *s = '\0';
        int mode = atoi(databuf);
        if (mode == 1) { // NMEA MODE (LOG MODE)
            err = vconf_set_str(VCONF_FILENAME, s+1);
            LOGFAIL(err, "Set FileName failed. name = %s", s+1);
            err = vconf_set_int(VCONF_REPLAYMODE, mode);
            LOGFAIL(err, "Set ReplayMode failed. mode = %d", mode);
        } else if (mode == 2) {
            char* ptr = strtok_r(s+1, ",", &saveptr);
            double value = 0.0;

            // Latitude
            value = atof(ptr);
            err = vconf_set_dbl(VCONF_MLATITUDE, value);
            LOGFAIL(err, "Set ManualLatitude failed. value = %f", value);

            // Longitude
            ptr = strtok_r(NULL, ",", &saveptr);
            value = atof(ptr);
            err = vconf_set_dbl(VCONF_MLONGITUDE, value);
            LOGFAIL(err, "Set ManualLongitude failed. value = %f", value);

            // Altitude
            ptr = strtok_r(NULL, ",", &saveptr);
            value = atof(ptr);
            err = vconf_set_dbl(VCONF_MALTITUDE, value);
            LOGFAIL(err, "Set ManualAltitude failed. value = %f", value);

            // Accuracy
            ptr = strtok_r(NULL, ",", &saveptr);
            value = atof(ptr);
            err = vconf_set_dbl(VCONF_MHACCURACY, value);
            LOGFAIL(err, "Set ManualHAccuracy failed. value = %f", value);
        }
    }
}

bool msgproc_location(ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_location");
    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return true;

        param->ActionID = ijcmd->msg.action;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[TID_LOCATION], NULL, getting_location, (void*) param) != 0)
        {
            LOGERR("location pthread create fail!");
            return true;
        }
    }
    else
    {
        setting_location(ijcmd->data);
    }
    return true;
}
