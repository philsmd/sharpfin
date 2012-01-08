#define TEA5767_IOCTL_BASE  'S'

#define IOC_TEA5767_TUNE       _IOW(TEA5767_IOCTL_BASE, 0, int)
#define IOC_TEA5767_STATUS     _IOR(TEA5767_IOCTL_BASE, 1, int)
#define IOC_TEA5767_SEEK       _IOW(TEA5767_IOCTL_BASE, 2, int)
#define IOC_TEA5767_MONO       _IOW(TEA5767_IOCTL_BASE, 3, int)
#define IOC_TEA5767_GET_FREQ   _IOR(TEA5767_IOCTL_BASE, 4, int)
#define IOC_TEA5767_TOGGLE_HI_LO_BIT _IOW(TEA5767_IOCTL_BASE, 5, int)
#define IOC_TEA5767_TUNE_ASYNC _IOW(TEA5767_IOCTL_BASE, 6, int)

struct tea5767_packet
{
	int code;
	union {
		char c[28];
		struct {
			unsigned int a, b, c, d;
		} rds;
		struct {
			int status;
			int rssi;
			int frequency;
		} seek;
	} u;
};

#define TEA5767_PACKET_RDS		1
#define TEA5767_PACKET_SEEK_STATUS	2
#define TEA5767_PACKET_TUNE_COMPLETE    3


