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

#include <sys/time.h>
#include <sys/reboot.h>
#include <unistd.h>
#include "emuld.h"

#define POWEROFF_DURATION   2

static struct timeval tv_start_poweroff;

enum emuld_system_action {
    EMULD_SYSTEM_ACTION_FORCE_CLOSE,
    EMULD_SYSTEM_ACTION_REBOOT
};

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

bool msgproc_system(ijcommand* ijcmd)
{
    int action = ijcmd->msg.action;
    LOGDEBUG("msgproc_system with %d action", action);

    if (action == EMULD_SYSTEM_ACTION_FORCE_CLOSE) {
        LOGINFO("/etc/rc.d/rc.shutdown, sync, reboot(RB_POWER_OFF)");

        sync();

        systemcall("/etc/rc.d/rc.shutdown &");

        gettimeofday(&tv_start_poweroff, NULL);

        powerdown_by_force();
    } else if (action == EMULD_SYSTEM_ACTION_REBOOT) {
        LOGINFO("sync, unmount all hds");

        sync();

        hds_unmount_all();
    } else {
        LOGERR("unknown system action %d", action);
    }

    return true;
}
