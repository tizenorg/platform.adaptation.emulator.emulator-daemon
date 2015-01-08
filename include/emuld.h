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


#ifndef __EMULD_H__
#define __EMULD_H__

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>

#include <map>

#include "evdi.h"

#define FDTYPE_MAX          6

enum
{
    fdtype_device     = 1,
    fdtype_ij         = 4,
    fdtype_max        = FDTYPE_MAX
};

/* definition */
#define MAX_CLIENT          10000
#define MAX_EVENTS          10000
#define MAX_GETCNT          10
#define ID_SIZE             10
#define HEADER_SIZE         4

// Thread TID profile uses >= 5
#define TID_BOOT            1
#define TID_SDCARD          2
#define TID_LOCATION        3
#define TID_HDS             4

extern pthread_t tid[MAX_CLIENT + 1];
extern int g_fd[fdtype_max];
extern bool exit_flag;
extern int g_epoll_fd;
extern struct epoll_event g_events[MAX_EVENTS];

void writelog(const char* fmt, ...);

#if defined(ENABLE_DLOG_OUT)
#  define LOG_TAG           "EMULD"
#  include <dlog/dlog.h>
#  define LOGINFO LOGI
#  define LOGERR LOGE
#  define LOGDEBUG LOGD
#else
#  define LOGINFO(fmt, ...) { writelog(fmt, ##__VA_ARGS__); }
#  define LOGERR LOGINFO
#  define LOGDEBUG LOGINFO
#endif

typedef unsigned short  CliSN;

struct Cli
{
    Cli(CliSN clisn, int fdtype, int fd, unsigned short port) :
        clisn(clisn), fdtype(fdtype), sockfd(fd), cli_port(port) {}

    CliSN clisn;
    int fdtype;
    int sockfd;             /* client socket fds */
    unsigned short cli_port;        /* client connection port */
};

typedef std::map<CliSN, Cli*> CliMap;

void clipool_add(int fd, unsigned short port, const int fdtype);
void clipool_delete(int fd);
void close_cli(int cli_fd);

bool send_to_cli(const int fd, char* data, const int len);
bool send_to_all_ij(char* data, const int len);
bool is_ij_exist();
void stop_listen(void);

bool epoll_ctl_add(const int fd);
void userpool_add(int cli_fd, unsigned short cli_port, const int fdtype);
void userpool_delete(int cli_fd);

struct fd_info
{
    fd_info() : fd(-1){}
    int fd;
    int fdtype;
};

struct LXT_MESSAGE
{
    unsigned short length;
    unsigned char group;
    unsigned char action;
    void *data;
};

typedef struct LXT_MESSAGE LXT_MESSAGE;

struct ijcommand
{
    enum { CMD_SIZE = 48 };
    ijcommand() : data(NULL)
    {
        memset(cmd, 0, CMD_SIZE);
    }
    ~ijcommand()
    {
        if (data)
        {
            free(data);
            data = NULL;
        }
    }
    char cmd[CMD_SIZE];
    char* data;
    fd_info fdinfo;

    LXT_MESSAGE msg;
};

struct _auto_mutex
{
    _auto_mutex(pthread_mutex_t* t)
    {
        _mutex = t;
        pthread_mutex_lock(_mutex);

    }
    ~_auto_mutex()
    {
        pthread_mutex_unlock(_mutex);
    }

    pthread_mutex_t* _mutex;
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

bool read_ijcmd(const int fd, ijcommand* ijcmd);
int recv_data(int event_fd, char** r_databuf, int size);
void recv_from_evdi(evdi_fd fd);
bool accept_proc(const int server_fd);

void send_to_ecs(const char* cat, int group, int action, char* data);
void send_emuld_connection(void);
void send_default_suspend_req(void);
void* dbus_booting_done_check(void* data);
void systemcall(const char* param);
int parse_val(char *buff, unsigned char data, char *parsbuf);

#define IJTYPE_SUSPEND      "suspend"
#define IJTYPE_GUEST        "guest"
#define IJTYPE_PACKAGE      "package"
#define IJTYPE_BOOT         "boot"

void msgproc_suspend(ijcommand* ijcmd);
void msgproc_system(ijcommand* ijcmd);
void msgproc_package(ijcommand* ijcmd);
void msgproc_hds(ijcommand* ijcmd);
void msgproc_location(ijcommand* ijcmd);
void msgproc_sdcard(ijcommand* ijcmd);
void* exec_cmd_thread(void *args);
void msgproc_cmd(ijcommand* ijcmd);

/*
 * For the multi-profile
 */
void process_evdi_command(ijcommand* ijcmd);
bool server_process(void);
void init_profile(void);
void exit_profile(void);

#endif
