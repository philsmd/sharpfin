#ifndef LINUX_STATUS_PINS_H
#define LINUX_STATUS_PINS_H

#include <linux/ioctl.h>

#define LCD_IOCTL_BASE  'L'
#define IOC_RSP_SET     _IOW(LCD_IOCTL_BASE, 1, int)

#endif
