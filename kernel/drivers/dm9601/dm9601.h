/* 
 * Sharpfin project
 * Copyright (C) 1999,2000 Petko Manolov - Petkan (petkan@dce.bg)
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 *  
 * This Library is free software: you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License as published by 
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *  
 * You should have received a copy of the GNU General Public License
 * along with this source files. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifndef	DM9601_DEV
#define	HAS_HOME_PNA		0x40000000

#define	DM9601_MTU		1500
#define	DM9601_MAX_MTU		1536

#define	EPROM_WRITE		0x01
#define	EPROM_READ		0x02
#define	EPROM_LOAD		0x20

#define	MII_BMCR		0x00
#define	MII_BMSR		0x01
#define	PHY_READ		0x40
#define	PHY_WRITE		0x20

#define	DM9601_PRESENT		0x00000001
#define	DM9601_RUNNING		0x00000002
#define	DM9601_TX_BUSY		0x00000004
#define	DM9601_RX_BUSY		0x00000008
#define	CTRL_URB_RUNNING	0x00000010
#define	CTRL_URB_SLEEP		0x00000020
#define	DM9601_UNPLUG		0x00000040
#define	DM9601_RESET_WAIT	0x00800000
#define	NET_CTRL_CHANGE		0x04000000
#define	NET_CTRL_CHANGED	0x08000000
#define	RX_CTRL_CHANGE		0x10000000
#define	RX_CTRL_CHANGED		0x20000000
#define	HASH_REGS_CHANGE	0x40000000
#define	HASH_REGS_CHANGED	0x80000000
#define ALL_REGS_CHANGE		(NET_CTRL_CHANGE | RX_CTRL_CHANGE | HASH_REGS_CHANGE)
#define ALL_REGS_CHANGED	(NET_CTRL_CHANGED | RX_CTRL_CHANGED | HASH_REGS_CHANGED)
#define	DEFAULT_GPIO_RESET	0x24
#define	LINKSYS_GPIO_RESET	0x24
#define	DEFAULT_GPIO_SET	0x26

#define	RX_PASS_MULTICAST	8
#define	RX_PROMISCUOUS		2

#define	REG_TIMEOUT		(HZ)
#define	DM9601_TX_TIMEOUT	(HZ*10)

#define	TX_UNDERRUN		0x80
#define	EXCESSIVE_COL		0x40
#define	LATE_COL		0x20
#define	NO_CARRIER		0x10
#define	LOSS_CARRIER		0x08
#define	JABBER_TIMEOUT		0x04

#define	DM9601_REQT_READ	0xc0
#define	DM9601_REQ_GET_REGS	0x00
#define	DM9601_REQ_GET_MEMS	0x02

#define	DM9601_REQT_WRITE	0x40
#define	DM9601_REQ_SET_REGS	0x01
#define	DM9601_REQ_SET_REG	0x03
#define	DM9601_REQ_SET_MEMS	0x05
#define	DM9601_REQ_SET_MEM	0x07

#define DM9601_10MHF		0
#define DM9601_100MHF		1
#define DM9601_10MFD		4
#define DM9601_100MFD		5
#define DM9601_AUTO		8
#define DM9601_1M_HPNA		0x10

#define DM9601_REG5		0x30
#define DM9601_REG8		0x27
#define DM9601_REG9		0x38
#define DM9601_REGA		0xff

#define DM9801_NOISE_FLOOR	0x08
#define DM9802_NOISE_FLOOR	0x05

enum DM9601_NIC_TYPE {
	FASTETHER_NIC = 0, HOMERUN_NIC = 1, LONGRUN_NIC = 2 };

enum DM9601_MII_TYPE {
	MII_TYPE_INT = 0, MII_TYPE_EXT = 1 };

#define	ALIGN(x)		x __attribute__((aligned(L1_CACHE_BYTES)))

typedef struct dm9601_board_info {
	struct usb_device	*usb;
	struct net_device	*net;
	struct net_device_stats	stats;
	unsigned long		rx_longf_errors, rx_runtf_errors, rx_lc_errors,
				rx_wdt_errors, rx_ple_errors;
	unsigned		flags;
	unsigned		features;
	int			dev_index;
	int			intr_interval;
	struct urb		ctrl_urb, rx_urb, tx_urb, intr_urb, dump_urb;
	struct usb_ctrlrequest	dr;
	wait_queue_head_t	ctrl_wait;
	struct semaphore	ctrl_sem;
	unsigned char		ALIGN(rx_buff[DM9601_MAX_MTU]);
	unsigned char		ALIGN(rx_buff2[DM9601_MAX_MTU]);
	unsigned char		ALIGN(tx_buff[DM9601_MAX_MTU]);
	unsigned char		ALIGN(intr_buff[8]);
	unsigned char		ALIGN(dump_buff[8]);
	__u16			hash_table[4];
	__u8			rx_ctrl_reg, net_ctrl_reg, reg08, reg09, reg0a;
	__u8			phy;
	__u8			gpio_res;
	__u8			rx_buf_flag;
	__u8			nic_type;
	__u8			op_mode;
} dm9601_board_info_t;


struct usb_eth_dev {
	char	*name;
	__u16	vendor;
	__u16	device;
	__u32	private; /* LSB is gpio reset value */
};


#define VENDOR_ACCTON           0x083a
#define VENDOR_ADMTEK           0x07a6
#define VENDOR_BILLIONTON       0x08dd
#define VENDOR_COREGA           0x07aa
#define VENDOR_DLINK1           0x2001
#define VENDOR_DLINK2           0x07b8
#define VENDOR_IODATA           0x04bb
#define VENDOR_LANEED           0x056e
#define VENDOR_LINKSYS          0x066b
#define VENDOR_MELCO            0x0411
#define VENDOR_SMC              0x0707
#define VENDOR_SOHOWARE         0x15e8


#else	/* DM9601_DEV */


DM9601_DEV( "Accton USB 10/100 Ethernet Adapter", VENDOR_ACCTON, 0x1046,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "ADMtek AN986 \"Pegasus\" USB Ethernet (eval board)",
		VENDOR_ADMTEK, 0x0986,
		DEFAULT_GPIO_RESET | HAS_HOME_PNA )
DM9601_DEV( "Davicom USB-100", 0x0a46, 0x9601,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Davicom USB-100", 0x3334, 0x1701,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Billionton USB-100", VENDOR_BILLIONTON, 0x0986,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Billionton USBLP-100", VENDOR_BILLIONTON, 0x0987,
		DEFAULT_GPIO_RESET | HAS_HOME_PNA )
DM9601_DEV( "Billionton USBEL-100", VENDOR_BILLIONTON, 0x0988,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Corega FEter USB-TX", VENDOR_COREGA, 0x0004,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Corega FEter USB-TXC", VENDOR_COREGA, 0x9601, 
		DEFAULT_GPIO_RESET )
DM9601_DEV( "D-Link DSB-650TX", VENDOR_DLINK1, 0x4001,
		LINKSYS_GPIO_RESET )
DM9601_DEV( "D-Link DSB-650TX", VENDOR_DLINK1, 0x4002,
		LINKSYS_GPIO_RESET )
DM9601_DEV( "D-Link DSB-650TX(PNA)", VENDOR_DLINK1, 0x4003,
		DEFAULT_GPIO_RESET | HAS_HOME_PNA )
DM9601_DEV( "D-Link DSB-650", VENDOR_DLINK1, 0xabc1,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "D-Link DU-E10", VENDOR_DLINK2, 0xabc1,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "D-Link DU-E100", VENDOR_DLINK2, 0x4002,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "IO DATA USB ET/TX", VENDOR_IODATA, 0x0904,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "LANEED USB Ethernet LD-USB/TX", VENDOR_LANEED, 0x4002,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "Linksys USB10TX", VENDOR_LINKSYS, 0x2202,
		LINKSYS_GPIO_RESET )
DM9601_DEV( "Linksys USB100TX", VENDOR_LINKSYS, 0x2203,
		LINKSYS_GPIO_RESET )
DM9601_DEV( "Linksys USB100TX", VENDOR_LINKSYS, 0x2204,
		LINKSYS_GPIO_RESET | HAS_HOME_PNA )
DM9601_DEV( "Linksys USB Ethernet Adapter", VENDOR_LINKSYS, 0x2206,
		LINKSYS_GPIO_RESET )
DM9601_DEV( "MELCO/BUFFALO LUA-TX", VENDOR_MELCO, 0x0001,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "SMC 202 USB Ethernet", VENDOR_SMC, 0x0200,
		DEFAULT_GPIO_RESET )
DM9601_DEV( "SOHOware NUB100 Ethernet", 0x0a46, 0x9601,
		DEFAULT_GPIO_RESET )
#endif	/* _DEV */
