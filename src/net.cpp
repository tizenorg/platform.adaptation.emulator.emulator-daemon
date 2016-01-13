/*
 * emulator-daemon
 *
 * Copyright (c) 2013 Samsung Electronics Co., Ltd. All rights reserved.
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
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <glib.h>
#include "emuld.h"
#include <net_connection.h>
#define PROC_CMDLINE_PATH        "/proc/cmdline"
#define IP_SUFFIX                " ip="
#define IJTYPE_GUESTIP           "guest_ip"
#define NETLABEL_PATH            "/smack/netlabel"
#define NETLABEL_BACKUP_PATH     "/tmp/netlabel.bak"
#define DEFAULT_GUEST_IP         "10.0.2.15"
#define DEFAULT_GUEST_GW         "10.0.2.2"
#define MINIMUM_IP_LENGTH        8
#define CONNMAN_NOT_READY       -29424639

void set_guest_addr();
connection_h connection = NULL;
connection_profile_h profile;

static int update_ip_info(connection_profile_h profile,
        connection_address_family_e address_family, gchar **ip)
{
    int rv = 0;

    rv = connection_profile_set_ip_address(profile,
            address_family,
            ip[0]);
    if (rv != CONNECTION_ERROR_NONE)
        return -1;
    LOGINFO("update IP address to %s", ip[0]);

    rv = connection_profile_set_gateway_address(profile,
            address_family,
            ip[2]);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update gateway: %s", ip[2]);
        return -1;
    }

    rv = connection_profile_set_subnet_mask(profile,
            address_family,
            ip[3]);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update subnet: %s", ip[3]);
        return -1;
    }

    rv = connection_profile_set_dns_address(profile,
            1,
            address_family,
            ip[7]);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to update dns: %s", ip[7]);
        return -1;
    }

    return 1;
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
        LOGERR("Fail to get profile iterator [%d]", rv);
        return false;
    }

    while (connection_profile_iterator_has_next(profile_iter)) {
        if (connection_profile_iterator_next(profile_iter, &profile_h) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile handle");
            return false;
        }

        if (connection_profile_get_name(profile_h, &profile_name) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile name");
            return false;
        }
        LOGINFO("profile_name: %s", profile_name);

        if (connection_profile_get_type(profile_h, &profile_type) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile type");
            free(profile_name);
            return false;
        }

        if (connection_profile_get_state(profile_h, &profile_state) != CONNECTION_ERROR_NONE) {
            LOGERR("Fail to get profile state");
            free(profile_name);
            return false;
        }

        if (profile_type == CONNECTION_PROFILE_TYPE_ETHERNET) {
            LOGINFO("[%s] : %s", print_state(profile_state), profile_name);
            profile = profile_h;
            return true;
        }
    }
    LOGERR("Fail to get ethernet profile!");
    return false;
}

static void update_ip_netlabel(const char*guest_ip)
{
    if (strlen(guest_ip) >= MINIMUM_IP_LENGTH && strcmp(guest_ip, DEFAULT_GUEST_IP)) {
        gchar *temp = g_strdup_printf("echo \"%s/32 system::debugging_network\" >> %s", guest_ip, NETLABEL_PATH);
        LOGINFO("updating netlabel for default ip: %s", temp);
        systemcall(temp);
        g_free(temp);
    }
}

static void update_gw_netlabel(const char *guest_gw)
{
    if (strlen(guest_gw) >= MINIMUM_IP_LENGTH && strcmp(guest_gw, DEFAULT_GUEST_GW)) {
        gchar *temp = g_strdup_printf("echo \"%s/32 system::debugging_network\" >> %s", guest_gw, NETLABEL_PATH);
        LOGINFO("updating netlabel for default gateway: %s", temp);
        systemcall(temp);
        g_free(temp);
    }
}

static void update_netlabel(const char *guest_ip, const char *guest_gw)
{
    if (guest_ip) {
        update_ip_netlabel(guest_ip);
    } else {
        LOGERR("cannot update ip address to netlabel. ip is NULL");
    }

    if (guest_gw) {
        update_gw_netlabel(guest_gw);
    } else {
        LOGERR("cannot update gateway address to netlabel. gateway is NULL");
    }
}

static void send_guest_ip_req(const char *guest_ip)
{
    LXT_MESSAGE *packet = (LXT_MESSAGE *)calloc(1 ,sizeof(LXT_MESSAGE));
    if (packet == NULL) {
        return;
    }

    packet->length = strlen(guest_ip);
    packet->group = 0;
    packet->action = STATUS;

    const int tmplen = HEADER_SIZE + packet->length;
    char *tmp = (char *)malloc(tmplen);
    if (!tmp) {
        if (packet)
            free(packet);
        return;
    }

    memcpy(tmp, packet, HEADER_SIZE);
    memcpy(tmp + HEADER_SIZE, guest_ip, packet->length);

    LOGINFO("send guest IP to host: %s", guest_ip);
    ijmsg_send_to_evdi(g_fd[fdtype_device], IJTYPE_GUESTIP, (const char*) tmp, tmplen);


    if (tmp)
        free(tmp);
    if (packet)
        free(packet);
}

static char *get_gateway_address()
{
    char *gateway = NULL;
    int rv = 0;

    if (get_profile() == false) {
        return NULL;
    }

    connection_profile_get_gateway_address(profile, CONNECTION_ADDRESS_FAMILY_IPV4, &gateway);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Fail to get gateway");
        return NULL;
    }
    LOGINFO("gateway: %s", gateway);
    return gateway;
}

static void ip_changed_cb(const char* ipv4_address, const char* ipv6_address, void* user_data)
{
    LOGINFO("IP changed callback, IPv4 address : %s, IPv6 address : %s",
            ipv4_address, (ipv6_address ? ipv6_address : "NULL"));
    char *gateway = get_gateway_address();
    update_netlabel(ipv4_address, gateway);
    send_guest_ip_req(ipv4_address);
    if (gateway) {
        free(gateway);
    }
}

static int update_network_info(connection_profile_h profile, gchar **ip)
{
    int rv = 0;
    rv = connection_profile_set_ip_config_type(profile,
            CONNECTION_ADDRESS_FAMILY_IPV4,
            CONNECTION_IP_CONFIG_TYPE_STATIC);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Failed to connection_profile_set_ip_config_type() : %d", rv);
        return -1;
    }

    if (update_ip_info(profile, CONNECTION_ADDRESS_FAMILY_IPV4, ip) == -1)
        return -1;

    rv = connection_update_profile(connection, profile);
    if (rv != CONNECTION_ERROR_NONE) {
        LOGERR("Failed to update profile: %d", rv);
        return -1;
    }
    update_netlabel(ip[0], ip[2]);
    send_guest_ip_req(ip[0]);
    return 1;
}

static int update_connection(gchar **ip)
{
    if (get_profile() == false) {
        return -1;
    }

    if (update_network_info(profile, ip) == -1) {
        return -1;
    }

    return 1;
}

void *register_connection(void* data)
{
    int ret;
    while ((ret = connection_create(&connection)) == CONNMAN_NOT_READY) {
        usleep(100);
    }
    if (CONNECTION_ERROR_NONE == ret) {
        LOGINFO("connection_create() success!: [%p]", connection);
        connection_set_ip_address_changed_cb(connection, ip_changed_cb, NULL);
        set_guest_addr();
    } else {
        LOGERR("Client registration failed %d", ret);
        return NULL;
    }
    LOGINFO("network connection pthread is quitting.");
    return NULL;
}

void destroy_connection(void)
{
    if (connection != NULL) {
        connection_destroy(connection);
        connection = NULL;
    }
    connection_profile_destroy(profile);
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


static int get_str_cmdline(char *src, const char *dest, char str[], int str_size)
{
    char *s = strstr(src, dest);
    if (s == NULL) {
        return -1;
    }
    /* remove first blank */
    char *new_s = s + 1;
    char *e = strstr(new_s, " ");
    if (e == NULL) {
        return -1;
    }
    int len = e - s - strlen(dest);

    LOGINFO("len: %d", len);
    if (len >= str_size) {
        LOGERR("buffer size(%d) should be bigger than %d", str_size, len+1);
        return -1;
    }

    s_strncpy(str, s + strlen(dest), len);
    return len;
}

static int get_network_info(char str[], int str_size)
{
    size_t len = 0;
    char *line = NULL;
    FILE *fp = fopen(PROC_CMDLINE_PATH, "r");
    int ret = 0;

    if (fp == NULL) {
        LOGERR("failed to open /proc/cmdline");
        return -1;
    }

    if (getline(&line, &len, fp) != -1) {
        LOGINFO("line: %s", line);
        LOGINFO("len: %d", len);
    }

    if (!line)
    {
        LOGERR("failed to read /proc/cmdline");
        fclose(fp);
        return -1;
    }

    if (get_str_cmdline(line, IP_SUFFIX, str, str_size) < 1) {
        LOGINFO("could not get the (%s) value from cmdline. static ip does not set.", IP_SUFFIX);
        ret = -1;
    }

    free(line);
    fclose(fp);

    LOGINFO("succeeded to get guest_net: %s", str);

    return ret;
}

void set_guest_addr()
{
    int fd;
    struct ifreq ifrq;
    struct sockaddr_in *sin;
    char guest_net[1024] = {0,};
    if (get_network_info(guest_net, sizeof guest_net) == 0) {
        /* use static IP */
        char *str = strdup(guest_net);
        gchar **ip = g_strsplit(str, ":", -1);
        if (g_strv_length(ip) > 7) {
            if (update_connection(ip) == 1) {
                LOGINFO("Succeed to update connection");
            } else {
                LOGERR("failed to update connection");
            }
        } else {
            LOGERR("static IP information is incomplete. need more information.");
        }
        /* If use static IP address, need notification to connman to update it */
        g_strfreev(ip);
        free(str);
    } else {
        /* use DHCP */
        char *gateway = NULL;
        LOGINFO("try to get guest ip from eth0");
        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd < 0)
        {
            LOGERR("Socket open error");
            return;
        }
        strncpy(ifrq.ifr_name, "eth0", sizeof("eth0"));
        while (ioctl(fd, SIOCGIFADDR, &ifrq) < 0) {
            /* while to get ip address */
            usleep(100);
        }
        sin = (struct sockaddr_in *)&ifrq.ifr_addr;
        LOGINFO("use dynamic IP. do not need update network information.");
        close(fd);
        gateway = get_gateway_address();
        if (gateway) {
            update_netlabel(inet_ntoa(sin->sin_addr), gateway);
        }
        send_guest_ip_req(inet_ntoa(sin->sin_addr));
    }
}
