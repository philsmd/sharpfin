#define SI4700_IOCTL_BASE  'S'

#define IOC_SI4700_TUNE    _IOW(SI4700_IOCTL_BASE, 0, int)
#define IOC_SI4700_STATUS  _IOR(SI4700_IOCTL_BASE, 1, int)
#define IOC_SI4700_SEEK    _IOW(SI4700_IOCTL_BASE, 2, int)
#define IOC_SI4700_VOLUME  _IOW(SI4700_IOCTL_BASE, 3, int)

struct si4700_packet
{
	int code;
	union {
		char c[28];
		struct {
			unsigned int a, b, c, d;
		} rds;
	} u;
};

#define SI4700_PACKET_RDS	1
