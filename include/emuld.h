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


#ifndef __EMULD_H__
#define __EMULD_H__

#include <pthread.h>
#include <sys/epoll.h>
#include <errno.h>
#include <cassert>
#include <map>

#include "synbuf.h"
#include "libemuld.h"

// plugin
#define EMULD_PLUGIN_DIR "/usr/lib/emuld"
#define EMULD_PLUGIN_INIT_FN "emuld_plugin_init"
#define MAX_PLUGINS 16

// epoll & evdi
#define MAX_EVENTS  10000
#define DEVICE_NODE_PATH "/dev/evdi0"

// DBUS & Boot done signal
#define BOOT_DONE_SIGNAL "BootingDone"
#define DBUS_PATH_BOOT_DONE  "/Org/Tizen/System/DeviceD/Core"
#define DBUS_IFACE_BOOT_DONE "org.tizen.system.deviced.core"

// Network
#define MAX_CLIENT  10000
#define TID_NETWORK 1
void* register_connection(void *data);
void destroy_connection(void);
extern pthread_t tid[MAX_CLIENT+1];

// Common
#define DEFAULT_MSGPROC     "default"
#define HDS_DEFAULT_ID      "fsdef0"
#define HDS_DEFAULT_TAG     "fileshare"
#define HDS_DEFAULT_PATH    "/mnt/host"
#define COMPAT_DEFAULT_DATA "fileshare\n/mnt/host\n"
#define DBUS_SEND_EXTCON    "ExtCon"
#define DBUS_SEND_SYSNOTI   "SysNoti"
#define DEVICE_CHANGED      "device_changed"
#define DBUS_MSG_BUF_SIZE   512
#define MSG_GROUP_HDS       100
#define HDS_ACTION_DEFAULT  99
#define GROUP_MEMORY        30
#define STATUS              15

#define TID_NETWORK         1
#define TID_LOCATION        3
#define TID_HDS_ATTACH      4
#define TID_HDS_DETACH      5
#define TID_SENSOR          6
#define TID_VCONF           7

/* common vconf keys */
#define VCONF_LOW_MEMORY    "memory/sysman/low_memory"
#define VCONF_REPLAYMODE    "db/location/replay/ReplayMode"
#define VCONF_FILENAME      "db/location/replay/FileName"
#define VCONF_MLATITUDE     "db/location/replay/ManualLatitude"
#define VCONF_MLONGITUDE    "db/location/replay/ManualLongitude"
#define VCONF_MALTITUDE     "db/location/replay/ManualAltitude"
#define VCONF_MHACCURACY    "db/location/replay/ManualHAccuracy"

#define VCONF_SET 1
#define VCONF_GET 0

struct vconf_res_type {
    char *vconf_key;
    char *vconf_val;
    int group;
    vconf_t vconf_type;
};

struct setting_device_param
{
    setting_device_param() : ActionID(0)
    {
        memset(type_cmd, 0, ID_SIZE);
    }
    unsigned char ActionID;
    char type_cmd[ID_SIZE];
};

void add_vconf_map_common(void);
void add_vconf_map_profile(void);
void send_default_suspend_req(void);
void send_default_mount_req(void);
bool valid_hds_path(char* path);
int try_mount(char* tag, char* path);
void hds_unmount_all(void);
void set_vconf_cb(void);
void send_emuld_connection(void);
void add_msg_proc_common(void);
void add_msg_proc_ext(void);
void dbus_send(const char* device, const char* target, const char* option);
int parse_val(char *buff, unsigned char data, char *parsbuf);
bool msgproc_hds(ijcommand* ijcmd);
bool msgproc_cmd(ijcommand* ijcmd);
bool msgproc_suspend(ijcommand* ijcmd);
bool msgproc_system(ijcommand* ijcmd);
bool msgproc_package(ijcommand* ijcmd);
bool msgproc_vconf(ijcommand* ijcmd);
bool msgproc_location(ijcommand* ijcmd);


#endif // __EMULD_H__
