/*
 * linux/drivers/video/s3c2410fb.c
 *	Copyright (c) Arnaud Patard, Ben Dooks
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file COPYING in the main directory of this archive for
 * more details.
 *
 *	    S3C2410 LCD Controller Frame Buffer Driver
 *	    based on skeletonfb.c, sa1100fb.c and others
 *
 * ChangeLog
 * 2005-04-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - u32 state -> pm_message_t state
 *      - S3C2410_{VA,SZ}_LCD -> S3C24XX
 *
 * 2005-03-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Removed the ioctl
 *      - use readl/writel instead of __raw_writel/__raw_readl
 *
 * 2004-12-04: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Added the possibility to set on or off the
 *      debugging mesaages
 *      - Replaced 0 and 1 by on or off when reading the
 *      /sys files
 *
 * 2005-03-23: Ben Dooks <ben-linux@fluff.org>
 *	- added non 16bpp modes
 *	- updated platform information for range of x/y/bpp
 *	- add code to ensure palette is written correctly
 *	- add pixel clock divisor control
 *
 * 2004-11-11: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- Removed the use of currcon as it no more exist
 * 	- Added LCD power sysfs interface
 *
 * 2004-11-03: Ben Dooks <ben-linux@fluff.org>
 *	- minor cleanups
 *	- add suspend/resume support
 *	- s3c2410fb_setcolreg() not valid in >8bpp modes
 *	- removed last CONFIG_FB_S3C2410_FIXED
 *	- ensure lcd controller stopped before cleanup
 *	- added sysfs interface for backlight power
 *	- added mask for gpio configuration
 *	- ensured IRQs disabled during GPIO configuration
 *	- disable TPAL before enabling video
 *
 * 2004-09-20: Arnaud Patard <arnaud.patard@rtp-net.org>
 *      - Suppress command line options
 *
 * 2004-09-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- code cleanup
 *
 * 2004-09-07: Arnaud Patard <arnaud.patard@rtp-net.org>
 * 	- Renamed from h1940fb.c to s3c2410fb.c
 * 	- Add support for different devices
 * 	- Backlight support
 *
 * 2004-09-05: Herbert P�tzl <herbert@13thfloor.at>
 *	- added clock (de-)allocation code
 *	- added fixem fbmem option
 *
 * 2004-07-27: Arnaud Patard <arnaud.patard@rtp-net.org>
 *	- code cleanup
 *	- added a forgotten return in h1940fb_init
 *
 * 2004-07-19: Herbert P�tzl <herbert@13thfloor.at>
 *	- code cleanup and extended debugging
 *
 * 2004-07-15: Arnaud Patard <arnaud.patard@rtp-net.org>
 *	- First version
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/version.h>

#include <asm/io.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <asm/mach/map.h>
#include <asm/arch/regs-lcd.h>
#include <asm/arch/regs-gpio.h>
#include <asm/arch/fb.h>

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

#include "reciva_fb_s3c2410fb.h"

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,0)
#define RECIVA_MODULE_PARM(x) module_param(x, int, S_IRUGO)
#else
#define RECIVA_MODULE_PARM(x) MODULE_PARM(x, "i")
#endif

// Screen width in pixels
static int screen_width;
RECIVA_MODULE_PARM(screen_width);

// Screen height in pixels
static int screen_height;
RECIVA_MODULE_PARM(screen_height);

// Bits per pixel
static int bits_per_pixel;
RECIVA_MODULE_PARM(bits_per_pixel);

// Bits per pixel
// 0 = hydra tft (cf690)
// 1 = cf672
static int display_type;
RECIVA_MODULE_PARM(display_type);

// Register masks and default values
static int gpccon;
static int gpccon_mask;
static int gpcup;
static int gpcup_mask;
static int gpcdat = 0x0000;
static int gpcdat_mask = 0xf868;

static int gpdcon;
static int gpdcon_mask;
static int gpdup;
static int gpdup_mask;
static int gpddat = 0x0000;
static int gpddat_mask = 0xf8fc;


RECIVA_MODULE_PARM(gpccon);
RECIVA_MODULE_PARM(gpccon_mask);
RECIVA_MODULE_PARM(gpcup);
RECIVA_MODULE_PARM(gpcup_mask);
RECIVA_MODULE_PARM(gpdcon);
RECIVA_MODULE_PARM(gpdcon_mask);
RECIVA_MODULE_PARM(gpdup);
RECIVA_MODULE_PARM(gpdup_mask);


// Framebuffer memory gets initialised with this data
static unsigned char init_data; 

#define PFX "FB:"

static struct s3c2410fb_mach_info *mach_info;

/* Debugging stuff */
#ifdef CONFIG_FB_S3C2410_DEBUG
static int debug	   = 1;
#else
static int debug	   = 0;
#endif

#define dprintk(msg...)	if (debug) { printk(KERN_DEBUG "s3c2410fb: " msg); }


// Moved this out of arch/arm/mach-23c2412/ so that it's easy to change
static struct s3c2410fb_mach_info reciva_tftexpander_cfg = {
  .regs = {

    .lcdcon1  = S3C2410_LCDCON1_TFT16BPP |      //16 bit per pixel
      S3C2410_LCDCON1_TFT |                     //TFT
      S3C2410_LCDCON1_CLKVAL(29),             //VCLK=2MHz ~25F/s ,CLKVAL=29, HCLK=250Mhz/2

    .lcdcon2  = S3C2410_LCDCON2_VBPD(3) |     // 4 lines
      S3C2410_LCDCON2_LINEVAL(319) |           //320, LINEVAL=319
      S3C2410_LCDCON2_VFPD(1) |                // 2 lines 
      S3C2410_LCDCON2_VSPW(0),                 //1 line

    .lcdcon3  = S3C2410_LCDCON3_HBPD(16) |      //17 VCLK
      S3C2410_LCDCON3_HOZVAL(239) |             //240 pixels on 1 line
      S3C2410_LCDCON3_HFPD(32),                //33 VCLK

    .lcdcon4  = S3C2410_LCDCON4_HSPW(4),       //5 VCLK

    .lcdcon5  = S3C2410_LCDCON5_FRM565 |          //5:6:5 format
      S3C2410_LCDCON5_INVVFRAME |                 //Invert VSYNC
      S3C2410_LCDCON5_INVVLINE |                  //Invert HSYNC
      S3C2410_LCDCON5_BSWP,                       // Byte swap
  },

  .width    = 240,
  .height   = 320,

  .xres   = { .min  = 240, .max  = 240, .defval = 240, },
  .yres   = { .min  = 320, .max  = 320, .defval = 320, },
  .bpp    = { .min  = 16, .max  = 16, .defval = 16, },

  // GPIO config - VD3-7, VD10-15, VD19-23
  .gpccon      = 0x55401668,
  .gpccon_mask = 0xffff3ffc,
  .gpcup       = 0xff7E,
  .gpcup_mask  = 0xff7E,
  .gpdcon      = 0x55405550,
  .gpdcon_mask = 0xffc0fff0,
  .gpdup       = 0xf8fc,
  .gpdup_mask  = 0xf8fc,
};

static struct s3c2410fb_mach_info reciva_vfd_cfg = {
  .regs = {

    .lcdcon1  = S3C2410_LCDCON1_STN1BPP |     // 1 bits per pixel
      S3C2410_LCDCON1_STN4 |                  // 4 bit STN
      S3C2410_LCDCON1_MMODE |                 // STNCOL - monochrome
      S3C2410_LCDCON1_CLKVAL(768),            // Clock comes out about 160khz

    .lcdcon2  = S3C2410_LCDCON2_LINEVAL(20),  // 20 vertical lines (on VFD - 20 grids)

    .lcdcon3  = S3C2410_LCDCON3_WDLY(16)|         // WDLY
      S3C2410_LCDCON3_HOZVAL(640) |               // 640/4 = 160 (bit per VFD grid)
      S3C2410_LCDCON3_LINEBLANK(0),               // LINEBLANK

    .lcdcon4  = S3C2410_LCDCON4_WLH(255),         //5 VCLK

    .lcdcon5  = S3C2410_LCDCON5_BSWP |            // Byte swap
                S3C2410_LCDCON5_INVVCLK,          // data on rising edge
  },

  .width    = 160,
  .height   = 20,

  .xres   = { .min  = 160, .max  = 160, .defval = 160, },
  .yres   = { .min  = 20, .max  = 20, .defval = 20, },
  .bpp    = { .min  = 4, .max  = 4, .defval = 4, },

  // GPC (1 to 6) GPC (8 to 15)
  .gpccon      = 0xaaaa2aaa,
  .gpccon_mask = 0xffff3ffc,
  .gpcup       = 0xff7F,
  .gpcup_mask  = 0xff7F,

  // GPD11 is shared with GPC8. Make sure this is set to input
  .gpdcon      = 0x00000000,
  .gpdcon_mask = 0x00c00000,
  .gpdup       = 0x0800,
  .gpdup_mask  = 0x0800,
};





/****************************************************************************
 * initialise lcd controller address pointers
 ****************************************************************************/
static void s3c2410fb_set_lcdaddr(struct s3c2410fb_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->fb->var;
	unsigned long saddr1, saddr2, saddr3;

	saddr1  = fbi->fb->fix.smem_start >> 1;
	saddr2  = fbi->fb->fix.smem_start;
	saddr2 += (var->xres * var->yres * var->bits_per_pixel)/8;
	saddr2>>= 1;

	saddr3 =  S3C2410_OFFSIZE(0) | S3C2410_PAGEWIDTH((var->xres * var->bits_per_pixel / 16) & 0x3ff);

	dprintk("LCDSADDR1 = 0x%08lx (0x%08lx)\n", saddr1, saddr1 << 1);
	dprintk("LCDSADDR2 = 0x%08lx (0x%08lx)\n", saddr2, saddr2 << 1);
	dprintk("LCDSADDR3 = 0x%08lx\n", saddr3);

	writel(saddr1, S3C2410_LCDSADDR1);
	writel(saddr2, S3C2410_LCDSADDR2);
	writel(saddr3, S3C2410_LCDSADDR3);
}

/****************************************************************************
 * calculate divisor for clk->pixclk
 ****************************************************************************/
static unsigned int s3c2410fb_calc_pixclk(struct s3c2410fb_info *fbi,
					  unsigned long pixclk)
{
	unsigned long clk = clk_get_rate(fbi->clk);
	unsigned long long div;

	/* pixclk is in picoseoncds, our clock is in Hz
	 *
	 * Hz -> picoseconds is / 10^-12
	 */

	div = (unsigned long long)clk * pixclk;
	do_div(div,1000000UL);
	do_div(div,1000000UL);

	dprintk("pixclk %ld, divisor is %ld\n", pixclk, (long)div);
	return div;
}

/****************************************************************************
 * Get the video params out of 'var'. If a value doesn't fit, round it up,
 * if it's too big, return -EINVAL.
 ****************************************************************************/
static int s3c2410fb_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;

	dprintk("check_var(var=%p, info=%p)\n", var, info);

	/* validate x/y resolution */

	if (var->yres > fbi->mach_info->yres.max)
		var->yres = fbi->mach_info->yres.max;
	else if (var->yres < fbi->mach_info->yres.min)
		var->yres = fbi->mach_info->yres.min;

	if (var->xres > fbi->mach_info->xres.max)
		var->yres = fbi->mach_info->xres.max;
	else if (var->xres < fbi->mach_info->xres.min)
		var->xres = fbi->mach_info->xres.min;

	/* validate bpp */

	if (var->bits_per_pixel > fbi->mach_info->bpp.max)
		var->bits_per_pixel = fbi->mach_info->bpp.max;
	else if (var->bits_per_pixel < fbi->mach_info->bpp.min)
		var->bits_per_pixel = fbi->mach_info->bpp.min;

	/* set r/g/b positions */
	switch (var->bits_per_pixel) {
		case 1:
		case 2:
		case 4:
			var->red.offset    	= 0;
			var->red.length    	= var->bits_per_pixel;
			var->green         	= var->red;
			var->blue          	= var->red;
			var->transp.offset 	= 0;
			var->transp.length 	= 0;
			break;
		case 8:
			if ( fbi->mach_info->type != S3C2410_LCDCON1_TFT ) {
				/* 8 bpp 332 */
				var->red.length		= 3;
				var->red.offset		= 5;
				var->green.length	= 3;
				var->green.offset	= 2;
				var->blue.length	= 2;
				var->blue.offset	= 0;
				var->transp.length	= 0;
			} else {
				var->red.offset    	= 0;
				var->red.length    	= var->bits_per_pixel;
				var->green         	= var->red;
				var->blue          	= var->red;
				var->transp.offset 	= 0;
				var->transp.length 	= 0;
			}
			break;
		case 12:
			/* 12 bpp 444 */
			var->red.length		= 4;
			var->red.offset		= 8;
			var->green.length	= 4;
			var->green.offset	= 4;
			var->blue.length	= 4;
			var->blue.offset	= 0;
			var->transp.length	= 0;
			break;

		default:
		case 16:
			if (fbi->regs.lcdcon5 & S3C2410_LCDCON5_FRM565 ) {
				/* 16 bpp, 565 format */
				var->red.offset		= 11;
				var->green.offset	= 5;
				var->blue.offset	= 0;
				var->red.length		= 5;
				var->green.length	= 6;
				var->blue.length	= 5;
				var->transp.length	= 0;
			} else {
				/* 16 bpp, 5551 format */
				var->red.offset		= 11;
				var->green.offset	= 6;
				var->blue.offset	= 1;
				var->red.length		= 5;
				var->green.length	= 5;
				var->blue.length	= 5;
				var->transp.length	= 0;
			}
			break;
		case 24:
			/* 24 bpp 888 */
			var->red.length		= 8;
			var->red.offset		= 16;
			var->green.length	= 8;
			var->green.offset	= 8;
			var->blue.length	= 8;
			var->blue.offset	= 0;
			var->transp.length	= 0;
			break;


	}
	return 0;
}

/****************************************************************************
 * activate (set) the controller from the given framebuffer
 * information
 ****************************************************************************/
static void s3c2410fb_activate_var(struct s3c2410fb_info *fbi,
				   struct fb_var_screeninfo *var)
{
	int hs;

	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_MODEMASK;
	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_TFT;

	dprintk("%s: var->xres  = %d\n", __FUNCTION__, var->xres);
	dprintk("%s: var->yres  = %d\n", __FUNCTION__, var->yres);
	dprintk("%s: var->bpp   = %d\n", __FUNCTION__, var->bits_per_pixel);

	fbi->regs.lcdcon1 |= fbi->mach_info->type;

	if (fbi->mach_info->type == S3C2410_LCDCON1_TFT)
		switch (var->bits_per_pixel) {
		case 1:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT1BPP;
			break;
		case 2:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT2BPP;
			break;
		case 4:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT4BPP;
			break;
		case 8:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT8BPP;
			break;
		case 16:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_TFT16BPP;
			break;

		default:
			/* invalid pixel depth */
			dev_err(fbi->dev, "invalid bpp %d\n", var->bits_per_pixel);
		}
	else
		switch (var->bits_per_pixel) {
		case 1:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN1BPP;
			break;
		case 2:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN2GREY;
			break;
		case 4:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN4GREY;
			break;
		case 8:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN8BPP;
			break;
		case 12:
			fbi->regs.lcdcon1 |= S3C2410_LCDCON1_STN12BPP;
			break;

		default:
			/* invalid pixel depth */
			dev_err(fbi->dev, "invalid bpp %d\n", var->bits_per_pixel);
		}

	/* check to see if we need to update sync/borders */

	if (!fbi->mach_info->fixed_syncs) {
		dprintk("setting vert: up=%d, low=%d, sync=%d\n",
			var->upper_margin, var->lower_margin,
			var->vsync_len);

		dprintk("setting horz: lft=%d, rt=%d, sync=%d\n",
			var->left_margin, var->right_margin,
			var->hsync_len);

		fbi->regs.lcdcon2 =
			S3C2410_LCDCON2_VBPD(var->upper_margin - 1) |
			S3C2410_LCDCON2_VFPD(var->lower_margin - 1) |
			S3C2410_LCDCON2_VSPW(var->vsync_len - 1);

		fbi->regs.lcdcon3 =
			S3C2410_LCDCON3_HBPD(var->right_margin - 1) |
			S3C2410_LCDCON3_HFPD(var->left_margin - 1);

		fbi->regs.lcdcon4 &= ~S3C2410_LCDCON4_HSPW(0xff);
		fbi->regs.lcdcon4 |=  S3C2410_LCDCON4_HSPW(var->hsync_len - 1);
	}

	/* update X/Y info */

	fbi->regs.lcdcon2 &= ~S3C2410_LCDCON2_LINEVAL(0x3ff);
	fbi->regs.lcdcon2 |=  S3C2410_LCDCON2_LINEVAL(var->yres - 1);

	switch(fbi->mach_info->type) {
		case S3C2410_LCDCON1_DSCAN4:
		case S3C2410_LCDCON1_STN8:
			hs = var->xres / 8;
			break;
		case S3C2410_LCDCON1_STN4:
			hs = var->xres / 4;
			break;
		default:
		case S3C2410_LCDCON1_TFT:
			hs = var->xres;
			break;

	}

	/* Special cases : STN color displays */
	if ( ((fbi->regs.lcdcon1 & S3C2410_LCDCON1_MODEMASK) == S3C2410_LCDCON1_STN8BPP) \
	  || ((fbi->regs.lcdcon1 & S3C2410_LCDCON1_MODEMASK) == S3C2410_LCDCON1_STN12BPP) ) {
		hs = hs * 3;
	}


	fbi->regs.lcdcon3 &= ~S3C2410_LCDCON3_HOZVAL(0x7ff);
	fbi->regs.lcdcon3 |=  S3C2410_LCDCON3_HOZVAL(hs - 1);

	if (var->pixclock > 0) {
		int clkdiv = s3c2410fb_calc_pixclk(fbi, var->pixclock);

		if (fbi->mach_info->type == S3C2410_LCDCON1_TFT) {
			clkdiv = (clkdiv / 2) -1;
			if (clkdiv < 0)
				clkdiv = 0;
		}
		else {
			clkdiv = (clkdiv / 2);
			if (clkdiv < 2)
				clkdiv = 2;
		}

		fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_CLKVAL(0x3ff);
		fbi->regs.lcdcon1 |=  S3C2410_LCDCON1_CLKVAL(clkdiv);
	}

	/* write new registers */

	dprintk("new register set:\n");
	dprintk("lcdcon[1] = 0x%08lx\n", fbi->regs.lcdcon1);
	dprintk("lcdcon[2] = 0x%08lx\n", fbi->regs.lcdcon2);
	dprintk("lcdcon[3] = 0x%08lx\n", fbi->regs.lcdcon3);
	dprintk("lcdcon[4] = 0x%08lx\n", fbi->regs.lcdcon4);
	dprintk("lcdcon[5] = 0x%08lx\n", fbi->regs.lcdcon5);

	writel(fbi->regs.lcdcon1 & ~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);
	writel(fbi->regs.lcdcon2, S3C2410_LCDCON2);
	writel(fbi->regs.lcdcon3, S3C2410_LCDCON3);
	writel(fbi->regs.lcdcon4, S3C2410_LCDCON4);
	writel(fbi->regs.lcdcon5, S3C2410_LCDCON5);

	/* set lcd address pointers */
	s3c2410fb_set_lcdaddr(fbi);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
}

/****************************************************************************
 * Optional function. Alters the hardware state.
 * info - frame buffer structure that represents a single frame buffer
 ****************************************************************************/
static int s3c2410fb_set_par(struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;
	struct fb_var_screeninfo *var = &info->var;

	switch (var->bits_per_pixel)
	{
		case 16:
			fbi->fb->fix.visual = FB_VISUAL_TRUECOLOR;
			break;
		case 1:
			 fbi->fb->fix.visual = FB_VISUAL_MONO01;
			 break;
		default:
			 fbi->fb->fix.visual = FB_VISUAL_PSEUDOCOLOR;
			 break;
	}

	fbi->fb->fix.line_length     = (var->width*var->bits_per_pixel)/8;

	/* activate this new configuration */

	s3c2410fb_activate_var(fbi, var);
	return 0;
}

/****************************************************************************
 * Schedule a palette update.
 * Palette registers will be updated on next frame sync interrupt
 ****************************************************************************/
static void schedule_palette_update(struct s3c2410fb_info *fbi,
				    unsigned int regno, unsigned int val)
{
	unsigned long flags;
	unsigned long irqen;

	local_irq_save(flags);

	fbi->palette_buffer[regno] = val;

	if (!fbi->palette_ready) {
		fbi->palette_ready = 1;

		/* enable IRQ */
		irqen = readl(S3C2412_LCDINTMSK);
		irqen &= ~S3C2410_LCDINT_FRSYNC;
		writel(irqen, S3C2412_LCDINTMSK);
	}

	local_irq_restore(flags);
}

/****************************************************************************
 * Palette utility function
 ****************************************************************************/
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}

/****************************************************************************
 * Request a change to the colour palette
 ****************************************************************************/
static int s3c2410fb_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct s3c2410fb_info *fbi = info->par;
	unsigned int val;

	/* dprintk("setcol: regno=%d, rgb=%d,%d,%d\n", regno, red, green, blue); */

	switch (fbi->fb->fix.visual) {
	case FB_VISUAL_TRUECOLOR:
		/* true-colour, use pseuo-palette */

		if (regno < 16) {
			u32 *pal = fbi->fb->pseudo_palette;

			val  = chan_to_field(red,   &fbi->fb->var.red);
			val |= chan_to_field(green, &fbi->fb->var.green);
			val |= chan_to_field(blue,  &fbi->fb->var.blue);

			pal[regno] = val;
		}
		break;

	case FB_VISUAL_PSEUDOCOLOR:
		if (regno < 256) {
			/* currently assume RGB 5-6-5 mode */

			val  = ((red   >>  0) & 0xf800);
			val |= ((green >>  5) & 0x07e0);
			val |= ((blue  >> 11) & 0x001f);

			writel(val, S3C2410_TFTPAL(regno));
			schedule_palette_update(fbi, regno, val);
		}

		break;

	default:
		return 1;   /* unknown type */
	}

	return 0;
}

/****************************************************************************
 * @blank_mode: the blank mode we want.
 * @info: frame buffer structure that represents a single frame buffer
 *
 * Blank the screen if blank_mode != 0, else unblank. Return 0 if
 * blanking succeeded, != 0 if un-/blanking failed due to e.g. a
 * video mode which doesn't support it. Implements VESA suspend
 * and powerdown modes on hardware that supports disabling hsync/vsync:
 * blank_mode == 2: suspend vsync
 * blank_mode == 3: suspend hsync
 * blank_mode == 4: powerdown
 *
 * Returns negative errno on error, or zero on success.
 ****************************************************************************/
static int s3c2410fb_blank(int blank_mode, struct fb_info *info)
{
	dprintk("blank(mode=%d, info=%p)\n", blank_mode, info);

	if (mach_info == NULL)
		return -EINVAL;

	if (blank_mode == FB_BLANK_UNBLANK)
		writel(0x0, S3C2412_TPAL);
	else {
		dprintk("setting TPAL to output 0x000000\n");
		writel(S3C2410_TPAL_EN, S3C2412_TPAL);
	}

	return 0;
}

static int s3c2410fb_debug_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", debug ? "on" : "off");
}
static int s3c2410fb_debug_store(struct device *dev, struct device_attribute *attr,
					   const char *buf, size_t len)
{
	if (mach_info == NULL)
		return -EINVAL;

	if (len < 1)
		return -EINVAL;

	if (strnicmp(buf, "on", 2) == 0 ||
	    strnicmp(buf, "1", 1) == 0) {
		debug = 1;
		printk(KERN_DEBUG "s3c2410fb: Debug On");
	} else if (strnicmp(buf, "off", 3) == 0 ||
		   strnicmp(buf, "0", 1) == 0) {
		debug = 0;
		printk(KERN_DEBUG "s3c2410fb: Debug Off");
	} else {
		return -EINVAL;
	}

	return len;
}


static DEVICE_ATTR(debug, 0666,
		   s3c2410fb_debug_show,
		   s3c2410fb_debug_store);

static struct fb_ops s3c2410fb_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= s3c2410fb_check_var,
	.fb_set_par	= s3c2410fb_set_par,
	.fb_blank	= s3c2410fb_blank,
	.fb_setcolreg	= s3c2410fb_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};


/****************************************************************************
 * Allocates the DRAM memory for the frame buffer.  This buffer is
 * remapped into a non-cached, non-buffered, memory region to
 * allow palette and pixel writes to occur without flushing the
 * cache.  Once this area is remapped, all virtual memory
 * access to the video memory should occur at the new region.
 ****************************************************************************/
static int __init s3c2410fb_map_video_memory(struct s3c2410fb_info *fbi)
{
	int i;
	int j = 80;

	dprintk("map_video_memory(fbi=%p)\n", fbi);
	dprintk("  mach_info->xres.max=%d\n", mach_info->xres.max);
	dprintk("  mach_info->yres.max=%d\n", mach_info->yres.max);
	dprintk("  mach_info->bpp.max=%d\n", mach_info->bpp.max);
	dprintk("  smem_len=%d\n", fbi->fb->fix.smem_len);

	fbi->map_size = PAGE_ALIGN(fbi->fb->fix.smem_len + PAGE_SIZE);
	fbi->map_cpu  = dma_alloc_writecombine(fbi->dev, fbi->map_size,
					       &fbi->map_dma, GFP_KERNEL);

	fbi->map_size = fbi->fb->fix.smem_len;

	if (fbi->map_cpu) {
		/* prevent initial garbage on screen */
		dprintk("  clear %p:%08x (data=%02x)\n", fbi->map_cpu, fbi->map_size, init_data);
		memset(fbi->map_cpu, init_data, fbi->map_size);

		// XXX testing
		// Following test pattern comes out as 00011111 00000011
		if (display_type == 1)
		{
			// test pattern
			for (i=0; i<20; i++)
			{
				fbi->map_cpu[i*j + 0] = 0x00;
				fbi->map_cpu[i*j + 1] = 0x01;
				fbi->map_cpu[i*j + 2] = 0x11;
				fbi->map_cpu[i*j + 3] = 0x11;

				fbi->map_cpu[i*j + 4] = 0x00;
				fbi->map_cpu[i*j + 5] = 0x00;
				fbi->map_cpu[i*j + 6] = 0x00;
				fbi->map_cpu[i*j + 7] = 0x11;
			}
		}

		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;

		dprintk("  dma=%08x cpu=%p size=%08x\n",
			fbi->map_dma, fbi->map_cpu, fbi->fb->fix.smem_len);
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}

/****************************************************************************
 * Unmap DMA video buffer
 ****************************************************************************/
static inline void s3c2410fb_unmap_video_memory(struct s3c2410fb_info *fbi)
{
	dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}

/****************************************************************************
 * Modify GPIO pins
 ****************************************************************************/
static inline void modify_gpio(void __iomem *reg,
			       unsigned long set, unsigned long mask)
{
	unsigned long tmp;

	tmp = readl(reg) & ~mask;
	writel(tmp | set, reg);
}

/****************************************************************************
 * Initialise all LCD-related registers
 ****************************************************************************/
static int s3c2410fb_init_registers(struct s3c2410fb_info *fbi)
{
	unsigned long flags;

	/* Initialise LCD with values from haret */

	local_irq_save(flags);

	/* modify the gpio(s) with interrupts set (bjd) */

	modify_gpio(S3C2410_GPCUP,  mach_info->gpcup,  mach_info->gpcup_mask);
	modify_gpio(S3C2410_GPCCON, mach_info->gpccon, mach_info->gpccon_mask);
        modify_gpio(S3C2410_GPCDAT, gpcdat, gpcdat_mask);
	modify_gpio(S3C2410_GPDUP,  mach_info->gpdup,  mach_info->gpdup_mask);
	modify_gpio(S3C2410_GPDCON, mach_info->gpdcon, mach_info->gpdcon_mask);
        modify_gpio(S3C2410_GPDDAT, gpddat, gpddat_mask);
	dprintk("GPCUP=0x%08x\n", readl(S3C2410_GPCUP))
	dprintk("GPCCON=0x%08x\n", readl(S3C2410_GPCCON))
	dprintk("GPCDAT=0x%08x\n", readl(S3C2410_GPCDAT))
	dprintk("GPDUP=0x%08x\n", readl(S3C2410_GPDUP))
	dprintk("GPDCON=0x%08x\n", readl(S3C2410_GPDCON))
	dprintk("GPDDAT=0x%08x\n", readl(S3C2410_GPDDAT))
        
	local_irq_restore(flags);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
	writel(fbi->regs.lcdcon2, S3C2410_LCDCON2);
	writel(fbi->regs.lcdcon3, S3C2410_LCDCON3);
	writel(fbi->regs.lcdcon4, S3C2410_LCDCON4);
	writel(fbi->regs.lcdcon5, S3C2410_LCDCON5);

 	s3c2410fb_set_lcdaddr(fbi);

	dprintk("LPCSEL    = 0x%08lx\n", mach_info->lpcsel);
	//writel(mach_info->lpcsel, S3C2410_LPCSEL);
	writel(mach_info->lpcsel, S3C2412_TCONSEL);

	dprintk("replacing TPAL %08x\n", readl(S3C2412_TPAL));

	/* ensure temporary palette disabled */
	writel(0x00, S3C2412_TPAL);

	/* Enable video by setting the ENVID bit to 1 */
	fbi->regs.lcdcon1 |= S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);

	// Need at least 2 frames delay
	// eg for a 240x320 display
	// (240+33) x (320+7) x (1/2000000)
	// 44.6 ms / frame
	// == 90ms
	if (display_type == 0)
	{
		mdelay(500);
		modify_gpio(S3C2410_GPCCON, 0xaa801668, 0xffffffff);
		modify_gpio(S3C2410_GPDCON, 0xaa95aaa5, mach_info->gpdcon_mask);
		modify_gpio(S3C2410_GPCDAT, 0x48, 0xffffffff);
	}
	return 0;
}

/****************************************************************************
 * Write colour palette
 ****************************************************************************/
static void s3c2410fb_write_palette(struct s3c2410fb_info *fbi)
{
	unsigned int i;
	unsigned long ent;

	fbi->palette_ready = 0;

	for (i = 0; i < 256; i++) {
		if ((ent = fbi->palette_buffer[i]) == PALETTE_BUFF_CLEAR)
			continue;

		writel(ent, S3C2410_TFTPAL(i));

		/* it seems the only way to know exactly
		 * if the palette wrote ok, is to check
		 * to see if the value verifies ok
		 */

		if (readw(S3C2410_TFTPAL(i)) == ent)
			fbi->palette_buffer[i] = PALETTE_BUFF_CLEAR;
		else
			fbi->palette_ready = 1;   /* retry */
	}
}

/****************************************************************************
 * Framesync IRQ. 
 * Update the palette
 ****************************************************************************/
static irqreturn_t s3c2410fb_irq(int irq, void *dev_id)
{
	struct s3c2410fb_info *fbi = dev_id;
	unsigned long lcdirq = readl(S3C2412_LCDINTPND);

	if (lcdirq & S3C2410_LCDINT_FRSYNC) {
		if (fbi->palette_ready)
			s3c2410fb_write_palette(fbi);

		writel(S3C2410_LCDINT_FRSYNC, S3C2412_LCDINTPND);
		writel(S3C2410_LCDINT_FRSYNC, S3C2412_LCDSRCPND);
	}

	return IRQ_HANDLED;
}



/****************************************************************************
 * platform_driver START
 ****************************************************************************/

static char driver_name[]="s3c2410fb";

/****************************************************************************
 * platform_driver
 * knl core will call this
 ****************************************************************************/
static int __init s3c2410fb_probe(struct platform_device *pdev)
{
	struct s3c2410fb_info *info;
	struct fb_info	   *fbinfo;
	struct s3c2410fb_hw *mregs;
	int ret;
	int irq;
	int i;
	u32 lcdcon1;

	if (mach_info == NULL) {
		dev_err(&pdev->dev,"no platform data for lcd, cannot attach\n");
		return -EINVAL;
	}

	mregs = &mach_info->regs;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "no irq for device\n");
		return -ENOENT;
	}

	fbinfo = framebuffer_alloc(sizeof(struct s3c2410fb_info), &pdev->dev);
	if (!fbinfo) {
		return -ENOMEM;
	}


	info = fbinfo->par;
	info->fb = fbinfo;
	info->dev = &pdev->dev;

	platform_set_drvdata(pdev, fbinfo);

	dprintk("devinit\n");

	strcpy(fbinfo->fix.id, driver_name);

	memcpy(&info->regs, &mach_info->regs, sizeof(info->regs));

	/* Stop the video and unset ENVID if set */
	info->regs.lcdcon1 &= ~S3C2410_LCDCON1_ENVID;
	lcdcon1 = readl(S3C2410_LCDCON1);
	writel(lcdcon1 & ~S3C2410_LCDCON1_ENVID, S3C2410_LCDCON1);

	info->mach_info		    = pdev->dev.platform_data;

	fbinfo->fix.type	    = FB_TYPE_PACKED_PIXELS;
	fbinfo->fix.type_aux	    = 0;
	fbinfo->fix.xpanstep	    = 0;
	fbinfo->fix.ypanstep	    = 0;
	fbinfo->fix.ywrapstep	    = 0;
	fbinfo->fix.accel	    = FB_ACCEL_NONE;

	fbinfo->var.nonstd	    = 0;
	fbinfo->var.activate	    = FB_ACTIVATE_NOW;
	fbinfo->var.height	    = mach_info->height;
	fbinfo->var.width	    = mach_info->width;
	fbinfo->var.accel_flags     = 0;
	fbinfo->var.vmode	    = FB_VMODE_NONINTERLACED;

	fbinfo->fbops		    = &s3c2410fb_ops;
	fbinfo->flags		    = FBINFO_FLAG_DEFAULT;
	fbinfo->pseudo_palette      = &info->pseudo_pal;

	fbinfo->var.xres	    = mach_info->xres.defval;
	fbinfo->var.xres_virtual    = mach_info->xres.defval;
	fbinfo->var.yres	    = mach_info->yres.defval;
	fbinfo->var.yres_virtual    = mach_info->yres.defval;
	fbinfo->var.bits_per_pixel  = mach_info->bpp.defval;

	fbinfo->var.upper_margin    = S3C2410_LCDCON2_GET_VBPD(mregs->lcdcon2) + 1;
	fbinfo->var.lower_margin    = S3C2410_LCDCON2_GET_VFPD(mregs->lcdcon2) + 1;
	fbinfo->var.vsync_len	    = S3C2410_LCDCON2_GET_VSPW(mregs->lcdcon2) + 1;

	fbinfo->var.left_margin	    = S3C2410_LCDCON3_GET_HFPD(mregs->lcdcon3) + 1;
	fbinfo->var.right_margin    = S3C2410_LCDCON3_GET_HBPD(mregs->lcdcon3) + 1;
	fbinfo->var.hsync_len	    = S3C2410_LCDCON4_GET_HSPW(mregs->lcdcon4) + 1;

	fbinfo->var.red.offset      = 11;
	fbinfo->var.green.offset    = 5;
	fbinfo->var.blue.offset     = 0;
	fbinfo->var.transp.offset   = 0;
	fbinfo->var.red.length      = 5;
	fbinfo->var.green.length    = 6;
	fbinfo->var.blue.length     = 5;
	fbinfo->var.transp.length   = 0;
	fbinfo->fix.smem_len        =	mach_info->xres.max *
					mach_info->yres.max *
					mach_info->bpp.max / 8;

	for (i = 0; i < 256; i++)
		info->palette_buffer[i] = PALETTE_BUFF_CLEAR;

	if (!request_mem_region((unsigned long)S3C24XX_VA_LCD, SZ_1M, "s3c2412-lcd")) {
		ret = -EBUSY;
		goto dealloc_fb;
	}


	dprintk("got LCD region\n");

	ret = request_irq(irq, s3c2410fb_irq, IRQF_DISABLED, pdev->name, info);
	if (ret) {
		dev_err(&pdev->dev, "cannot get irq %d - err %d\n", irq, ret);
		ret = -EBUSY;
		goto release_mem;
	}

	info->clk = clk_get(NULL, "lcd");
	if (!info->clk || IS_ERR(info->clk)) {
		printk(KERN_ERR "failed to get lcd clock source\n");
		ret = -ENOENT;
		goto release_irq;
	}

	clk_enable(info->clk);
	dprintk("got and enabled clock\n");

	msleep(1);

	/* Initialize video memory */
	ret = s3c2410fb_map_video_memory(info);
	if (ret) {
		printk( KERN_ERR "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto release_clock;
	}
	dprintk("got video memory\n");

	ret = s3c2410fb_init_registers(info);

	ret = s3c2410fb_check_var(&fbinfo->var, fbinfo);

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

	/* create device files */
	device_create_file(&pdev->dev, &dev_attr_debug);

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
		fbinfo->node, fbinfo->fix.id);

	return 0;

free_video_memory:
	s3c2410fb_unmap_video_memory(info);
release_clock:
	clk_disable(info->clk);
	clk_put(info->clk);
release_irq:
	free_irq(irq,info);
release_mem:
 	release_mem_region((unsigned long)S3C24XX_VA_LCD, S3C24XX_SZ_LCD);
dealloc_fb:
	framebuffer_release(fbinfo);
	return ret;
}

/****************************************************************************
 * platform_driver helper
 * Shutdown the lcd controller
 ****************************************************************************/
static void s3c2410fb_stop_lcd(struct s3c2410fb_info *fbi)
{
	unsigned long flags;

	local_irq_save(flags);

	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);

	local_irq_restore(flags);
}

/****************************************************************************
 * platform_driver
 * Remove
 ****************************************************************************/
static int s3c2410fb_remove(struct platform_device *pdev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(pdev);
	struct s3c2410fb_info *info = fbinfo->par;
	int irq;

	s3c2410fb_stop_lcd(info);
	msleep(1);

	s3c2410fb_unmap_video_memory(info);

 	if (info->clk) {
 		clk_disable(info->clk);
 		clk_put(info->clk);
 		info->clk = NULL;
	}

	irq = platform_get_irq(pdev, 0);
	free_irq(irq,info);
	release_mem_region((unsigned long)S3C24XX_VA_LCD, S3C24XX_SZ_LCD);
	unregister_framebuffer(fbinfo);

	return 0;
}

#ifdef CONFIG_PM

/* suspend and resume support for the lcd controller */

/****************************************************************************
 * platform_driver
 * Suspend support
 ****************************************************************************/
static int s3c2410fb_suspend(struct platform_device *dev, pm_message_t state)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s3c2410fb_info *info = fbinfo->par;

	s3c2410fb_stop_lcd(info);

	/* sleep before disabling the clock, we need to ensure
	 * the LCD DMA engine is not going to get back on the bus
	 * before the clock goes off again (bjd) */

	msleep(1);
	clk_disable(info->clk);

	return 0;
}

/****************************************************************************
 * platform_driver
 * Resume support
 ****************************************************************************/
static int s3c2410fb_resume(struct platform_device *dev)
{
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct s3c2410fb_info *info = fbinfo->par;

	clk_enable(info->clk);
	msleep(1);

	s3c2410fb_init_registers(info);

	return 0;
}

#else
#define s3c2410fb_suspend NULL
#define s3c2410fb_resume  NULL
#endif

static struct platform_driver s3c2410fb_driver = {
	.probe		= s3c2410fb_probe,
	.remove		= s3c2410fb_remove,
	.suspend	= s3c2410fb_suspend,
	.resume		= s3c2410fb_resume,
	.driver		= {
		.name	= "s3c2412-lcd",
		.owner	= THIS_MODULE,
	},
};

/****************************************************************************
 * platform_driver END
 ****************************************************************************/





/****************************************************************************
 * Module initialisation
 ****************************************************************************/
int __devinit s3c2410fb_init(void)
{
	printk(PFX "init\n");
	printk(PFX "  display_type = %d\n", display_type);

	if (display_type == 1)
	{
		writel(0x3f80, S3C2412_GREENLUT(0)); // need this for all 1 bit displays
		writel( ((0x0d << 26) | 0x02) ,S3C2412_LCDCON8); // disable dithering
		init_data = 0x00;
		mach_info = &reciva_vfd_cfg;
	}
	else
	{
		init_data = 0xff; // White
		mach_info = &reciva_tftexpander_cfg;
	}
	printk(PFX "  init_data = 0x%02x\n", init_data);

	if (screen_width)
	{
		mach_info->width = screen_width;
		mach_info->xres.min = screen_width;
		mach_info->xres.max = screen_width;
		mach_info->xres.defval = screen_width;
	}
	if (screen_height)
	{
		mach_info->height = screen_height;
		mach_info->yres.min = screen_height;
		mach_info->yres.max = screen_height;
		mach_info->yres.defval = screen_height;
	}
	if (bits_per_pixel)
	{
		mach_info->bpp.min = bits_per_pixel;
		mach_info->bpp.max = bits_per_pixel;
		mach_info->bpp.defval = bits_per_pixel;
	}
        if (gpccon)
		mach_info->gpccon = gpccon;
        if (gpccon_mask)
		mach_info->gpccon_mask = gpccon_mask;
        if (gpcup)
		mach_info->gpcup = gpcup;
        if (gpcup_mask)
		mach_info->gpcup_mask = gpcup_mask;
        if (gpdcon)
		mach_info->gpdcon = gpdcon;
        if (gpdcon_mask)
		mach_info->gpdcon_mask = gpdcon_mask;
        if (gpdup)
		mach_info->gpdup = gpdup;
        if (gpdup_mask)
		mach_info->gpdup_mask = gpdup_mask;


	printk(PFX "  screen_width = %d\n", mach_info->width);
	printk(PFX "  screen_height = %d\n", mach_info->height);
	printk(PFX "  bits_per_pixel = %d\n", mach_info->bpp.defval);
	printk(PFX "  gpccon = 0x%08x\n", (int)mach_info->gpccon);
	printk(PFX "  gpccon_mask = 0x%08x\n", (int)mach_info->gpccon_mask);
	printk(PFX "  gpcup = 0x%08x\n", (int)mach_info->gpcup);
	printk(PFX "  gpcup_mask = 0x%08x\n", (int)mach_info->gpcup_mask);
        printk(PFX "  gpcdat = 0x%08x\n", (int)gpcdat);
        printk(PFX "  gpcdat_mask = 0x%08x\n", (int)gpcdat_mask);
        
	printk(PFX "  gpdcon = 0x%08x\n", (int)mach_info->gpdcon);
	printk(PFX "  gpdcon_mask = 0x%08x\n", (int)mach_info->gpdcon_mask);
	printk(PFX "  gpdup = 0x%08x\n", (int)mach_info->gpdup);
	printk(PFX "  gpdup_mask = 0x%08x\n", (int)mach_info->gpdup_mask);
        printk(PFX "  gpddat = 0x%08x\n", gpddat);
        printk(PFX "  gpddat_mask = 0x%08x\n", gpddat_mask);

        printk(PFX "  lcdcon1 = 0x%08lx\n", mach_info->regs.lcdcon1);
        printk(PFX "  lcdcon2 = 0x%08lx\n", mach_info->regs.lcdcon2);
        printk(PFX "  lcdcon3 = 0x%08lx\n", mach_info->regs.lcdcon3);
        printk(PFX "  lcdcon4 = 0x%08lx\n", mach_info->regs.lcdcon4);
        printk(PFX "  lcdcon5 = 0x%08lx\n", mach_info->regs.lcdcon5);
        
	return platform_driver_register(&s3c2410fb_driver);
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit s3c2410fb_cleanup(void)
{
	platform_driver_unregister(&s3c2410fb_driver);
}


module_init(s3c2410fb_init);
module_exit(s3c2410fb_cleanup);

MODULE_AUTHOR("Arnaud Patard <arnaud.patard@rtp-net.org>, Ben Dooks <ben-linux@fluff.org>");
MODULE_DESCRIPTION("Framebuffer driver for the s3c2410");
MODULE_LICENSE("GPL");
