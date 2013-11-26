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

/*
 * emuld_proc.cpp
 *
 *  Created on: 2013. 4. 12.
 *      Author: dykim
 */

#include "emuld_common.h"
#include "emuld.h"
#include "emuld_proc.h"
#include <errno.h>

#include <sys/mount.h>
#include <stdio.h>
#include <stdlib.h>
#include <mntent.h>

char SDpath[256];
static struct timeval tv_start_poweroff;

struct setting_device_param
{
    setting_device_param() : get_status_sockfd(-1), ActionID(0), is_evdi(false)
    {
        memset(type_cmd, 0, ID_SIZE);
    }
    int get_status_sockfd;
    unsigned char ActionID;
    bool is_evdi;
    char type_cmd[ID_SIZE];
};

struct mount_param
{
    mount_param(int _fd) : fd(_fd), is_evdi(false) {}
    int fd;
    bool is_evdi;
};

char* get_mount_info() {
    struct mntent *ent;
    FILE *aFile;

    aFile = setmntent("/proc/mounts", "r");
    if (aFile == NULL) {
        LOG("/proc/mounts is not exist");
        return NULL;
    }
    char* mountinfo = new char[512];
    memset(mountinfo, 0, 512);

    while (NULL != (ent = getmntent(aFile))) {

        if (strcmp(ent->mnt_dir, "/opt/storage/sdcard") == 0)
        {
            LOG(",%s,%s,%d,%s,%d,%s\n",
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

    LOG("start sdcard mount thread");

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
            LOG( "%s is exist", file_name);
            packet->length = strlen(SDpath);        // length
            packet->group = 11;             // sdcard
            if (ret == 0)
                packet->action = 1; // mounted
            else
                packet->action = 5; // failed

            //
            LOG("SDpath is %s", SDpath);

            const int tmplen = HEADER_SIZE + packet->length;
            char* tmp = (char*) malloc(tmplen);

            if (tmp)
            {
                memcpy(tmp, packet, HEADER_SIZE);
                if (packet->length > 0)
                {
                    memcpy(tmp + HEADER_SIZE, SDpath, packet->length);
                }

                if (param->is_evdi)
                {
                    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);
                }
                else
                {
                    send_to_cli(param->fd, (char*) tmp, tmplen);
                }

                free(tmp);
            }

            //

            if (ret == 0) {
                ret = system("/usr/bin/sys_event mmcblk_add");
            }

            break;
        }
        else
        {
            LOG( "%s is not exist", file_name);
            sleep(1);
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

int umount_sdcard(const int fd, bool is_evdi)
{
    int ret = -1, i = 0;
    char file_name[128];
    memset(file_name, '\0', sizeof(file_name));

    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    if(packet == NULL){
        return ret;
    }
    memset(packet, 0, sizeof(LXT_MESSAGE));

    LOG("start sdcard umount");

    pthread_cancel(tid[1]);

    for (i = 0; i < 10; i++)
    {
        sprintf(file_name, "/dev/mmcblk%d", i);
        ret = access(file_name, F_OK);
        if (ret == 0)
        {
            if(fd != -1)
            {
                packet->length = strlen(SDpath);        // length
                packet->group = 11;             // sdcard
                packet->action = 0;             // unmounted

                send(fd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);

                LOG("SDpath is %s", SDpath);
                send(fd, SDpath, packet->length, 0);
            }
            else if (is_evdi)
            {
                LOG("SDpath is %s", SDpath);

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
            }

            memset(SDpath, '\0', sizeof(SDpath));
            sprintf(SDpath, "umounted");
            ret = system("/usr/bin/sys_event mmcblk_remove");

            break;
        }
        else
        {
            LOG( "%s is not exist", file_name);
        }
    }

    if(packet){
        free(packet);
        packet = NULL;
    }
    return ret;
}


int powerdown_by_force(void)
{
    struct timeval now;
    int poweroff_duration = POWEROFF_DURATION;

    gettimeofday(&now, NULL);
    /* Waiting until power off duration and displaying animation */
    while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
        LOG("wait");
        usleep(100000);
        gettimeofday(&now, NULL);
    }

    LOG("Power off by force");
    LOG("sync");

    sync();

    LOG("poweroff");

    reboot(RB_POWER_OFF);

    return 1;
}

// location event
char command[512];
char latitude[128];
char longitude[128];
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
            LOG("error(%s) : stop replay mode", databuf);
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 0 -f");
            break;
        }
        LOG("Location Command : %s", command);
        systemcall(command);
    } else {
        *s = '\0';
        int mode = atoi(databuf);
        if(mode == 1) { // NMEA MODE (LOG MODE)
            sprintf(command, "vconftool set -t string db/location/replay/FileName \"%s\"", s+1);
            LOG("%s", command);
            systemcall(command);
            memset(command, 0, 256);
            sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 1 -f");
            LOG("%s", command);
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
            LOG("%s", command);
            systemcall(command);

            // Longitude
            sprintf(command, "vconftool set -t double db/location/replay/ManualLongitude %s -f", longitude);
            LOG("%s", command);
            systemcall(command);
        }
    }
}


void* setting_device(void* data)
{
    pthread_detach(pthread_self());

    setting_device_param* param = (setting_device_param*) data;

    if (!param)
        return 0;

    int sockfd = param->get_status_sockfd;

    bool is_evdi = param->is_evdi;

    char* msg = 0;
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));

    switch(param->ActionID)
    {
    case BATTERY_LEVEL:
        msg = get_battery_level((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting battery level");
        }
        break;
    case BATTERY_CHARGER:
        msg = get_battery_charger((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting battery charger state");
        }
        break;
    case USB_STATUS:
        msg = get_usb_status((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting usb status");
        }
        break;
    case EARJACK_STATUS:
        msg = get_earjack_status((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting earjack status");
        }
        break;
    case RSSI_LEVEL:
        msg = get_rssi_level((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting rssi level");
        }
        break;
    case MOTION_VALUE:
        LOG("not support getting motion value");
        break;
    case LOCATION_STATUS:
        msg = get_location_status((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting location status");
        }
        break;
    case NFC_STATUS:
        msg = get_nfc_status((void*)packet, is_evdi);
        if (msg ==0) {
            LOG("failed getting nfc status");
        }
        break;
    case ACCEL_VALUE:
        msg = get_acceleration_value((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("falied getting acceleration value");
        }
        break;
    case GYRO_VALUE:
        msg = get_gyroscope_value((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting gyroscope value");
        }
        break;
    case MAG_VALUE:
        msg = get_magnetic_value((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting magnetic value");
        }
        break;
    case LIGHT_VALUE:
        msg = get_light_level((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting light level");
        }
        break;
    case PROXI_VALUE:
        msg = get_proximity_status((void*)packet, is_evdi);
        if (msg == 0) {
            LOG("failed getting proximity status");
        }
        break;
    default:
        //TODO
    break;
    }

    if (sockfd != -1)
    {
        if (msg != 0)
        {
            LOG("send data to injector");
        }
        else
        {
            LOG("send error message to injector");
            memset(packet, 0, sizeof(LXT_MESSAGE));
            packet->length = 0;
            packet->group  = STATUS;
            packet->action = param->ActionID;
        }

        send(sockfd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);

        if (packet->length > 0)
        {
            send(sockfd, msg, packet->length, 0);
        }
    }
    else if (param->is_evdi)
    {
        if (msg == 0)
        {
            LOG("send error message to injector");
            memset(packet, 0, sizeof(LXT_MESSAGE));
            packet->length = 0;
            packet->group = STATUS;
            packet->action = param->ActionID;
        }
        else
        {
            LOG("send data to injector");
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


// event injector message handlers

bool msgproc_telephony(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_telephony\n");

    if (!is_vm_connected())
        return false;

    int sent;
    sent = send(g_fd[fdtype_vmodem], &ijcmd->msg, HEADER_SIZE, 0);
    if (sent == -1)
    {
        perror("vmodem send error");
    }

    LOG("sent to vmodem = %d, err = %d\n", sent, errno);

    sent = send(g_fd[fdtype_vmodem], ijcmd->data, ijcmd->msg.length, 0);
    if (sent == -1)
    {
        perror("vmodem send error");
    }

    LOG("sent to vmodem = %d, err = %d\n", sent, errno);

    return true;
}

bool msgproc_sap(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_sap\n");

    if (!is_sap_connected())
        return false;

    int sent;
    sent = send(g_fd[fdtype_sap], &ijcmd->msg, HEADER_SIZE, 0);
    if (sent == -1)
    {
        perror("vmodem send error");
    }

    LOG("sent to sap daemon = %d, err = %d\n", sent, errno);

    sent = send(g_fd[fdtype_sap], ijcmd->data, ijcmd->msg.length, 0);
    if (sent == -1)
    {
        perror("sap send error");
    }

    LOG("sent to sap daemon = %d, err = %d\n", sent, errno);

    return true;
}

bool msgproc_sensor(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_sensor\n");
    int sslen = sizeof(si_sensord_other);

    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return false;

        memset(param, 0, sizeof(*param));

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        param->is_evdi = is_evdi;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[2], NULL, setting_device, (void*)param) != 0)
        {
            LOG("pthread create fail!");
            return false;
        }

    }
    else if (sendto(g_fd[fdtype_sensor], ijcmd->data, ijcmd->msg.length, 0,
            (struct sockaddr*) &si_sensord_other, sslen) == -1)
    {
        LOG("sendto error!");
        return false;
    }
    return true;
}

bool msgproc_location(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_location\n");
    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return false;

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        param->is_evdi = is_evdi;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[2], NULL, setting_device, (void*) param) != 0)
        {
            LOG("pthread create fail!");
            return false;
        }
    }
    else
    {
        setting_location(ijcmd->data);
    }
    return true;
}

bool msgproc_nfc(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_nfc\n");

    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return false;

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        param->is_evdi = is_evdi;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[2], NULL, setting_device, (void*) param) != 0)
        {
            LOG("pthread create fail!");
            return false;
        }
    }
    else
    {
        FILE* fd;
        fd = fopen(PATH_NFC_DATA, "w");
        if (!fd) {
            LOG("nfc file open fail!");
            return false;
        }
        fprintf(fd, "%s", ijcmd->data);
        fclose(fd);
    }
    return true;
}


bool msgproc_system(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_system\n");

    LOG("/etc/rc.d/rc.shutdown, sync, reboot(RB_POWER_OFF)");

    sync();

    systemcall("/etc/rc.d/rc.shutdown &");


    gettimeofday(&tv_start_poweroff, NULL);

    powerdown_by_force();

    return true;
}

bool msgproc_sdcard(const int sockfd, ijcommand* ijcmd, const bool is_evdi)
{
    LOG("msgproc_sdcard\n");

    const int tmpsize = ijcmd->msg.length;

    char token[] = "\n";
    char tmpdata[tmpsize];
    memcpy(tmpdata, ijcmd->data, tmpsize);

    char* ret = NULL;
    ret = strtok(tmpdata, token);

    LOG("%s", ret);

    int mount_val = atoi(ret);
    int mount_status = 0;

    switch (mount_val)
    {
        case 0:                         // umount
            {
                mount_status = umount_sdcard(sockfd, is_evdi);
                if (mount_status == 0)
                    send_guest_server(ijcmd->data);
            }
            break;
        case 1:                         // mount
            {
                memset(SDpath, '\0', sizeof(SDpath));
                ret = strtok(NULL, token);
                strcpy(SDpath, ret);
                LOG("sdcard path is %s", SDpath);

                send_guest_server(ijcmd->data);

                mount_param* param = new mount_param(sockfd);
                if (!param)
                    break;

                param->is_evdi = is_evdi;

                if (pthread_create(&tid[1], NULL, mount_sdcard, (void*) param) != 0)
                    LOG("pthread create fail!");
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

                LOG("SDpath is %s", SDpath);

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

                                if (is_evdi)
                                {
                                    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);
                                }
                                else
                                {
                                    send_to_cli(sockfd, (char*) tmp, tmplen);
                                }

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

                                if (is_evdi)
                                {
                                    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SDCARD, (const char*) tmp, tmplen);
                                }
                                else
                                {
                                    send_to_cli(sockfd, (char*) tmp, tmplen);
                                }

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
            LOG("unknown data %s", ret);
            break;
    }
    return true;
}

//sdcard event
void send_guest_server(char* databuf)
{
    if (!databuf)
    {
        LOG("invalid data buf");
        return;
    }

    char buf[32];
    struct sockaddr_in si_other;
    int s, slen=sizeof(si_other);
    FILE* fd;
    char fbuf[16];
    int port = 3581;

    fd = fopen(SDB_PORT_FILE, "r");
    if(!fd)
    {
        LOG("fail to open %s file", SDB_PORT_FILE);
    }

    if (fgets(fbuf, 16, fd))
    {
        port = atoi(fbuf) + 3;
    }
    else
    {
        LOG("fgets failure\n");
    }

    fclose(fd);

    s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == -1)
    {
        LOG("socket error!");
        return;
    }

    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(port);
    if (inet_aton(SRV_IP, &si_other.sin_addr) == 0)
    {
          LOG("inet_aton() failed\n");
    }

    memset(buf, '\0', sizeof(buf));
    snprintf(buf, sizeof(buf), "4\n%s", databuf);
    LOG("sendGuestServer msg: %s", buf);
    if (sendto(s, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, slen) == -1)
    {
        LOG("sendto error!");
    }

    close(s);
}

