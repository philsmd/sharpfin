#ifndef LINUX_RECIVA_SLAVE_MODE_STATUS_SLAVE_H
#define LINUX_RECIVA_SLAVE_MODE_STATUS_SLAVE_H

#include <linux/ioctl.h>

#define SMSS_IOCTL_BASE  'S'
#define IOC_SMSS_SET_NMRDY            _IOW(SMSS_IOCTL_BASE, 0, int)
#define IOC_SMSS_SET_NINTERRUPT       _IOW(SMSS_IOCTL_BASE, 1, int)

#endif
