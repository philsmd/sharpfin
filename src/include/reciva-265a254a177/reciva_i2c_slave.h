#ifndef LINUX_RECIVA_I2C_SLAVE_H
#define LINUX_RECIVA_I2C_SLAVE_H

#include <linux/ioctl.h>

#define I2C_IOCTL_BASE  'I'
#define IOC_I2C_RESET                 _IO(I2C_IOCTL_BASE,  0)
#define IOC_I2C_EXIT_ON_NEXT_TX       _IO(I2C_IOCTL_BASE,  1)

#endif
