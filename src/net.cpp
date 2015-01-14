/*
 * emulator-daemon
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
 * Munkyu Im <munkyu.im@samsnung.com>
 * Sangho Park <sangho1206.park@samsung.com>
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
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>

#include "emuld.h"
#include "net_connection.h"

#define PROC_CMDLINE_PATH "/proc/cmdline"
#define IP_SUFFIX "ip="
#define IJTYPE_GUESTIP "guest_ip"

pthread_t g_main_thread;
char g_guest_ip[64];
char g_guest_subnet[64];
char g_guest_gw[64];
char g_guest_dns[64];
connection_h connection = NULL;
connection_profile_h profile;
GMainLoop *mainloop;

static void send_guest_ip_req(void);

static int update_ip_info(connection_profile_h profile,
        connection_address_family_e address_family)
{
    int rv = 0;

    rv = connection_profile_set_ip_address(profile,
            address_family,
            g_guest_ip);
    if (rv != CONNECTION_ERROR_NONE)
        return -1;
    LOGINFO("update IP address to %s\n", g_guest_ip);
    rv = connection_profile_set_subnet_mask(profile,
            address_family,
            g_guest_subnet);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update subnet: %s\n", g_guest_subnet);
        return -1;
    }

    rv = connection_profile_set_gateway_address(profile,
            address_family,
            g_guest_gw);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update gateway: %s\n", g_guest_gw);
        return -1;
    }

    rv = connection_profile_set_dns_address(profile,
            1,
            address_family,
            g_guest_dns);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update dns: %s\n", g_guest_dns);
        return -1;
    }

    return 1;
}

static void ip_changed_cb(const char* ipv4_address, const char* ipv6_address, void* user_data)
{
    LOGINFO("IP changed callback, IPv4 address : %s, IPv6 address : %s\n",
            ipv4_address, (ipv6_address ? ipv6_address : "NULL"));
    strcpy(g_guest_ip, ipv4_address);
    send_guest_ip_req();
}

static const char *print_state(connection_profile_state_e state)
{
    switch (state) {
    case CONNECTION_PROFILE_STATE_DISCONNECTED:
        return "Disconnected";
    case CONNECTION_PROFILE_STATE_ASSOCIATION:
        return "Association";
    case CONNECTION_PROFILE_STATE_CONFIGURATION:
        return "Configuration";
    case CONNECTION_PROFILE_STATE_CONNECTED:
        return "Connected";
    default:
        return "Unknown";
    }
}

static bool get_profile()
{
    int rv = 0;
    char *profile_name;
    connection_profile_type_e profile_type;
    connection_profile_state_e profile_state;
    connection_profile_iterator_h profile_iter;
    connection_profile_h profile_h;

    rv = connection_get_profile_iterator(connection, CONNECTION_ITERATOR_TYPE_REGISTERED, &profile_iter);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to get profile iterator [%d]\n", rv);
        return false;
    }

    while (connection_profile_iterator_has_next(profile_iter)) {
        if (connection_profile_iterator_next(profile_iter, &profile_h) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile handle\n");
            return false;
        }

        if (connection_profile_get_name(profile_h, &profile_name) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile name\n");
            return false;
        }
        LOGINFO("profile_name: %s\n", profile_name);

        if (connection_profile_get_type(profile_h, &profile_type) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile type\n");
            g_free(profile_name);
            return false;
        }

        if (connection_profile_get_state(profile_h, &profile_state) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile state\n");
            g_free(profile_name);
            return false;
        }

        if (profile_type == CONNECTION_PROFILE_TYPE_ETHERNET) {
            LOGINFO("[%s] : %s\n", print_state(profile_state), profile_name);
            profile = profile_h;
            return true;
        }
    }
    LOGERR("Fail to get ethernet profile!\n");
    return false;
}


static int update_network_info(connection_profile_h profile)
{
    int rv = 0;
    char *interface_name;
    rv = connection_profile_set_ip_config_type(profile,
            CONNECTION_ADDRESS_FAMILY_IPV4,
            CONNECTION_IP_CONFIG_TYPE_STATIC);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Failed to connection_profile_set_ip_config_type() : %d\n", rv);
        return -1;
    }

    rv = connection_profile_get_network_interface_name(profile, &interface_name);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Failed to get interface name: %d\n", rv);
        return -1;
    }

    LOGINFO("interface name: %s\n", interface_name);
    if (update_ip_info(profile, CONNECTION_ADDRESS_FAMILY_IPV4) == -1)
        return -1;

    rv = connection_update_profile(connection, profile);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Failed to update profile: %d\n", rv);
        return -1;
    }
    send_guest_ip_req();
    return 1;
}

static int update_connection()
{
    int rv = 0;
    if (get_profile() == false) {
        return -1;
    }

    if (update_network_info(profile) == -1) {
        return -1;
    }
    rv = connection_update_profile(connection, profile);
    if (rv != CONNECTION_ERROR_NONE)
        return -1;

    return 1;
}

static void send_guest_ip_req(void)
{
    LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
    if (packet == NULL){
        return;
    }
    memset(packet, 0, sizeof(LXT_MESSAGE));

    packet->length = strlen(g_guest_ip);
    packet->group = 0;
    packet->action = 15;

    const int tmplen = HEADER_SIZE + packet->length;
    char* tmp = (char*) malloc(tmplen);
    if (!tmp) {
        if (packet)
            free(packet);
        return;
    }

    memcpy(tmp, packet, HEADER_SIZE);
    memcpy(tmp + HEADER_SIZE, g_guest_ip, packet->length);

    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_GUESTIP, (const char*) tmp, tmplen);


    if (tmp)
        free(tmp);
    if (packet)
        free(packet);
}

void *g_main_thread_cb(void *arg)
{
    mainloop = g_main_loop_new(NULL, FALSE);

    int err = connection_create(&connection);
    if (CONNECTION_ERROR_NONE == err) {
        LOGINFO("connection_create() success!: [%p]", connection);
        connection_set_ip_address_changed_cb(connection, ip_changed_cb, NULL);
    } else {
        LOGERR("Client registration failed %d\n", err);
        return NULL;
    }
    get_guest_addr();
    g_main_loop_run(mainloop);
    return NULL;
}

int register_connection()
{
    LOGINFO("register_connection\n");

    if(pthread_create(&g_main_thread, NULL, g_main_thread_cb, NULL) != 0) {
        LOGERR("fail to create g_main_thread!\n");
        return -1;
    }
    return 1;
}

void destroy_connection()
{
    if (connection != NULL) {
        connection_destroy(connection);
    }
    connection_profile_destroy(profile);
    g_main_loop_quit(mainloop);
    pthread_detach(g_main_thread);
}

static char *s_strncpy(char *dest, const char *source, size_t n)
{
    char *start = dest;

    while (n && (*dest++ = *source++)) {
        n--;
    }
    if (n) {
        while (--n) {
            *dest++ = '\0';
        }
    }
    return start;
}


static int get_str_cmdline(char *src, char *dest, char str[], int str_size)
{
    char *s = strstr(src, dest);
    if (s == NULL) {
        return -1;
    }
    char *e = strstr(s, " ");
    if (e == NULL) {
        return -1;
    }

    int len = e-s-strlen(dest);

    if (len >= str_size) {
        LOGERR("buffer size(%d) should be bigger than %d\n", str_size, len+1);
        return -1;
    }

    s_strncpy(str, s + strlen(dest), len);
    return len;
}

static int get_network_info(char str[], int str_size)
{
    size_t len = 0;
    ssize_t read;
    char *line = NULL;
    FILE *fp = fopen(PROC_CMDLINE_PATH, "r");

    if (fp == NULL) {
        LOGERR("fail to read /proc/cmdline\n");
        return -1;
    }
    while((read = getline(&line, &len, fp)) != -1) {
        LOGINFO("line: %s\n", line);
        LOGINFO("len: %d\n", len);
    }
    if (get_str_cmdline(line, IP_SUFFIX, str, str_size) < 1) {
        LOGINFO("could not get the (%s) value from cmdline\n", IP_SUFFIX);
        fclose(fp);
        return -1;
    }
    fclose(fp);
    return 0;
}

void get_guest_addr()
{
    int fd;
    struct ifreq ifrq;
    struct sockaddr_in *sin;
    char guest_net[256] = {0,};
    if (get_network_info(guest_net, sizeof guest_net) == 0) {
        char *token;
        int i = 0;
        char *str = strdup(guest_net);
        while ((token = strsep(&str, ":"))) {
            if (i == 0) {
                memset(&g_guest_ip[0], 0, sizeof(g_guest_ip));
                strncpy(g_guest_ip, token, strlen(token));
                LOGINFO("set guest_ip: %s\n", g_guest_ip);
            } else if(i == 2) {
                strncpy(g_guest_gw, token, strlen(token));
            } else if(i == 3) {
                strncpy(g_guest_subnet, token, strlen(token));
            } else if(i == 7) {
                strncpy(g_guest_dns, token, strlen(token));
            }
            LOGINFO("token[%d]: %s\n",i++, token);
        }
        free(str);
    } else {
        fd = socket (AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            perror("Socket open error");
            return;
        }
        strcpy(ifrq.ifr_name, "eth0");
        if (ioctl(fd, SIOCGIFADDR, &ifrq) < 0)
        {
            perror("IPADDR Error");
            close(fd);
            return;
        }
        sin = (struct sockaddr_in *)&ifrq.ifr_addr;
        LOGINFO("IPADDR : %s\n", inet_ntoa(sin->sin_addr));
        strncpy(g_guest_ip,  inet_ntoa(sin->sin_addr), strlen(inet_ntoa(sin->sin_addr)));
        LOGINFO("set guest_ip: %s\n", g_guest_ip);

        close(fd);
    }
    if (update_connection() == 1) {
        LOGINFO("Succeed to update connection\n");
    }
}

