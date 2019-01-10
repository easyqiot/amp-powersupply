#ifndef _DEPLOY_CONFIG_H__
#define _DEPLOY_CONFIG_H__


#define EASYQ_RECV_BUFFER_SIZE  4096
#define EASYQ_SEND_BUFFER_SIZE  512 
#define EASYQ_PORT				1085

#define EASYQ_LOGIN				"ampsupply"
#define DEVICE_NAME				"amp:supply"
#define STATUS_QUEUE			DEVICE_NAME":status"
#define FOTA_QUEUE				DEVICE_NAME":fota"
#define FOTA_STATUS_QUEUE		DEVICE_NAME":fota:status"
#define RELAY_MAIN_QUEUE		DEVICE_NAME	

/* GPIO */

// LED
#define LED_MUX			PERIPHS_IO_MUX_U0RXD_U
#define LED_NUM			3
#define LED_FUNC		FUNC_GPIO3

// Main Power Relay #1
#define RELAY_MAIN_MUX		PERIPHS_IO_MUX_GPIO2_U
#define RELAY_MAIN_NUM		2
#define RELAY_MAIN_FUNC		FUNC_GPIO2

// AMP Ground Relay #2
#define RELAY_DC_MUX		PERIPHS_IO_MUX_GPIO0_U
#define RELAY_DC_NUM		0
#define RELAY_DC_FUNC		FUNC_GPIO0

#endif

