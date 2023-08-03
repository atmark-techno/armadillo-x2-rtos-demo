/*
 * Copyright (c) 2016, Freescale Semiconductor, Inc.
 * Copyright 2016-2020 NXP
 * Copyright 2023 Atmark Techno
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rpmsg_lite.h"
#include "rpmsg_queue.h"
#include "rpmsg_ns.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"
#include "fsl_debug_console.h"
#include "FreeRTOS.h"
#include "task.h"

#include "fsl_gpio.h"
#include "rsc_table.h"
#define RPMSG_LITE_LINK_ID (RL_PLATFORM_IMX8MP_M7_USER_LINK_ID)
#define RPMSG_LITE_SHMEM_BASE (VDEV0_VRING_BASE)

#define APP_TASK_STACK_SIZE (256U)
#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30U)
#endif

#include "../common/protocol.h"

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t app_task_handle = NULL;

static struct rpmsg_lite_instance *volatile rpmsg = NULL;

static struct rpmsg_lite_endpoint *volatile rpmsg_endpoint = NULL;
static volatile rpmsg_queue_handle rpmsg_queue = NULL;

// info
static int log_level = 2;

void app_destroy_task(void)
{
	if (app_task_handle) {
		vTaskDelete(app_task_handle);
		app_task_handle = NULL;
	}

	if (rpmsg_endpoint) {
		rpmsg_lite_destroy_ept(rpmsg, rpmsg_endpoint);
		rpmsg_endpoint = NULL;
	}

	if (rpmsg_queue) {
		rpmsg_queue_destroy(rpmsg, rpmsg_queue);
		rpmsg_queue = NULL;
	}

	if (rpmsg) {
		rpmsg_lite_deinit(rpmsg);
		rpmsg = NULL;
	}
}

static void app_nameservice_isr_cb(uint32_t new_ept, const char *new_ept_name,
				   uint32_t flags, void *user_data)
{
}

// sends constant strings to linux for logging if
// requested level is higher than current log level
static void log(uint32_t remote_addr, int level, char *log)
{
	if (level - TYPE_LOG_DEBUG < log_level)
		return;

	struct msg msg = {
		.type = level,
		.data = strlen(log) + 1,
	};

	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, remote_addr, (char *)&msg,
			      sizeof(msg), RL_BLOCK);
	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, remote_addr, log, msg.data,
			      RL_BLOCK);
}

// main task:
// setup communication with linux and waits forever for messages
static void app_task(void *param)
{
	struct msg msg;
	volatile uint32_t remote_addr;
	volatile rpmsg_ns_handle ns_handle;

	rpmsg = rpmsg_lite_remote_init((void *)RPMSG_LITE_SHMEM_BASE,
				       RPMSG_LITE_LINK_ID, RL_NO_FLAGS);
	rpmsg_lite_wait_for_link_up(rpmsg, RL_BLOCK);

	rpmsg_queue = rpmsg_queue_create(rpmsg);
	rpmsg_endpoint = rpmsg_lite_create_ept(rpmsg, LOCAL_EPT_ADDR,
					       rpmsg_queue_rx_cb, rpmsg_queue);
	ns_handle = rpmsg_ns_bind(rpmsg, app_nameservice_isr_cb, NULL);
	/* Introduce some delay to avoid NS announce message not being
	 * captured by the master side.
         * This could happen when the remote side execution is too fast and
	 * the NS announce message is triggered before the nameservice_isr_cb
	 * is registered on the master side.
	 */
	SDK_DelayAtLeastUs(1000000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);
	(void)rpmsg_ns_announce(rpmsg, rpmsg_endpoint,
				RPMSG_LITE_NS_ANNOUNCE_STRING,
				(uint32_t)RL_NS_CREATE);

	/* Verify version matches and send our's as well */
	(void)rpmsg_queue_recv(rpmsg, rpmsg_queue, (uint32_t *)&remote_addr,
			       (char *)&msg, sizeof(msg), NULL, RL_BLOCK);
	assert(msg.type == TYPE_VERSION);
	assert(msg.data == 0);
	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, remote_addr, (char *)&msg,
			      sizeof(msg), RL_BLOCK);

	while (true) {
		(void)rpmsg_queue_recv(rpmsg, rpmsg_queue,
				       (uint32_t *)&remote_addr, (char *)&msg,
				       sizeof(msg), NULL, RL_BLOCK);
		switch (msg.type) {
		case TYPE_GPIO:
			if (msg.data > 1) {
				log(remote_addr, TYPE_LOG_WARN,
				    "bad gpio value, stopping loop");
				goto out;
			}
			log(remote_addr, TYPE_LOG_DEBUG, "toggling gpio");
			GPIO_WritePinOutput(GPIO1, 15U, msg.data);
			break;
		case TYPE_LOG_SET_LEVEL:
			log_level = msg.data;
			log(remote_addr, TYPE_LOG_INFO, "set log level");
			break;
		default:
			log(remote_addr, TYPE_LOG_WARN,
			    "bad type, stopping loop");
			goto out;
		}
	}
out:

	(void)rpmsg_lite_destroy_ept(rpmsg, rpmsg_endpoint);
	rpmsg_endpoint = NULL;
	(void)rpmsg_queue_destroy(rpmsg, rpmsg_queue);
	rpmsg_queue = NULL;
	(void)rpmsg_ns_unbind(rpmsg, ns_handle);
	(void)rpmsg_lite_deinit(rpmsg);
	rpmsg = NULL;

	for (;;)
		;
}

void app_create_task(void)
{
	if (app_task_handle == NULL &&
	    xTaskCreate(app_task, "APP_TASK", APP_TASK_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + 1, &app_task_handle) != pdPASS) {
		PRINTF("\r\nFailed to create application task\r\n");
		for (;;)
			;
	}
}

/*!
 * @brief Main function
 */
int main(void)
{
	/* Initialize standard SDK demo application pins */
	/* M7 has its local cache and enabled by default,
     * need to set smart subsystems (0x28000000 ~ 0x3FFFFFFF)
     * non-cacheable before accessing this address region */
	BOARD_InitMemory();

	/* Board specific RDC settings */
	BOARD_RdcInit();

	BOARD_InitBootPins();
	BOARD_BootClockRUN();
	BOARD_InitDebugConsole();

	/* demo uses GPIO1_IO15, CON11 pin 24 on Armadillo IoT G4 / X2 */
	gpio_pin_config_t config = {kGPIO_DigitalOutput, 1, kGPIO_NoIntmode};
	GPIO_PinInit(GPIO1, 15U, &config);

	copyResourceTable();

	app_create_task();
	vTaskStartScheduler();

	(void)PRINTF("Failed to start FreeRTOS on core0.\r\n");
	for (;;) {
	}
}
