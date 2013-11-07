/*
 * evdi_protocol.h
 *
 *  Created on: 2013. 4. 15.
 *      Author: dykim
 */

#ifndef EVDI_PROTOCOL_H_
#define EVDI_PROTOCOL_H_

/* device protocol */

#define __MAX_BUF_SIZE	1024

enum
{
	route_qemu = 0,
	route_control_server = 1,
	route_monitor = 2
};

typedef unsigned int CSCliSN;

struct msg_info {
	char buf[__MAX_BUF_SIZE];

	uint32_t route;
	uint32_t use;
	uint16_t count;
	uint16_t index;

	CSCliSN cclisn;
};

/* device protocol */


#endif /* EVDI_PROTOCOL_H_ */
