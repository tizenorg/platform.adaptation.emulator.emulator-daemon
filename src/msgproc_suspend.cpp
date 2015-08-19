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

#include <unistd.h>
#include "emuld.h"
#include "dd-display.h"

#define PMAPI_RETRY_COUNT   3
#define SUSPEND_UNLOCK      0
#define SUSPEND_LOCK        1

static int lock_state = SUSPEND_UNLOCK;

static void set_lock_state(int state) {
    int i = 0;
    int ret = 0;

    // FIXME: int lock_state = get_lock_state();
    LOGINFO("current lock state : %d (1: lock, other: unlock)", lock_state);

    while (i < PMAPI_RETRY_COUNT) {
        if (state == SUSPEND_LOCK) {
            // Now we blocking to enter "SLEEP".
            ret = display_lock_state(LCD_OFF, STAY_CUR_STATE, 0);
        } else if (lock_state == SUSPEND_LOCK) {
            ret = display_unlock_state(LCD_OFF, PM_SLEEP_MARGIN);
        } else {
            LOGINFO("meaningless unlock -> unlock state request. RETURN!");
            return;
        }

        LOGINFO("display_(lock/unlock)_state return: %d", ret);

        if(ret == 0)
        {
            break;
        }
        ++i;
        sleep(10);
    }
    if (i == PMAPI_RETRY_COUNT) {
        LOGERR("Emulator Daemon: Failed to set lock state.\n");
        return;
    }
    lock_state = state;
}

bool msgproc_suspend(ijcommand* ijcmd)
{
    LOGINFO("[Suspend] Set lock state as %d (1: lock, other: unlock)", ijcmd->msg.action);

    if (ijcmd->msg.action == SUSPEND_LOCK) {
        set_lock_state(SUSPEND_LOCK);
    } else {
        set_lock_state(SUSPEND_UNLOCK);
    }
    return true;
}
