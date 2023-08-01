// SPDX-License-Identifier: GPL-2.0
/*
 * Remote processor messaging - armadillo demo
 *
 * Copyright 2023 Atmark Techno
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/rpmsg.h>

#include "../common/protocol.h"

enum ra_state {
	INIT,
	READY,
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARN,
	DEAD,
};

struct rpmsg_armadillo {
	enum ra_state state;
	// for debug messages
	uint32_t length;
};

static int rpmsg_armadillo_cb(struct rpmsg_device *rpdev, void *data, int len,
			      void *priv, u32 src)
{
	struct rpmsg_armadillo *ra = dev_get_drvdata(&rpdev->dev);
	struct msg *msg = data;

	if (ra->state == READY) {
		if (len != sizeof(*msg)) {
			dev_err(&rpdev->dev, "message had unexpected size %d\n",
				len);
			ra->state = DEAD;
			return 0;
		}
		if (msg->type >= TYPE_LOG_DEBUG && msg->type <= TYPE_LOG_WARN) {
			ra->state = LOG_DEBUG + msg->type - TYPE_LOG_DEBUG;
			ra->length = msg->data;
		}
		return 0;
	}
	if (ra->state == INIT) {
		if (len != sizeof(*msg)) {
			dev_err(&rpdev->dev, "message had unexpected size %d\n",
				len);
			ra->state = DEAD;
			return 0;
		}
		if (msg->type != TYPE_VERSION) {
			dev_err(&rpdev->dev,
				"got non-init message in init state: %d\n",
				msg->type);
			ra->state = DEAD;
			return 0;
		}
		if (msg->data != VERSION) {
			dev_err(&rpdev->dev, "bad version %d\n", msg->data);
			ra->state = DEAD;
			return 0;
		}
		ra->state = READY;
		dev_info(&rpdev->dev, "rpmsg_armadillo ready\n");
		return 0;
	}
	// log
	if (len != ra->length) {
		dev_err(&rpdev->dev,
			"log not of expected length: got %d, expected %d\n",
			len, ra->length);
		ra->state = DEAD;
		return 0;
	}
	dev_info(&rpdev->dev, "%s\n", (char *)data);
	ra->state = READY;

	return 0;
}

static int rpmsg_armadillo_probe(struct rpmsg_device *rpdev)
{
	int ret;
	struct rpmsg_armadillo *ra;
	struct msg msg = {
		.type = TYPE_VERSION,
		.data = VERSION,
	};

	dev_info(&rpdev->dev,
		 "probing rpmsg_armadillo on channel 0x%x -> 0x%x\n",
		 rpdev->src, rpdev->dst);

	ra = devm_kzalloc(&rpdev->dev, sizeof(*ra), GFP_KERNEL);
	if (!ra)
		return -ENOMEM;

	ra->state = INIT;
	dev_set_drvdata(&rpdev->dev, ra);

	/* check remote version */
	ret = rpmsg_send(rpdev->ept, &msg, sizeof(msg));
	if (ret) {
		dev_err(&rpdev->dev, "rpmsg_send failed: %d\n", ret);
		return ret;
	}

	return 0;
}

static void rpmsg_armadillo_remove(struct rpmsg_device *rpdev)
{
	dev_info(&rpdev->dev, "removed rpmsg armadillo\n");
}

static struct rpmsg_device_id rpmsg_driver_armadillo_id_table[] = {
	{ .name = RPMSG_LITE_NS_ANNOUNCE_STRING },
	{},
};
MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_armadillo_id_table);

static struct rpmsg_driver rpmsg_armadillo = {
	.drv.name = KBUILD_MODNAME,
	.id_table = rpmsg_driver_armadillo_id_table,
	.probe = rpmsg_armadillo_probe,
	.callback = rpmsg_armadillo_cb,
	.remove = rpmsg_armadillo_remove,
};
module_rpmsg_driver(rpmsg_armadillo);

MODULE_DESCRIPTION("rpmsg interface with armadillo RTOS demo");
MODULE_LICENSE("GPL v2");
