/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __LINUX_COMPAT_H__
#define __LINUX_COMPAT_H__

#include <malloc.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/compat.h>

#define device_init_wakeup(dev, a) do {} while (0)

#define platform_data device_data

#define msleep(a)	udelay(a * 1000)

#endif /* __LINUX_COMPAT_H__ */
