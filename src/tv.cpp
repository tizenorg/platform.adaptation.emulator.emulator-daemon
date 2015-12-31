/*
 * emulator-daemon
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Jinhyung Choi <jinhyung2.choi@samsnung.com>
 * DaiYoung Kim <daiyoung777.kim@samsnung.com>
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

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <vconf/vconf.h>
#include <vconf/vconf-keys.h>

#include "emuld.h"
#include "tv.h"
//#include "si_service_native.h"
//#include "si_service_signal.h"

#ifdef SI_ENABLE

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

static int motion_handle = 0;
static int INIT_SERVER = 0;

void init_gesture()
{
    /* initialize gesture structure */
}


int gesture_onehand(void)
{
    init_gesture();

    //EmitSignal(SIGNAL_MOTION_START, NULL);
    EmitSignal(SIGNAL_MOTION_BEGIN_MONITOR, NULL);
    //EmitSignal(SIGNAL_MOTION_FUN_GESTURE_THUMBUP, NULL);
    return 0;
}

int gesture_twohand(void)
{
    //EmitSignal(SIGNAL_MOTION_START, NULL);
    return 0;
}

void gesture_end(void)
{

    EmitSignal(SIGNAL_MOTION_END_MONITOR, NULL);
}


void gesture_return(void)
{
//  EmitSignal(SIGNAL_MOTION_ZOOM, (void *)val);

}


void gesture_slapleft(void)
{
    EmitSignal(SIGNAL_MOTION_SLAP_LEFT, NULL);
}


void gesture_slapright(void)
{

    EmitSignal(SIGNAL_MOTION_SLAP_RIGHT, NULL);
}

void gesture_circle(void)
{

    EmitSignal(SIGNAL_MOTION_FUN_GESTURE_THUMBUP, NULL);
}

void gesture_rotate(int data)
{
    //int p_angle[1];
    //*p_angle = data;
    //printf(" Rotation : angle=%d  \n", *p_angle);
    EmitSignal(SIGNAL_MOTION_ROTATE, (const void*)&data);
    //EmitSignal(SIGNAL_MOTION_ROTATE, (const void*)p_angle);
}

void gesture_zoom(int data)
{
    //int val;
    //val = data;
    EmitSignal(SIGNAL_MOTION_ZOOM, (const void *)&data);
}

char *s1 = (char *)calloc(sizeof(char), 10);

void si_get_cmd_list(const si_voice_cmd_item_s *cmdList, const int currentCnt)
{

    //if(NULL != cmdList && currentCnt > 0)
    //{
    //  printf("cmd list, cnt=%d, list=%s\n", currentCnt, cmdList[0].excute_command);
    //}
    //return cmdList[0].excute_command;
}

void si_get_bgcmd_list(const char** bg_cmd_list, const int currentCnt)
{
    //if(NULL != bg_cmd_list && currentCnt > 0)
    //{
    //  printf("bg cmd list, cnt=%d, list=%s\n", currentCnt, bg_cmd_list[0]);
    //}
    //return bg_cmd_list[0];
}

int send_hi_tv()
{
    //printf("Emit begin_monitor signal to SI server : HI TV\n");

    /* Begin voice recognition */
    EmitSignal(SIGNAL_VOICE_BEGIN_MONITOR, NULL);

    return 0;
}

int get_voice_cmd()
{

    //printf("Set voice callback function! \n");
    SetCmdChangedCallback(si_get_cmd_list);
    SetBgCmdChangedCallback(si_get_bgcmd_list);

    return 0;
}

int send_guide_me()
{

    sprintf(s1, "%s", "guide me");

    si_voice_result_data_s t_voice;
    t_voice.handle = 0;
    t_voice.m_Domain = 0;
    t_voice.m_Data = s1;

    //printf("Emit signal to SI server : %s\n", s1);

    EmitSignal(SIGNAL_VOICE_CUSTOM_RECOG_RESULT, &t_voice);

    return 0;
}

int send_describe()
{

    sprintf(s1, "%s", "describe");

    si_voice_result_data_s t_voice;
    t_voice.handle = 0;
    t_voice.m_Domain = 0;
    t_voice.m_Data = s1;

    //printf("Emit signal to SI server : %s\n", s1);

    EmitSignal(SIGNAL_VOICE_DEFAULT_RECOG_RESULT, &t_voice);

    return 0;
}

int send_left()
{

    sprintf(s1, "%s", "left");

    si_voice_result_data_s t_voice;
    t_voice.m_Domain = 20;
    t_voice.m_Data = s1;

    //printf("Emit signal to SI server : %s\n", s1);

    EmitSignal(SIGNAL_VOICE_DEFAULT_RECOG_RESULT, &t_voice);

    return 0;
}

int send_right()
{

    sprintf(s1, "%s", "right");

    si_voice_result_data_s t_voice;
    t_voice.m_Domain = 20;
    t_voice.m_Data = s1;

    //printf("Emit signal to SI server : %s\n", s1);

    EmitSignal(SIGNAL_VOICE_DEFAULT_RECOG_RESULT, &t_voice);

    return 0;
}

int send_close()
{
    //printf("Emit signal to SI server : close\n");

    EmitSignal(SIGNAL_VOICE_END_MONITOR, NULL);

    return 0;
}
#endif
bool msgproc_gesture(ijcommand* ijcmd)
{
#ifdef SI_ENABLE
    int command, ret, data;
    const int tmpsize = ijcmd->msg.length;

    char* data_p = NULL;
    char token[] = "\n";
    char tmpdata[tmpsize+1];
    char* s_cnt = NULL;
    int cnt = 0;

    if (!SI_INIT) {
        ret = InitServer();

        if (ret) {
            SI_INIT = true;
        } else {
            SI_INIT = false;
            LOGERR("Failed to init server %s", ret);
        }
    }

    memset(tmpdata, 0, tmpsize+1);
    memcpy(tmpdata, ijcmd->data, tmpsize);

    s_cnt = strtok(tmpdata, token);
    cnt = atoi(s_cnt);

    while(data_p = strtok(NULL, token)) {
        data = atoi(data_p);
    }
    printf("msgproc_gesture action : %d\n", ijcmd->msg.action);

    command = ijcmd->msg.action;

    switch(command) {
        case ONE_HAND :
            gesture_onehand();
            break;
        case TWO_HAND :
            gesture_twohand();
            break;
        case END_HAND :
            gesture_end();
            break;
        case RETURN_HAND :
            gesture_return();
            break;
        case SLAP_LEFT :
            gesture_slapleft();
            break;
        case SLAP_RIGHT :
            gesture_slapright();
            break;
        case CIRCLE :
            gesture_circle();
            break;
        case ROTATE :
            gesture_rotate(data);
            break;
        case ZOOM :
            gesture_zoom(data);
            break;
        default :
            LOGERR("invalid command");
    }
#endif
    return true;
}

bool msgproc_voice(ijcommand* ijcmd)
{
#ifdef SI_ENABLE
    int command, ret;

    if (!SI_INIT) {
        ret = InitServer();

        if (ret) {
            SI_INIT = true;
        } else {
            LOGERR("Failed to init server %s", ret);
        }
    }
    printf("msgproc_voice() action:%d\n", ijcmd->msg.action);
    //parsing cmd
    command = ijcmd->msg.action;

    switch(command) {
        case HI_TV :
            get_voice_cmd();
            send_hi_tv();
            break;
        case GUIDE_ME :
            send_guide_me();
            break;
        case DESCRIBE :
            send_describe();
            break;
        case LEFT :
            send_left();
            break;
        case RIGHT :
            send_right();
            break;
        case V_CLOSE :
        //  SI_INIT = false;
            send_close();
            break;
        default :
            printf("ERROR : incorrect voice-command\n");
    }
/*
    int sslen = sizeof(si_sensord_other);

    if (ijcmd->msg.group == STATUS)
    {
        setting_device_param* param = new setting_device_param();
        if (!param)
            return false;

        memset(param, 0, sizeof(param));

        param->get_status_sockfd = sockfd;
        param->ActionID = ijcmd->msg.action;
        param->is_evdi = is_evdi;
        memcpy(param->type_cmd, ijcmd->cmd, ID_SIZE);

        if (pthread_create(&tid[2], NULL, setting_device, (void*)param) != 0)
        {
            LOG("pthread create fail!");
            return false;
        }

    }
    else if (sendto(g_fd[fdtype_sensor], ijcmd->data, ijcmd->msg.length, 0,
            (struct sockaddr*) &si_sensord_other, sslen) == -1)
    {
        LOG("sendto error!");
        return false;
    } */
#endif
    return true;
}

enum {
    EXTINPUT_HDMI1 = 1,
    EXTINPUT_HDMI2,
    EXTINPUT_HDMI3,
    EXTINPUT_HDMI4,
    EXTINPUT_AV1,
    EXTINPUT_AV2,
    EXTINPUT_COMP1,
    EXTINPUT_COMP2,
};
#define STATE_CHECKER       0xffff
#define STATE_SHIFT         16
#define MAX_EXTINPUT_COUNT  8
#define EXTINPUT_GET_INFO   2
#define HDMI_CNT            4
#define AV_CNT              2
#define COMP_CNT            1
static unsigned int extinput[MAX_EXTINPUT_COUNT];
bool msgproc_extinput(ijcommand* ijcmd)
{
    char command[128];
    memset(command, '\0', sizeof(command));

    if (ijcmd->msg.action == EXTINPUT_GET_INFO) {
        int i = 0;
        char *tmp;
        int tmplen = HEADER_SIZE + 16;
        int onoff_state = 0;
        LXT_MESSAGE *packet = (LXT_MESSAGE *)malloc(sizeof(LXT_MESSAGE));
        if (packet) {
            memset(packet, 0, sizeof(LXT_MESSAGE));
        }
        /* get hdmi plugged state */
        for (i = 0; i < HDMI_CNT; i++) {
            sprintf(command, "memory/sysman/hdmi%d", i+1);
            if (vconf_get_int(command, &onoff_state)) {
                onoff_state = 0;
            }
            extinput[i] = onoff_state;
        }
        /* get av plugged state */
        for (i = 0; i < AV_CNT; i++) {
            sprintf(command, "memory/sysman/av%d", i+1);
            if (vconf_get_int(command, &onoff_state)) {
                onoff_state = 0;
            }
            extinput[i + HDMI_CNT] = onoff_state;
        }
        /* get comp plugged state */
        for (i = 0; i < COMP_CNT; i++) {
            sprintf(command, "memory/sysman/comp%d", i+1);
            if (vconf_get_int(command, &onoff_state)) {
                onoff_state = 0;
            }
            extinput[i + HDMI_CNT + AV_CNT] = onoff_state;
        }

        /* send current state to Emulator Control Panel */
        tmp = (char *)malloc(tmplen);
        if (tmp && packet) {
            memset(command, 0, sizeof(command));
            packet->length = 16;
            packet->group = 15;
            packet->action = 211;
            memcpy(tmp, packet, HEADER_SIZE);
            sprintf(command, "%d:%d:%d:%d:%d:%d:%d:%d:",
                    extinput[0], extinput[1], extinput[2], extinput[3], extinput[4], extinput[5], extinput[6], extinput[7]);
            memcpy(tmp + HEADER_SIZE, command, packet->length);
            ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_EI, (const char *)tmp, tmplen);
            free(tmp);
        }

        if (packet) {
            free(packet);
            packet = NULL;
        }
    } else {
        char plugged[128];
        char token[] = ":";
        char *section = NULL;
        unsigned int number = 0;
        unsigned int state = 0;
        memset(plugged, '\0', sizeof(plugged));
        memcpy(plugged, ijcmd->data, ijcmd->msg.length);

        section = strtok(plugged, token);
        number = (unsigned int)atoi(section);
        section = strtok(NULL, token);
        state = (unsigned int)atoi(section);

        if (number > MAX_EXTINPUT_COUNT) {
            LOGDEBUG("external_input number : %d is invalid.", number);
            return false;
        }

        switch (number) {
        case EXTINPUT_HDMI1:
        case EXTINPUT_HDMI2:
        case EXTINPUT_HDMI3:
        case EXTINPUT_HDMI4:
            sprintf(command, "vconftool set -t int memory/sysman/hdmi%d %d -i -f", number, state);
            break;
        case EXTINPUT_AV1:
            sprintf(command, "vconftool set -t int memory/sysman/av1 %d -i -f", state);
            break;
        case EXTINPUT_AV2:
            sprintf(command, "vconftool set -t int memory/sysman/av2 %d -i -f", state);
            break;
        case EXTINPUT_COMP1:
            sprintf(command, "vconftool set -t int memory/sysman/comp1 %d -i -f", state);
            break;
        case EXTINPUT_COMP2:
            sprintf(command, "vconftool set -t int memory/sysman/comp2 %d -i -f", state);
            break;
        }
        extinput[number-1] = state;
        systemcall(command);
    }

    return true;
}

bool extra_evdi_command(ijcommand* ijcmd) {

    if (strncmp(ijcmd->cmd, IJTYPE_GESTURE, 7) == 0)
    {
        msgproc_gesture(ijcmd);
        return true;
    }
    else if (strncmp(ijcmd->cmd, IJTYPE_VOICE, 5) == 0)
    {
        msgproc_voice(ijcmd);
        return true;
    }
	else if (strncmp(ijcmd->cmd, IJTYPE_EI, 2) == 0)
	{
		msgproc_extinput(ijcmd);
		return true;
	}

    return false;
}

void add_vconf_map_profile(void)
{
    /* tv */
    add_vconf_map(TV, VCONF_HDMI1);
    add_vconf_map(TV, VCONF_HDMI2);
    add_vconf_map(TV, VCONF_HDMI3);
    add_vconf_map(TV, VCONF_HDMI4);
    add_vconf_map(TV, VCONF_AV1);
    add_vconf_map(TV, VCONF_AV2);
    add_vconf_map(TV, VCONF_COMP1);
    add_vconf_map(TV, VCONF_COMP2);
}
