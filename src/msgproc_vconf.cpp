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

#include <stdio.h>
#include "emuld.h"
#include <bundle.h>
#include <eventsystem.h>
#include <vconf-internal-sysman-keys.h>

static void* get_vconf_value(void* data)
{
    pthread_detach(pthread_self());

    char *value = NULL;
    vconf_res_type *vrt = (vconf_res_type*)data;

    if (!check_possible_vconf_key(vrt->vconf_key)) {
        LOGERR("%s is not available key.", vrt->vconf_key);
    } else {
        int length = get_vconf_status(&value, vrt->vconf_type, vrt->vconf_key);
        if (length == 0 || !value) {
            LOGERR("send error message to injector");
            send_to_ecs(IJTYPE_VCONF, vrt->group, STATUS, NULL);
        } else {
            LOGDEBUG("send data to injector");
            send_to_ecs(IJTYPE_VCONF, vrt->group, STATUS, value);
            free(value);
        }
    }

    free(vrt->vconf_key);
    free(vrt);

    pthread_exit((void *) 0);
}

static void* set_vconf_value(void* data)
{
    pthread_detach(pthread_self());

    vconf_res_type *vrt = (vconf_res_type*)data;
    int val = -1;

    if (!check_possible_vconf_key(vrt->vconf_key)) {
        LOGERR("%s is not available key.", vrt->vconf_key);
    } else {
        keylist_t *get_keylist;
        keynode_t *pkey_node = NULL;
        get_keylist = vconf_keylist_new();
        if (!get_keylist) {
            LOGERR("vconf_keylist_new() failed");
        } else {
            vconf_get(get_keylist, vrt->vconf_key, VCONF_GET_ALL);
            int ret = vconf_keylist_lookup(get_keylist, vrt->vconf_key, &pkey_node);
            if (ret == 0) {
                LOGERR("%s key not found", vrt->vconf_key);
            } else {
                if (vconf_keynode_get_type(pkey_node) != vrt->vconf_type) {
                    LOGERR("inconsistent type (prev: %d, new: %d)",
                                vconf_keynode_get_type(pkey_node), vrt->vconf_type);
                }
            }
            vconf_keylist_free(get_keylist);
        }

        /* TODO: to be implemented another type */
        if (vrt->vconf_type == VCONF_TYPE_INT) {
            val = atoi(vrt->vconf_val);
            vconf_set_int(vrt->vconf_key, val);
            LOGDEBUG("key: %s, val: %d", vrt->vconf_key, val);
        } else if (vrt->vconf_type == VCONF_TYPE_DOUBLE) {
            LOGERR("not implemented");
        } else if (vrt->vconf_type == VCONF_TYPE_STRING) {
            LOGERR("not implemented");
        } else if (vrt->vconf_type == VCONF_TYPE_BOOL) {
            LOGERR("not implemented");
        } else if (vrt->vconf_type == VCONF_TYPE_DIR) {
            LOGERR("not implemented");
        } else {
            LOGERR("undefined vconf type");
        }

        /* Low memory event */
        if (!strcmp(vrt->vconf_key, VCONF_LOW_MEMORY))
        {
            bundle* b = bundle_create();
            if (!b) {
                LOGWARN("failed to create bundle");
                goto out;
            }
            switch (val)
            {
                case VCONFKEY_SYSMAN_LOW_MEMORY_NORMAL:
                    bundle_add_str(b, EVT_KEY_LOW_MEMORY, EVT_VAL_MEMORY_NORMAL);
                    eventsystem_send_system_event(SYS_EVENT_LOW_MEMORY, b);
                    break;
                case VCONFKEY_SYSMAN_LOW_MEMORY_SOFT_WARNING:
                    bundle_add_str(b, EVT_KEY_LOW_MEMORY, EVT_VAL_MEMORY_SOFT_WARNING);
                    eventsystem_send_system_event(SYS_EVENT_LOW_MEMORY, b);
                    break;
                case VCONFKEY_SYSMAN_LOW_MEMORY_HARD_WARNING:
                    bundle_add_str(b, EVT_KEY_LOW_MEMORY, EVT_VAL_MEMORY_HARD_WARNING);
                    eventsystem_send_system_event(SYS_EVENT_LOW_MEMORY, b);
                    break;
                default:
                    LOGWARN("undefined low memory value(%d)", val);
                    break;
            }
            bundle_free(b);
        }
    }
out:
    free(vrt->vconf_key);
    free(vrt->vconf_val);
    free(vrt);

    pthread_exit((void *) 0);
}

bool msgproc_vconf(ijcommand* ijcmd)
{
    LOGDEBUG("msgproc_vconf");

    const int tmpsize = ijcmd->msg.length;
    char token[] = "\n";
    char tmpdata[tmpsize];
    char *saveptr;
    int vconf_key_size;
    int vconf_val_size;

    memcpy(tmpdata, ijcmd->data, tmpsize);

    char* ret = NULL;
    ret = strtok_r(tmpdata, token, &saveptr);
    if (!ret) {
        LOGERR("vconf type is empty");
        return true;
    }

    vconf_res_type *vrt = (vconf_res_type*)malloc(sizeof(vconf_res_type));
    if (!vrt) {
        LOGERR("insufficient memory available");
        return true;
    }

    if (strcmp(ret, "int") == 0) {
        vrt->vconf_type = VCONF_TYPE_INT;
    } else if (strcmp(ret, "double") == 0) {
        vrt->vconf_type = VCONF_TYPE_DOUBLE;
    } else if (strcmp(ret, "string") == 0) {
        vrt->vconf_type = VCONF_TYPE_STRING;
    } else if (strcmp(ret, "bool") == 0) {
        vrt->vconf_type = VCONF_TYPE_BOOL;
    } else if (strcmp(ret, "dir") ==0) {
        vrt->vconf_type = VCONF_TYPE_DIR;
    } else {
        LOGERR("undefined vconf type");
        goto error;
    }

    ret = strtok_r(NULL, token, &saveptr);
    if (!ret) {
        LOGERR("vconf key is empty");
        goto error;
    }
    vconf_key_size = strlen(ret);
    vrt->vconf_key = (char*)malloc(vconf_key_size + 1);
    if (!vrt->vconf_key) {
        LOGERR("insufficient memory available");
        goto error;
    }
    snprintf(vrt->vconf_key, vconf_key_size + 1, "%s", ret);

    if (ijcmd->msg.action == VCONF_SET) {
        ret = strtok_r(NULL, token, &saveptr);
        if (!ret) {
            LOGERR("vconf value is empty");
            goto error2;
        }

        vconf_val_size = strlen(ret);
        vrt->vconf_val = (char*)malloc(vconf_val_size + 1);
        if (!vrt->vconf_val) {
            LOGERR("insufficient memory available");
            goto error2;
        }
        snprintf(vrt->vconf_val, vconf_val_size + 1, "%s", ret);

        if (pthread_create(&tid[TID_VCONF], NULL, set_vconf_value, (void*)vrt) == 0) {
            return true;
        }
        LOGERR("set vconf pthread create fail!");
        free(vrt->vconf_val);
    } else if (ijcmd->msg.action == VCONF_GET) {
        vrt->group = ijcmd->msg.group;
        if (pthread_create(&tid[TID_VCONF], NULL, get_vconf_value, (void*)vrt) == 0) {
            return true;
        }
        LOGERR("get vconf pthread create fail!");
    } else {
        LOGERR("undefined action %d", ijcmd->msg.action);
    }

error2:
    free(vrt->vconf_key);
error:
    free(vrt);

    return true;
}
