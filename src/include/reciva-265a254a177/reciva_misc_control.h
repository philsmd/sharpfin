/* Reciva Misc Control */


#ifndef LINUX_RECIVA_MISC_CONTROL_H
#define LINUX_RECIVA_MISC_CONTROL_H

/* NB numbering starts from 1 on the connector and pin numbers */
struct pin_control {
  int connector;
  int pin;
  int data; 
};

/* IOCTL Stuff */
#define RECIVA_MISC_CONTROL_IOCTL_BASE  'X'

/* deprecated old interface (not to be used by app branch 257-a or later) */
#define IOC_RMC_STANDBY          _IOW(RECIVA_MISC_CONTROL_IOCTL_BASE, 0, int)
#define IOC_RMC_ETHERNET_POWER   _IOW(RECIVA_MISC_CONTROL_IOCTL_BASE, 1, int)

/* shiny new interface */
#define IOC_RMC_PIN_WRITE   _IOW(RECIVA_MISC_CONTROL_IOCTL_BASE, 2, struct pin_control)
#define IOC_RMC_PIN_READ    _IOR(RECIVA_MISC_CONTROL_IOCTL_BASE, 3, struct pin_control)
#define IOC_RMC_PIN_PULLUP  _IOW(RECIVA_MISC_CONTROL_IOCTL_BASE, 4, struct pin_control)

#endif
