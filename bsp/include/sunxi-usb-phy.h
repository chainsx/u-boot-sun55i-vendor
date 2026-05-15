/* SPDX-License-Identifier:	GPL-2.0+
 * (C) Copyright 2018 allwinnertech  <wangwei@allwinnertech.com>
 */

#ifndef __SUNXI_USB_PHY_H
#define __SUNXI_USB_PHY_H

/**
 * sunxi_usb_phy_id_detect - detect ID pin of USB PHY
 *
 * @phy:	USB PHY port to detect ID pin
 * Return: 0 if OK, or a negative error code
 */
int sunxi_usb_phy_id_detect(struct phy *phy);

/**
 * sunxi_usb_phy_vbus_detect - detect VBUS pin of USB PHY
 *
 * @phy:	USB PHY port to detect VBUS pin
 * Return: 0 if OK, or a negative error code
 */
int sunxi_usb_phy_vbus_detect(struct phy *phy);

/**
 * sunxi_usb_phy_set_squelch_detect() - Enable/disable squelch detect
 *
 * @phy: reference to a sunxi usb phy
 * @enabled: wether to enable or disable squelch detect
 */
void sunxi_usb_phy_set_squelch_detect(struct phy *phy, bool enabled);

#endif /*__SUNXI_USB_PHY_H */
