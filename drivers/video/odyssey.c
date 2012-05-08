/*
 * linux/drivers/video/odyssey.c -- SGI Octane Odyssey graphics
 *
 *  Copyright (c) 2005 by Stanislaw Skowronek
 *  Copyright (c) 2011 by Joshua Kinard (Fixes, Maintenance)
 *
 *  Based on linux/drivers/video/skeletonfb.c
 *
 *  This driver, as most of the IP30 (SGI Octane) port, is a result of massive
 *  amounts of reverse engineering and trial-and-error. If anyone is interested
 *  in helping with it, please contact me: <skylark@linux-mips.org>.
 *
 *  Note: the driver is specialcased for 8x16 font (will be a bit faster).
 *
 *  Odyssey is a really cool graphics device. It is a dual-chip OpenGL
 *  implementation with ARB_imaging support, and overall a very elegant design.
 *
 *  This file is subject to the terms and conditions of the GNU General Public
 *  License. See the file COPYING in the main directory of this archive for
 *  more details.
 */

#define DRV_VERSION	"0.42"

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/tty.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/fb.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/spinlock.h>
#include <linux/font.h>
#include <linux/platform_device.h>

#include <asm/mach-ip30/xtalk.h>
#include <video/odyssey.h>

struct odyssey_par {
	/* physical mmio base in HEART XTalk space */
	unsigned long mmio_base;

	/* virtual mmio base in kernel space */
	unsigned long mmio_virt;

	/* locks to prevent simultaneous user and kernel access */
	int open_flag;
	int mmap_flag;
	spinlock_t lock;
};

static struct fb_fix_screeninfo odyssey_fix = {
	.id =		"Odyssey", 
	.smem_start = 	0,
	.smem_len =	0,
	.type =		FB_TYPE_PACKED_PIXELS,
	.visual =	FB_VISUAL_TRUECOLOR,
	.xpanstep =	0,
	.ypanstep =	0,
	.ywrapstep =	0, 
	.line_length =	0,
	.accel =	FB_ACCEL_SGI_ODYSSEY,
};

static struct fb_var_screeninfo odyssey_var = {
	.xres =		1280,
	.yres =		1024,
	.xres_virtual =	1280,
	.yres_virtual =	1024,
	.bits_per_pixel = 24,
	.red =		{ .offset = 0, .length = 8 },
	.green =	{ .offset = 8, .length = 8 },
	.blue =		{ .offset = 16, .length = 8 },
	.transp =	{ .offset = 24, .length = 8 },
};

static struct fb_info info;
static unsigned int pseudo_palette[256];
static struct odyssey_par current_par;
int odyssey_init(void);


/* Most of the hex numbers seen in the various functions, especially those in
 * the hardware init functions, were discovered via reverse engineering of IRIX
 * drivers.  Little is known as to what they do or what they mean.
 *
 * Possibly, these hex numbers are addresses to locations outside of what we
 * perceive as normal reality, and instead reference a location within the void
 * itself, from which various dark and black magiks flow forth and breathe life
 * into this hardware.
 *
 * If you think you can come up with a better explanation, then feel free to
 * send a patch!
 */


/* --------------------- Gory Details --------------------- */
#define MMIO (((struct odyssey_par *)p->par)->mmio_virt)
#define PAR (*((struct odyssey_par *)p->par))

static unsigned int pack_ieee754(int val)
{
	unsigned sign, exp;

	if (!val)
		return 0;

	sign = (val & 0x80000000);

	if (sign)
		val = -val;

	if (val & 0xff000000)
		return 0;

	exp = 150;
	while (!(val & 0x00800000)) {
		val <<= 1;
		exp--;
	}

	return (sign | (exp << 23) | (val & 0x007fffff));
}

static void odyssey_flush(unsigned long mmio)
{
	odyssey_wait_cfifo(mmio);
	ODY_CFIFO_W(mmio) = 0x00010443;
	ODY_CFIFO_W(mmio) = 0x000000fa;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010019;
	ODY_CFIFO_W(mmio) = 0x00010443;
	ODY_CFIFO_W(mmio) = 0x00000096;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010443;
	ODY_CFIFO_W(mmio) = 0x000000fa;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
}

static void odyssey_smallflush(unsigned long mmio)
{
	odyssey_wait_cfifo(mmio);
	ODY_CFIFO_W(mmio) = 0x00010443;
	ODY_CFIFO_W(mmio) = 0x000000fa;
	ODY_CFIFO_W(mmio) = 0x00010046;
	ODY_CFIFO_W(mmio) = 0x00010046;
}

static void odyssey_initbuzzgfe(unsigned long mmio)
{
	ODY_CFIFO_W(mmio) = 0x20008003;
	ODY_CFIFO_W(mmio) = 0x21008010;
	ODY_CFIFO_W(mmio) = 0x22008000;
	ODY_CFIFO_W(mmio) = 0x23008002;
	ODY_CFIFO_W(mmio) = 0x2400800c;
	ODY_CFIFO_W(mmio) = 0x2500800e;
	ODY_CFIFO_W(mmio) = 0x27008000;
	ODY_CFIFO_W(mmio) = 0x28008000;
	ODY_CFIFO_W(mmio) = 0x290080d6;
	ODY_CFIFO_W(mmio) = 0x2a0080e0;
	ODY_CFIFO_W(mmio) = 0x2c0080ea;
	ODY_CFIFO_W(mmio) = 0x2e008380;
	ODY_CFIFO_W(mmio) = 0x2f008000;
	ODY_CFIFO_W(mmio) = 0x30008000;
	ODY_CFIFO_W(mmio) = 0x31008000;
	ODY_CFIFO_W(mmio) = 0x32008000;
	ODY_CFIFO_W(mmio) = 0x33008000;
	ODY_CFIFO_W(mmio) = 0x34008000;
	ODY_CFIFO_W(mmio) = 0x35008000;
	ODY_CFIFO_W(mmio) = 0x310081e0;
	odyssey_flush(mmio);
}

static void odyssey_initbuzzxform(unsigned long mmio)
{
	ODY_CFIFO_W(mmio) = 0x9080bda2;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x3f000000;
	ODY_CFIFO_W(mmio) = 0xbf800000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x4e000000;
	ODY_CFIFO_W(mmio) = 0x40400000;
	ODY_CFIFO_W(mmio) = 0x4e000000;
	ODY_CFIFO_W(mmio) = 0x4d000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x34008000;
	ODY_CFIFO_W(mmio) = 0x9080bdc8;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x34008010;
	ODY_CFIFO_W(mmio) = 0x908091df;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x3f800000;
	ODY_CFIFO_W(mmio) = 0x34008000;
	odyssey_flush(mmio);
}

static void odyssey_initbuzzrast(unsigned long mmio)
{
	ODY_CFIFO_W(mmio) = 0x0001203b;
	ODY_CFIFO_W(mmio) = 0x00001000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00001000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00001000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00001000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x0001084a;
	ODY_CFIFO_W(mmio) = 0x00000080;
	ODY_CFIFO_W(mmio) = 0x00000080;
	ODY_CFIFO_W(mmio) = 0x00010845;
	ODY_CFIFO_W(mmio) = 0x000000ff;
	ODY_CFIFO_W(mmio) = 0x000076ff;
	ODY_CFIFO_W(mmio) = 0x0001141b;
	ODY_CFIFO_W(mmio) = 0x00000001;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00011c16;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x03000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00010404;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00011023;
	ODY_CFIFO_W(mmio) = 0x00ff0ff0;
	ODY_CFIFO_W(mmio) = 0x00ff0ff0;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x000000ff;
	ODY_CFIFO_W(mmio) = 0x00011017;
	ODY_CFIFO_W(mmio) = 0x00002000;
	ODY_CFIFO_W(mmio) = 0x00000050;
	ODY_CFIFO_W(mmio) = 0x20004950;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x0001204b;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x004ff3ff;
	ODY_CFIFO_W(mmio) = 0x00ffffff;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00ffffff;
	ODY_CFIFO_W(mmio) = 0x00000000;
	ODY_CFIFO_W(mmio) = 0x00ffffff;
	ODY_CFIFO_W(mmio) = 0x00000000;
	odyssey_flush(mmio);
}

static void odyssey_initpbjvc(unsigned long mmio)
{
	int x;

	odyssey_wait_dfifo(mmio, 0);
	for (x = 0; x < 16; x++)
		odyssey_dfifo_write(mmio, (0x2900 | x), 0x905215a6);

	odyssey_wait_dfifo(mmio, 0);
	for (x = 16; x < 32; x++)
		odyssey_dfifo_write(mmio, (0x2900 | x), 0x905215a6);

	odyssey_wait_dfifo(mmio, 0);
	odyssey_dfifo_write(mmio, 0x2581, 0x00000000);
}

static void odyssey_initpbjgamma(unsigned long mmio)
{
	u32 i, v;

	for (i = 0; i < 0x200; i++) {
		if ((i & 15) == 0)
			odyssey_wait_dfifo(mmio, 0);
		v = (i >> 2);
		v = ((v << 20) | (v << 10) | v);
		odyssey_dfifo_write(mmio, (i + 0x1a00), v);
	}

	for (i = 0x200; i < 0x300; i++) {
		if ((i & 15) == 0)
			odyssey_wait_dfifo(mmio, 0);
		v = (((i - 0x200) >> 1) + 0x80);
		v = ((v << 20) | (v << 10) | v);
		odyssey_dfifo_write(mmio, (i + 0x1a00), v);
	}

	for (i = 0x300; i < 0x600; i++) {
		if ((i & 15) == 0)
			odyssey_wait_dfifo(mmio, 0);
		v = ((i - 0x300) + 0x100);
		v = ((v << 20) | (v << 10) | v);
		odyssey_dfifo_write(mmio, (i + 0x1a00), v);
	}
}

static void odyssey_rect(unsigned long mmio, int x, int y, int w, int h,
                         unsigned c, int lo)
{
	if ( lo != ODY_LO_COPY) {
		ODY_CFIFO_W(mmio) = 0x00010404;
		ODY_CFIFO_W(mmio) = 0x00100000;
		ODY_CFIFO_W(mmio) = 0x00010422;	/* glLogicOp */
		ODY_CFIFO_W(mmio) = lo;
		odyssey_smallflush(mmio);
	}

	ODY_CFIFO_W(mmio) = 0x00014400;		/* glBegin */
	ODY_CFIFO_W(mmio) = 0x00000007;		/* GL_QUADS */
	ODY_CFIFO_W(mmio) = 0xc580cc08;		/* glColor3ub */
	ODY_CFIFO_W(mmio) = (c & 255);
	ODY_CFIFO_W(mmio) = ((c >> 8) & 255);
	ODY_CFIFO_W(mmio) = ((c >> 16) & 255);
	ODY_CFIFO_W(mmio) = 0x8080c800;		/* glVertex2i */
	ODY_CFIFO_W(mmio) = x;
	ODY_CFIFO_W(mmio) = y;
	ODY_CFIFO_W(mmio) = 0x8080c800;		/* glVertex2i */
	ODY_CFIFO_W(mmio) = (x + w);
	ODY_CFIFO_W(mmio) = y;
	ODY_CFIFO_W(mmio) = 0x8080c800;		/* glVertex2i */
	ODY_CFIFO_W(mmio) = (x + w);
	ODY_CFIFO_W(mmio) = (y + h);
	ODY_CFIFO_W(mmio) = 0x8080c800;		/* glVertex2i */
	ODY_CFIFO_W(mmio) = x;
	ODY_CFIFO_W(mmio) = (y + h);
	ODY_CFIFO_W(mmio) = 0x00014001;		/* glEnd */
	odyssey_smallflush(mmio);

	if (lo != ODY_LO_COPY) {
		ODY_CFIFO_W(mmio) = 0x00010404;
		ODY_CFIFO_W(mmio) = 0x00000000;
		ODY_CFIFO_W(mmio) = 0x00010422;	/* glLogicOp */
		ODY_CFIFO_W(mmio) = ODY_LO_COPY;
		odyssey_smallflush(mmio);
	}
}

static inline unsigned odyssey_getpalreg(struct fb_info *p, unsigned i)
{
	return ((unsigned *)p->pseudo_palette)[i];
}


/* ------------ Accelerated Functions --------------------- */
static void odyssey_fillrect(struct fb_info *p, const struct fb_fillrect *region)
{
	unsigned long flags;
	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		switch(region->rop) {
		case ROP_XOR:
			odyssey_rect(MMIO, region->dx, region->dy,
			             region->width, region->height,
			             odyssey_getpalreg(p, region->color),
			             ODY_LO_XOR);
			break;

		case ROP_COPY:
		default:
			odyssey_rect(MMIO, region->dx, region->dy,
			             region->width, region->height,
			             odyssey_getpalreg(p, region->color),
			             ODY_LO_COPY);
			break;
		}
 	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

static void odyssey_copyarea(struct fb_info *p, const struct fb_copyarea *area) 
{
	unsigned sx, sy, dx, dy, w, h;
	unsigned long flags;

	w = area->width;
	h = area->height;

	if ((w < 1) || (h < 1))
		return;

	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		sx = area->sx;
		sy = area->sy;
		dx = area->dx;
		dy = area->dy;

		odyssey_flush(MMIO);
		ODY_CFIFO_W(MMIO) = 0x00010658;
		ODY_CFIFO_W(MMIO) = 0x00120000;
		ODY_CFIFO_W(MMIO) = 0x00002031;
		ODY_CFIFO_W(MMIO) = 0x00002000;
		ODY_CFIFO_W(MMIO) = (sx | (sy << 16));
		ODY_CFIFO_W(MMIO) = 0x80502050;
		ODY_CFIFO_W(MMIO) = (w | (h << 16));	/* size */
		ODY_CFIFO_W(MMIO) = 0x82223042;
		ODY_CFIFO_W(MMIO) = 0x00002000;
		ODY_CFIFO_W(MMIO) = (dx | (dy << 16));	/* dest */
		ODY_CFIFO_W(MMIO) = 0x3222204b;
	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

static void odyssey_imageblit_8bpp(struct fb_info *p, const struct fb_image *image)
{
	int i, j, l;
	const unsigned char *dp;
	unsigned pal[256];

	dp = image->data;
	for (i = 0; i < 256; i++)
		pal[i] = odyssey_getpalreg(p, i);

	/* perform a PIO blit to card */
	odyssey_smallflush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x00010405;
	ODY_CFIFO_W(MMIO) = 0x00002400;
	ODY_CFIFO_W(MMIO) = 0xc580cc08;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00011453;
	ODY_CFIFO_W(MMIO) = 0x00000002;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	odyssey_flush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x2900812f;
	ODY_CFIFO_W(MMIO) = 0x00014400;
	ODY_CFIFO_W(MMIO) = 0x0000000a;
	ODY_CFIFO_W(MMIO) = 0xcf80a92f;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dx);
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dy);
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dx + image->width);
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dy + image->height);
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00004570;
	ODY_CFIFO_W(MMIO) = 0x0f00104c;
	ODY_CFIFO_W(MMIO) = 0x00000071;

	for (j = 0; j < image->height; j++) {
		ODY_CFIFO_W(MMIO) = 0x00004570;
		ODY_CFIFO_W(MMIO) = 0x0fd1104c;
		ODY_CFIFO_W(MMIO) = 0x00000071;
		i = image->width;
		while (i > 0) {
			l = ((i > 14) ? 14 : i);
			i -= l;
			ODY_CFIFO_W(MMIO) = (0x00014011 | (l << 10));
			while (l--)
				ODY_CFIFO_W(MMIO) = pal[*(dp++)];
		}
	}

	ODY_CFIFO_W(MMIO) = 0x00014001;
	odyssey_smallflush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x290080d6;
	ODY_CFIFO_W(MMIO) = 0x00011453;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00010405;
	ODY_CFIFO_W(MMIO) = 0x00002000;
	odyssey_flush(MMIO);
}

static void odyssey_imageblit_1bpp_stp(struct fb_info *p, const struct fb_image *image) 
{
	unsigned palf, palb, buf;
	int i;
	const unsigned char *pic;

	palf = odyssey_getpalreg(p, image->fg_color);
	palb = odyssey_getpalreg(p, image->bg_color);
	odyssey_smallflush(MMIO);

	if ((image->dy & 31) < 16)
		ODY_CFIFO_W(MMIO) = 0x00013c4e;
	else
		ODY_CFIFO_W(MMIO) = 0x00013c4f;

	pic = image->data;

	for (i = 0; i < 16; i++) {
		buf = *(pic++);
		buf |= (buf << 8);
		ODY_CFIFO_W(MMIO) = (buf | (buf << 16));
	}

	ODY_CFIFO_W(MMIO) = 0x00010404;
	ODY_CFIFO_W(MMIO) = 0x00000018;
	odyssey_smallflush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x0001043f;
	ODY_CFIFO_W(MMIO) = 0x00000010;
	ODY_CFIFO_W(MMIO) = 0x00010c21;
	ODY_CFIFO_W(MMIO) = 0x00000200;
	ODY_CFIFO_W(MMIO) = (((palb & 255) << 16) | ((palb >> 4) & 0xff0));
	ODY_CFIFO_W(MMIO) = (palb & 0xff0000) | 0xfff;
	ODY_CFIFO_W(MMIO) = 0x00014400;
	ODY_CFIFO_W(MMIO) = 0x00010407;
	ODY_CFIFO_W(MMIO) = 0xc580cc08;
	ODY_CFIFO_W(MMIO) = (palf & 255);
	ODY_CFIFO_W(MMIO) = ((palf >> 8) & 255);
	ODY_CFIFO_W(MMIO) = ((palf >> 16) & 255);
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = image->dx;
	ODY_CFIFO_W(MMIO) = image->dy;
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = (image->dx + 8);
	ODY_CFIFO_W(MMIO) = image->dy;
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = (image->dx + 8);
	ODY_CFIFO_W(MMIO) = (image->dy + 16);
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = image->dx;
	ODY_CFIFO_W(MMIO) = (image->dy + 16);
	ODY_CFIFO_W(MMIO) = 0x00014001;
	odyssey_flush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x00010404;
	ODY_CFIFO_W(MMIO) = 0x00000000;
}

/* this is a slow fallback */
static void odyssey_imageblit_1bpp_pio(struct fb_info *p, const struct fb_image *image) 
{
	int i, j, l, c;
	const unsigned char *dp;
	unsigned char d = 0;
	unsigned palf, palb;

	dp = image->data;
	palf = odyssey_getpalreg(p, image->fg_color);
	palb = odyssey_getpalreg(p, image->bg_color);

	/* perform a PIO blit to card */
	odyssey_smallflush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x00010405;
	ODY_CFIFO_W(MMIO) = 0x00002400;
	ODY_CFIFO_W(MMIO) = 0xc580cc08;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00011453;
	ODY_CFIFO_W(MMIO) = 0x00000002;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	odyssey_flush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x2900812f;
	ODY_CFIFO_W(MMIO) = 0x00014400;
	ODY_CFIFO_W(MMIO) = 0x0000000a;
	ODY_CFIFO_W(MMIO) = 0xcf80a92f;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dx);
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dy);
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dx + image->width);
	ODY_CFIFO_W(MMIO) = pack_ieee754(image->dy + image->height);
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x8080c800;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00004570;
	ODY_CFIFO_W(MMIO) = 0x0f00104c;
	ODY_CFIFO_W(MMIO) = 0x00000071;

	for (j = 0; j < image->height; j++) {
		c = 0;
		ODY_CFIFO_W(MMIO) = 0x00004570;
		ODY_CFIFO_W(MMIO) = 0x0fd1104c;
		ODY_CFIFO_W(MMIO) = 0x00000071;
		i = image->width;
		while (i > 0) {
			l = ((i > 14) ? 14 : i);
			i -= l;
			ODY_CFIFO_W(MMIO) = (0x00014011 | (l << 10));
			while (l--) {
				if (!c)
					d = *(dp++);
				ODY_CFIFO_W(MMIO) = ((d & 0x80) ? palf : palb);
				d <<= 1;
				c = ((c + 1) & 7);
			}
		}
	}

	ODY_CFIFO_W(MMIO) = 0x00014001;
	odyssey_smallflush(MMIO);
	ODY_CFIFO_W(MMIO) = 0x290080d6;
	ODY_CFIFO_W(MMIO) = 0x00011453;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00000000;
	ODY_CFIFO_W(MMIO) = 0x00010405;
	ODY_CFIFO_W(MMIO) = 0x00002000;
	odyssey_flush(MMIO);
}

static void odyssey_imageblit(struct fb_info *p, const struct fb_image *image) 
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
	if (!PAR.open_flag) {
		switch (image->depth) {
		case 1:
			if ((image->width != 8) || (image->height != 16) ||
			    (image->dx & 7) || (image->dy & 15))
			{
				odyssey_imageblit_1bpp_pio(p, image);
			} else
				odyssey_imageblit_1bpp_stp(p, image);
			break;
		case 8:
			odyssey_imageblit_8bpp(p, image);
			break;
		}
	}
	spin_unlock_irqrestore(&PAR.lock, flags);
}

static int odyssey_sync(struct fb_info *info)
{
	return 0;
}

static int odyssey_blank(int blank_mode, struct fb_info *info)
{
	/* TODO */
	return 0;
}

static int odyssey_setcolreg(unsigned regno, unsigned red, unsigned green,
                            unsigned blue, unsigned transp,
                            struct fb_info *info)
{
	if (regno > 255)
		return 1;

	((unsigned *)info->pseudo_palette)[regno]=
	    (red >> 8) |
	    (green & 0xff00) |
	    ((blue <<8 ) & 0xff0000);

	return 0;
}


/* ------------------- Framebuffer Access -------------------- */
ssize_t odyssey_read(struct fb_info *info, char __user *buf, size_t count,
                     loff_t *ppos)
{
	return -EINVAL;
}

ssize_t odyssey_write(struct fb_info *info, const char __user *buf,
                      size_t count, loff_t *ppos)
{
	return -EINVAL;
}


/* --------------------- Userland Access --------------------- */
int odyssey_ioctl(struct fb_info *info, unsigned int cmd, unsigned long arg)
{
	return -EINVAL;
}

int odyssey_mmap(struct fb_info *p, struct vm_area_struct *vma)
{
	unsigned long size = vma->vm_end - vma->vm_start;
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	unsigned long start = vma->vm_start;

	switch (offset) {
	case 0x0000000:
		if (size != 0x410000)
			return -EINVAL;

		if (vma->vm_pgoff > (~0UL >> PAGE_SHIFT))
			return -EINVAL;

		vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
		vma->vm_flags |= VM_IO;

		offset += MMIO;
		if (remap_pfn_range(vma, start, (offset >> PAGE_SHIFT),
		                    size, vma->vm_page_prot))
			return -EAGAIN;

		PAR.mmap_flag = 1;
		return 0;

	default:
		return -EINVAL;
	}

	return 0;
}

static int odyssey_open(struct fb_info *p, int user)
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
        if (user)
		PAR.open_flag++;
	spin_unlock_irqrestore(&PAR.lock, flags);

	return 0;
}

static int odyssey_release(struct fb_info *p, int user)
{
	unsigned long flags;

	spin_lock_irqsave(&PAR.lock, flags);
        if (user && PAR.open_flag) {
                PAR.open_flag--;
		if (PAR.open_flag == 0)
                        PAR.mmap_flag = 0;
	}
	spin_unlock_irqrestore(&PAR.lock, flags);

	return 0;
}


/* ---------------- Framebuffer Operations ------------------- */
static struct fb_ops odyssey_ops = {
	.owner		= THIS_MODULE,
	.fb_read	= odyssey_read,
	.fb_write	= odyssey_write,
	.fb_blank	= odyssey_blank,
	.fb_fillrect	= odyssey_fillrect,
	.fb_copyarea	= odyssey_copyarea,
	.fb_imageblit	= odyssey_imageblit,
	.fb_sync	= odyssey_sync,
	.fb_ioctl	= odyssey_ioctl,
	.fb_setcolreg	= odyssey_setcolreg,
	.fb_mmap	= odyssey_mmap,
	.fb_open	= odyssey_open,
	.fb_release	= odyssey_release,
};


/* -------------------- Initialization ----------------------- */
static void __devinit odyssey_hwinit(unsigned long mmio)
{
	/* initialize hardware */
	odyssey_initbuzzgfe(mmio);
	odyssey_initbuzzxform(mmio);
	odyssey_initbuzzrast(mmio);
	odyssey_initpbjvc(mmio);
	odyssey_initpbjgamma(mmio);
}

static int __devinit odyssey_devinit(void)
{
	int xwid;

	xwid = ip30_xtalk_find(ODY_XTALK_MFGR, ODY_XTALK_PART,
			       IP30_XTALK_NUM_WID);

	if (xwid == -1)
		return -ENODEV;

	current_par.open_flag = 0;
	current_par.mmap_flag = 0;
	current_par.lock = __SPIN_LOCK_UNLOCKED(current_par.lock);

	current_par.mmio_base = ip30_xtalk_get_id(xwid);
	current_par.mmio_virt = (unsigned long)ioremap(current_par.mmio_base,
	                        0x410000);

	odyssey_fix.mmio_start = current_par.mmio_base;
	odyssey_fix.mmio_len = 0x410000;

	info.flags = FBINFO_FLAG_DEFAULT;
	info.screen_base = NULL;
	info.fbops = &odyssey_ops;
	info.fix = odyssey_fix;
	info.var = odyssey_var;
	info.par = &current_par;
	info.pseudo_palette = pseudo_palette;

	odyssey_hwinit(current_par.mmio_virt);

	/* Required. */
	fb_alloc_cmap(&info.cmap, 256, 0);

	if (register_framebuffer(&info) < 0)
		return -EINVAL;

	printk(KERN_INFO "fb%d: %s frame buffer device\n", info.node,
			 info.fix.id);

	return 0;
}

static int __devinit odyssey_probe(struct device *dev)
{
	return odyssey_devinit();
}

static struct device_driver odyssey_driver = {
	.name = "odyssey",
	.bus = &platform_bus_type,
	.owner  = THIS_MODULE,
	.probe = odyssey_probe,
	/* Add .remove someday -- these cards aren't exactly PnP. */
};

static struct platform_device odyssey_device = {
	.name = "odyssey",
};

int __init odyssey_init(void)
{
	int ret = driver_register(&odyssey_driver);

	if (!ret) {
		ret = platform_device_register(&odyssey_device);
		if (ret)
			driver_unregister(&odyssey_driver);
	}

	return ret;
}

void __exit odyssey_exit(void)
{
	 driver_unregister(&odyssey_driver);
}

module_init(odyssey_init);
module_exit(odyssey_exit);

MODULE_AUTHOR("Stanislaw Skowronek <skylark@linux-mips.org>");
MODULE_AUTHOR("Joshua Kinard <kumba@gentoo.org>");
MODULE_DESCRIPTION("Odyssey (Buzz) Video Driver for SGI Octane");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
MODULE_ALIAS("platform:odyssey");
