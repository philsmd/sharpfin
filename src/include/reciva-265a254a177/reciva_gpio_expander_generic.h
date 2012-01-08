#ifndef RECIVA_GPIO_EXPANDER_GENERIC_H
#define RECIVA_GPIO_EXPANDER_GENERIC_H

#include <linux/ioctl.h>


typedef struct 
{
  const char *name;
  int (*pin_write)(int pin, int data);
  int (*pin_read)(int pin);

} reciva_rge_driver_t;

extern int reciva_rge_register_driver(const reciva_rge_driver_t *driver);


#define RECIVA_GPIO_EXPANDER_IOCTL_BASE  'Y'
#define IOC_RGE_PIN_WRITE _IOW(RECIVA_GPIO_EXPANDER_IOCTL_BASE, 1, struct reciva_rge_pin_control)
#define IOC_RGE_PIN_READ _IOR(RECIVA_GPIO_EXPANDER_IOCTL_BASE, 2, struct reciva_rge_pin_control)
struct reciva_rge_pin_control 
{
  int expander;
  int pin;
  int data;
};

#endif
