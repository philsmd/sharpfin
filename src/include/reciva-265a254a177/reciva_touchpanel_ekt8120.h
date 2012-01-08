/* Reciva eKT8120 Touchpanel */


#ifndef LINUX_RECIVA_TOUCHPANEL_EKT8120_H
#define LINUX_RECIVA_TOUCHPANEL_EKT8120_H

struct reciva_touchpanel_map
{
  int nbuttons;
  int *codes;
};

/* IOCTL Stuff */
#define TOUCHPANEL_IOCTL_BASE  'T'
#define IOC_TOUCHPANEL_SENSITIVITY    _IOW(TOUCHPANEL_IOCTL_BASE, 0, int)
#define IOC_TOUCHPANEL_BUTTONSTATE    _IOW(TOUCHPANEL_IOCTL_BASE, 1, int)
#define IOC_TOUCHPANEL_LOAD_MAP       _IOW(TOUCHPANEL_IOCTL_BASE, 2, struct reciva_touchpanel_map)

#endif
