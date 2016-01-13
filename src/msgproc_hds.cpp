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

#include <sys/mount.h>
#include <unistd.h>
#include "emuld.h"

static const char* hds_available_path [] = {
    "/mnt/",
    NULL,
};

static bool get_tag_path(char* data, char** tag, char** path)
{
    char token[] = "\n";
    char *saveptr;

    LOGINFO("get_tag_path data : %s", data);
    *tag = strtok_r(data, token, &saveptr);
    if (*tag == NULL) {
        LOGERR("data does not have a correct tag: %s", data);
        return false;
    }

    *path = strtok_r(NULL, token, &saveptr);
    if (*path == NULL) {
        LOGERR("data does not have a correct path: %s", data);
        return false;
    }

    return true;
}

static bool secure_hds_path(char* path) {
    int index = 0;
    int len = sizeof(hds_available_path) / sizeof(char*);
    for (index = 0; index < len; index++) {
        if (hds_available_path[index] &&
            !strncmp(path, hds_available_path[index], strlen(hds_available_path[index]))) {
            return true;
        }
    }
    return false;
}

static void* mount_hds(void* args)
{
    int i, ret = 0;
    int action = 2;
    char* tag;
    char* path;
    char* data = (char*)args;

    LOGINFO("start hds mount thread");

    pthread_detach(pthread_self());

    if (!get_tag_path(data, &tag, &path)) {
        free(data);
        return NULL;
    }

    if (!strncmp(tag, HDS_DEFAULT_ID, 6)) {
        free(data);
        return NULL;
    }

    if (!secure_hds_path(path)) {
        send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, 11, tag);
        free(data);
        return NULL;
    }

    if (!valid_hds_path(path)) {
        send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, 12, tag);
        free(data);
        return NULL;
    }

    LOGINFO("tag : %s, path: %s", tag, path);

    for (i = 0; i < 10; i++)
    {
        ret = try_mount(tag, path);
        if(ret == 0) {
            action = 1;
            break;
        } else {
            LOGERR("%d trial: mount is failed with errno: %d", i, errno);
        }
        usleep(500000);
    }

    send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, action, tag);

    free(data);

    return NULL;
}


static void* umount_hds(void* args)
{
    int ret = 0;
    int action = 3;
    char* tag;
    char* path;
    char* data = (char*)args;

    LOGINFO("unmount hds.");
    pthread_detach(pthread_self());

    if (!get_tag_path(data, &tag, &path)) {
        LOGERR("wrong tag or path.");
        free(data);
        return NULL;
    }

    ret = umount(path);
    if (ret != 0) {
        LOGERR("unmount failed with error num: %d", errno);
        action = 4;
    }

    ret = rmdir(path);
    LOGINFO("remove path result '%d:%d' with %s", ret, errno, path);

    send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, action, tag);

    LOGINFO("send result with action %d to evdi", action);

    free(data);

    return NULL;
}

bool msgproc_hds(ijcommand* ijcmd)
{
    char* data;
    char* tag;
    char* path;
    LOGDEBUG("msgproc_hds");

    if (!strncmp(ijcmd->data, HDS_DEFAULT_PATH, 9)) {
        LOGINFO("hds compatibility mode with %s", ijcmd->data);
        data = strdup(COMPAT_DEFAULT_DATA);
    } else {
        data = strdup(ijcmd->data);
    }

    if (data == NULL) {
        LOGERR("data dup is failed. out of memory.");
        return true;
    }

    LOGINFO("action: %d, data: %s", ijcmd->msg.action, data);
    if (ijcmd->msg.action == 1) {
        if (pthread_create(&tid[TID_HDS_ATTACH], NULL, mount_hds, (void*)data) != 0) {
            if (!get_tag_path(data, &tag, &path)) {
                LOGERR("mount pthread_create fail - wrong tag or path.");
                free(data);
                return true;
            }
            LOGERR("mount hds pthread create fail!");
            send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, 2, tag);
        }
    } else if (ijcmd->msg.action == 2) {
        if (pthread_create(&tid[TID_HDS_DETACH], NULL, umount_hds, (void*)data) != 0) {
            if (!get_tag_path(data, &tag, &path)) {
                LOGERR("umount pthread_create fail - wrong tag or path.");
                free(data);
                return true;
            }
            LOGERR("umount hds pthread create fail!");
            send_to_ecs(IJTYPE_HDS, MSG_GROUP_HDS, 4, tag);
        }
    } else {
        LOGERR("unknown action cmd.");
    }
    free(data);
    return true;
}


