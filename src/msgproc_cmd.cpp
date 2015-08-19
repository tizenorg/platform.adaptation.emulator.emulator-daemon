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

#include "emuld.h"

static pthread_mutex_t mutex_cmd = PTHREAD_MUTEX_INITIALIZER;

void* exec_cmd_thread(void *args)
{
    char *command = (char*)args;

    systemcall(command);
    LOGDEBUG("executed cmd: %s", command);
    free(command);

    pthread_exit(NULL);
}

bool msgproc_cmd(ijcommand* ijcmd)
{
    _auto_mutex _(&mutex_cmd);
    pthread_t cmd_thread_id;
    char *cmd = (char*) malloc(ijcmd->msg.length + 1);

    if (!cmd) {
        LOGERR("malloc failed.");
        return true;
    }

    memset(cmd, 0x00, ijcmd->msg.length + 1);
    strncpy(cmd, ijcmd->data, ijcmd->msg.length);
    LOGDEBUG("cmd: %s, length: %d", cmd, ijcmd->msg.length);

    if (pthread_create(&cmd_thread_id, NULL, exec_cmd_thread, (void*)cmd) != 0) {
        LOGERR("cmd pthread create fail!");
    }
    return true;
}
