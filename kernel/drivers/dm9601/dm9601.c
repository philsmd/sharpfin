/* 
 * Sharpfin project
 * Copyright (C) 1999,2000 Petko Manolov - Petkan (petkan@dce.bg)
 * 2011-11-30 Philipp Schmidt
 *   Added to github 
 *
 * This file is part of the sharpfin project
 * DM9601: USB 10/100Mbps/HomePNA (1Mbps) Fast Ethernet
 *	ChangeLog:
 *	v0.01		 Work for DM9601 now.
 *	v0.02		 rename to dm9601.
 *	v0.03		 Debug TX full to cause TX hang problem.
 *	v0.04		 Support MAC/Hash address
 *			 REG5 get better RX performance when bit4=0
 *			 Power-Down PHY when driver close
 *	v0.05		 Support dynamic reset
 *			 Support automatically switch IntPHY/EXT MII
 *			 Support REG_8, REG_9, REG_A for flow control
 *	V0.06   06/14/01 Dynamic select INT/EXT MII by REG1 bit 4
 *			 Support Force and Auto mode
 *	V0.07	06/14/01 Program HPNA chip E3/E4/E5
 *	V0.08	06/15/01 Enable REG_F4 bit5 to force "INT always return"
 *	V0.09	06/19/01 Default set REG_0A bit3 to enable BP with DA match
 *		06/22/01 Modify DM9801 progrmming	
 *			 E3: R25 = ((R24 + NF) & 0x00ff) | 0xf000
 *			 E4: R25 = ((R24 + NF) & 0x00ff) | 0xc200
 *			     R17 = (R17 & 0xfff0) | NF + 3
 *			 E5: R25 = ((R24 + NF - 3) & 0x00ff) | 0xc200
 *			     R17 = (R17 & 0xfff0) | NF
 * 
 *  V1.00   03/05/03 Weilun Huang <weilun_huang@davicom.com.tw>:
 *			 Added semaphore mechanism to solve the problem.
 *			 While device is being plugged, it makes kernel
 *			 hang on VIA chipset. Removed the devrequest typedef
 *			 use "struct usb_ctrlrequest".
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

#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/usb.h>
#include <linux/module.h>
#include <linux/crc32.h>
#include "dm9601.h"
#define	DM9601_USE_INTR
static const char *version = __FILE__ ": v0.0.6 2001/05/24 (C) 1999-2000 Petko Manolov (petkan@dce.bg)";
static struct usb_eth_dev usb_dev_id[] = {
#define	DM9601_DEV(pn, vid, pid, flags)	\
	{name:pn, vendor:vid, device:pid, private:flags},
#include "dm9601.h"
#undef	DM9601_DEV
	{NULL, 0, 0, 0}
};

static struct usb_device_id dm9601_ids[] = {
#define	DM9601_DEV(pn, vid, pid, flags) \
	{match_flags: USB_DEVICE_ID_MATCH_DEVICE, idVendor:vid, idProduct:pid},
#include "dm9601.h"
#undef	DM9601_DEV
	{ }
};

/* For module input parameter */
static int mode = DM9601_AUTO, dm9601_mode;
static 	u8 reg5 = DM9601_REG5, reg8 = DM9601_REG8, reg9 = DM9601_REG9, 
	rega = DM9601_REGA, nfloor = 0;

MODULE_AUTHOR("Petko Manolov <petkan@dce.bg>");
MODULE_DESCRIPTION("DAVICOM DM9601 USB Fast Ethernet driver");
MODULE_LICENSE("GPL") ;
MODULE_PARM(mode, "i");
MODULE_PARM(reg5, "i");
MODULE_PARM(reg8, "i");
MODULE_PARM(reg9, "i");
MODULE_PARM(rega, "i");
MODULE_PARM(nfloor, "i");
MODULE_PARM_DESC(mode, "Media mode select: 0:10MH 1:100MHF 4:10MF 5:100MF 8:AUTO");
MODULE_DEVICE_TABLE (usb, dm9601_ids);

static int write_eprom_word(dm9601_board_info_t *, __u8, __u16);
static int update_eth_regs_async(dm9601_board_info_t *);

/* Aargh!!! I _really_ hate such tweaks */
static void ctrl_callback(struct urb *urb) {
	dm9601_board_info_t	*dbi = urb->context;
	if (!dbi)
		return;

	switch (urb->status & 0xff) {
		case USB_ST_NOERROR:
		case 0x92:
			if (dbi->flags & ALL_REGS_CHANGE) {
				update_eth_regs_async(dbi);
				return;
			}
			break;
		case USB_ST_URB_PENDING:
		case 0x8d:
			return;
		case USB_ST_URB_KILLED:
			break;
		default:
			warn("%s: status %x",__FUNCTION__, urb->status);
	}

	dbi->flags &= ~ALL_REGS_CHANGED;
	
	if (dbi->flags & CTRL_URB_SLEEP) {
		dbi->flags &= ~CTRL_URB_SLEEP;
		wake_up_interruptible(&dbi->ctrl_wait);
	}
}


static int get_registers(dm9601_board_info_t *dbi, __u16 indx, __u16 size, void *data) {
	int	ret;
	DECLARE_WAITQUEUE(wait, current);

	while ( dbi->flags & ALL_REGS_CHANGED ) {
		dbi->flags |= CTRL_URB_SLEEP;
		interruptible_sleep_on( &dbi->ctrl_wait );
	}
	
	dbi->dr.bRequestType = DM9601_REQT_READ;
	dbi->dr.bRequest     = DM9601_REQ_GET_REGS;
	dbi->dr.wValue       = cpu_to_le16 (0);
	dbi->dr.wIndex       = cpu_to_le16p(&indx);
	dbi->dr.wLength      = cpu_to_le16p(&size);
	dbi->ctrl_urb.transfer_buffer_length = size;

	FILL_CONTROL_URB( &dbi->ctrl_urb, dbi->usb,
			  usb_rcvctrlpipe(dbi->usb,0),
			  (char *)&dbi->dr,
			  data, size, ctrl_callback, dbi );

	add_wait_queue( &dbi->ctrl_wait, &wait );
	set_current_state( TASK_INTERRUPTIBLE );
	dbi->flags |= CTRL_URB_SLEEP;

	if ( (ret = usb_submit_urb( &dbi->ctrl_urb )) ) {
		err("%s: BAD CTRLs %d",__FUNCTION__,ret);
		goto out;
	}

	schedule();
	remove_wait_queue( &dbi->ctrl_wait, &wait );
out:
	return	ret;
}


static int set_registers(dm9601_board_info_t *dbi, __u16 indx, __u16 size, void *data)
{
	int	ret;
	DECLARE_WAITQUEUE(wait, current);

	while (dbi->flags & ALL_REGS_CHANGED) {
		dbi->flags |= CTRL_URB_SLEEP ;
		interruptible_sleep_on(&dbi->ctrl_wait);
	}
	
	dbi->dr.bRequestType = DM9601_REQT_WRITE;
	dbi->dr.bRequest     = DM9601_REQ_SET_REGS;
	dbi->dr.wValue       = cpu_to_le16(0);
	dbi->dr.wIndex       = cpu_to_le16p(&indx);
	dbi->dr.wLength      = cpu_to_le16p(&size);
	dbi->ctrl_urb.transfer_buffer_length = size;

	FILL_CONTROL_URB(&dbi->ctrl_urb, dbi->usb,
			  usb_sndctrlpipe(dbi->usb, 0),
			  (char *)&dbi->dr,
			  data, size, ctrl_callback, dbi);
			  
	add_wait_queue(&dbi->ctrl_wait, &wait);
	set_current_state(TASK_INTERRUPTIBLE);
	dbi->flags |= CTRL_URB_SLEEP;

	if ( (ret = usb_submit_urb(&dbi->ctrl_urb)) ) {
		err("%s: BAD CTRL %d",__FUNCTION__,ret);
		return	ret;
	}

	schedule();
	remove_wait_queue( &dbi->ctrl_wait, &wait );

	return	ret;
}


static int set_register( dm9601_board_info_t *dbi, __u16 indx, __u8 data )
{
	int	ret;
	__u16 dat = data;
	DECLARE_WAITQUEUE(wait, current);
	
	while ( dbi->flags & ALL_REGS_CHANGED ) {
		dbi->flags |= CTRL_URB_SLEEP;
		interruptible_sleep_on( &dbi->ctrl_wait );
	}
	
	dbi->dr.bRequestType = DM9601_REQT_WRITE;
	dbi->dr.bRequest     = DM9601_REQ_SET_REG;
	dbi->dr.wValue 	     = cpu_to_le16p( &dat);
	dbi->dr.wIndex 	     = cpu_to_le16p( &indx );
	dbi->dr.wLength      = cpu_to_le16( 0 );
	dbi->ctrl_urb.transfer_buffer_length = 0;

	FILL_CONTROL_URB( &dbi->ctrl_urb, dbi->usb,
			  usb_sndctrlpipe(dbi->usb,0),
			  (char *)&dbi->dr,
			  &data, 0, ctrl_callback, dbi );

	add_wait_queue( &dbi->ctrl_wait, &wait );
	set_current_state( TASK_INTERRUPTIBLE );
	dbi->flags |= CTRL_URB_SLEEP;

	if ( (ret = usb_submit_urb( &dbi->ctrl_urb )) ) {
		err("%s: BAD CTRL %d",__FUNCTION__,ret);
		return	ret;
	}

	schedule();
	remove_wait_queue( &dbi->ctrl_wait, &wait );

	return	ret;
}

static int update_eth_regs_async( dm9601_board_info_t *dbi )
{
	int	ret;

	if (dbi->flags & HASH_REGS_CHANGE) {
		dbi->flags &= ~HASH_REGS_CHANGE;
		dbi->flags |= HASH_REGS_CHANGED;
		dbi->dr.bRequestType = DM9601_REQT_WRITE;
		dbi->dr.bRequest     = DM9601_REQ_SET_REGS;
		dbi->dr.wValue       = cpu_to_le16(0);
		dbi->dr.wIndex       = cpu_to_le16(0x16);
		dbi->dr.wLength      = cpu_to_le16(8);
		dbi->ctrl_urb.transfer_buffer_length = 8;

		FILL_CONTROL_URB( &dbi->ctrl_urb, dbi->usb,
			  usb_sndctrlpipe(dbi->usb,0),
			  (char *)&dbi->dr,
			  dbi->hash_table, 8, ctrl_callback, dbi );
	} else if (dbi->flags & RX_CTRL_CHANGE) {
		dbi->flags &= ~RX_CTRL_CHANGE;
		dbi->flags |= RX_CTRL_CHANGED;
		dbi->dr.bRequestType = DM9601_REQT_WRITE;
		dbi->dr.bRequest     = DM9601_REQ_SET_REG;
		dbi->dr.wValue       = cpu_to_le16(dbi->rx_ctrl_reg);
		dbi->dr.wIndex       = cpu_to_le16(0x5);
		dbi->dr.wLength       = cpu_to_le16(0);
		dbi->ctrl_urb.transfer_buffer_length = 0;

		FILL_CONTROL_URB( &dbi->ctrl_urb, dbi->usb,
			  usb_sndctrlpipe(dbi->usb,0),
			  (char *)&dbi->dr,
			  &dbi->rx_ctrl_reg, 0, ctrl_callback, dbi );
	} else {
		dbi->flags &= ~NET_CTRL_CHANGE;
		dbi->flags |= NET_CTRL_CHANGED;
		dbi->dr.bRequestType = DM9601_REQT_WRITE;
		dbi->dr.bRequest     = DM9601_REQ_SET_REG;
		dbi->dr.wValue       = cpu_to_le16(dbi->net_ctrl_reg);
		dbi->dr.wIndex       =  cpu_to_le16(0x0);
		dbi->dr.wLength      = cpu_to_le16(0);
		dbi->ctrl_urb.transfer_buffer_length = 0;

		FILL_CONTROL_URB( &dbi->ctrl_urb, dbi->usb,
			  usb_sndctrlpipe(dbi->usb,0),
			  (char *)&dbi->dr,
			  &dbi->net_ctrl_reg, 0, ctrl_callback, dbi );
	}

	if ( (ret = usb_submit_urb( &dbi->ctrl_urb )) )
		err("%s: BAD CTRL %d, flags %x",__FUNCTION__,ret,dbi->flags );

	return	ret;
}

static int read_mii_word( dm9601_board_info_t *dbi, __u8 phy, __u8 index, __u16 *regd )
{
	set_register( dbi, 0x0c, index | 0x40 );
	set_register( dbi, 0x0b, 0x0c );
	wait_ms(100);
	set_register( dbi, 0x0b, 0x0 );
	get_registers( dbi, 0xd, 2, regd);

	return 0;
}


static int write_mii_word( dm9601_board_info_t *dbi, __u8 phy, __u8 index, __u16 regd )
{
	set_register( dbi, 0x0c, index | 0x40 );
	set_registers( dbi, 0xd, 2, &regd);
	set_register( dbi, 0x0b, 0x0a );
	wait_ms(100);
	set_register( dbi, 0x0b, 0x0 );

	return 0;
}


static int read_eprom_word( dm9601_board_info_t *dbi, __u8 index, __u16 *retdata )
{
	set_register( dbi, 0x0c, index );
	set_register( dbi, 0x0b, 0x4 );
	wait_ms(100);
	set_register( dbi, 0x0b, 0x0 );
	get_registers( dbi, 0xd, 2, retdata);

	return 0;
}

static int write_eprom_word( dm9601_board_info_t *dbi, __u8 index, __u16 data )
{
	set_register(dbi, 0x0c, index);
	set_registers(dbi, 0x0d, 2, &data);
	set_register(dbi, 0x0b, 0x12);
	wait_ms(100);
	set_register(dbi, 0x0b, 0x0);
	return 0;
}

static void read_bulk_callback( struct urb *urb )
{
	dm9601_board_info_t *dbi = urb->context;
	struct net_device *net = dbi->net;
	int count = urb->actual_length, res;
	__u8 rx_status;
	struct sk_buff	*skb;
	__u16 pkt_len;
	unsigned char * bufptr;

	if ( !dbi || !(dbi->flags & DM9601_RUNNING) )
		return;

	if ( !netif_device_present(net) )
		return;

	if ( dbi->flags & DM9601_RX_BUSY ) {
		dbi->stats.rx_errors++;
		dbg("DM9601 Rx busy");
		return;
	}
	dbi->flags |= DM9601_RX_BUSY;

	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_NORESPONSE:
			dbg( "reset MAC" );
			dbi->flags &= ~DM9601_RX_BUSY;
			break;
		default:
#ifdef RX_IMPROVE
			dbg("%s: RX status %d",net->name, urb->status );
			goto goon;
#endif
	}

/* For RX improve ---------------------------*/
#ifdef RX_IMPROVE
	if (dbi->rx_buf_flag) {
		bufptr = dbi->rx_buff;
		FILL_BULK_URB( &dbi->rx_urb, dbi->usb,
				usb_rcvbulkpipe(dbi->usb, 1),
				dbi->rx_buff2, DM9601_MAX_MTU, 
				read_bulk_callback, dbi );
	} else {
		bufptr = dbi->rx_buff2;
		FILL_BULK_URB( &dbi->rx_urb, dbi->usb,
				usb_rcvbulkpipe(dbi->usb, 1),
				dbi->rx_buff, DM9601_MAX_MTU, 
				read_bulk_callback, dbi );
	}

	if ( (res = usb_submit_urb(&dbi->rx_urb)) )
		warn("%s: failed submint rx_urb %d",__FUNCTION__,res);

	dbi->flags &= ~DM9601_RX_BUSY;
	dbi->rx_buf_flag = dbi->rx_buf_flag ? 0:1;
#else
	bufptr = dbi->rx_buff;
#endif
/* ----------------------------------------------------------*/

	if ( !count )
		goto goon;

	rx_status = *(__u8 *)(bufptr);
	pkt_len = *(__u16 *)(bufptr + 1) - 4;

	dbi->stats.rx_bytes += pkt_len;
	if ( (rx_status & 0xbf) || (pkt_len > 1518) ) {
		dbi->stats.rx_errors++;
		if (pkt_len > 1518) dbi->rx_longf_errors++;
		if (rx_status & 0x80) dbi->rx_runtf_errors++;
		if (rx_status & 0x20) dbi->rx_lc_errors++;
		if (rx_status & 0x10) dbi->rx_wdt_errors++;
		if (rx_status & 0x08) dbi->rx_ple_errors++;
		if (rx_status & 0x04) dbi->stats.rx_frame_errors++;
		if (rx_status & 0x02) dbi->stats.rx_crc_errors++;
		if (rx_status & 0x1) dbi->stats.rx_fifo_errors++;
		goto goon;
	}

	if ( !(skb = dev_alloc_skb(pkt_len + 2)) )
		goto goon;

	skb->dev = net;
	skb_reserve(skb, 2);
	memcpy(skb_put(skb, pkt_len), bufptr + 3, pkt_len);

	skb->protocol = eth_type_trans(skb, net);
	netif_rx(skb);
	dbi->stats.rx_packets++;
	dbi->stats.rx_bytes += pkt_len;

goon:
#ifndef RX_IMPROVE
	FILL_BULK_URB( &dbi->rx_urb, dbi->usb,
			usb_rcvbulkpipe(dbi->usb, 1),
			dbi->rx_buff, DM9601_MAX_MTU, 
			read_bulk_callback, dbi );
	if ( (res = usb_submit_urb(&dbi->rx_urb)) )
		warn("%s: failed submint rx_urb %d",__FUNCTION__,res);
	dbi->flags &= ~DM9601_RX_BUSY;
#endif
}


static void write_bulk_callback( struct urb *urb )
{
	dm9601_board_info_t *dbi = urb->context;

	if ( !dbi || !(dbi->flags & DM9601_RUNNING) )
		return;

	if ( !netif_device_present(dbi->net) )
		return;
		
	if ( urb->status )
		info("%s: TX status %d", dbi->net->name, urb->status);

	dbi->net->trans_start = jiffies;
	netif_wake_queue( dbi->net );
}

#ifdef	DM9601_USE_INTR
static void intr_callback( struct urb *urb )
{
	dm9601_board_info_t *dbi = urb->context;
	struct net_device *net;
	__u8	*d;

	if ( !dbi )
		return;
		
	switch ( urb->status ) {
		case USB_ST_NOERROR:
			break;
		case USB_ST_URB_KILLED:
			return;
		default:
			info("intr status %d", urb->status);
	}

	d = urb->transfer_buffer;
	net = dbi->net;

	if ( !(d[6] & 0x04) && (d[0] & 0x10) ) { 
		printk("<WARN> TX FULL %x %x\n", d[0], d[6]); 
		dbi->flags |= DM9601_RESET_WAIT;
	}

	/* Auto Sense Media Policy:
		Fast EtherNet NIC: don't need to do.
		Force media mode: don't need to do.
		HomeRun/LongRun NIC and AUTO_Mode:
			INT_MII not link, select EXT_MII
			EXT_MII not link, select INT_MII
	*/
	if ( 	!(d[0] & 0x40) && 
		(dbi->nic_type != FASTETHER_NIC) &&
		(dbi->op_mode == DM9601_AUTO) ) {
		dbi->net_ctrl_reg ^= 0x80;
		netif_stop_queue(net);
		dbi->flags |= NET_CTRL_CHANGE;
		ctrl_callback(&dbi->ctrl_urb);
		netif_wake_queue(net);
	}

	if ( (d[1] | d[2]) & 0xf4 ) {
		dbi->stats.tx_errors++;
		if ( (d[0] | d[1]) & 0x84) 	/* EXEC & JABBER */
			dbi->stats.tx_aborted_errors++;
		if ( (d[0] | d[1]) & 0x10 )	/* LATE COL */
			dbi->stats.tx_window_errors++;
		if ( (d[0] | d[1]) & 0x60 )	/* NO or LOST CARRIER */
			dbi->stats.tx_carrier_errors++;
	}
}
#endif

#if LINUX_VERSION_CODE > KERNEL_VERSION(2,3,48)
static void dm9601_tx_timeout( struct net_device *net )
{
	dm9601_board_info_t *dbi = net->priv;

	if ( !dbi )
		return;
		
	warn("%s: Tx timed out.", net->name);
	dbi->tx_urb.transfer_flags |= USB_ASYNC_UNLINK;
	usb_unlink_urb( &dbi->tx_urb );
	dbi->stats.tx_errors++;
}
#endif


static int dm9601_start_xmit( struct sk_buff *skb, struct net_device *net )
{
	dm9601_board_info_t	*dbi = net->priv;
	int 	count = skb->len + 2;
	int 	res;
	__u16 l16 = skb->len;
	
	netif_stop_queue( net );

	if (!(count & 0x3f)) { count++; l16++; }

	((__u16 *)dbi->tx_buff)[0] = cpu_to_le16(l16);
	memcpy(dbi->tx_buff + 2, skb->data, skb->len);

	FILL_BULK_URB_TO( &dbi->tx_urb, dbi->usb,
			usb_sndbulkpipe(dbi->usb, 2),
			dbi->tx_buff, count, 
			write_bulk_callback, dbi, jiffies + HZ );

	if ((res = usb_submit_urb(&dbi->tx_urb))) {
		warn("failed tx_urb %d", res);
		dbi->stats.tx_errors++;
		netif_start_queue( net );
	} else {
		dbi->stats.tx_packets++;
		dbi->stats.tx_bytes += skb->len;
		net->trans_start = jiffies;
	}

	dev_kfree_skb(skb);

	return 0;
}


static struct net_device_stats *dm9601_netdev_stats( struct net_device *dev )
{
	return &((dm9601_board_info_t *)dev->priv)->stats;
}


static inline void disable_net_traffic( dm9601_board_info_t *dbi )
{
	__u8 reg5;

	write_mii_word(dbi, 1, 0, 0x8000);	/* RESET PHY */

	get_registers(dbi, 0x5, 1, &reg5);
	reg5 &= 0xfe;
	set_register(dbi, 0x5, reg5);		/* RX disable */
	set_register(dbi, 0x1f, 0x01);		/* PHY power down */
}

static void set_phy_mode(dm9601_board_info_t *dbi)
{
	__u16	phy_reg0 = 0x1000, phy_reg4 = 0x01e1;

	/* PHY media mode setting */
	if ( !(dbi->op_mode & DM9601_AUTO) ) {	
		switch(dbi->op_mode) {
			case DM9601_10MHF:  
				phy_reg4 = 0x0021; break;
			case DM9601_10MFD:  
				phy_reg4 = 0x0041; break;
			case DM9601_100MHF: 
				phy_reg4 = 0x0081; break;
			case DM9601_100MFD: 
				phy_reg4 = 0x0101; break;
			default: 
				phy_reg0 = 0x8000; break;
		}
		write_mii_word(dbi, 1, 4, phy_reg4); /* Set PHY capability */
		write_mii_word(dbi, 1, 0, phy_reg0);
	}

	/* Active PHY */
	set_register( dbi, 0x1e, 0x01 );	/* Let GPIO0 output */
	set_register( dbi, 0x1f, 0x00 );	/* Power_on PHY */
}

/*
	Init HomeRun DM9801
*/
static void program_dm9801(dm9601_board_info_t *dbi, u16 HPNA_rev)
{
	__u16 reg16, reg17, reg24, reg25;

	if ( !nfloor ) nfloor = DM9801_NOISE_FLOOR;

	read_mii_word(dbi, 1, 16, &reg16);
	read_mii_word(dbi, 1, 17, &reg17);
	read_mii_word(dbi, 1, 24, &reg24);
	read_mii_word(dbi, 1, 25, &reg25);

	switch(HPNA_rev) {
	case 0xb900: /* DM9801 E3 */
		reg16 |= 0x1000;
		reg25 = ( (reg24 + nfloor) & 0x00ff) | 0xf000;
		break;
	case 0xb901: /* DM9801 E4 */
		reg25 = ( (reg24 + nfloor) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor + 3;
		break;
	case 0xb902: /* DM9801 E5 */
	case 0xb903: /* DM9801 E6 */
	default:
		reg16 |= 0x1000;
		reg25 = ( (reg24 + nfloor - 3) & 0x00ff) | 0xc200;
		reg17 = (reg17 & 0xfff0) + nfloor;
		break;
	}

	write_mii_word(dbi, 1, 16, reg16);
	write_mii_word(dbi, 1, 17, reg17);
	write_mii_word(dbi, 1, 25, reg25);
}

/*
	Init LongRun DM9802
*/
static void program_dm9802(dm9601_board_info_t *dbi)
{
	__u16 reg25;

	if ( !nfloor ) nfloor = DM9802_NOISE_FLOOR;

	read_mii_word(dbi, 1, 25, &reg25);
	reg25 = (reg25 & 0xff00) + nfloor;
	write_mii_word(dbi, 1, 25, reg25);
}

/*
	Identify NIC type
*/
static void identify_nic(dm9601_board_info_t* dbi)
{
	__u16	phy_tmp;

	/* Select EXT_MII */
	dbi->net_ctrl_reg |= 0x80;
	set_register(dbi, 0x00, dbi->net_ctrl_reg);	/* EXT-MII */

	read_mii_word(dbi, 1, 3, &phy_tmp);
	switch(phy_tmp & 0xfff0) {
	case 0xb900:
		read_mii_word(dbi, 1, 31, &phy_tmp);
		if (phy_tmp == 0x4404) { 
			dbi->nic_type =  HOMERUN_NIC;
			program_dm9801(dbi, phy_tmp);
		} else {
			dbi->nic_type = LONGRUN_NIC;
			program_dm9802(dbi);
		}
		break;
	default:
		dbi->nic_type = FASTETHER_NIC;
	}

	/* Select INT_MII */
	dbi->net_ctrl_reg &= ~0x80;
	set_register(dbi, 0x00, dbi->net_ctrl_reg);
}

static void init_dm9601(struct net_device *net)
{
	dm9601_board_info_t *dbi = (dm9601_board_info_t *)net->priv;

	/* User passed argument */
	dbi->rx_ctrl_reg = reg5 | 0x01;
	dbi->net_ctrl_reg = 0x00;
	dbi->reg08 = reg8;
	dbi->reg09 = reg9;
	dbi->reg0a = rega;

	/* RESET device */
	set_register(dbi, 0x00, 0x01);	/* Reset */
	wait_ms(100);

	/* NIC type: FASTETHER, HOMERUN, LONGRUN */
	identify_nic(dbi);

	/* Set PHY */
	dbi->op_mode = dm9601_mode;
	set_phy_mode(dbi);

	/* MII selection */
	if ( 	(dbi->nic_type != FASTETHER_NIC) && 
		(dbi->op_mode == DM9601_1M_HPNA) 	)
		dbi->net_ctrl_reg |= 0x80;

	/* Program operating register */
	set_register(dbi, 0x00, dbi->net_ctrl_reg);
	set_register(dbi, 0x08, dbi->reg08);
	set_register(dbi, 0x09, dbi->reg09);
	set_register(dbi, 0x0a, dbi->reg0a);
	set_register(dbi, 0xf4, 0x26);	/* Reset EP1/EP2, INT always return */
	set_registers(dbi, 0x10, 0x06, net->dev_addr); /*  MAC addr */
	dbi->hash_table[3] = 0x8000;	/* Broadcast Address */
	set_registers(dbi, 0x16, 0x08, dbi->hash_table); /*  Hash Table */
	set_register(dbi, 0x05, dbi->rx_ctrl_reg); /* Active RX */
}


static int dm9601_open(struct net_device *net)
{
	dm9601_board_info_t *dbi = (dm9601_board_info_t *)net->priv;
	int	res;

	down(&dbi->ctrl_sem);
	MOD_INC_USE_COUNT;

	FILL_BULK_URB( &dbi->rx_urb, dbi->usb,
			usb_rcvbulkpipe(dbi->usb, 1),
			dbi->rx_buff, DM9601_MAX_MTU, 
			read_bulk_callback, dbi );
	if ( (res = usb_submit_urb(&dbi->rx_urb)) )
		warn("%s: failed rx_urb %d",__FUNCTION__,res);
	dbi->rx_buf_flag = 1;

#ifdef	DM9601_USE_INTR
	FILL_INT_URB( &dbi->intr_urb, dbi->usb,
			usb_rcvintpipe(dbi->usb, 3),
			dbi->intr_buff, sizeof(dbi->intr_buff),
			intr_callback, dbi, dbi->intr_interval );
	if ( (res = usb_submit_urb(&dbi->intr_urb)) )
		warn("%s: failed intr_urb %d",__FUNCTION__,res);
#endif

	init_dm9601(net);

	netif_start_queue( net );
	dbi->flags |= DM9601_RUNNING;
	up(&dbi->ctrl_sem);

	return 0;
}


static int dm9601_close( struct net_device *net )
{
	dm9601_board_info_t	*dbi = net->priv;

	dbi->flags &= ~DM9601_RUNNING;
	netif_stop_queue(net);
	if ( !(dbi->flags & DM9601_UNPLUG) )
		disable_net_traffic(dbi);

	usb_unlink_urb(&dbi->rx_urb);
	usb_unlink_urb(&dbi->tx_urb);
	usb_unlink_urb(&dbi->ctrl_urb);
#ifdef	DM9601_USE_INTR
	usb_unlink_urb(&dbi->intr_urb);
#endif
	MOD_DEC_USE_COUNT;

#ifdef STS_DBUG
	printk("<DM9601> rx errors: %lx \n", dbi->stats.rx_errors);
	printk("<DM9601> fifo over errors: %lx \n", dbi->stats.rx_fifo_errors);
	printk("<DM9601> crc errors: %lx \n", dbi->stats.rx_crc_errors);
	printk("<DM9601> alignment errors: %lx \n", dbi->stats.rx_frame_errors);
	printk("<DM9601> physical layer errors: %lx \n", dbi->rx_ple_errors);
	printk("<DM9601> watchdog errors: %lx \n", dbi->rx_wdt_errors);
	printk("<DM9601> late collision errors: %lx \n", dbi->rx_lc_errors);
	printk("<DM9601> runt frame errors: %lx \n", dbi->rx_runtf_errors);
	printk("<DM9601> long frame errors: %lx \n", dbi->rx_longf_errors);
#endif
	return 0;
}


static int dm9601_ioctl( struct net_device *net, struct ifreq *rq, int cmd )
{
	__u16 *data = (__u16 *)&rq->ifr_data;
	dm9601_board_info_t	*dbi = net->priv;

	switch(cmd) {
		case SIOCDEVPRIVATE:
			data[0] = dbi->phy;
		case SIOCDEVPRIVATE+1:
			read_mii_word(dbi, data[0], data[1]&0x1f, &data[3]);
			return 0;
		case SIOCDEVPRIVATE+2:
			if ( !capable(CAP_NET_ADMIN) )
				return -EPERM;
			write_mii_word(dbi, dbi->phy, data[1] & 0x1f, data[2]);
			return 0;
		default:
			return -EOPNOTSUPP;
	}
}

/*
  Calculate the CRC valude of the Rx packet
  flag = 1 : return the reverse CRC (for the received packet CRC)
         0 : return the normal CRC (for Hash Table index)
*/
static unsigned long cal_CRC(unsigned char * Data, unsigned int Len, u8 flag)
{

   u32 crc = ether_crc_le(Len, Data);
   
   if (flag) 
	   return ~crc;
   
   return crc;
}

static void dm9601_set_multicast( struct net_device *net )
{
	dm9601_board_info_t *dbi = net->priv;
	struct dev_mc_list *mcptr = net->mc_list;
	int count = net->mc_count, i, hash_val;

	netif_stop_queue(net);

	if (net->flags & IFF_PROMISC) {
		dbi->rx_ctrl_reg |= RX_PROMISCUOUS;
		info("%s: Promiscuous mode enabled", net->name);
	} else if (net->flags & IFF_ALLMULTI) {
		dbi->rx_ctrl_reg |= RX_PASS_MULTICAST;
		dbi->rx_ctrl_reg &= ~RX_PROMISCUOUS;
		info("%s set allmulti", net->name);
	} else {
		dbi->rx_ctrl_reg &= ~RX_PASS_MULTICAST;
		dbi->rx_ctrl_reg &= ~RX_PROMISCUOUS;
		/* Clear Hash Table */
		for (i = 0; i < 4; i++) dbi->hash_table[i] = 0;
		/* Set Broadcast Address */
		dbi->hash_table[3] = 0x8000;
		/* the multicast address in Hash Table : 64 bits */
		for (i = 0; i < count; i++, mcptr = mcptr->next) {
			hash_val = cal_CRC((char *)mcptr->dmi_addr, 6, 0) & 0x3f; 
			dbi->hash_table[hash_val / 16] |= (u16) 1 << (hash_val % 16);
		}
		info("%s: set Rx mode", net->name);
	}

	dbi->flags |= HASH_REGS_CHANGE | RX_CTRL_CHANGE;
	ctrl_callback(&dbi->ctrl_urb);

	netif_wake_queue(net);
}


static void * dm9601_probe( struct usb_device *dev, unsigned int ifnum,
			     const struct usb_device_id *id)
{
	struct net_device	*net;
	dm9601_board_info_t	*dbi;
	int dev_index = id - dm9601_ids;
	
	if (usb_set_configuration(dev, dev->config[0].bConfigurationValue)) {
		err("usb_set_configuration() failed");
		return NULL;
	}

	if(!(dbi = kmalloc(sizeof(dm9601_board_info_t), GFP_KERNEL))) {
		err("out of memory allocating device structure");
		return NULL;
	}

	usb_inc_dev_use( dev );
	memset(dbi, 0, sizeof(dm9601_board_info_t));
	dbi->dev_index = dev_index;
	init_waitqueue_head( &dbi->ctrl_wait );

	net = init_etherdev( NULL, 0 );
	if ( !net ) {
		kfree( dbi );
		return	NULL;
	}
	
	init_MUTEX(&dbi->ctrl_sem);
	down(&dbi->ctrl_sem);
	dbi->usb = dev;
	dbi->net = net;
	net->priv = dbi;
	net->open = dm9601_open;
	net->stop = dm9601_close;
	net->watchdog_timeo = DM9601_TX_TIMEOUT;
	net->tx_timeout = dm9601_tx_timeout;
	net->do_ioctl = dm9601_ioctl;
	net->hard_start_xmit = dm9601_start_xmit;
	net->set_multicast_list = dm9601_set_multicast;
	net->get_stats = dm9601_netdev_stats;
	net->mtu = DM9601_MTU;

	dbi->intr_interval = 0xff;	/* Default is 0x80 */

	/* Get Node Address */
	read_eprom_word(dbi, 0, (__u16 *)net->dev_addr);
	read_eprom_word(dbi, 1, (__u16 *)(net->dev_addr + 2));
	read_eprom_word(dbi, 2, (__u16 *)(net->dev_addr + 4));

	dbi->features = usb_dev_id[dev_index].private;

	info( "%s: %s", net->name, usb_dev_id[dev_index].name );
	
	up(&dbi->ctrl_sem);
	return dbi;
}


static void dm9601_disconnect( struct usb_device *dev, void *ptr )
{
	dm9601_board_info_t *dbi = ptr;

	if ( !dbi ) {
		warn("unregistering non-existant device");
		return;
	}

	dbi->flags |= DM9601_UNPLUG;
	unregister_netdev( dbi->net );
	usb_dec_dev_use( dev );
	kfree( dbi );
	dbi = NULL;
}


static struct usb_driver dm9601_driver = {
	name:		"dm9601",
	probe:		dm9601_probe,
	disconnect:	dm9601_disconnect,
	id_table:	dm9601_ids,
};

int __init dm9601_init(void) {
	info( "%s", version );

 	switch(mode) {
   		case DM9601_10MHF:
		case DM9601_100MHF:
		case DM9601_10MFD:
		case DM9601_100MFD:
		case DM9601_1M_HPNA:
			dm9601_mode = mode;
			break;
		default:
			dm9601_mode = DM9601_AUTO;
	}

	nfloor = (nfloor > 15) ? 0:nfloor;

	return usb_register( &dm9601_driver );
}

void __exit dm9601_exit(void) {
	usb_deregister( &dm9601_driver );
}

module_init( dm9601_init );
module_exit( dm9601_exit );
