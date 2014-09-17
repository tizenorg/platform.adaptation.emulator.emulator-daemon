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

// SD Card
#include <dirent.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>

// Location
#include <vconf/vconf.h>
#include <vconf/vconf-keys.h>

// Common
#include <unistd.h>
#include "emuld.h"

// SDCard
#define IJTYPE_SDCARD       "sdcard"

char SDpath[256];

// Location
#define STATUS              15
#define LOCATION_STATUS     120
char command[512];
char latitude[128];
char longitude[128];

/*
 * SD Card functions
 */

struct mount_param
{
    mount_param(int _fd) : fd(_fd) {}
    int fd;
};

char* get_mount_info() {
    struct mntent *ent;
    FILE *aFile;

    aFile = setmntent("/proc/mounts", "r");
    if (aFile == NULL) {
        LOGERR("/proc/mounts is not exist");
        return NULL;
    }
    char* mountinfo = new char[512];
    memset(mountinfo, 0, 512);

    while (NULL != (ent = getmntent(aFile))) {

        if (strcmp(ent->mnt_dir, "/opt/storage/sdcard") == 0)
        {
            LOGDEBUG(",%s,%s,%d,%s,%d,%s",
                            ent->mnt_fsname, ent->mnt_dir, ent->mnt_freq, ent->mnt_opts, ent->mnt_passno, ent->mnt_type);
            sprintf(mountinfo,",%s,%s,%d,%s,%d,%s\n",
                            ent->mnt_fsname, ent->mnt_dir, ent->mnt_freq, ent->mnt_opts, ent->mnt_passno, ent->mnt_type);
            break;
        }
    }
    endmntent(aFile);

    return mountinfo;
}

int is_mounted()
{
    int ret = -1, i = 0;
    struct stat buf;
    char file_name[128];
    memset(file_name, '\0', sizeof(file_name));

    for(i = 0; i < 10; i++)
    {
        sprintf(file_name, "/dev/mmcblk%d", i);
        ret = access(file_name, F_OK);
        if( ret == 0 )
        {
            lstat(file_name, &buf);
            if(S_ISBLK(buf.st_mode))
                return 1;
            else
                return 0;
        }
    }

    return 0;
}

void* mount_sdcard(void* data)
{
    mount_param* param = (mount_param*) data;

    int ret = -1, i = 0;
    struct stat buf;
    char file_name[128], command[256];
    memset(file_name, '\0', sizeof(file_name));
    memset(command, '\0', sizeof(command));

    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    memset(packet, 0, sizeof(LXT_MESSAGE));

    LOGINFO("start sdcard mount thread");

    pthread_detach(pthread_self());

    while (ret < 0)
    {
        for (i = 0; i < 10; i++)
        {
            sprintf(file_name, "/dev/mmcblk%d", i);
            ret = access( file_name, F_OK );
            if( ret == 0 )
            {
                lstat(file_name, &buf);
                if(!S_ISBLK(buf.st_mode))
                {
                    sprintf(command, "rm -rf %s", file_name);
                    systemcall(command);
                }
                else
                    break;
            }
        }

        if (i != 10)
        {
            LOGDEBUG( "%s is exist", file_name);
            packet->length = strlen(SDpath);        // length
            packet->group = 11;             // sdcard
            if (ret == 0)
                packet->action = 1; // mounted
            else
                packet->action = 5; // failed

            //
            LOGDEBUG("SDpath is %s", SDpath);

            const int tmplen = HEADER_SIZE + packet->length;
            char* tmp = (char*) malloc(tmplen);

            if (tmp)
            {
                memcpy(tmp, packet, HEADER_SIZE);
                if (packet->length > 0)
                {
                    memcpy(tmp + HEADER_SIZE, SDpath, packet->length);
                }

                ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);

                free(tmp);
            }

            break;
        }
        else
        {
            LOGERR( "%s is not exist", file_name);
        }
    }

    if(packet)
    {
        free(packet);
        packet = NULL;
    }

    if (param)
    {
        delete param;
        param = NULL;
    }
    pthread_exit((void *) 0);
}

int umount_sdcard(const int fd)
{
    int ret = -1, i = 0;
    char file_name[128];
    memset(file_name, '\0', sizeof(file_name));

    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    if(packet == NULL){
        return ret;
    }
    memset(packet, 0, sizeof(LXT_MESSAGE));

    LOGINFO("start sdcard umount");

    pthread_cancel(tid[1]);

    for (i = 0; i < 10; i++)
    {
        sprintf(file_name, "/dev/mmcblk%d", i);
        ret = access(file_name, F_OK);
        if (ret == 0)
        {
			LOGDEBUG("SDpath is %s", SDpath);

			packet->length = strlen(SDpath);        // length
			packet->group = 11;                     // sdcard
			packet->action = 0;                     // unmounted

			const int tmplen = HEADER_SIZE + packet->length;
			char* tmp = (char*) malloc(tmplen);
			if (!tmp)
				break;

			memcpy(tmp, packet, HEADER_SIZE);
			memcpy(tmp + HEADER_SIZE, SDpath, packet->length);

			ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);

			free(tmp);

            memset(SDpath, '\0', sizeof(SDpath));
            sprintf(SDpath, "umounted");

            break;
        }
        else
        {
            LOGERR( "%s is not exist", file_name);
        }
    }

    if(packet){
        free(packet);
        packet = NULL;
    }
    return ret;
}


void msgproc_sdcard(const int sockfd, ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_sdcard");

    const int tmpsize = ijcmd->msg.length;

    char token[] = "\n";
    char tmpdata[tmpsize];
    memcpy(tmpdata, ijcmd->data, tmpsize);

    char* ret = NULL;
    ret = strtok(tmpdata, token);

    LOGDEBUG("%s", ret);

    int mount_val = atoi(ret);
    int mount_status = 0;

    switch (mount_val)
    {
        case 0:                         // umount
            {
                mount_status = umount_sdcard(sockfd);
            }
            break;
        case 1:                         // mount
            {
                memset(SDpath, '\0', sizeof(SDpath));
                ret = strtok(NULL, token);
                strcpy(SDpath, ret);
                LOGDEBUG("sdcard path is %s", SDpath);

                mount_param* param = new mount_param(sockfd);
                if (!param)
                    break;

                if (pthread_create(&tid[1], NULL, mount_sdcard, (void*) param) != 0)
                    LOGERR("mount sdcard pthread create fail!");
            }

            break;
        case 2:                         // mount status
            {
                mount_status = is_mounted();
                LXT_MESSAGE* mntData = (LXT_MESSAGE*) malloc(sizeof(LXT_MESSAGE));
                if (mntData == NULL)
                {
                    break;
                }
                memset(mntData, 0, sizeof(LXT_MESSAGE));

                mntData->length = strlen(SDpath);   // length
                mntData->group = 11;            // sdcard

                LOGDEBUG("SDpath is %s", SDpath);

                switch (mount_status)
                {
                    case 0:
                        {
                            mntData->action = 2;            // umounted status

                            const int tmplen = HEADER_SIZE + mntData->length;
                            char* tmp = (char*) malloc(tmplen);

                            if (tmp)
                            {
                                memcpy(tmp, mntData, HEADER_SIZE);
                                if (mntData->length > 0)
                                {
                                    memcpy(tmp + HEADER_SIZE, SDpath, mntData->length);
                                }

                                ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);

                                free(tmp);
                            }

                            memset(SDpath, '\0', sizeof(SDpath));
                            sprintf(SDpath, "umounted");
                        }
                        break;
                    case 1:
                        {
                            mntData->action = 3;            // mounted status

                            int mountinfo_size = 0;
                            char* mountinfo = get_mount_info();
                            if (mountinfo)
                            {
                                mountinfo_size = strlen(mountinfo);
                            }

                            const int tmplen = HEADER_SIZE + mntData->length + mountinfo_size;
                            char* tmp = (char*) malloc(tmplen);

                            if (tmp)
                            {
                                memcpy(tmp, mntData, HEADER_SIZE);
                                if (mntData->length > 0)
                                {
                                    memcpy(tmp + HEADER_SIZE, SDpath, mntData->length);
                                }

                                if (mountinfo)
                                {
                                    memcpy(tmp + HEADER_SIZE + mntData->length, mountinfo, mountinfo_size);
                                    mntData->length += mountinfo_size;
                                    memcpy(tmp, mntData, HEADER_SIZE);
                                    delete mountinfo;
                                    mountinfo = NULL;
                                }

                                ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);

                                free(tmp);
                            }
                        }
                        break;
                    default:
                        break;
                }
                free(mntData);
            }
            break;
        default:
            LOGERR("unknown data %s", ret);
            break;
    }
}


/*
 * Location function
 */
static char* get_location_status(void* p)
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

void setting_location(char* databuf)
{
    char* s = strchr(databuf, ',');
    memset(command, 0, 256);
    if (s == NULL) { // SET MODE
        int mode = atoi(databuf);
        switch (mode) {
        case 0: // STOP MODE
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 0 -f");
            break;
        case 1: // NMEA MODE (LOG MODE)
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 1 -f");
            break;
        case 2: // MANUAL MODE
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 2 -f");
            break;
        default:
            LOGERR("error(%s) : stop replay mode", databuf);
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 0 -f");
            break;
        }
        LOGDEBUG("Location Command : %s", command);
        systemcall(command);
    } else {
        *s = '\0';
        int mode = atoi(databuf);
        if(mode == 1) { // NMEA MODE (LOG MODE)
            sprintf(command, "vconftool set -t string db/location/replay/FileName \"%s\"", s+1);
            LOGDEBUG("%s", command);
            systemcall(command);
            memset(command, 0, 256);
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 1 -f");
            LOGDEBUG("%s", command);
            systemcall(command);
        } else if(mode == 2) {
            memset(latitude,  0, 128);
            memset(longitude, 0, 128);
            char* t = strchr(s+1, ',');
            *t = '\0';
            strcpy(latitude, s+1);
            strcpy(longitude, t+1);
            //strcpy(longitude, s+1);
            //strcpy(latitude, databuf);
            // Latitude
            sprintf(command, "vconftool set -t double db/location/replay/ManualLatitude %s -f", latitude);
            LOGDEBUG("%s", command);
            systemcall(command);

            // Longitude
            sprintf(command, "vconftool set -t double db/location/replay/ManualLongitude %s -f", longitude);
            LOGDEBUG("%s", command);
            systemcall(command);
        }
    }
}

void msgproc_location(const int sockfd, ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_location");
    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return;

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[2], NULL, setting_device, (void*) param) != 0)
        {
            LOGERR("location pthread create fail!");
            return;
        }
    }
    else
    {
        setting_location(ijcmd->data);
    }
}



