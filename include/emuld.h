/*
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
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

/* definition */
#define MAX_CLIENT		10000
#define MAX_EVENTS		10000
#define MAX_GETCNT		10
#define SDBD_PORT		26101
#define DEFAULT_PORT		3577
#define VMODEM_PORT		3578
#define GPSD_PORT		3579
#define SENSORD_PORT		3580
#define SRV_IP 			"10.0.2.2"
#define ID_SIZE			10
#define HEADER_SIZE		4
#define EMD_DEBUG
#define POWEROFF_DURATION     	2

/* function prototype */

void set_vm_connect_status(const int v);
bool is_vm_connected(void);
void init_data0(void);            /* initialize data. */
void init_server0(int svr_port);  /* server socket bind/listen */
void* init_vm_connect(void* data);
void epoll_init(void);            /* epoll fd create */
void epoll_cli_add(int cli_fd);   /* client fd add to epoll set */
void userpool_add(int cli_fd, unsigned short cli_port);
void userpool_delete(int cli_fd);
int parse_val(char *buff, unsigned char data, char *parsbuf);
void udp_init(void);
int recv_data(int event_fd, char** r_databuf, int size);
void client_recv(int event_fd);
bool server_process(void);
void end_server(int sig);
int is_mounted(void);

static int log_print = -1;
FILE* log_fd;

// location
void setting_location(char* databuf);

#define LOG(fmt, arg...) \
	do { \
		log_print_out("[%s:%d] "fmt"\n", __FUNCTION__, __LINE__, ##arg); \
	} while (0)

static inline void log_print_out(char *fmt, ...)
{

#ifdef EMD_DEBUG
	char *buf;

	if(log_print == -1){
		/* not initialized */
		buf = getenv("EMULD_LOG");
		if(buf != NULL){
			/* log print */
			fprintf(stdout, "env EMULD_LOG is set => print logs \n");
			log_print = 1;
		}else{
			/* log not print */
			fprintf(stdout, "env EMULD_LOG is not set => Not print logs \n");
			log_print = 0;
		}
	}

	if(log_print == 1){
		char buf[4096]; 
		va_list ap;

		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		fprintf(stdout, "%s", buf);
		// for emuld log file
		log_fd = fopen("/opt/sensor/emuld.log", "a"); 
		fprintf(log_fd, "%s", buf);
		fclose(log_fd);
	}

#endif

	return;
}
