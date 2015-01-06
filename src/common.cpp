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

#include <sys/time.h>
#include <sys/reboot.h>
#include <stdio.h>
#include <unistd.h>

#include "emuld.h"
#include "deviced/dd-display.h"

#define PMAPI_RETRY_COUNT   3
#define POWEROFF_DURATION   2

#define SUSPEND_UNLOCK      0
#define SUSPEND_LOCK        1

#define MAX_PKGS_BUF        1024

#define PATH_PACKAGE_INSTALL	"/opt/usr/apps/tmp/sdk_tools/"
#define RPM_CMD_QUERY		"-q"
#define RPM_CMD_INSTALL		"-U"
static pthread_mutex_t mutex_pkg = PTHREAD_MUTEX_INITIALIZER;

static struct timeval tv_start_poweroff;

void systemcall(const char* param)
{
    if (!param)
        return;

    if (system(param) == -1)
        LOGERR("system call failure(command = %s)", param);
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

void powerdown_by_force(void)
{
    struct timeval now;
    int poweroff_duration = POWEROFF_DURATION;

    gettimeofday(&now, NULL);
    /* Waiting until power off duration and displaying animation */
    while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
        LOGINFO("power down wait");
        usleep(100000);
        gettimeofday(&now, NULL);
    }

    LOGINFO("Power off by force");
    LOGINFO("sync");

    sync();

    LOGINFO("poweroff");

    reboot(RB_POWER_OFF);
}

void msgproc_system(ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_system");

    LOGINFO("/etc/rc.d/rc.shutdown, sync, reboot(RB_POWER_OFF)");

    sync();

    systemcall("/etc/rc.d/rc.shutdown &");

    gettimeofday(&tv_start_poweroff, NULL);

    powerdown_by_force();
}

static void set_lock_state(int state) {
    int i = 0;
    int ret = 0;
    // Now we blocking to enter "SLEEP".
    while(i < PMAPI_RETRY_COUNT ) {
        ret = display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
        LOGINFO("display_lock_state() return: %d", ret);
        if(ret == 0)
        {
            break;
        }
        ++i;
        sleep(10);
    }
    if (i == PMAPI_RETRY_COUNT) {
        LOGERR("Emulator Daemon: Failed to call display_lock_state().\n");
    }
}

void msgproc_suspend(ijcommand* ijcmd)
{
    if (ijcmd->msg.action == SUSPEND_LOCK) {
        set_lock_state(SUSPEND_LOCK);
    } else {
        set_lock_state(SUSPEND_UNLOCK);
    }

    LOGINFO("[Suspend] Set lock state as %d (1: lock, other: unlock)", ijcmd->msg.action);
}

void send_to_ecs(const char* cat, int group, int action, char* data)
{
	int datalen = 0;
    int tmplen = HEADER_SIZE;
	if (data != NULL) {
		datalen = strlen(data);
		tmplen += datalen;
	}

    char* tmp = (char*) malloc(tmplen);
    if (!tmp)
        return;

	if (data != NULL) {
		memcpy(tmp, &datalen, 2);
	}
	memcpy(tmp + 2, &group, 1);
	memcpy(tmp + 3, &action, 1);
	if (data != NULL) {
		memcpy(tmp + 4, data, datalen);
	}

    ijmsg_send_to_evdi(g_fd[fdtype_device], cat, (const char*) tmp, tmplen);

	if (tmp)
		free(tmp);
}

void send_emuld_connection(void)
{
	send_to_ecs(IJTYPE_GUEST, 0, 1, NULL);
}

void send_default_suspend_req(void)
{
	send_to_ecs(IJTYPE_SUSPEND, 5, 15, NULL);
}

static bool do_validate(char* pkgs)
{
	char buf[MAX_PKGS_BUF];

	FILE* fp = popen(pkgs, "r");
	if (fp == NULL) {
		LOGERR("Failed to popen %s", pkgs);
		return false;
	}

	memset(buf, 0, sizeof(buf));
	while(fgets(buf, sizeof(buf), fp)) {
		LOGINFO("[rpm]%s", buf);
		if (!strncmp(buf, IJTYPE_PACKAGE, 7)) {
			pclose(fp);
			return false;
		}
	    memset(buf, 0, sizeof(buf));
	}

	pclose(fp);
	return true;
}

static bool do_install(char* pkgs)
{
	char buf[MAX_PKGS_BUF];
	bool ret = true;

	FILE* fp = popen(pkgs, "r");
	if (fp == NULL) {
		LOGERR("Failed to popen %s", pkgs);
		return false;
	}

	memset(buf, 0, sizeof(buf));
	while(fgets(buf, sizeof(buf), fp)) {
		LOGINFO("[rpm] %s", buf);

		if (!strncmp(buf, "error", 5)) {
			ret = false;
		}
	    memset(buf, 0, sizeof(buf));
	}

	pclose(fp);

	return ret;
}

static void remove_package(char* data)
{
	char token[] = ", ";
	char pkg_list[MAX_PKGS_BUF];
	char *addon = NULL;
	char *copy = strdup(data);
	if (copy == NULL) {
		LOGERR("Failed to copy data.");
		return;
	}

	memset(pkg_list, 0, sizeof(pkg_list));

	strcpy(pkg_list, "rm -rf ");

	strcat(pkg_list, PATH_PACKAGE_INSTALL);
	addon = strtok(copy, token);
	strcat(pkg_list, addon);

	LOGINFO("remove packages: %s", pkg_list);

	systemcall(pkg_list);

	free(copy);
}

static bool do_package(int action, char* data)
{
	char token[] = ", ";
	char *pkg = NULL;
	char *addon = NULL;
	char pkg_list[MAX_PKGS_BUF];

	memset(pkg_list, 0, sizeof(pkg_list));

	strcpy(pkg_list, "rpm");

	if (action == 1) {
		strcat(pkg_list, " ");
		strcat(pkg_list, RPM_CMD_QUERY);
	} else if (action == 2) {
		strcat(pkg_list, " ");
		strcat(pkg_list, RPM_CMD_INSTALL);
	} else {
		LOGERR("Unknown action.");
		return false;
	}
	addon = strtok(data, token); // for addon path

	pkg = strtok(NULL, token);
	while (pkg != NULL) {
		if (action == 1) {
			pkg[strlen(pkg) - 4] = 0; //remove .rpm
		}
		strcat(pkg_list, " ");
		if (action == 2) {
			strcat(pkg_list, PATH_PACKAGE_INSTALL);
			strcat(pkg_list, addon);
			strcat(pkg_list, "/");
		}
		strcat(pkg_list, pkg);

		pkg = strtok(NULL, token);
	}

	strcat(pkg_list, " ");
	strcat(pkg_list, "2>&1");

	LOGINFO("[cmd]%s", pkg_list);
	if (action == 1 && do_validate(pkg_list)) {
		return true;
	} else if (action == 2 && do_install(pkg_list)) {
		return true;
	}

	return false;
}

static void* package_thread(void* args)
{
	LOGINFO("install package_thread starts.");
	int action = 0;
	ijcommand* ijcmd = (ijcommand*)args;
	char* data = strdup(ijcmd->data);
	if (data == NULL) {
		LOGERR("install data is failed to copied.");
		return NULL;
	}

	if (ijcmd->msg.action == 1) { // validate packages
		if (do_package(1, data)) {
			action = 1; // already installed
		} else {
			action = 2; // need to install
		}
	} else if (ijcmd->msg.action == 2) { // install packages
		if (do_package(2, data)) {
			action = 3; // install success
		} else {
			action = 4; // failed to install
		}
		remove_package(ijcmd->data);
	} else {
		LOGERR("invalid command (action:%d)", ijcmd->msg.action);
	}

	LOGINFO("send %d, with %s", action, ijcmd->data);
	send_to_ecs(IJTYPE_PACKAGE, 0, action, ijcmd->data);

	free(data);

	return NULL;
}

void msgproc_package(ijcommand* ijcmd)
{
	_auto_mutex _(&mutex_pkg);
	int ret = 0;
	void* retval = NULL;
	pthread_t pkg_thread_id;

	if (!ijcmd->data) {
		LOGERR("package data is empty.");
		return;
	}

	LOGINFO("received %d, with %s", ijcmd->msg.action, ijcmd->data);

	if (pthread_create(&pkg_thread_id, NULL, package_thread, (void*)ijcmd) != 0)
	{
		LOGERR("validate package pthread creation is failed!");
	}
	ret = pthread_join(pkg_thread_id, &retval);
	if (ret < 0) {
		LOGERR("validate package pthread join is failed.");
	}
}

