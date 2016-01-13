/*
 * emulator-daemon library
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Chulho Song   <ch81.song@samsung.com>
 * Hakhyun Kim   <haken.kim@samsung.com>
 * Jinhyung choi <jinh0.choi@samsung.com>
 * Sangho Park   <sangho.p@samsung.com>
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

#include <stdlib.h>
#include <string.h>
#include "libemuld.h"

emuld_msgproc msgproc_head[MSGPROC_PRIO_END];

bool msgproc_add(const char* name, const char* cmd, msgproc func, msgproc_prio priority)
{
    if (!name || !cmd || !func || priority == MSGPROC_PRIO_END)
    {
        LOGWARN("Invalid param : name = %s, cmd = %s, msgproc = %p, priority = %d",
            name, cmd, func, priority);
        return false;
    }

    if ((strnlen(name, NAME_LEN) == NAME_LEN) || (strnlen(cmd, CMD_SIZE) == CMD_SIZE))
    {
        LOGWARN("Invalid param : max name length = %d , max cmd length = %d",
            NAME_LEN-1, CMD_SIZE-1);
        return false;
    }

    emuld_msgproc *pPrev = &msgproc_head[priority];
    emuld_msgproc *pMsgProc = msgproc_head[priority].next;
    while (pMsgProc != NULL)
    {
        pPrev = pMsgProc;
        pMsgProc = pMsgProc->next;
    }
    pMsgProc = (emuld_msgproc*)malloc(sizeof(emuld_msgproc));
    if (!pMsgProc)
    {
        LOGWARN("Failed to allocate memory");
        return false;
    }
    memset(pMsgProc, 0, sizeof(emuld_msgproc));
    strncpy(pMsgProc->name, name, NAME_LEN - 1);
    strncpy(pMsgProc->cmd, cmd, CMD_SIZE - 1);
    pMsgProc->func = func;
    pMsgProc->next = NULL;
    pPrev->next = pMsgProc;

    return true;
}

static void msgproc_del_by_priority(msgproc_prio priority)
{
    emuld_msgproc *pPrev = &msgproc_head[priority];
    emuld_msgproc *pMsgProc = msgproc_head[priority].next;

    while (pMsgProc)
    {
        pPrev->next = pMsgProc->next;
        free(pMsgProc);
        pMsgProc = pPrev->next;
    }
}

static void msgproc_del_by_cmd(const char* cmd, msgproc_prio priority)
{
    emuld_msgproc *pPrev = &msgproc_head[priority];
    emuld_msgproc *pMsgProc = msgproc_head[priority].next;

    while (pMsgProc)
    {
        if (!strcmp(pMsgProc->cmd, cmd)) {
            pPrev->next = pMsgProc->next;
            free(pMsgProc);
            pMsgProc = pPrev->next;
        } else {
            pPrev = pMsgProc;
            pMsgProc = pMsgProc->next;
        }
    }
}

static void msgproc_del_by_name(const char* name, msgproc_prio priority)
{
    emuld_msgproc *pPrev = &msgproc_head[priority];
    emuld_msgproc *pMsgProc = msgproc_head[priority].next;

    while (pMsgProc)
    {
        if (!strcmp(pMsgProc->name, name)) {
            pPrev->next = pMsgProc->next;
            free(pMsgProc);
            pMsgProc = pPrev->next;
        } else {
            pPrev = pMsgProc;
            pMsgProc = pMsgProc->next;
        }
    }
}

static void msgproc_del_by_name_and_cmd(const char* name, const char* cmd, msgproc_prio priority)
{
    emuld_msgproc *pPrev = &msgproc_head[priority];
    emuld_msgproc *pMsgProc = msgproc_head[priority].next;

    while (pMsgProc)
    {
        if (!strcmp(pMsgProc->name, name) && !strcmp(pMsgProc->cmd, cmd)) {
            pPrev->next = pMsgProc->next;
            free(pMsgProc);
            pMsgProc = pPrev->next;
        } else {
            pPrev = pMsgProc;
            pMsgProc = pMsgProc->next;
        }
    }
}

bool msgproc_del(const char* name, const char* cmd, msgproc_prio priority)
{
    if (priority == MSGPROC_PRIO_END)
    {
        LOGWARN("Invalid param : priority = %d", priority);
        return false;
    }

    if ((name && (strnlen(name, NAME_LEN) == NAME_LEN)) || (cmd && (strnlen(cmd, CMD_SIZE) == CMD_SIZE)))
    {
        LOGWARN("Invalid param : max name length = %d , max cmd length = %d",
            NAME_LEN-1, CMD_SIZE-1);
        return false;
    }

    if (!name && !cmd) {
        msgproc_del_by_priority(priority);
    }  else if (!name) {
        msgproc_del_by_cmd(cmd, priority);
    } else if (!cmd) {
        msgproc_del_by_name(name, priority);
    } else {
        msgproc_del_by_name_and_cmd(name, cmd, priority);
    }

    return true;
}
