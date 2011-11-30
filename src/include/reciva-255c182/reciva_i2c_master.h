#ifndef LINUX_RECIVA_I2C_MASTER_H
#define LINUX_RECIVA_I2C_MASTER_H

#include <linux/ioctl.h>

#define I2C_IOCTL_BASE  'I'
#define IOC_RI2CM_RESET                 _IO(I2C_IOCTL_BASE,  0)
#define IOC_RI2CM_INIT                  _IOW(I2C_IOCTL_BASE, 1, int)
#define IOC_RI2CM_GET_STATUS            _IOR(I2C_IOCTL_BASE, 2, int)

/* Bit definitions for IOC_RI2CM_GET_STATUS */
#define RI2CM_STATUS_SLAVE_DETECTED 0x01

#endif
