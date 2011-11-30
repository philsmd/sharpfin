#ifndef LINUX_RECIVA_SLAVE_MODE_CONTROLLER_H
#define LINUX_RECIVA_SLAVE_MODE_CONTROLLER_H

/* Reciva Slave Mode Controller */
#include <linux/ioctl.h>

typedef enum
{
  SMC_DRIVER_UNKNOWN = 0,
  SMC_DRIVER_I2C     = 1,
  SMC_DRIVER_SERIAL  = 2,

} smc_driver_id_t;

/* Slave Mode Controller Driver */
typedef struct 
{
  const char * (*smc_get_name)(void);
  void (*smc_reset)(void);
  void (*smc_write)(const char *, int);
  smc_driver_id_t (*smc_get_driver_id)(void);
  int (*smc_get_tx_buffer_empty)(void);

} reciva_smc_driver_t;

extern int reciva_smc_register_driver(const reciva_smc_driver_t *driver);
extern void reciva_smc_command(const char *data, int length);



#define SMC_IOCTL_BASE  'S'
#define IOC_SMC_RESET                 _IO(SMC_IOCTL_BASE,  0)
#define IOC_SMC_ATTENTION_REQ         _IO(SMC_IOCTL_BASE,  1)
#define IOC_SMC_ATTENTION_ACK         _IO(SMC_IOCTL_BASE,  2)
#define IOC_SMC_GET_DRIVER_ID         _IOR(SMC_IOCTL_BASE, 3, int)
#define IOC_SMC_GET_TX_BUFFER_EMPTY   _IOR(SMC_IOCTL_BASE, 4, int)


#endif
