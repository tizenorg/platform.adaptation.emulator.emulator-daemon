/*
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

//#include <vconf.h>
#include "emuld.h"
#include "emuld_common.h"

static int file_read(FILE* fd)
{
	int ret = 0;
	int status = 0;
	ret = fscanf(fd, "%d", &status);
	if (ret < 0)
		LOG("fscanf failed with fd: %d", fd);

	return status;
}

char* message;
char* get_usb_status(void* p)
{
	FILE* fd = fopen("/sys/devices/platform/jack/usb_online", "r");
	if(!fd)
	{
		return 0;
	}

	int status = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (status & 0xff);
	message[2] = (char) (status >> 8 & 0xff);
	message[1] = (char) (status >> 16 & 0xff);
	message[0] = (char) (status >> 24 & 0xff);
	message[4] = '\0';

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = USB_STATUS;

	return message;
}

char* get_earjack_status(void* p)
{
	FILE* fd = fopen("/sys/devices/platform/jack/earjack_online", "r");
	if(!fd)
	{
		return 0;
	}

	int status = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (status & 0xff);
	message[2] = (char) (status >> 8 & 0xff);
	message[1] = (char) (status >> 16 & 0xff);
	message[0] = (char) (status >> 24 & 0xff);
	message[4] = '\0';

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = EARJACK_STATUS;

	return message;
}

char* get_rssi_level(void* p)
{
	int level;
	int ret = vconf_get_int("memory/telephony/rssi", &level);
	if (ret != 0) {
		return 0;
	}

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (level & 0xff);
	message[2] = (char) (level >> 8 & 0xff);
	message[1] = (char) (level >> 16 & 0xff);
	message[0] = (char) (level >> 24 & 0xff);
	message[4] = '\0';

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = RSSI_LEVEL;

	return message;
}

char* get_battery_level(void* p)
{
	FILE* fd = fopen("/sys/class/power_supply/battery/capacity", "r");
	if(!fd)
	{
		return 0;
	}

	int level = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (level & 0xff);
	message[2] = (char) (level >> 8 & 0xff);
	message[1] = (char) (level >> 16 & 0xff);
	message[0] = (char) (level >> 24 & 0xff);
	message[4] = '\0';

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = BATTERY_LEVEL;

	return message;
}

char* get_battery_charger(void* p)
{
	FILE* fd = fopen("/sys/class/power_supply/battery/charge_now", "r");
	if(!fd)
	{
		return 0;
	}

	int charge = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (charge & 0xff);
	message[2] = (char) (charge >> 8 & 0xff);
	message[1] = (char) (charge >> 16 & 0xff);
	message[0] = (char) (charge >> 24 & 0xff);
	message[4] = '\0';
	
	LXT_MESSAGE* packet = p;	
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = BATTERY_CHARGER;

	return message;
}

char* get_proximity_status(void* p)
{
	FILE* fd = fopen("/opt/sensor/proxi/vo", "r");
	if(!fd)
	{
		return 0;
	}

	int status = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);

	message[3] = (char) (status & 0xff);
	message[2] = (char) (status >> 8 & 0xff);
	message[1] = (char) (status >> 16 & 0xff);
	message[0] = (char) (status >> 24 & 0xff);
	message[4] = '\0';
	
	LXT_MESSAGE* packet = p;	
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = PROXI_VALUE;

	return message;
}

char* get_light_level(void* p)
{
	FILE* fd = fopen("/opt/sensor/light/adc", "r");
	if(!fd)
	{
		return 0;
	}

	int level = file_read(fd);
	fclose(fd);

	// int to byte
	message = (char*)malloc(5);
	message[3] = (char) (level & 0xff);
	message[2] = (char) (level >> 8 & 0xff);
	message[1] = (char) (level >> 16 & 0xff);
	message[0] = (char) (level >> 24 & 0xff);
	message[4] = '\0';

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = 4;
	packet->group  = STATUS;
	packet->action = LIGHT_VALUE;

	return message;
}

char* get_acceleration_value(void* p)
{
	char* ret = NULL;
	FILE* fd = fopen("/opt/sensor/accel/xyz", "r");
	if(!fd)
	{
		return 0;
	}

	message = (char*)malloc(128);
	//fscanf(fd, "%d, %d, %d", message);
	ret = fgets(message, 128, fd);
	if (ret == NULL)
		LOG("getting accel xyz value failed.");

	fclose(fd);

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = strlen(message);
	packet->group  = STATUS;
	packet->action = ACCEL_VALUE;

	return message;
}

char* get_gyroscope_value(void* p)
{
	FILE* fd = fopen("/opt/sensor/gyro/gyro_x_raw", "r");
	if(!fd)
	{
		return 0;
	}
	int x = file_read(fd);
	fclose(fd);

	fd = fopen("/opt/sensor/gyro/gyro_y_raw", "r");
	if(!fd)
	{
		return 0;
	}
	int y = file_read(fd);
	fclose(fd);

	fd = fopen("/opt/sensor/gyro/gyro_z_raw", "r");
	if(!fd)
	{
		return 0;
	}
	int z = file_read(fd);
	fclose(fd);
 
	message = (char*)malloc(128);
	memset(message, 0, 128);
	int ret = sprintf(message, "%d, %d, %d", x, y, z);	
	if (ret < 0) {
		free(message);
		return 0;
	}

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = strlen(message);
	packet->group  = STATUS;
	packet->action = GYRO_VALUE;

	return message;
}

char* get_magnetic_value(void* p)
{
	char* ret = NULL;
	FILE* fd = fopen("/opt/sensor/geo/tesla", "r");
	if(!fd)
	{
		return 0;
	}

	message = (char*)malloc(128);
	ret = fgets(message, 128, fd);
	if (ret == NULL)
		LOG("getting tesla value failed.");
	fclose(fd);

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = strlen(message);
	packet->group  = STATUS;
	packet->action = MAG_VALUE;
	return message;
}

char* get_location_status(void* p)
{
	int mode;
	int ret = vconf_get_int("db/location/replay/ReplayMode", &mode);
	if (ret != 0) {
		return 0;
	}

	if (mode == 0) { // STOP
		message = (char*)malloc(5);
		ret = sprintf(message, "%d", mode);
		if (ret < 0) {
			free(message);
			return 0;
		}
	} else if (mode == 1) { // NMEA MODE(LOG MODE)
		//char* temp = (char*)malloc(128);
		char* temp = 0;
		temp = vconf_get_str("db/location/replay/FileName");
		if (temp == 0) {
			//free(temp);
			return 0;
		}

		message = (char*)malloc(256);
		ret = sprintf(message, "%d,%s", mode, temp);
		if (ret < 0) {
			free(message);
			return 0;
		}
	} else if (mode == 2) { // MANUAL MODE
		double latitude;
		double logitude;
		ret = vconf_get_dbl("db/location/replay/ManualLatitude", &latitude);
		if (ret != 0) {
			return 0;
		}
		ret = vconf_get_dbl("db/location/replay/ManualLongitude", &logitude);
		if (ret != 0) {
			return 0;
		}
		message = (char*)malloc(128);
		ret = sprintf(message, "%d,%f,%f", mode, latitude, logitude);
		if (ret < 0) {
			free(message);
			return 0;
		}
	}

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = strlen(message);
	packet->group  = STATUS;
	packet->action = LOCATION_STATUS;

	return message;
}

char* get_nfc_status(void* p)
{
	int ret = 0;
	FILE* fd = fopen("/opt/nfc/sdkMsg", "r");
	if(!fd)
	{
		return 0;
	}

	message = (char*)malloc(5000);
	ret = fscanf(fd, "%s\n", message);
	if (ret < 0)
		LOG("fscanf failed with fd: %d", fd);
	fclose(fd);

	LXT_MESSAGE* packet = p;
	memset(packet, 0, sizeof(LXT_MESSAGE));
	packet->length = strlen(message);
	packet->group  = STATUS;
	packet->action = NFC_STATUS;

	return message;
}
