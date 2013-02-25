/* -*- Mode: C; c-basic-offset: 8; indent-tabs-mode: t -*-
 * emulator-daemon
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact:
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

/*-----------------------------------------------------------------

epoll server program by Sungmin Ha.
Platform : Linux 2.6.x (kernel)
compiler Gcc: 3.4.3. 
License: GNU General Public License   

------------------------------------------------------------------*/
#include "emuld_common.h"
#include "emuld.h"

/* global definition */
unsigned short sdbd_port = SDBD_PORT;
unsigned short vmodem_port = VMODEM_PORT;
unsigned short gpsd_port = GPSD_PORT;
unsigned short sensord_port = SENSORD_PORT;

int g_svr_sockfd;              	/* global server socket fd */
int g_svr_port;                	/* global server port number */
int g_vm_sockfd; 		/* vmodem fd */
int g_sdbd_sockfd = -1;		/* sdbd fd */
static int g_vm_connect_status;	/* connection status between emuld and vmodem  */
int g_sdcard_sockfd = -1;
int g_get_status_sockfd = -1;   /* return status of devive to the injector */

pthread_t tid[MAX_CLIENT + 1];

static struct timeval tv_start_poweroff;

/* udp socket */
struct sockaddr_in si_sensord_other, si_gpsd_other;
int uSensordFd, uGpsdFd, sslen=sizeof(si_sensord_other), sglen=sizeof(si_gpsd_other);

struct {
	int  cli_sockfd;  				/* client socket fds */
	unsigned short cli_port;              /* client connection port */
} g_client[MAX_CLIENT];

int g_epoll_fd;                		/* epoll fd */

struct epoll_event g_events[MAX_EVENTS]; 

void TAPIMessageInit(LXT_MESSAGE *packet)
{       
	packet->length = 0;
	packet->group = 0;
	packet->action = 0;
	packet->data = 0;
}

char SDpath[256];

unsigned char ActionID = 0;
char GetBuffer[256];
void setting_device(); 
/*--------------------------------------------------------------*/
/* FUNCTION PART 
   ---------------------------------------------------------------*/
/*---------------------------------------------------------------
function : init_data0
io: none
desc: initialize global client structure values
----------------------------------------------------------------*/
void init_data0(void)
{
	register int i;

	for(i = 0 ; i < MAX_CLIENT ; i++) {
		g_client[i].cli_sockfd = -1;
	}
}

/*------------------------------------------------------------- 
function: init_server0 
io: input : integer - server port (must be positive)
output: none
desc : tcp/ip listening socket setting with input variable
----------------------------------------------------------------*/

void init_server0(int svr_port)
{
	struct sockaddr_in serv_addr;

	/* Open TCP Socket */
	if( (g_svr_sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0 ) 
	{
		fprintf(stderr, "Server Start Fails. : Can't open stream socket \n");
		exit(0);
	}

	/* Address Setting */
	memset( &serv_addr , 0 , sizeof(serv_addr)) ; 

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(svr_port);

	/* Set Socket Option  */
	int nSocketOpt = 1;
	if( setsockopt(g_svr_sockfd, SOL_SOCKET, SO_REUSEADDR, &nSocketOpt, sizeof(nSocketOpt)) < 0 )
	{
		fprintf(stderr, "Server Start Fails. : Can't set reuse address\n");
		close(g_svr_sockfd); 
		exit(0);
	}

	/* Bind Socket */
	if(bind(g_svr_sockfd,(struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
	{
		fprintf(stderr, "Server Start Fails. : Can't bind local address\n");
		close(g_svr_sockfd);
		exit(0);
	}

	/* Listening */
	listen(g_svr_sockfd,15); /* connection queue is 15. */

	LOG("[START] Now Server listening on port %d, EMdsockfd: %d"
			,svr_port, g_svr_sockfd);
} 
/*------------------------------- end of function init_server0 */

void* init_vm_connect(void* data)
{
	struct sockaddr_in vm_addr;
	int ret = -1;	
	g_vm_connect_status = 0;

	LOG("start");

	pthread_detach(pthread_self());
	/* Open TCP Socket */
	if( (g_vm_sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0 ) 
	{
		fprintf(stderr, "Server Start Fails. : Can't open stream socket \n");
		exit(0);
	}

	/* Address Setting */
	memset( &vm_addr , 0 , sizeof(vm_addr)) ; 

	vm_addr.sin_family = AF_INET;
	vm_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	vm_addr.sin_port = htons(vmodem_port);

	while(ret < 0)
	{
		ret = connect(g_vm_sockfd, (struct sockaddr *)&vm_addr, sizeof(vm_addr));

		LOG("vm_sockfd: %d, connect ret: %d", g_vm_sockfd, ret);

		if(ret < 0) {
			LOG("connection failed to vmodem!");		
			sleep(1);
		}
	}
	
	userpool_add(g_vm_sockfd, vm_addr.sin_port);

	/* add client sockfd at event pool */
	epoll_cli_add(g_vm_sockfd);
	
	g_vm_connect_status = 1;
	pthread_exit((void *) 0); 
}

int is_mounted()
{
	int ret = -1, i = 0;
	struct stat buf;
	char file_name[128];
	memset(file_name, '\0', sizeof(file_name));

	for(i = 0; i < 10; i++)
	{
		sprintf(file_name, "/dev/emul_mmcblk%d", i);
		ret = access( file_name, F_OK );
		if( ret == 0 )
		{
			lstat(file_name, &buf);
			if(S_ISBLK(buf.st_mode))
				return 1;
			else
				return 0;
		}
	}

	return 0;
}

void* mount_sdcard(void* data)
{
	int ret = -1, i = 0, vconf_value = -1;	
	struct stat buf;
	char file_name[128], command[256];
	memset(file_name, '\0', sizeof(file_name));
	memset(command, '\0', sizeof(command));

	LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
	memset(packet, 0, sizeof(LXT_MESSAGE));

	LOG("start sdcard mount thread");

	pthread_detach(pthread_self());

	while(ret < 0)
	{	
		for(i = 0; i < 10; i++)
		{
			sprintf(file_name, "/dev/emul_mmcblk%d", i);
			ret = access( file_name, F_OK );
			if( ret == 0 )
			{
				lstat(file_name, &buf);
				if(!S_ISBLK(buf.st_mode))
				{
					sprintf(command, "rm -rf %s", file_name);
					system(command);
				}
				else
					break;
			}
		}

		if( i != 10 )
		{
			LOG( "%s is exist", file_name);
			ret = mount(file_name, "/mnt/mmc", "ext3", 0, "");
			LOG("mount ret = %d, errno = %d", ret, errno);
			
			LOG("sdcard fd: %d", g_sdcard_sockfd);
			if(g_sdcard_sockfd != -1)
			{
				packet->length = strlen(SDpath);		// length
				packet->group = 11;				// sdcard
				if(ret == 0)
					packet->action = 1;	// mounted
				else
					packet->action = 5;	// failed

				send(g_sdcard_sockfd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);
				LOG("SDpath is %s", SDpath);
				send(g_sdcard_sockfd, SDpath, packet->length, 0);
				
				if(ret == 0)
				{
					system("chmod -R 777 /opt/storage/sdcard");
					system("vconftool set -t int memory/sysman/mmc 1 -i -f");
				}
			}

			break;
		}
		else
		{
			LOG( "%s is not exist", file_name);
			sleep(1);	
		}		
	}

	pthread_exit((void *) 0); 
} 

int umount_sdcard(void)
{
	int ret = -1, i = 0;	
	char file_name[128];
	memset(file_name, '\0', sizeof(file_name));
	LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
	if(packet == NULL){
	    return ret;
	}
	memset(packet, 0, sizeof(LXT_MESSAGE));

	LOG("start sdcard umount");

	pthread_cancel(tid[1]);

	for(i = 0; i < 10; i++)
	{
		sprintf(file_name, "/dev/emul_mmcblk%d", i);
		ret = access( file_name, F_OK);
		if ( ret == 0 )
		{
			LOG( "%s is exist", file_name);
			ret = umount("/mnt/mmc");
			LOG("umount ret = %d, errno = %d", ret, errno);
			
			LOG("sdcard fd: %d", g_sdcard_sockfd);
			if(g_sdcard_sockfd != -1)
			{
				packet->length = strlen(SDpath);		// length
				packet->group = 11;				// sdcard
				if(ret == 0)
					packet->action = 0;				// unmounted
				else
					packet->action = 4;				// failed

				send(g_sdcard_sockfd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);
				LOG("SDpath is %s", SDpath);
				send(g_sdcard_sockfd, SDpath, packet->length, 0);
				
				if(ret == 0)
				{
					memset(SDpath, '\0', sizeof(SDpath));
					sprintf(SDpath, "umounted");
					system("vconftool set -t int memory/sysman/mmc 0 -i -f");
				}				
			}

			break;
		}
		else
		{
			LOG( "%s is not exist", file_name);
		}
	}

	if(packet){
	    free(packet);
	    packet = NULL;
	}
	return ret;
} 

void epoll_init(void)
{
	struct epoll_event events;

	g_epoll_fd = epoll_create(MAX_EVENTS); // create event pool
	if(g_epoll_fd < 0)
	{
		fprintf(stderr, "Epoll create Fails.\n");
		close(g_svr_sockfd);
		exit(0);
	}

	LOG("[START] epoll creation success");

	/* event control set */
	events.events = EPOLLIN;	// check In event
	events.data.fd = g_svr_sockfd;

	/* server events set(read for accept) */
	// add sockfd at event pool
	if( epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, g_svr_sockfd, &events) < 0 )
	{
		fprintf(stderr, "Epoll control fails.\n");
		close(g_svr_sockfd);
		close(g_epoll_fd);
		exit(0);
	}

	LOG("[START] epoll events set success for server");
}
/*------------------------------- end of function epoll_init */

void epoll_cli_add(int cli_fd)
{
	struct epoll_event events;

	/* event control set for read event */
	events.events = EPOLLIN;
	events.data.fd = cli_fd; 

	if( epoll_ctl(g_epoll_fd, EPOLL_CTL_ADD, cli_fd, &events) < 0 )
	{
		LOG("Epoll control fails.in epoll_cli_add");
	}
}

void userpool_add(int cli_fd, unsigned short cli_port)
{
	/* get empty element */
	register int i;

	for( i = 0 ; i < MAX_CLIENT ; i++ )
	{
		if(g_client[i].cli_sockfd == -1) break;
	}
	if( i >= MAX_CLIENT ){ 
	    close(cli_fd);
	    return;
	}

	LOG("g_client[%d].cli_port: %d", i, cli_port);

	g_client[i].cli_sockfd = cli_fd;
	g_client[i].cli_port = cli_port;
}

void userpool_delete(int cli_fd)
{
	register int i;

	for( i = 0 ; i < MAX_CLIENT ; i++)
	{
		if(g_client[i].cli_sockfd == cli_fd)
		{
			g_client[i].cli_sockfd = -1;	
			break;
		}
	}
}

int parse_val(char *buff, unsigned char data, char *parsbuf)
{
	int count=0;
	while(1)
	{
		if(count > 40)
			return -1;
		if(buff[count] == data)
		{
			count++;
			strncpy(parsbuf, buff, count);
			return count;
		}
		count++;
	}

	return 0;
}

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

void udp_init(void)
{
	char emul_ip[HOST_NAME_MAX+1];
	struct addrinfo *res;
	struct addrinfo hints;
	int rc;

	LOG("start");

	memset(emul_ip, 0, sizeof(emul_ip));
	if (gethostname(emul_ip, sizeof(emul_ip)) < 0)
	{
		LOG("gethostname(): %s", strerror(errno));
		assert(0);
	}

	memset(&hints, 0, sizeof(hints));
	hints.ai_family=AF_INET;
	hints.ai_socktype=SOCK_DGRAM;
	hints.ai_protocol=IPPROTO_UDP;

	if ((rc=getaddrinfo(emul_ip, STR(SENSORD_PORT), &hints, &res)) != 0)
	{
		if (rc == EAI_SYSTEM)
			LOG("getaddrinfo(sensord): %s", strerror(errno));
		else
			LOG("getaddrinfo(sensord): %s", gai_strerror(rc));
		assert(0);
	}

	if ((uSensordFd=socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1){
		fprintf(stderr, "socket error!\n");
	}

	if (res->ai_addrlen > sizeof(si_sensord_other))
	{
		LOG("sockaddr structure too big");
		/* XXX: if you `return' remember to clean up */
		assert(0);
	}
	memset((char *) &si_sensord_other, 0, sizeof(si_sensord_other));
	memcpy((char *) &si_sensord_other, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);

	if ((rc=getaddrinfo(emul_ip, STR(GPSD_PORT), &hints, &res)) != 0)
	{
		if (rc == EAI_SYSTEM)
			LOG("getaddrinfo(gpsd): %s", strerror(errno));
		else
			LOG("getaddrinfo(gpsd): %s", gai_strerror(rc));
		assert(0);
	}

	if ((uGpsdFd=socket(res->ai_family, res->ai_socktype, res->ai_protocol))==-1){
		fprintf(stderr, "socket error!\n");
	}

	if (res->ai_addrlen > sizeof(si_gpsd_other))
	{
		LOG("sockaddr structure too big");
		assert(0);
	}
	memset((char *) &si_gpsd_other, 0, sizeof(si_gpsd_other));
	memcpy((char *) &si_gpsd_other, res->ai_addr, res->ai_addrlen);
	freeaddrinfo(res);
}

#undef STR_HELPER
#undef STR

int recv_data(int event_fd, char** r_databuf, int size)
{
	int recvd_size = 0;
	int len = 0;
	int getcnt = 0;
	char* r_tmpbuf = NULL;

	r_tmpbuf = (char*)malloc(sizeof(char) * size + 1);
	if(r_tmpbuf == NULL){
	    return -1;
	}

	*r_databuf = (char*)malloc(sizeof(char) * size + 1);
	if(*r_databuf == NULL){
	    free(r_tmpbuf);
	    return -1;
	}
	memset(*r_databuf, '\0', sizeof(char) * size + 1);

	while(recvd_size < size)
	{
		memset(r_tmpbuf, '\0', sizeof(char) * size + 1);
		len = recv(event_fd, r_tmpbuf, size - recvd_size, 0);
		if (len < 0) {
			break;
		}

		memcpy((*r_databuf) + recvd_size, r_tmpbuf, len);
		recvd_size += len;
		getcnt++;
		if(getcnt > MAX_GETCNT) {
			break;
		}
	}
	free(r_tmpbuf);
	r_tmpbuf = NULL;

	return recvd_size;
}

int powerdown_by_force()
{
	struct timeval now;
	int poweroff_duration = POWEROFF_DURATION;
	char *buf;

	/* Getting poweroff duration */
	buf = getenv("PWROFF_DUR");
	if(buf == NULL)
	{
		LOG("PWROFF_DUR is null");
		assert(0);
	}
	
	if (strlen(buf) < 1024)
		poweroff_duration = atoi(buf);
	if (poweroff_duration < 0 || poweroff_duration > 60) 
		poweroff_duration = POWEROFF_DURATION;

	gettimeofday(&now, NULL);
	/* Waiting until power off duration and displaying animation */
	while (now.tv_sec - tv_start_poweroff.tv_sec < poweroff_duration) {
		LOG("wait");
		usleep(100000);
		gettimeofday(&now, NULL);
	}   

	LOG("Power off by force");
	LOG("sync");
	sync();
	LOG("poweroff");
	reboot(RB_POWER_OFF);

	return 1;
}

void client_recv(int event_fd)
{
	char* r_databuf = NULL;
	char tmpbuf[48];
	int len, recvd_size, parse_len = 0;
	LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
	if (packet == NULL)
	{
	    return;
	}
	memset(packet, 0, sizeof(LXT_MESSAGE));

	LOG("start (event fd: %d)", event_fd);
	/* there need to be more precise code here */ 
	/* for example , packet check(protocol needed) , real recv size check , etc. */
	if (event_fd == -1) {
		free(packet);
		packet = NULL;
		return;
	}

	// vmodem to event injector
	if(event_fd == g_vm_sockfd)
	{	
		recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
		LOG("receive size: %d", recvd_size);
		if(recvd_size <= 0)
		{
			free(r_databuf);
			r_databuf = NULL;
			free(packet);
			packet = NULL;
			userpool_delete(event_fd);
			close(event_fd); /* epoll set fd also deleted automatically by this call as a spec */
			if(pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
				LOG("pthread create fail!");
			return;
		}

		LOG("vmodem header recv buffer: %s", r_databuf);
		memcpy((void*)packet, (void*)r_databuf, sizeof(char) * HEADER_SIZE);

		LOG("first packet of vmodem to event injector %s", r_databuf);
		free(r_databuf);
		r_databuf = NULL;
		
		if(g_sdbd_sockfd != -1)
			len = send(g_sdbd_sockfd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);
		else {
			free(packet);
			packet = NULL;
			return;
		}

		LOG("send_len: %d, next packet length: %d", len, packet->length);

		if(packet->length <= 0)
		{
			free(packet);
			packet = NULL;
			return;
		}	
			
		if(g_sdbd_sockfd != -1)
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);
		else
		{
			// for packet clear
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);
			return;
		}

		LOG("recv_len: %d, vmodem data recv buffer: %s", recvd_size, r_databuf);

		if(recvd_size <= 0)
		{
			free(r_databuf);
			r_databuf = NULL;
			free(packet);
			packet = NULL;
			return;
		}
		len = send(g_sdbd_sockfd, r_databuf, packet->length, 0);

		LOG("send_len: %d", len);
	}
	else	// event injector to vmodem, sensord or gpsd
	{
		/* read from socket */
		// read identifier
		recvd_size = recv_data(event_fd, &r_databuf, ID_SIZE);
		LOG("receive size: %d", recvd_size);
		if( recvd_size <= 0 )
		{
			free(r_databuf);
			r_databuf = NULL;
			free(packet);
			packet = NULL;
			LOG("close event_fd: %d", event_fd);
			userpool_delete(event_fd);
			close(event_fd); /* epoll set fd also deleted automatically by this call as a spec */
			if(event_fd == g_sdbd_sockfd)
				g_sdbd_sockfd = -1;

			return;
		}

		LOG("Something may be added in the data end, but it does not matter.");
		LOG("identifier: %s", r_databuf);

		memset(tmpbuf, '\0', sizeof(tmpbuf));
		parse_len = parse_val(r_databuf, 0x0a, tmpbuf);

		LOG("parse_len: %d", parse_len);
		LOG("parsed_buffer: %s", tmpbuf);
		LOG("event fd: %d", event_fd);

		free(r_databuf);
		r_databuf = NULL;

		if(strncmp(tmpbuf, "telephony", 9) == 0)
		{
			g_sdbd_sockfd = event_fd;
			if(g_vm_connect_status != 1) {	// The connection is lost with vmodem
				free(packet);
				packet = NULL;
				return;
			}

			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);			
			len = send(g_vm_sockfd, r_databuf, HEADER_SIZE, 0);
			
			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);

			LOG("next packet length: %d", packet->length);
			LOG("packet->action: %d", packet->action);

			free(r_databuf);
			r_databuf = NULL;

			if(packet->length == 0)
			{	
				if(packet->action == 71)	// that's strange packet from telephony initialize
				{
					packet->length = 4;
				}
				else
				{
					free(packet);
					packet = NULL;
					return;
				}
			}				

			if(g_vm_connect_status != 1)	// The connection is lost with vmodem
				return;

			recvd_size = recv_data(event_fd, &r_databuf, packet->length);

			if(recvd_size <= 0)
			{
				free(r_databuf);
				r_databuf = NULL;				
				free(packet);
				packet = NULL;
				return;
			} else {
				LOG("Something may be added in the data end, but it does not matter.");
				LOG("telephony data recv buffer: %s", r_databuf);

				len = send(g_vm_sockfd, r_databuf, packet->length, 0);
			}
		}
		else if(strncmp(tmpbuf, "sensor", 6) == 0)
		{
			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);	

			LOG("sensor packet_length: %d", packet->length);

			free(r_databuf);
			r_databuf = NULL;
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);

			LOG("Something may be added in the data end, but it does not matter.");
			LOG("sensor data recv buffer: %s", r_databuf);

			if (packet->group == STATUS) {
				g_get_status_sockfd = event_fd;
				ActionID = packet->action;
				memset(GetBuffer, '\0', sizeof(GetBuffer));
				strcpy(GetBuffer, r_databuf);
				LOG("GetBuffer is %s", GetBuffer);

				if(pthread_create(&tid[2], NULL, setting_device, NULL) != 0) {
					LOG("pthread create fail!");
				}
			}
			else if(sendto(uSensordFd, r_databuf, packet->length, 0, (struct sockaddr*)&si_sensord_other, sslen) == -1) {     
				LOG("sendto error!");
			}
		}
		else if(strncmp(tmpbuf, "location", 8) == 0)
		{
			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);

			LOG("location packet_length: %d", packet->length);

			free(r_databuf);
			r_databuf = NULL;
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);

			LOG("Something may be added in the data end, but it does not matter.");
			LOG("location data recv buffer: %s", r_databuf);

			if (packet->group == STATUS) {
				g_get_status_sockfd = event_fd;
				ActionID = packet->action;
				memset(GetBuffer, '\0', sizeof(GetBuffer));
				strcpy(GetBuffer, r_databuf);
				LOG("GetBuffer is %s", GetBuffer);

				if(pthread_create(&tid[2], NULL, setting_device, NULL) != 0) {
					LOG("pthread create fail!");
				}
			} else {
				setting_location(r_databuf);
			}
		}
		else if(strncmp(tmpbuf, "nfc", 3) == 0)
		{
			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
			if( recvd_size <= 0 )
                	{
                        	free(r_databuf);
				r_databuf = NULL;
				free(packet);
				packet = NULL;
                        	return;
                	}

			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);
			LOG("nfc packet_length: %d", packet->length);

			free(r_databuf);
			r_databuf = NULL;
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);
			if(r_databuf == NULL){
			    free(packet);
			    packet = NULL;
			    return;
			}
			LOG("nfc data recv buffer: %s", r_databuf);

			if (packet->group == STATUS) {
				g_get_status_sockfd = event_fd;
				ActionID = packet->action;
				memset(GetBuffer, '\0', sizeof(GetBuffer));
				strcpy(GetBuffer, r_databuf);
				LOG("GetBuffer is %s", GetBuffer);

				if(pthread_create(&tid[2], NULL, setting_device, NULL) != 0) {
					LOG("pthread create fail!");
				}
			} else {
				FILE* fd;
				fd = fopen("/opt/nfc/sdkMsg", "w");
				if(!fd)
				{
					LOG("nfc file open fail!");
					free(r_databuf);
					r_databuf = NULL;
					free(packet);
					packet = NULL;
					return;
				}
				fprintf(fd, "%s", r_databuf);
				fclose(fd);
				//free(strbuf);
				//strbuf = NULL;
			}
		}
		else if(strncmp(tmpbuf, "system", 6) == 0)
		{

			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);

			LOG("system packet_length: %d", packet->length);

			free(r_databuf);
			r_databuf = NULL;
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);

			LOG("Something may be added in the data end, but it does not matter.");
			LOG("system data recv buffer: %s", r_databuf);
			LOG("/etc/rc.d/rc.shutdown, sync, reboot(RB_POWER_OFF)");

			LOG("sync");
			sync();
			system("/etc/rc.d/rc.shutdown &");
			gettimeofday(&tv_start_poweroff, NULL);
			powerdown_by_force();

		}
		else if(strncmp(tmpbuf, "sdcard", 6) == 0)
		{
			g_sdcard_sockfd = event_fd;
			recvd_size = recv_data(event_fd, &r_databuf, HEADER_SIZE);
			memcpy((void*)packet, (void*)r_databuf, HEADER_SIZE);	

			LOG("sdcard packet_length: %d", packet->length);

			free(r_databuf);
			r_databuf = NULL;
			recvd_size = recv_data(event_fd, &r_databuf, packet->length);
			if(recvd_size <= 0){
				LOG("client_recv: recv_data err");
				if(r_databuf) {
				    	free(r_databuf);
					r_databuf = NULL;
				}
				if(packet) {
					free(packet);
					packet = NULL;
				}
				LOG("close event_fd: %d", event_fd);
				userpool_delete(event_fd);
				close(event_fd); /* epoll set fd also deleted automatically by this call as a spec */
				if(event_fd == g_sdbd_sockfd)
					g_sdbd_sockfd = -1;
				return;
			}

			LOG("Something may be added in the data end, but it does not matter.");
			LOG("sdcard data recv buffer: %s", r_databuf);
			
			char token[] = "\n";
			char tmpdata[recvd_size];
			memcpy(tmpdata, r_databuf, recvd_size);

			char* ret = NULL;
			ret = strtok(tmpdata, token);
			LOG("%s", ret);
			int mount_param = atoi(ret);
			int mount_status = 0;

			switch(mount_param)
			{
			case 0:							// umount
				mount_status = umount_sdcard();
				if(mount_status == 0)				
					send_guest_server(r_databuf);
				break;
			case 1:							// mount
				memset(SDpath, '\0', sizeof(SDpath));
				ret = strtok(NULL, token);
				strcpy(SDpath, ret);
				LOG("sdcard path is %s", SDpath);

				send_guest_server(r_databuf);
				if(pthread_create(&tid[1], NULL, mount_sdcard, NULL) != 0)
					LOG("pthread create fail!");
				break;
			case 2:							// mount status
				mount_status = is_mounted();

				LXT_MESSAGE* mntData = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));
				if(mntData == NULL){
				    break;
				}
				memset(mntData, 0, sizeof(LXT_MESSAGE));

				mntData->length = strlen(SDpath);	// length
				mntData->group = 11;			// sdcard

				switch(mount_status)
				{
				case 0:
					mntData->action = 2;			// umounted status
					send(g_sdcard_sockfd, (void*)mntData, sizeof(char) * HEADER_SIZE, 0);
					
					LOG("SDpath is %s", SDpath);
					send(g_sdcard_sockfd, SDpath, mntData->length, 0);
					memset(SDpath, '\0', sizeof(SDpath));
					sprintf(SDpath, "umounted");
					break;
				case 1:
					mntData->action = 3;			// mounted status
					send(g_sdcard_sockfd, (void*)mntData, sizeof(char) * HEADER_SIZE, 0);

					LOG("SDpath is %s", SDpath);
					send(g_sdcard_sockfd, SDpath, mntData->length, 0);
					break;
				default:
					break;
				}
				free(mntData);
				break;
			default:
				LOG("unknown data %s", ret);
				break;
			}	
		}		
		else
		{
			LOG("Unknown packet: %s", tmpbuf);
			userpool_delete(event_fd);
			close(event_fd); /* epoll set fd also deleted automatically by this call as a spec */
		}
	}
	if(r_databuf)
	{
		free(r_databuf);
		r_databuf = NULL;
	}
	if(packet)
	{
		free(packet);
		packet = NULL;
	}
}

void server_process(void)
{
	struct sockaddr_in cli_addr;
	int i,nfds;
	int cli_sockfd;
	int cli_len = sizeof(cli_addr);

	nfds = epoll_wait(g_epoll_fd,g_events,MAX_EVENTS,100); /* timeout 100ms */

	if(nfds == 0){
		/* no event , no work */
		return;
	}


	if(nfds < 0) {
		fprintf(stderr, "epoll wait error\n");
		/* return but this is epoll wait error */
		return;
	} 

	for( i = 0 ; i < nfds ; i++ )
	{
		/* connect client */
		if(g_events[i].data.fd == g_svr_sockfd)
		{
			cli_sockfd = accept(g_svr_sockfd, (struct sockaddr *)&cli_addr,(socklen_t *)&cli_len);
			if(cli_sockfd < 0) 
			{
				/* accept error */
				fprintf(stderr, "accept error\n");
			}
			else
			{
				LOG("[Accpet] New client connected. fd:%d, port:%d"
						,cli_sockfd, cli_addr.sin_port);

				/* save client port */
				userpool_add(cli_sockfd, cli_addr.sin_port);

				/* for vmodem to sdbd directly */
				//g_sdbd_sockfd = cli_sockfd;

				/* add client sockfd at event pool */
				epoll_cli_add(cli_sockfd);
			}
			continue; /* next fd */
		}
		/* if not server socket , this socket is for client socket, so we read it */

		client_recv(g_events[i].data.fd);    
	} /* end of for 0-nfds */
}
/*------------------------------- end of function server_process */

void end_server(int sig)
{
	close(g_svr_sockfd); /* close server socket */
	close(uSensordFd);
	close(uGpsdFd);
	LOG("[SHUTDOWN] Server closed by signal %d",sig);

	exit(0);
}

void setting_device()
{
	pthread_detach(pthread_self());

	char* msg = 0;
	LXT_MESSAGE* packet = (LXT_MESSAGE*)malloc(sizeof(LXT_MESSAGE));

	switch(ActionID)
	{
	case BATTERY_LEVEL:
		msg = get_battery_level((void*)packet);
		if (msg == 0) {
			LOG("failed getting battery level");
		}
	break;
	case BATTERY_CHARGER:
		msg = get_battery_charger((void*)packet);
		if (msg == 0) {
			LOG("failed getting battery charger state");
		}
	break;
	case USB_STATUS:
		msg = get_usb_status((void*)packet);
		if (msg == 0) {
			LOG("failed getting usb status");
		}
	break;
	case EARJACK_STATUS:
		msg = get_earjack_status((void*)packet);
		if (msg == 0) {
			LOG("failed getting earjack status");
		}
	break;
	case RSSI_LEVEL:
		msg = get_rssi_level((void*)packet);
		if (msg == 0) {
			LOG("failed getting rssi level");
		}
	break;
	case ACCEL_VALUE:
		msg = get_acceleration_value((void*)packet);
		if (msg == 0) {
			LOG("falied getting acceleration value");
		}
	break;
	case GYRO_VALUE:
		msg = get_gyroscope_value((void*)packet);
		if (msg == 0) {
			LOG("failed getting gyroscope value");
		}
	break;
	case MAG_VALUE:
		msg = get_magnetic_value((void*)packet);
		if (msg == 0) {
			LOG("failed getting magnetic value");
		}
	break;
	case LIGHT_VALUE:
		msg = get_light_level((void*)packet);
		if (msg == 0) {
			LOG("failed getting light level");
		}
	break;
	case PROXI_VALUE:
		msg = get_proximity_status((void*)packet);
		if (msg == 0) {
			LOG("failed getting proximity status");
		}
	break;
	case MOTION_VALUE:
		LOG("not support getting motion value");
	break;
	case LOCATION_STATUS:
		msg = get_location_status((void*)packet);
		if (msg == 0) {
			LOG("failed getting location status");
		}
	break;
	case NFC_STATUS:
		msg = get_nfc_status((void*)packet);
		if (msg ==0) {
			LOG("failed getting nfc status");
		}
	break;
	default:
		//TODO
	break;
	}

	if (g_get_status_sockfd != -1) {
		if (msg != 0) {
			LOG("send data to injector");
		} else {
			LOG("send error message to injector");
			memset(packet, 0, sizeof(LXT_MESSAGE));
			packet->length = 0;
			packet->group  = STATUS;
			packet->action = ActionID;
		}
		send(g_get_status_sockfd, (void*)packet, sizeof(char) * HEADER_SIZE, 0);
		if (packet->length != 0) {
			send(g_get_status_sockfd, msg, packet->length, 0);
		}
	}

	if(msg != 0) {
		free(msg);
	}
	free(packet);

	pthread_exit((void *) 0); 
}

// location event
char command[512];
char latitude[128];
char longitude[128];
void setting_location(char* databuf)
{
	char* s = strchr(databuf, ',');
	memset(command, 0, 256);
	if (s == NULL) { // SET MODE
		int mode = atoi(databuf);
		switch (mode) {
		case 0: // STOP MODE
			sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 0 -f");
			break;
		case 1: // NMEA MODE (LOG MODE)
			sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 1 -f");
			break;
		case 2: // MANUAL MODE
			sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 2 -f");
			break;
		default:
			LOG("error(%s) : stop replay mode", databuf);
			sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 0 -f");
			break;
		}
		LOG("Location Command : %s", command);
		system(command);
	} else {
		*s = '\0';
		int mode = atoi(databuf);
		if(mode == 1) { // NMEA MODE (LOG MODE)
			sprintf(command, "vconftool set -t string db/location/replay/FileName \"%s\"", s+1);
			LOG("%s", command);
			system(command);
			memset(command, 0, 256);
			sprintf(command, "vconftool set -t int db/location/replay/ReplayMode 1 -f");
			LOG("%s", command);
			system(command);
		} else if(mode == 2) {
			memset(latitude,  0, 128);
			memset(longitude, 0, 128);
			char* t = strchr(s+1, ',');
			*t = '\0';
			strcpy(latitude, s+1);
			strcpy(longitude, t+1);
			//strcpy(longitude, s+1);
			//strcpy(latitude, databuf);
			// Latitude
			sprintf(command, "vconftool set -t double db/location/replay/ManualLatitude %s -f", latitude);
			LOG("%s", command);
			system(command);

			// Longitude
			sprintf(command, "vconftool set -t double db/location/replay/ManualLongitude %s -f", longitude);
			LOG("%s", command);
			system(command);
		}
	}
}

//sdcard event
void send_guest_server(char* databuf)
{
	char buf[32];
	struct sockaddr_in si_other;
	int s, slen=sizeof(si_other);
 	FILE* fd;
	char fbuf[16];
	int port;
	fd = fopen("/opt/home/sdb_port.txt", "r");
	LOG("sdb_port.txt fopen fd is %d", fd);
	if(!fd)
	{
		LOG("fopen /opt/home/sdb_port.txt fail");
		port = 3581;
	}
	else
	{
		fgets(fbuf, 16, fd);
		fclose(fd);
		port = atoi(fbuf) + 3;
	}

	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))==-1)
	{
		LOG("socket error!");
		return;
	}
	    
	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(port);
	if (inet_aton(SRV_IP, &si_other.sin_addr)==0) {
		  fprintf(stderr, "inet_aton() failed\n");
	}

	memset(buf, '\0', sizeof(buf));
	sprintf(buf, "4\n%s", databuf);
	LOG("sendGuestServer msg: %s", buf);
	if(sendto(s, buf, sizeof(buf), 0, (struct sockaddr*)&si_other, slen) == -1)
	{     
		LOG("sendto error!");
	}
	
	close(s);
}

int main( int argc , char *argv[])
{
	if(log_print == 1)
	{
		// for emuld log file
		system("rm /opt/sensor/emuld.log");
		system("touch /opt/sensor/emuld.log");
	}

	LOG("start");
	/* entry , argument check and process */
	if(argc < 3){ 

		g_svr_port = DEFAULT_PORT;

	}else {

		if(strcmp("-port",argv[1]) ==  0 ) {

			g_svr_port = atoi(argv[2]);
			if(g_svr_port < 1024) {
				fprintf(stderr, "[STOP] port number invalid : %d\n",g_svr_port);
				exit(0);
			} 
		}
	}

	init_data0();   

	/* init server */
	init_server0(g_svr_port);

	epoll_init();    /* epoll initialize  */
	if(pthread_create(&tid[0], NULL, init_vm_connect, NULL) != 0)
		LOG("pthread create fail!");

	udp_init();

	/* main loop */
	while(1)
	{
		server_process();  /* accept process. */
	} /* infinite loop while end. */
}

