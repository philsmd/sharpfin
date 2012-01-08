/*
 * reciva_knl_drivers/reciva_vfd_hydra.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * S3C2412 LCD Controller Frame Buffer VFD Driver
 * based on linux/drivers/video/s3c2410fb.c
 *
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

#include "reciva_vfd_hydra.h"

#define PFX "VFDHYDRA:"

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

RECIVA_MODULE_PARM(gpcdat);
RECIVA_MODULE_PARM(gpcdat_mask);
RECIVA_MODULE_PARM(gpddat);
RECIVA_MODULE_PARM(gpddat_mask);



struct reciva_vfd_info {
	struct fb_info		*fb;
	struct device		*dev;
	struct clk		*clk;

	struct s3c2410fb_mach_info *mach_info;

	/* raw memory addresses */
	dma_addr_t		map_dma;	/* physical */
	u_char *		map_cpu;	/* virtual */
	u_int			map_size;

	struct s3c2410fb_hw	regs;

	/* addresses of pieces placed in raw buffer */
	u_char *		screen_cpu;	/* virtual address of buffer */
	dma_addr_t		screen_dma;	/* physical address of buffer */
	unsigned int		palette_ready;

	/* keep these registers in case we need to re-write palette */
	u32			palette_buffer[256];
	u32			pseudo_pal[16];
};

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


#define PALETTE_BUFF_CLEAR (0x80000000)	/* entry is clear/invalid */


// XXX
static struct s3c2410fb_mach_info *mach_info;

/* Debugging stuff */
#ifdef CONFIG_FB_S3C2410_DEBUG
static int debug	   = 1;
#else
static int debug	   = 0;
#endif

#define dprintk(msg...)	if (debug) { printk(KERN_DEBUG "s3c2410fb: " msg); }


/****************************************************************************
 * initialise lcd controller address pointers
 ****************************************************************************/
#if 0
static void reciva_vfd_set_lcdaddr(struct reciva_vfd_info *fbi)
{
	struct fb_var_screeninfo *var = &fbi->fb->var;
	unsigned long saddr1, saddr2, saddr3;

	saddr1  = fbi->fb->fix.smem_start >> 1;
	saddr2  = fbi->fb->fix.smem_start;
	saddr2 += (var->xres * var->yres * var->bits_per_pixel)/8;
	saddr2>>= 1;

	saddr3 =  S3C2410_OFFSIZE(0) | S3C2410_PAGEWIDTH((var->xres * var->bits_per_pixel / 16) & 0x3ff);

	dprintk("LCDSADDR1 = 0x%08lx\n", saddr1);
	dprintk("LCDSADDR2 = 0x%08lx\n", saddr2);
	dprintk("LCDSADDR3 = 0x%08lx\n", saddr3);

	writel(saddr1, S3C2410_LCDSADDR1);
	writel(saddr2, S3C2410_LCDSADDR2);
	writel(saddr3, S3C2410_LCDSADDR3);
}
#endif

/****************************************************************************
 * calculate divisor for clk->pixclk
 ****************************************************************************/
#if 0
static unsigned int reciva_vfd_calc_pixclk(struct reciva_vfd_info *fbi,
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
#endif

/****************************************************************************
 * Get the video params out of 'var'. If a value doesn't fit, round it up,
 * if it's too big, return -EINVAL.
 ****************************************************************************/
#if 0
static int reciva_vfd_check_var(struct fb_var_screeninfo *var,
			       struct fb_info *info)
{
	struct reciva_vfd_info *fbi = info->par;

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
#endif

/****************************************************************************
 * activate (set) the controller from the given framebuffer
 * information
 ****************************************************************************/
#if 0
static void reciva_vfd_activate_var(struct reciva_vfd_info *fbi,
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
		int clkdiv = reciva_vfd_calc_pixclk(fbi, var->pixclock);

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
	reciva_vfd_set_lcdaddr(fbi);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
}
#endif

/****************************************************************************
 * Optional function. Alters the hardware state.
 * info - frame buffer structure that represents a single frame buffer
 ****************************************************************************/
#if 0
static int reciva_vfd_set_par(struct fb_info *info)
{
	struct reciva_vfd_info *fbi = info->par;
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

	reciva_vfd_activate_var(fbi, var);
	return 0;
}
#endif

/****************************************************************************
 * Schedule a palette update.
 * Palette registers will be updated on next frame sync interrupt
 ****************************************************************************/
#if 0
static void schedule_palette_update(struct reciva_vfd_info *fbi,
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
#endif

/****************************************************************************
 * Palette utility function
 ****************************************************************************/
#if 0
static inline unsigned int chan_to_field(unsigned int chan, struct fb_bitfield *bf)
{
	chan &= 0xffff;
	chan >>= 16 - bf->length;
	return chan << bf->offset;
}
#endif

/****************************************************************************
 * Request a change to the colour palette
 ****************************************************************************/
#if 0
static int reciva_vfd_setcolreg(unsigned regno,
			       unsigned red, unsigned green, unsigned blue,
			       unsigned transp, struct fb_info *info)
{
	struct reciva_vfd_info *fbi = info->par;
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
#endif

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
#if 0
static int reciva_vfd_blank(int blank_mode, struct fb_info *info)
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
#endif

#if 0
static struct fb_ops reciva_vfd_ops = {
	.owner		= THIS_MODULE,
	.fb_check_var	= reciva_vfd_check_var,
	.fb_set_par	= reciva_vfd_set_par,
	.fb_blank	= reciva_vfd_blank,
	.fb_setcolreg	= reciva_vfd_setcolreg,
	.fb_fillrect	= cfb_fillrect,
	.fb_copyarea	= cfb_copyarea,
	.fb_imageblit	= cfb_imageblit,
};
#endif


/****************************************************************************
 * Allocates the DRAM memory for the frame buffer.  This buffer is
 * remapped into a non-cached, non-buffered, memory region to
 * allow palette and pixel writes to occur without flushing the
 * cache.  Once this area is remapped, all virtual memory
 * access to the video memory should occur at the new region.
 ****************************************************************************/
#if 0
static int __init reciva_vfd_map_video_memory(struct reciva_vfd_info *fbi)
{
	printk(PFX "%s\n", __FUNCTION__);
	dprintk("map_video_memory(fbi=%p)\n", fbi);

	fbi->map_size = PAGE_ALIGN(fbi->fb->fix.smem_len + PAGE_SIZE);
	fbi->map_cpu  = dma_alloc_writecombine(fbi->dev, fbi->map_size,
					       &fbi->map_dma, GFP_KERNEL);

	fbi->map_size = fbi->fb->fix.smem_len;

	if (fbi->map_cpu) {
		/* prevent initial garbage on screen */
		dprintk("map_video_memory: clear %p:%08x\n",
			fbi->map_cpu, fbi->map_size);
		memset(fbi->map_cpu, 0xf0, fbi->map_size);

		fbi->screen_dma		= fbi->map_dma;
		fbi->fb->screen_base	= fbi->map_cpu;
		fbi->fb->fix.smem_start  = fbi->screen_dma;

		dprintk("map_video_memory: dma=%08x cpu=%p size=%08x\n",
			fbi->map_dma, fbi->map_cpu, fbi->fb->fix.smem_len);
	}

	return fbi->map_cpu ? 0 : -ENOMEM;
}
#endif

/****************************************************************************
 * Unmap DMA video buffer
 ****************************************************************************/
#if 0
static inline void reciva_vfd_unmap_video_memory(struct reciva_vfd_info *fbi)
{
	printk(PFX "%s\n", __FUNCTION__);
	dma_free_writecombine(fbi->dev,fbi->map_size,fbi->map_cpu, fbi->map_dma);
}
#endif

/****************************************************************************
 * Modify GPIO pins
 ****************************************************************************/
#if 0
static inline void modify_gpio(void __iomem *reg,
			       unsigned long set, unsigned long mask)
{
	unsigned long tmp;

	tmp = readl(reg) & ~mask;
	writel(tmp | set, reg);
}
#endif

/****************************************************************************
 * Initialise all LCD-related registers
 ****************************************************************************/
#if 0
static int reciva_vfd_init_registers(struct reciva_vfd_info *fbi)
{
	printk(PFX "%s\n", __FUNCTION__);
	unsigned long flags;

	/* Initialise LCD with values from haret */

	local_irq_save(flags);

	/* modify the gpio(s) with interrupts set (bjd) */

	modify_gpio(S3C2410_GPCUP,  mach_info->gpcup,  mach_info->gpcup_mask);
	modify_gpio(S3C2410_GPCCON, mach_info->gpccon, mach_info->gpccon_mask);
	modify_gpio(S3C2410_GPDUP,  mach_info->gpdup,  mach_info->gpdup_mask);
	modify_gpio(S3C2410_GPDCON, mach_info->gpdcon, mach_info->gpdcon_mask);

	local_irq_restore(flags);

	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
	writel(fbi->regs.lcdcon2, S3C2410_LCDCON2);
	writel(fbi->regs.lcdcon3, S3C2410_LCDCON3);
	writel(fbi->regs.lcdcon4, S3C2410_LCDCON4);
	writel(fbi->regs.lcdcon5, S3C2410_LCDCON5);

 	reciva_vfd_set_lcdaddr(fbi);

	dprintk("LPCSEL    = 0x%08lx\n", mach_info->lpcsel);
	//writel(mach_info->lpcsel, S3C2410_LPCSEL);
	writel(mach_info->lpcsel, S3C2412_TCONSEL);

	dprintk("replacing TPAL %08x\n", readl(S3C2412_TPAL));

	/* ensure temporary palette disabled */
	writel(0x00, S3C2412_TPAL);

	/* Enable video by setting the ENVID bit to 1 */
	fbi->regs.lcdcon1 |= S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);
	return 0;
}
#endif

/****************************************************************************
 * Write colour palette
 ****************************************************************************/
#if 0
static void reciva_vfd_write_palette(struct reciva_vfd_info *fbi)
{
	printk(PFX "%s\n", __FUNCTION__);
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
#endif

/****************************************************************************
 * Framesync IRQ. 
 * Currently used to update the palette at the correct time
 ****************************************************************************/
#if 0
static irqreturn_t reciva_vfd_irq(int irq, void *dev_id)
{
	struct reciva_vfd_info *fbi = dev_id;
	unsigned long lcdirq = readl(S3C2412_LCDINTPND);

	if (lcdirq & S3C2410_LCDINT_FRSYNC) {
		if (fbi->palette_ready)
			reciva_vfd_write_palette(fbi);

		writel(S3C2410_LCDINT_FRSYNC, S3C2412_LCDINTPND);
		writel(S3C2410_LCDINT_FRSYNC, S3C2412_LCDSRCPND);
	}

	return IRQ_HANDLED;
}
#endif



/****************************************************************************
 * platform_driver START
 ****************************************************************************/

static char driver_name[]="reciva_vfd_hydra";

/****************************************************************************
 * platform_driver
 * knl core will call this
 * XXX mach_stingray
 * XXX not for reciva_vfd - just call this at initialisation
 ****************************************************************************/
static int __init reciva_vfd_probe(struct platform_device *pdev)
{
	printk(PFX "%s\n", __FUNCTION__);
        if (1)
		return 0;
#if 0
	struct reciva_vfd_info *info;
	struct fb_info	   *fbinfo;
	struct s3c2410fb_hw *mregs;
	int ret;
	int irq;
	int i;
	u32 lcdcon1;

	mach_info = pdev->dev.platform_data;
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

	fbinfo = framebuffer_alloc(sizeof(struct reciva_vfd_info), &pdev->dev);
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

	fbinfo->fbops		    = &reciva_vfd_ops;
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

	ret = request_irq(irq, reciva_vfd_irq, IRQF_DISABLED, pdev->name, info);
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
	ret = reciva_vfd_map_video_memory(info);
	if (ret) {
		printk( KERN_ERR "Failed to allocate video RAM: %d\n", ret);
		ret = -ENOMEM;
		goto release_clock;
	}
	dprintk("got video memory\n");

	ret = reciva_vfd_init_registers(info);

	ret = reciva_vfd_check_var(&fbinfo->var, fbinfo);

	ret = register_framebuffer(fbinfo);
	if (ret < 0) {
		printk(KERN_ERR "Failed to register framebuffer device: %d\n", ret);
		goto free_video_memory;
	}

	/* create device files */
//	device_create_file(&pdev->dev, &dev_attr_debug);

	printk(KERN_INFO "fb%d: %s frame buffer device\n",
		fbinfo->node, fbinfo->fix.id);

	return 0;

free_video_memory:
	reciva_vfd_unmap_video_memory(info);
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
#endif
}

/****************************************************************************
 * platform_driver helper
 * Shutdown the lcd controller
 ****************************************************************************/
static void reciva_vfd_stop_lcd(struct reciva_vfd_info *fbi)
{
	printk(PFX "%s\n", __FUNCTION__);
#if 0
	unsigned long flags;

	local_irq_save(flags);

	fbi->regs.lcdcon1 &= ~S3C2410_LCDCON1_ENVID;
	writel(fbi->regs.lcdcon1, S3C2410_LCDCON1);

	local_irq_restore(flags);
#endif
}

/****************************************************************************
 * platform_driver
 * Remove
 ****************************************************************************/
static int reciva_vfd_remove(struct platform_device *pdev)
{
	printk(PFX "remove\n");
	return 0;

#if 0
	struct fb_info	   *fbinfo = platform_get_drvdata(pdev);
	struct reciva_vfd_info *info = fbinfo->par;
	int irq;

	reciva_vfd_stop_lcd(info);
	msleep(1);

	reciva_vfd_unmap_video_memory(info);

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
#endif
}

#ifdef CONFIG_PM

/****************************************************************************
 * platform_driver
 * Suspend support
 ****************************************************************************/
static int reciva_vfd_suspend(struct platform_device *dev, pm_message_t state)
{
	printk(PFX "suspend\n");
	return 0;

#if 0
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct reciva_vfd_info *info = fbinfo->par;

	reciva_vfd_stop_lcd(info);

	/* sleep before disabling the clock, we need to ensure
	 * the LCD DMA engine is not going to get back on the bus
	 * before the clock goes off again (bjd) */

	msleep(1);
	clk_disable(info->clk);

	return 0;
#endif
}

/****************************************************************************
 * platform_driver
 * Resume support
 ****************************************************************************/
static int reciva_vfd_resume(struct platform_device *dev)
{
	printk(PFX "resume\n");
	return 0;

#if 0
	struct fb_info	   *fbinfo = platform_get_drvdata(dev);
	struct reciva_vfd_info *info = fbinfo->par;

	clk_enable(info->clk);
	msleep(1);

	reciva_vfd_init_registers(info);

	return 0;
#endif
}

#else
#define reciva_vfd_suspend NULL
#define reciva_vfd_resume  NULL
#endif

static struct platform_driver reciva_vfd_driver = {
	.probe		= reciva_vfd_probe,
	.remove		= reciva_vfd_remove,
	.suspend	= reciva_vfd_suspend,
	.resume		= reciva_vfd_resume,
	.driver		= {
	.name	= "s3c2412-lcd", // This name needs to match arch/arm/mach-s3c2412/s3c2412.c
	.owner	= THIS_MODULE,
	},
};

/****************************************************************************
 * platform_driver END
 ****************************************************************************/





/****************************************************************************
 * Module initialisation
 ****************************************************************************/
int __devinit reciva_vfd_init(void)
{
	printk(PFX "init\n");

	mach_info = &reciva_tftexpander_cfg;
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

	return platform_driver_register(&reciva_vfd_driver);
}

/****************************************************************************
 * Module tidy up
 ****************************************************************************/
static void __exit reciva_vfd_cleanup(void)
{
	printk(PFX "exit\n");
	platform_driver_unregister(&reciva_vfd_driver);
}


module_init(reciva_vfd_init);
module_exit(reciva_vfd_cleanup);

MODULE_DESCRIPTION("Framebuffer VFD driver");
MODULE_LICENSE("GPL");
