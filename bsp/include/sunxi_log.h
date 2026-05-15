/*
 * Copyright (C) 2023 Allwinner.
 *
 * SPDX-License-Identifier: GPL-2.0+
 */

#ifndef __SUNXI_LOG_H__
#define __SUNXI_LOG_H__


#ifdef SUNXI_MODNAME
#ifdef LOG_CATEGORY
#undef LOG_CATEGORY
#endif
#define LOG_CATEGORY   SUNXI_MODNAME
#else
#ifndef LOG_CATEGORY
#define LOG_CATEGORY   LOGC_NONE
#endif
#endif

#include <dm/device.h>

static inline const char *sunxi_log_dev_name(const struct udevice *dev)
{
#ifdef CONFIG_LOGF_DEVNAME
	if (dev && dev->name)
		return dev->name;
#endif
	return NULL;
}

#define sunxi_err(dev, fmt, ...)							\
do {											\
	if (sunxi_log_dev_name(dev))							\
		log_err("%s: "fmt, sunxi_log_dev_name(dev), ## __VA_ARGS__);		\
	else										\
		log_err(fmt,  ## __VA_ARGS__);						\
} while (0)

#define sunxi_warn(dev, fmt, ...)							\
do {											\
	if (sunxi_log_dev_name(dev))							\
		log_warn("%s: "fmt, sunxi_log_dev_name(dev), ## __VA_ARGS__);		\
	else										\
		log_warn(fmt,  ## __VA_ARGS__);						\
} while (0)

#define sunxi_info(dev, fmt, ...)							\
do {											\
	if (sunxi_log_dev_name(dev))							\
		log_info("%s: "fmt, sunxi_log_dev_name(dev), ## __VA_ARGS__);		\
	else										\
		log_info(fmt,  ## __VA_ARGS__);						\
} while (0)

#define sunxi_debug(dev, fmt, ...)							\
do {											\
	if (sunxi_log_dev_name(dev))							\
		log_debug("%s: "fmt, sunxi_log_dev_name(dev), ## __VA_ARGS__);		\
	else										\
		log_debug(fmt,  ## __VA_ARGS__);					\
} while (0)

#endif /* __SUNXI_LOG_H__ */

