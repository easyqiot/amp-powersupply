
// Internal 
#include "partition.h"
#include "wifi.h"
#include "io_config.h"

// SDK

#include <eagle_soc.h>
#include <ets_sys.h>
#include <osapi.h>
#include <gpio.h>
#include <mem.h>
#include <user_interface.h>
#include <driver/uart.h>
#include <upgrade.h>

// LIB: EasyQ
#include "easyq.h" 
#include "debug.h"
#include "params.h"


#define STATUS_INTERVAL		200	
#define VERSION				"0.1.1"


static EasyQSession eq;
static ETSTimer status_timer;
static ETSTimer relay_timer;
static bool led_status;
static bool main_is_on;
static bool remote;
static uint32_t ticks;
static Params params;

enum led_status {
	LED_OFF = 1,
	LED_ON = 2,
	BLINK_SLOW = 3,
	BLINK_FAST = 4
};


void
fota_report_status(const char *q) {
	char str[50];
	float vdd = system_get_vdd33() / 1024.0;

	uint8_t image = system_upgrade_userbin_check();
	os_sprintf(str, "Image: %s Version: "VERSION" VDD: %d.%03d", 
			(UPGRADE_FW_BIN1 == image)? "FOTA": "APP",
			(int)vdd, 
			(int)(vdd*1000)%1000
		);
	easyq_push(&eq, q, str);
}


void
update_led(bool on) {
	led_status = on;
	GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_NUM), !on);
}

#define BLINK_LED()	update_led(!led_status)
#define UPDATE_RELAY(num, on) GPIO_OUTPUT_SET(GPIO_ID_PIN(num), !on)


void ICACHE_FLASH_ATTR
status_timer_func() {
	ticks++;
	BLINK_LED();
	if (eq.status == EASYQ_CONNECTED && ticks % 20 == 0) {
		fota_report_status(STATUS_QUEUE);
	}
}


void update_led_status(enum led_status n) {
    os_timer_disarm(&status_timer);
	if (n <= 2) {
		update_led(n-1);
		return;
	}
    os_timer_setfn(&status_timer, (os_timer_func_t *)status_timer_func, NULL);
    os_timer_arm(&status_timer, STATUS_INTERVAL/n, 1);
}


void relay_timer_func() {
    os_timer_disarm(&relay_timer);
	if (main_is_on) {
		UPDATE_RELAY(RELAY_MAIN_NUM, main_is_on);
	} else {
		UPDATE_RELAY(RELAY_DC_NUM, main_is_on);
	}
	update_led_status(main_is_on ? LED_ON : LED_OFF);
	remote = true;
}


void update_relays(bool on) {
	remote = false;
	if (on) {
		UPDATE_RELAY(RELAY_DC_NUM, on);
	} else {
		UPDATE_RELAY(RELAY_MAIN_NUM, on);
	}
	main_is_on = on;
	update_led_status(BLINK_FAST);
    os_timer_disarm(&relay_timer);
    os_timer_setfn(&relay_timer, (os_timer_func_t *)relay_timer_func, NULL);
    os_timer_arm(&relay_timer, 600, 0);
}


void ICACHE_FLASH_ATTR
easyq_message_cb(void *arg, const char *queue, const char *msg, 
		uint16_t message_len) {
	//INFO("EASYQ: Message: %s From: %s\r\n", msg, queue);
	system_soft_wdt_feed();
	if (strcmp(queue, RELAY_MAIN_QUEUE) == 0) {
		if (!remote) {
			return;
		}
		if (strcmp(msg, "toggle") == 0) {
			update_relays(!main_is_on);
			return;
		}
		update_relays(strcmp(msg, "on") == 0);
	}
	else if (strcmp(queue, FOTA_QUEUE) == 0) {
		if (msg[0] == 'R') {
			UPDATE_RELAY(RELAY_MAIN_NUM, false);
			UPDATE_RELAY(RELAY_DC_NUM, false);
			os_timer_disarm(&status_timer);
			INFO("Rebooting to FOTA ROM\r\n");
			system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
			system_upgrade_reboot();
		}
		else if (msg[0] == 'I') {
			fota_report_status(FOTA_STATUS_QUEUE);
		}
	}
}


void ICACHE_FLASH_ATTR
easyq_connect_cb(void *arg) {
	INFO("EASYQ: Connected to %s:%d\r\n", eq.hostname, eq.port);
	INFO("\r\n***** "DEVICE_NAME" "VERSION" ****\r\n");
	update_led_status(LED_OFF);
	const char * queues[] = {RELAY_MAIN_QUEUE, FOTA_QUEUE};
	easyq_pull_all(&eq, queues, 2);
	remote = true;
}


void ICACHE_FLASH_ATTR
easyq_connection_error_cb(void *arg) {
	EasyQSession *e = (EasyQSession*) arg;
	INFO("EASYQ: Connection error: %s:%d\r\n", e->hostname, e->port);
	INFO("EASYQ: Reconnecting to %s:%d\r\n", e->hostname, e->port);
}


void easyq_disconnect_cb(void *arg)
{
	EasyQSession *e = (EasyQSession*) arg;
	INFO("EASYQ: Disconnected from %s:%d\r\n", e->hostname, e->port);
	easyq_delete(&eq);
}


void setup_easyq() {
	EasyQError err = \
			easyq_init(&eq, params.easyq_host, EASYQ_PORT, EASYQ_LOGIN);
	if (err != EASYQ_OK) {
		ERROR("EASYQ INIT ERROR: %d\r\n", err);
		return;
	}
	eq.onconnect = easyq_connect_cb;
	eq.ondisconnect = easyq_disconnect_cb;
	eq.onconnectionerror = easyq_connection_error_cb;
	eq.onmessage = easyq_message_cb;
}


void wifi_connect_cb(uint8_t status) {
    if(status == STATION_GOT_IP) {
		update_led_status(BLINK_SLOW);
        easyq_connect(&eq);
    } else {
		update_led_status(BLINK_FAST);
        easyq_disconnect(&eq);
    }
}


void user_init(void) {
    uart_init(BIT_RATE_115200, BIT_RATE_115200);
    os_delay_us(60000);

	/* Relays */

	// AMP AC MAIN 
	PIN_FUNC_SELECT(RELAY_MAIN_MUX, RELAY_MAIN_FUNC);
	PIN_PULLUP_EN(RELAY_MAIN_MUX);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(RELAY_MAIN_NUM), 1);
	
	// AMP DC GND
	PIN_FUNC_SELECT(RELAY_DC_MUX, RELAY_DC_FUNC);
	PIN_PULLUP_EN(RELAY_DC_MUX);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(RELAY_DC_NUM), 1);

	// LED
	PIN_FUNC_SELECT(LED_MUX, LED_FUNC);
	//PIN_PULLUP_DIS(LED_MUX);
	GPIO_OUTPUT_SET(GPIO_ID_PIN(LED_NUM), 1);

	bool ok = params_load(&params);
	if (!ok) {
		ERROR("Cannot load Params\r\n");
		system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
		system_upgrade_reboot();
		return;
	}
	INFO("Params loaded sucessfully: ssid: %s psk: %s easyq: %s\r\n",
			params.wifi_ssid, 
			params.wifi_psk,
			params.easyq_host
		);
	update_led_status(BLINK_FAST);
	setup_easyq();
    wifi_connect(params.wifi_ssid, params.wifi_psk, wifi_connect_cb);
    INFO("System started ...\r\n");
}


void ICACHE_FLASH_ATTR user_pre_init(void)
{
    if(!system_partition_table_regist(at_partition_table, 
				sizeof(at_partition_table)/sizeof(at_partition_table[0]),
				SPI_FLASH_SIZE_MAP)) {
		FATAL("system_partition_table_regist fail\r\n");
		while(1);
	}
}

