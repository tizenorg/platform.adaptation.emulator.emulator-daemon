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
#include <unistd.h>

#include "emuld.h"

#include "deviced/dd-display.h"

#define PMAPI_RETRY_COUNT   3
#define POWEROFF_DURATION   2

#define IJTYPE_SUSPEND      "suspend"

#define SUSPEND_UNLOCK      0
#define SUSPEND_LOCK        1

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

void msgproc_system(const int sockfd, ijcommand* ijcmd)
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

void msgproc_suspend(int fd, ijcommand* ijcmd)
{
    if (ijcmd->msg.action == SUSPEND_LOCK) {
        set_lock_state(SUSPEND_LOCK);
    } else {
        set_lock_state(SUSPEND_UNLOCK);
    }

    LOGINFO("[Suspend] Set lock state as %d (1: lock, other: unlock)", ijcmd->msg.action);
}

void send_default_suspend_req(void)
{
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    if(packet == NULL){
        return;
    }
    memset(packet, 0, sizeof(LXT_MESSAGE));

    packet->length = 0;
    packet->group = 5;
    packet->action = 15;

    int tmplen = HEADER_SIZE;
    char* tmp = (char*) malloc(tmplen);
    if (!tmp)
        return;

    memcpy(tmp, packet, HEADER_SIZE);

    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_SUSPEND, (const char*) tmp, tmplen);


    if (tmp)
        free(tmp);
    if (packet)
        free(packet);
}


