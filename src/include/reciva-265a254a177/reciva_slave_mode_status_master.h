#ifndef LINUX_RECIVA_SLAVE_MODE_STATUS_MASTER_H
#define LINUX_RECIVA_SLAVE_MODE_STATUS_MASTER_H

#include <linux/ioctl.h>

#define SMSM_BITFIELD_nMRDY        0x01
#define SMSM_BITFIELD_nINTERRUPT   0x02

#define SMSM_IOCTL_BASE  'S'
#define IOC_SMSM_GET_STATUS            _IOR(SMSM_IOCTL_BASE, 0, int)
#define IOC_SMSM_OK_TO_SEND            _IOR(SMSM_IOCTL_BASE, 1, int)

#endif
