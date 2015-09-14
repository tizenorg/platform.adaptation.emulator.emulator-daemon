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
#include <unistd.h>
#include <sys/stat.h>
#include "emuld.h"

#define MAX_PKGS_BUF            1024
#define MAX_DATA_BUF            1024
#define PATH_PACKAGE_INSTALL    "/opt/usr/apps/tmp/sdk_tools/"
#define SCRIPT_INSTALL          "post-install.sh"
#define RPM_CMD_QUERY           "-q"
#define RPM_CMD_INSTALL         "-U"

static pthread_mutex_t mutex_pkg = PTHREAD_MUTEX_INITIALIZER;

static bool do_rpm_execute(char* pkgs)
{
    char buf[MAX_PKGS_BUF];
    int ret = 0;

    FILE* fp = popen(pkgs, "r");
    if (fp == NULL) {
        LOGERR("[rpm] Failed to popen %s", pkgs);
        return false;
    }

    memset(buf, 0, sizeof(buf));
    while(fgets(buf, sizeof(buf), fp)) {
        LOGINFO("[rpm]%s", buf);
        memset(buf, 0, sizeof(buf));
    }

    ret = pclose(fp);
    if (ret == -1) {
        LOGINFO("[rpm] pclose error: %d", errno);
        return false;
    }

    if (ret >= 0 && WIFEXITED(ret) && WEXITSTATUS(ret) == 0) {
        LOGINFO("[rpm] RPM execution success: %s", pkgs);
        return true;
    }

    LOGINFO("[rpm] RPM execution fail: [%x,%x,%x] %s", ret, WIFEXITED(ret), WEXITSTATUS(ret), pkgs);

    return false;
}

static void remove_package(char* data)
{
    char token[] = ", ";
    char pkg_list[MAX_PKGS_BUF];
    char *addon = NULL;
    char *copy = strdup(data);
    size_t remain;
    if (copy == NULL) {
        LOGERR("Failed to copy data.");
        return;
    }

    memset(pkg_list, 0, sizeof(pkg_list));

    strcpy(pkg_list, "rm -rf ");

    strcat(pkg_list, PATH_PACKAGE_INSTALL);
    addon = strtok(copy, token);

    if (addon == NULL) {
        LOGERR("Addon is null");
        free(copy);
        return;
    }

    remain = MAX_PKGS_BUF - strnlen(pkg_list, MAX_PKGS_BUF);

    if (remain < strnlen(addon, MAX_PKGS_BUF)) {
        LOGERR("Cannot copy add-on name. Insufficient buf size");
        free(copy);
        return;
    }

    strncat(pkg_list, addon, remain - 1); // terminating null byte

    LOGINFO("remove packages: %s", pkg_list);

    systemcall(pkg_list);

    free(copy);
}

static void run_script(char* addon)
{
    int ret = -1;
    char file_name[MAX_PKGS_BUF];

    snprintf(file_name, sizeof(file_name), "%s%s/%s", PATH_PACKAGE_INSTALL, addon, SCRIPT_INSTALL);
    ret = access(file_name, F_OK);
    if (ret != 0)
    {
        LOGWARN("[RunScript] file does not exist: %s", file_name);
        return;
    }

    ret = chmod(file_name, 0755);
    if (ret != 0)
    {
        LOGWARN("[RunScript] file permission setting is failed : [%d] %s", ret, file_name);
        return;
    }
    systemcall(file_name);
}

static bool do_package(int action, char* data)
{
    char token[] = ", ";
    char *pkg = NULL;
    char *addon = NULL;
    char pkg_list[MAX_PKGS_BUF];
    size_t remain;

    if (data == NULL) {
        LOGERR("Add on package list is empty");
        return false;
    }

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

    if (addon == NULL) {
        LOGERR("Addon is null");
        return false;
    }

    pkg = strtok(NULL, token);
    while (pkg != NULL) {
        if (action == 1) {
            pkg[strlen(pkg) - 4] = 0; //remove .rpm
        }
        strcat(pkg_list, " ");
        if (action == 2) {
            strcat(pkg_list, PATH_PACKAGE_INSTALL);
            remain = MAX_PKGS_BUF - strnlen(pkg_list, MAX_PKGS_BUF) - strlen("/");
            strncat(pkg_list, addon, remain - 1); // terminating null byte
            strcat(pkg_list, "/");
        }

        remain = MAX_PKGS_BUF - strnlen(pkg_list, MAX_PKGS_BUF) - strlen(" 2>&1");

        if (remain < strnlen(addon, MAX_PKGS_BUF)) {
            LOGERR("Cannot copy package name. Insufficient buf size");
            return false;
        }

        strncat(pkg_list, pkg, remain - 1); // terminating null byte

        pkg = strtok(NULL, token);
    }

    strcat(pkg_list, " ");
    strcat(pkg_list, "2>&1");

    LOGINFO("[cmd] %s", pkg_list);
    if (action == 1 && do_rpm_execute(pkg_list)) {
        return true;
    } else if (action == 2 && do_rpm_execute(pkg_list)) {
        run_script(addon);
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


bool msgproc_package(ijcommand* ijcmd)
{
    _auto_mutex _(&mutex_pkg);
    int ret = 0;
    void* retval = NULL;
    pthread_t pkg_thread_id;

    if (!ijcmd->data) {
        LOGERR("package data is empty.");
        return true;
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
    return true;
}
