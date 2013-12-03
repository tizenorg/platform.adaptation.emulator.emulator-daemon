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


#ifndef __emuld_h__
#define __emuld_h__

/* header files */
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>
#include <stdarg.h>
#include <sys/time.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
#include <assert.h>
#include <errno.h>
#include <sys/mount.h>
#include <stdbool.h>
#include <fcntl.h>
#include <queue>
#include <map>

#include "log.h"
#include "emuld_common.h"
#include "evdi_protocol.h"
#include "evdi.h"
#include "emuld_proc.h"

/* definition */
#define MAX_CLIENT          10000
#define MAX_EVENTS          10000
#define MAX_GETCNT          10
#define SDBD_PORT           26101
#define DEFAULT_PORT        3577
#define VMODEM_PORT         3578
#define GPSD_PORT           3579
#define SENSORD_PORT        3580
#define SRV_IP              "10.0.2.2"
#define ID_SIZE             10
#define HEADER_SIZE         4
#define EMD_DEBUG
#define POWEROFF_DURATION   2

#define SUSPEND_UNLOCK      0
#define SUSPEND_LOCK        1

#define SDB_PORT_FILE       "/opt/home/sdb_port.txt"

enum
{
    fdtype_server = 0,
    fdtype_device = 1,
    fdtype_vmodem = 2,
    fdtype_ij     = 3,
    fdtype_sensor = 4, //udp
    fdtype_max    = 5
};

extern pthread_t tid[MAX_CLIENT + 1];
extern struct sockaddr_in si_sensord_other;
extern int g_fd[fdtype_max];


#define IJTYPE_TELEPHONY    "telephony"
#define IJTYPE_SDCARD       "sdcard"
#define IJTYPE_SUSPEND      "suspend"

bool epoll_ctl_add(const int fd);

void userpool_add(int cli_fd, unsigned short cli_port, const int fdtype);
void userpool_delete(int cli_fd);

bool epoll_init(void);            /* epoll fd create */
bool epoll_ctl_add(const int fd);

void init_data0(void);            /* initialize data. */
bool init_server0(int svr_port, int* ret_fd);
void end_server(int sig);

void udp_init(void);
void emuld_ready(void);

bool server_process(void);
bool accept_proc(const int server_fd);

int recv_data(int event_fd, char** r_databuf, int size);
int parse_val(char *buff, unsigned char data, char *parsbuf);

void set_vm_connect_status(const int v);
bool is_vm_connected(void);

void* init_vm_connect(void* data);

void systemcall(const char* param);

void recv_from_evdi(evdi_fd fd);

int powerdown_by_force(void);
// location
void setting_location(char* databuf);

#define LOG(fmt, arg...) \
    do { \
        log_print_out("[%s:%d] "fmt"\n", __FUNCTION__, __LINE__, ##arg); \
    } while (0)

#include <map>

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

Cli* find_cli(const int fd);
bool send_to_cli(const int fd, char* data, const int len);
bool send_to_all_ij(char* data, const int len);
bool is_ij_exist();
void stop_listen(void);

struct fd_info
{
    fd_info() : fd(-1){}
    int fd;
    int fdtype;
};

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

void process_evdi_command(ijcommand* ijcmd);
bool read_ijcmd(const int fd, ijcommand* ijcmd);

void* setting_device(void* data);

// msg proc
bool msgproc_telephony(const int sockfd, ijcommand* ijcmd, const bool is_evdi);
bool msgproc_sensor(const int sockfd, ijcommand* ijcmd, const bool is_evdi);
bool msgproc_location(const int sockfd, ijcommand* ijcmd, const bool is_evdi);
bool msgproc_nfc(const int sockfd, ijcommand* ijcmd, const bool is_evdi);
bool msgproc_system(const int sockfd, ijcommand* ijcmd, const bool is_evdi);
bool msgproc_sdcard(const int sockfd, ijcommand* ijcmd, const bool is_evdi);

#endif //__emuld_h__
