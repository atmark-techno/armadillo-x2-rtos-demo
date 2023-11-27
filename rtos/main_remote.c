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
#include "fsl_ecspi.h"
#include "FreeRTOS.h"
#include "task.h"

#include "fsl_gpio.h"
#include "rsc_table.h"
#define RPMSG_LITE_LINK_ID (RL_PLATFORM_IMX8MP_M7_USER_LINK_ID)
#define RPMSG_LITE_SHMEM_BASE (VDEV0_VRING_BASE)

#define APP_TASK_STACK_SIZE (256U)
#define SPI_TASK_STACK_SIZE (256U)
#ifndef LOCAL_EPT_ADDR
#define LOCAL_EPT_ADDR (30U)
#endif

#include "../common/protocol.h"

// enable either spi slave or master code
//#define SPI_SLAVE
#define SPI_MASTER
#define SPI_TRANSFER_SIZE 64U

/*******************************************************************************
 * Code
 ******************************************************************************/
static TaskHandle_t app_task_handle = NULL;
#ifdef SPI_SLAVE
static TaskHandle_t spi_task_handle = NULL;
#endif

static struct rpmsg_lite_instance *volatile rpmsg = NULL;

static struct rpmsg_lite_endpoint *volatile rpmsg_endpoint = NULL;
static volatile rpmsg_queue_handle rpmsg_queue = NULL;
static volatile uint32_t rpmsg_remote_addr;
static volatile bool rpmsg_ready = false;

// default to info -- we compare diff to debug.
static int log_level = TYPE_LOG_INFO - TYPE_LOG_DEBUG;

void app_destroy_task(void)
{
	if (app_task_handle) {
		vTaskDelete(app_task_handle);
		app_task_handle = NULL;
	}

#ifdef SPI_SLAVE
	if (spi_task_handle) {
		vTaskDelete(spi_task_handle);
		spi_task_handle = NULL;
	}
#endif

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
static void log(int level, char *log)
{
	if (level - TYPE_LOG_DEBUG < log_level)
		return;

	struct msg msg = {
		.type = level,
		.data = strlen(log) + 1,
	};

	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, rpmsg_remote_addr,
			      (char *)&msg, sizeof(msg), RL_BLOCK);
	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, rpmsg_remote_addr, log,
			      msg.data, RL_BLOCK);
}

// main task:
// setup communication with linux and waits forever for messages
static void app_task(void *param)
{
	struct msg msg;
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
	(void)rpmsg_queue_recv(rpmsg, rpmsg_queue,
			       (uint32_t *)&rpmsg_remote_addr, (char *)&msg,
			       sizeof(msg), NULL, RL_BLOCK);
	assert(msg.type == TYPE_VERSION);
	assert(msg.data == 0);
	(void)rpmsg_lite_send(rpmsg, rpmsg_endpoint, rpmsg_remote_addr,
			      (char *)&msg, sizeof(msg), RL_BLOCK);

// spi master code
// mostly based on example from mcuxsdk:
// mcuxsdk/examples/evkmimx8mp/driver_examples/ecspi/polling_b2b_transfer/master/ecspi_polling_b2b_transfer_master.c
#ifdef SPI_MASTER
	/* clock init */
#define ECSPI_MASTER_CLK_FREQ                           \
	(CLOCK_GetPllFreq(kCLOCK_SystemPll1Ctrl) /      \
	 (CLOCK_GetRootPreDivider(kCLOCK_RootEcspi1)) / \
	 (CLOCK_GetRootPostDivider(kCLOCK_RootEcspi1)))
	CLOCK_SetRootMux(
		kCLOCK_RootEcspi1,
		kCLOCK_EcspiRootmuxSysPll1); /* Set ECSPI1 source to SYSTEM PLL1 800MHZ */
	CLOCK_SetRootDivider(kCLOCK_RootEcspi1, 2U,
			     5U); /* Set root clock to 800MHZ / 10 = 80MHZ */

	ecspi_master_config_t masterConfig;
	ecspi_transfer_t masterXfer;
	uint32_t masterData[SPI_TRANSFER_SIZE];
	int counter = 0, i;
#define TRANSFER_BAUDRATE 500000U /*! Transfer baudrate - 500k */

	/* Master config:
	 * masterConfig.channel = kECSPI_Channel0;
	 * masterConfig.burstLength = 8;
	 * masterConfig.samplePeriodClock = kECSPI_spiClock;
	 * masterConfig.baudRate_Bps = TRANSFER_BAUDRATE;
	 * masterConfig.chipSelectDelay = 0;
	 * masterConfig.samplePeriod = 0;
	 * masterConfig.txFifoThreshold = 1;
	 * masterConfig.rxFifoThreshold = 0;
	 */
	ECSPI_MasterGetDefaultConfig(&masterConfig);
	// burstLength should be at least the size of a byte to not lose data per word
	masterConfig.burstLength = SPI_TRANSFER_SIZE * 32;
	masterConfig.baudRate_Bps = TRANSFER_BAUDRATE;

	ECSPI_MasterInit(ECSPI1, &masterConfig, ECSPI_MASTER_CLK_FREQ);

	log(TYPE_LOG_INFO, "spi master ready");
#endif

	rpmsg_ready = true;
	while (true) {
		(void)rpmsg_queue_recv(rpmsg, rpmsg_queue,
				       (uint32_t *)&rpmsg_remote_addr,
				       (char *)&msg, sizeof(msg), NULL,
				       RL_BLOCK);
		switch (msg.type) {
		case TYPE_GPIO:
			if (msg.data > 1) {
				log(TYPE_LOG_WARN,
				    "bad gpio value, stopping loop");
				goto out;
			}
			log(TYPE_LOG_DEBUG, "toggling gpio");
			GPIO_WritePinOutput(GPIO1, 15U, msg.data);
			break;
		case TYPE_LOG_SET_LEVEL:
			log_level = msg.data;
			log(TYPE_LOG_INFO, "set log level");
			break;
		case TYPE_SPI:
#ifdef SPI_MASTER
			log(TYPE_LOG_INFO, "Sending spi message...");

			// generate some data and send it
			for (i = 0; i < SPI_TRANSFER_SIZE - 2; i++) {
				// ensure counter is 4 digits so we get one digit per int
				if (counter > 9999)
					counter = 0;
				sprintf((char *)(masterData + i), "%4d",
					counter++);
			}
			masterData[SPI_TRANSFER_SIZE - 1] = 0;

			masterXfer.txData = masterData;
			masterXfer.rxData = NULL;
			masterXfer.dataSize = SPI_TRANSFER_SIZE;
			masterXfer.channel = kECSPI_Channel0;
			ECSPI_MasterTransferBlocking(ECSPI1, &masterXfer);

			log(TYPE_LOG_INFO, "Done sending, receiving...");

			// wait a bit for slave to prepare data to send back
			SDK_DelayAtLeastUs(
				20000U, SDK_DEVICE_MAXIMUM_CPU_CLOCK_FREQUENCY);

			// read data back and print it
			memset(masterData, 0, sizeof(masterData));
			masterXfer.txData = NULL;
			masterXfer.rxData = masterData;
			masterXfer.dataSize = SPI_TRANSFER_SIZE;
			masterXfer.channel = kECSPI_Channel0;
			ECSPI_MasterTransferBlocking(ECSPI1, &masterXfer);

			log(TYPE_LOG_INFO, "Done receiving:");
			// truncate end to be safe for log(), as that uses strlen
			masterData[SPI_TRANSFER_SIZE - 1] = 0;
			log(TYPE_LOG_INFO, (char *)masterData);

#else
			log(TYPE_LOG_INFO, "spi not compiled in");
#endif
			break;
		default:
			log(TYPE_LOG_WARN, "bad type, stopping loop");
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

// spi slave code
// mostly based on example from mcuxsdk:
// mcuxsdk/examples/evkmimx8mp/driver_examples/ecspi/polling_b2b_transfer/slave/ecspi_polling_b2b_transfer_slave.c
#ifdef SPI_SLAVE
uint32_t slaveData[SPI_TRANSFER_SIZE];
ecspi_slave_handle_t g_s_handle;
volatile bool isTransferCompleted = false;

// callback for slave
void ECSPI_SlaveUserCallback(ECSPI_Type *base, ecspi_slave_handle_t *handle,
			     status_t status, void *userData)
{
	if (status == kStatus_Success) {
		isTransferCompleted = true;
	}

	if (status == kStatus_ECSPI_HardwareOverFlow) {
		log(TYPE_LOG_WARN, "Overflow in spi transfer");
	}
}

static void spi_task(void *param)
{
	ecspi_slave_config_t slaveConfig;
	ecspi_transfer_t slaveXfer;

	/* Slave config:
	 * slaveConfig.channel = kECSPI_Channel0;
	 * slaveConfig.burstLength = 8;
	 * slaveConfig.txFifoThreshold = 1;
	 * slaveConfig.rxFifoThreshold = 0;
	 */
	ECSPI_SlaveGetDefaultConfig(&slaveConfig);
	// burstLength should be at least the size of a byte to not lose data per word
	slaveConfig.burstLength = SPI_TRANSFER_SIZE * 32;
	ECSPI_SlaveInit(ECSPI1, &slaveConfig);

	ECSPI_SlaveTransferCreateHandle(ECSPI1, &g_s_handle,
					ECSPI_SlaveUserCallback, NULL);

	// wait for rpmsg to have finished init handshake
	while (!rpmsg_ready)
		;
	log(TYPE_LOG_INFO, "spi ready");

	for (;;) {
		memset(slaveData, 0, sizeof(slaveData));

		isTransferCompleted = false;
		slaveXfer.txData = NULL;
		slaveXfer.rxData = slaveData;
		slaveXfer.dataSize = SPI_TRANSFER_SIZE;
		slaveXfer.channel = kECSPI_Channel0;
		ECSPI_SlaveTransferNonBlocking(ECSPI1, &g_s_handle, &slaveXfer);

		while (!isTransferCompleted)
			;

		log(TYPE_LOG_INFO, "Got SPI message");
		// truncate end to be safe for log(), as that uses strlen
		slaveData[SPI_TRANSFER_SIZE - 1] = 0;
		log(TYPE_LOG_INFO, (char *)slaveData);

		isTransferCompleted = false;
		slaveXfer.txData = slaveData;
		slaveXfer.rxData = NULL;
		slaveXfer.dataSize = SPI_TRANSFER_SIZE;
		slaveXfer.channel = kECSPI_Channel0;
		ECSPI_SlaveTransferNonBlocking(ECSPI1, &g_s_handle, &slaveXfer);

		while (!isTransferCompleted)
			;

		log(TYPE_LOG_INFO, "Sent message back");
	}
}
#endif

void app_create_task(void)
{
	if (app_task_handle == NULL &&
	    xTaskCreate(app_task, "APP_TASK", APP_TASK_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + 2, &app_task_handle) != pdPASS) {
		PRINTF("\r\nFailed to create application task\r\n");
		for (;;)
			;
	}
#ifdef SPI_SLAVE
	if (spi_task_handle == NULL &&
	    xTaskCreate(spi_task, "SPI_TASK", SPI_TASK_STACK_SIZE, NULL,
			tskIDLE_PRIORITY + 1, &spi_task_handle) != pdPASS) {
		PRINTF("\r\nFailed to create spi task\r\n");
		for (;;)
			;
	}
#endif
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
	gpio_pin_config_t config = { kGPIO_DigitalOutput, 1, kGPIO_NoIntmode };
	GPIO_PinInit(GPIO1, 15U, &config);

	copyResourceTable();

	app_create_task();
	vTaskStartScheduler();

	(void)PRINTF("Failed to start FreeRTOS on core0.\r\n");
	for (;;) {
	}
}
