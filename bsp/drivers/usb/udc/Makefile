obj-$(CONFIG_AW_UDC_CONTROLLER) += sunxi-udc.o
obj-$(CONFIG_AW_UDC_CONTROLLER) += sunxi_udc_core.o sunxi_udc_gadget.o sunxi_udc_gadget_ep0.o

obj-$(CONFIG_AW_USB_PHY) += phy/sunxi-usb-phy.o

ccflags-$(CONFIG_AW_UDC_CONTROLLER) += -I$(srctree)/bsp/drivers/usb/udc/include
