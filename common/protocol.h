// SPDX-License-Identifier: MIT

// channel name, up to 32 bytes
#define RPMSG_LITE_NS_ANNOUNCE_STRING "rpmsg-armadillo-demo-channel"

// initial handshake must be TYPE_VERSION, data = VERSION
#define TYPE_VERSION 0x0
#define VERSION 0

// request (linux -> rtos) GPIO state change
// data = 0 or 1 (unset or set)
#define TYPE_GPIO 0x1

// trigger sending spi message
#define TYPE_SPI 0x2

// rtos cannot use serial so logs are send over rpmsg
// linux -> rtos: set log level
// data = level (0 = debug+, 1 = info+, 2 = warn+, 3 = nothing)
#define TYPE_LOG_SET_LEVEL 0x10
// rtos -> linux: log content
// data = log length, immediately followed by another message
// of said length
#define TYPE_LOG_DEBUG 0x11
#define TYPE_LOG_INFO 0x12
#define TYPE_LOG_WARN 0x13

// any other type is an error.
struct msg {
	uint32_t type;
	uint32_t data;
} __attribute__((packed));
