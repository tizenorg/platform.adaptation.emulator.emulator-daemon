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

#include <sys/mount.h>
#include <unistd.h>
#include <stdio.h>
#include <utility>

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdlib.h>
#include <mntent.h>

#include "emuld.h"

#define CMD_BUF 1024
#define OPT_SIZE 64
#define P9_FCALL_SIZE 20
#define DEFAULT_PAYLOAD_SIZE 8192
#define MAX_PAYLOAD_SIZE 65536
#define MKDIR_MODE (S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

unsigned int msize = MAX_PAYLOAD_SIZE;

void add_vconf_map_common(void)
{

#ifdef ENABLE_LOCATION
    /* location */
    add_vconf_map(LOCATION, VCONF_REPLAYMODE);
    add_vconf_map(LOCATION, VCONF_FILENAME);
    add_vconf_map(LOCATION, VCONF_MLATITUDE);
    add_vconf_map(LOCATION, VCONF_MLONGITUDE);
    add_vconf_map(LOCATION, VCONF_MALTITUDE);
    add_vconf_map(LOCATION, VCONF_MHACCURACY);
#endif
    /* memory */
    add_vconf_map(MEMORY, VCONF_LOW_MEMORY);
}

int parse_val(char *buff, unsigned char data, char *parsbuf)
{
    int count=0;
    while(1)
    {
        if(count > 40)
            return -1;
        if(buff[count] == data)
        {
            count++;
            strncpy(parsbuf, buff, count);
            return count;
        }
        count++;
    }

    return 0;
}

void send_emuld_connection(void)
{
    send_to_ecs(IJTYPE_GUEST, 0, 1, NULL);
}

void send_default_suspend_req(void)
{
    send_to_ecs(IJTYPE_SUSPEND, 5, 15, NULL);
}

int get_vconf_status(char** value, vconf_t type, const char* key)
{
    if (type == VCONF_TYPE_INT) {
        int status;
        int ret = vconf_get_int(key, &status);
        if (ret != 0) {
            LOGERR("cannot get vconf key - %s", key);
            return 0;
        }

        ret = asprintf(value, "%d", status);
        if (ret == -1) {
            LOGERR("insufficient memory available");
            return 0;
        }
    } else if (type == VCONF_TYPE_DOUBLE) {
        LOGERR("not implemented");
        assert(type == VCONF_TYPE_INT);
        return 0;
    } else if (type == VCONF_TYPE_STRING) {
        LOGERR("not implemented");
        assert(type == VCONF_TYPE_INT);
        return 0;
    } else if (type == VCONF_TYPE_BOOL) {
        LOGERR("not implemented");
        assert(type == VCONF_TYPE_INT);
        return 0;
    } else if (type == VCONF_TYPE_DIR) {
        LOGERR("not implemented");
        assert(type == VCONF_TYPE_INT);
        return 0;
    } else {
        LOGERR("undefined vconf type");
        assert(type == VCONF_TYPE_INT);
        return 0;
    }

    return strlen(*value);
}

int try_mount(char* tag, char* path)
{
    int ret = 0;
    char mount_opt[OPT_SIZE] = {0, };

    snprintf(mount_opt, sizeof(mount_opt), "trans=virtio,version=9p2000.L,msize=%u", msize - P9_FCALL_SIZE);


    ret = mount(tag, path, "9p", 0, mount_opt);

    if (ret == -1)
    {
        if (errno == ENOMEM && msize > DEFAULT_PAYLOAD_SIZE)
        {
            LOGWARN("Mount failed with ENOMEM. Payload size (%u)", msize);
            msize = msize >> 1;
            LOGWARN("Shrink payload size (%u)", msize);
        }
        return errno;
    }
    msize = MAX_PAYLOAD_SIZE;

    return ret;
}

void hds_unmount_all(void)
{
    char tmp[CMD_BUF];
    snprintf(tmp, sizeof(tmp), "%s", "umount -a -t 9p");
    systemcall(tmp);
}

static int do_mkdir(const char *dir)
{
    char tmp[MAX_PATH];
    char *p = NULL;
    size_t len;

    if (!dir || *dir != '/')
    {
        LOGWARN("Invalid param. dir should be absolute path. dir = %s\n", dir);
        return -1;
    }

    // Create directories by the size of "MAX_PATH"
    // regardless of directory depth
    snprintf(tmp, sizeof(tmp), "%s", dir);
    len = strnlen(tmp, MAX_PATH);

    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;

    for (p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            if (mkdir(tmp, MKDIR_MODE) == -1 && errno != EEXIST)
                return -1;
            *p = '/';
        }
    }

    return mkdir(tmp, MKDIR_MODE);
}

bool valid_hds_path(char* path) {
    struct stat buf;
    int ret = -1;

    ret = access(path, F_OK);
    if (ret == -1) {
        if (errno == ENOENT) {
            ret = do_mkdir(path);
            if (ret == -1) {
                LOGERR("failed to create path : %d", errno);
                return false;
            }
        } else {
            LOGERR("failed to access path : %d", errno);
            return false;
        }
    }

    ret = lstat(path, &buf);
    if (ret == -1) {
        LOGERR("lstat is failed to validate path : %d", errno);
        return false;
    }

    if (!S_ISDIR(buf.st_mode)) {
        LOGERR("%s is not a directory.", path);
        return false;
    }

    LOGINFO("check '%s' complete.", path);

    return true;
}

void send_default_mount_req()
{
    send_to_ecs(IJTYPE_HDS, 0, 0, NULL);
}

static void low_memory_cb(keynode_t* pKey, void* pData)
{
    switch (vconf_keynode_get_type(pKey)) {
        case VCONF_TYPE_INT:
        {
            int value = vconf_keynode_get_int(pKey);
            LOGDEBUG("key = %s, value = %d(int)", vconf_keynode_get_name(pKey), value);
            char *buf = (char*)malloc(sizeof(int));
            if (!buf) {
                LOGERR("insufficient memory available");
                return;
            }

            sprintf(buf, "%d", vconf_keynode_get_int(pKey));
            send_to_ecs(IJTYPE_VCONF, GROUP_MEMORY, STATUS, buf);

            free(buf);
            break;
        }
        default:
            LOGERR("type mismatch in key: %s", vconf_keynode_get_name(pKey));
            break;
    }
}

void set_vconf_cb(void)
{
    int ret = 0;
    ret = vconf_notify_key_changed(VCONF_LOW_MEMORY, low_memory_cb, NULL);
    if (ret) {
        LOGERR("vconf_notify_key_changed() failed");
    }
}

#define DBUS_SEND_PRE_CMD   "dbus-send --system --type=method_call --print-reply --reply-timeout=120000 --dest=org.tizen.system.deviced /Org/Tizen/System/DeviceD/"
#define DBUS_SEND_MID_CMD   " org.tizen.system.deviced."

void dbus_send(const char* device, const char* target, const char* option)
{
    const char* dbus_send_pre_cmd = DBUS_SEND_PRE_CMD;
    const char* dbus_send_mid_cmd = DBUS_SEND_MID_CMD;
    char cmd[DBUS_MSG_BUF_SIZE];

    if (device == NULL || option == NULL || target == NULL)
        return;

    snprintf(cmd, sizeof(cmd), "%s%s%s%s.%s string:\"%s\" %s", dbus_send_pre_cmd, target, dbus_send_mid_cmd, target, device, device, option);

    systemcall(cmd);
    LOGINFO("dbus_send: %s", cmd);
}

void add_msg_proc_common(void)
{
    bool ret;
#ifdef ENABLE_SUSPEND
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_SUSPEND, &msgproc_suspend, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_SUSPEND);
#endif
#ifdef ENABLE_HDS
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_HDS, &msgproc_hds, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_HDS);
#endif
#ifdef ENABLE_SYSTEM
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_SYSTEM, &msgproc_system, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_SYSTEM);
#endif
#ifdef ENABLE_PACKAGE
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_PACKAGE, &msgproc_package, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_PACKAGE);
#endif
#ifdef ENABLE_CMD
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_CMD, &msgproc_cmd, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_CMD);
#endif
#ifdef ENABLE_VCONF
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_VCONF, &msgproc_vconf, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_VCONF);
#endif
#ifdef ENABLE_LOCATION
    ret = msgproc_add(DEFAULT_MSGPROC, IJTYPE_LOCATION, &msgproc_location, MSGPROC_PRIO_MIDDLE);
    LOGFAIL(!ret, "Msgproc add failed. plugin = %s, cmd = %s", DEFAULT_MSGPROC, IJTYPE_LOCATION);
#endif
}
