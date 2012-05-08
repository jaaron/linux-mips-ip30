/*
 * rad1.c - ALSA driver for SGI RAD1 (as found in Octane and Octane2)
 * Copyright (C) 2004-2007 by Stanislaw Skowronek <skylark@linux-mips.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA02111-1307 USA
 */

#include <sound/driver.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/spinlock.h>

#include <sound/core.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/control.h>

#include <sound/rad1.h>

typedef struct snd_rad1_pipe {
	unsigned long pma;		/* physical addr of the ring */
	int *vma;			/* virtual addr of the ring */
	struct snd_pcm_substream *subs;	/* ALSA substream */
	struct snd_pcm *pcm;
	unsigned int pnum;		/* number of periods */
	unsigned int plen;		/* length of period (in bytes) */
	unsigned int hptr;		/* hardware pointer */
	int adma;			/* DMA active flag */
	unsigned int qptr;		/* queue pointer */
} rad1_pipe_t;

typedef struct snd_rad1 {
	spinlock_t lock;
	struct snd_card *card;
	struct pci_dev *pci;
	unsigned long mmio_phys;
	volatile struct rad1regs *mmio;
	int timer_active;
	struct timer_list timer;
	rad1_pipe_t pipes[9];

	/* random stuff */
	int last_aesrx_rate;

	/* card controls */
	unsigned int attctrl;		/* attenuation control */
	unsigned int rt_atod;		/* AtoD routing */
	unsigned int rt_aesr;		/* AES Rx routing */
	unsigned int rt_adat;		/* ADAT Rx routing */
	unsigned int rt_opto;		/* Optical Out routing */
} rad1_t;
#define chip_t rad1_t

static void snd_rad1_hw_init(rad1_t *chip)
{
	/* The hex values listed here are, for the most part, unknown values
	 * determined by running portions of the IRIX kernel inside of Linux
	 * as a userland application and then extracting the run-time info.
	 *
	 * We could define them via macros if we wanted, but the macro names
	 * would be no more intelligible as RAD1_ANCIENT_MU_* than they are
	 * simple hex numbers, so until the purpose of each value is known,
	 * they shall remain as simple hex numbers.
	 *
	 * The same applies for pretty much any other hex number found in
	 * driver that isn't a bit mask or some sort.  One day, we may figure
	 * it all out, and create an appropriate header file to define them
	 * all as intelligible macros.
	 */

	chip->mmio->reset =			0xffffffff;
	udelay(1000);
	chip->mmio->reset =			0xffe3cffe;
	chip->mmio->pci_holdoff =		0x08000010;
	chip->mmio->pci_arb_control =		0x00fac688;

	/* I/O routing */
	chip->mmio->atod_control =		0x03000000;	/* Mike 03000000; LineIn 05000000 */
	chip->rt_atod =				0x03000000;
	chip->mmio->dtoa_control =		0x20000000;	/* Default */
	chip->mmio->aes_rx_control =		0x00000018;	/* Optical In 00000018; AES In 00000010 */
	chip->rt_aesr =				0x00000018;
	chip->mmio->aes_tx_control =		0x40000000;	/* Default */
	chip->mmio->adat_rx_control =		0xa0000000;	/* Disabled A0000000; Optical In A0000018 */
	chip->rt_adat =				0xa0000000;
	chip->mmio->adat_tx_control =		0x20000000;	/* Default */
	chip->mmio->gpio3 =			0x00000002;
	chip->mmio->misc_control =		0x00001500;
	chip->mmio->mpll0_lock_control =	0x9fffffff;
	chip->mmio->mpll1_lock_control =	0x9fffffff;
	chip->mmio->reset =			0xffe3c0fe;
	udelay(1000);
	chip->mmio->clockgen_ictl =		0x02000001;
	chip->mmio->reset =			0xffe24070;
	udelay(1000);
	chip->mmio->reset =			0xffe20000;
	chip->mmio->gpio2 =			0x00000002;
	chip->mmio->volume_control =		0xd6d6d6d6;
	chip->attctrl =				0xd6d6d6d6;
	udelay(1000);
	chip->mmio->misc_control =		0x00001040;	/* AES-Optical Out 00001040; AES-AES Out 00001440 */
	chip->rt_opto =				0x00001040;
	chip->mmio->reset =			0xffe20100;
	chip->mmio->freq_synth_mux_sel[3] =	0x00000001;
	chip->mmio->clockgen_rem =		0x0000ffff;
	chip->mmio->clockgen_ictl =		0x10000603;
	chip->mmio->reset =			0xffe20000;
	chip->mmio->reset =			0xffe20200;
	chip->mmio->freq_synth_mux_sel[2] =	0x00000001;
	chip->mmio->clockgen_rem =		0x0000ffff;
	chip->mmio->clockgen_ictl =		0x20000603;
	chip->mmio->reset =			0xffe20000;
	chip->mmio->reset =			0xffe20400;
	chip->mmio->freq_synth_mux_sel[1] =	0x00000001;
	chip->mmio->clockgen_rem =		0x0000ffff;
	chip->mmio->clockgen_ictl =		0x40000603;
	chip->mmio->reset =			0xffe20000;
	chip->mmio->reset =			0xffe20800;
	chip->mmio->freq_synth_mux_sel[0] =	0x00000001;
	chip->mmio->clockgen_rem =		0x0000ffff;
	chip->mmio->clockgen_ictl =		0x80000603;
	chip->mmio->reset =			0xffe20000;
	chip->mmio->gpio1 =			0x00000003;
	udelay(10000);
}

static void snd_rad1_setup_dma_pipe(rad1_t *chip, int pidx)
{
	rad1_pipe_t *pipe=chip->pipes+pidx;

	if ((-pipe->pnum * pipe->plen) & 0x7f)
		printk(KERN_WARNING "rad1: pipe %d has unaligned size %d\n", pidx, (pipe->pnum * pipe->plen));

	chip->mmio->pci_descr[pidx].hiadr = (pipe->pma >> 32);
	chip->mmio->pci_descr[pidx].loadr = (pipe->pma & 0xffffffff);
	chip->mmio->pci_descr[pidx].control = ((-pipe->pnum * pipe->plen) & 0xffffff80) | (pidx << 3);

	chip->mmio->pci_hiadr[pidx] = (pipe->pma >> 32);
	chip->mmio->pci_lc[pidx].loadr = (pipe->pma & 0xffffffff);
	chip->mmio->pci_lc[pidx].control = ((-pipe->pnum * pipe->plen) & 0xffffff80) | (pidx << 3);
}

static void snd_rad1_activate_timer(rad1_t *chip)
{
	if (!chip->timer_active) {
		chip->timer.expires = (jiffies + 1);
		add_timer(&chip->timer);
		chip->timer_active = 1;
	}
}

static void snd_rad1_run_pipe(rad1_t *chip, int pidx, int adma)
{
	rad1_pipe_t *pipe = (chip->pipes + pidx);

	if (pipe->adma != adma) {
		pipe->adma = adma;

		switch (pidx) {
		case RAD1_ATOD:
			chip->mmio->atod_control = (chip->rt_atod | adma);
			break;
		case RAD1_DTOA:
			chip->mmio->dtoa_control = (0x20000000 | adma);
			break;
		case RAD1_AESRX:
			chip->mmio->aes_rx_control = (chip->rt_aesr | adma);
			break;
		case RAD1_AESTX:
			chip->mmio->aes_tx_control = (0x40000000 | adma);
			break;
		}
	}

	if (adma)
		snd_rad1_activate_timer(chip);
}

static void snd_rad1_poll_pipe(rad1_t *chip, int pidx, int is_tx)
{
	rad1_pipe_t *pipe = (chip->pipes + pidx);
	unsigned int hptr = (pipe->pnum * pipe->plen) + (chip->mmio->pci_lc[pidx].control & 0xffffff80);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	if (pipe->adma && pipe->subs) {
		/* use hardware pointer to detect period crossing */
		if ((hptr / pipe->plen) != (pipe->hptr / pipe->plen)) {
			if (is_tx)
				pipe->qptr = (hptr / 8);
			else
				pipe->qptr = (pipe->hptr / 8);
			spin_unlock_irqrestore(&chip->lock, flags);
			snd_pcm_period_elapsed(pipe->subs);
			spin_lock_irqsave(&chip->lock, flags);
		}
		pipe->hptr = hptr;
	}
	spin_unlock_irqrestore(&chip->lock, flags);
}

static void snd_rad1_poll_timer(unsigned long chip_virt)
{
	rad1_t *chip = (rad1_t *)chip_virt;
	int adma = 0;

	if (chip->pipes[RAD1_ATOD].adma) {
		snd_rad1_poll_pipe(chip, RAD1_ATOD, 0);
		adma = 1;
	}
	if (chip->pipes[RAD1_DTOA].adma) {
		snd_rad1_poll_pipe(chip, RAD1_DTOA, 1);
		adma = 1;
	}
	if (chip->pipes[RAD1_AESRX].adma) {
		snd_rad1_poll_pipe(chip, RAD1_AESRX, 0);
		adma = 1;
	}
	if (chip->pipes[RAD1_AESTX].adma) {
		snd_rad1_poll_pipe(chip, RAD1_AESTX, 1);
		adma = 1;
	}

	if (adma) {
		chip->timer.expires = (jiffies + 1);
		add_timer(&chip->timer);
	} else
		chip->timer_active = 0;
}

static int snd_rad1_free_pipe(struct snd_pcm_substream *substream, int pidx)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	rad1_pipe_t *pipe = (chip->pipes + pidx);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	snd_rad1_run_pipe(chip, pidx, 0);
	pipe->subs = NULL;
	spin_unlock_irqrestore(&chip->lock, flags);
	return snd_pcm_lib_free_pages(substream);
}

static long snd_rad1_gcd(long x, long y)
{
	long t;
	if (x < y) {
		t = x;
		x = y;
		y = t;
	}

	while (y) {
		y = x % (t = y);
		x = t;
	}

	return x;
}

static void snd_rad1_red_frac(long *n, long *d, long max)
{
	long gcd = snd_rad1_gcd(*n, *d);
	if (!gcd)
		return;

	*n /= gcd;
	*d /= gcd;

	/* lose precision */
	while (*n > max || *d > max) {
		*n >>= 1;
		*d >>= 1;
	}
}

static void snd_rad1_set_cg(rad1_t *chip, int cg, long rate, long base, unsigned muxsel)
{
	long div, rem;
	unsigned flags;

	snd_rad1_red_frac(&base, &rate, 0xffff);
	div = (base / rate);
	rem = (base % rate);
	snd_rad1_red_frac(&rem, &rate, 0xffff);
	flags = ((rem * 2) < rate) ? 0x600 : 0x200;

	chip->mmio->reset = 0xffe20000 | (0x100 << cg);
	chip->mmio->freq_synth_mux_sel[3 - cg] = muxsel;
	chip->mmio->clockgen_rem = (rem << 16) | (0x10000 - rate);
	chip->mmio->clockgen_ictl = flags | (0x10000000 << cg) | (div - 1);
	chip->mmio->reset = 0xffe20000;
}

/* select best master clock source for low jitter */
static void snd_rad1_set_cgms(rad1_t *chip, int cg, long rate)
{
	if (!(176400 % rate))
		snd_rad1_set_cg(chip, cg, rate, 176400, 0);
	else
		snd_rad1_set_cg(chip, cg, rate, 192000, 1);
}

static void snd_rad1_set_aestx_subcode(rad1_t *chip, unsigned char *sub_lc, unsigned char *sub_rc)
{
	unsigned int i, j, lc[6], rc[6];

	for (i = 0; i < 6; i++) {
		lc[i] = rc[i] = 0;
		for (j = 0; j < 4; j++) {
			lc[i] |= sub_lc[i * 4 + j] << (j << 3);
			rc[i] |= sub_rc[i * 4 + j] << (j << 3);
		}
	}

	for (i = 0; i < 6; i++) {
		chip->mmio->aes_subcode_txa_lu[i] = 0x00000000;
		chip->mmio->aes_subcode_txa_lc[i] = lc[i];
		chip->mmio->aes_subcode_txa_lv[i] = 0x00000000;
		chip->mmio->aes_subcode_txa_ru[i] = 0x00000000;
		chip->mmio->aes_subcode_txa_rc[i] = rc[i];
		chip->mmio->aes_subcode_txb_lu[i] = 0x00000000;
		chip->mmio->aes_subcode_txb_lc[i] = lc[i];
		chip->mmio->aes_subcode_txb_lv[i] = 0x00000000;
		chip->mmio->aes_subcode_txb_ru[i] = 0x00000000;
		chip->mmio->aes_subcode_txb_rc[i] = rc[i];
	}

	for (i = 0; i < 2; i++) {
		chip->mmio->aes_subcode_txa_rv0[i] = 0x00000000;
		chip->mmio->aes_subcode_txb_rv0[i] = 0x00000000;
	}

	for (i = 0; i < 4; i++) {
		chip->mmio->aes_subcode_txa_rv2[i] = 0x00000000;
		chip->mmio->aes_subcode_txb_rv2[i] = 0x00000000;
	}
}

static void snd_rad1_genset_aestx_subcode(rad1_t *chip, int rate)
{
	unsigned char lc[24], rc[24];
	int i;
	for (i = 0; i < 24; i++)
		lc[i] = rc[i] = 0x00;
	lc[0] = rc[0] = 0x04;	/* PRO=0, !AUDIO=0, COPY=1, PRE=000, MODE=00 */
	lc[1] = rc[1] = 0x01;	/* Laser Optical, CD IEC-908 */
	lc[2] = 0x10;		/* SOURCE=0000, CHANNEL=0001 */
	rc[2] = 0x20;		/* SOURCE=0000, CHANNEL=0010 */

	/* RAD1 systems have generally decent clock sources, so we mark them Level I */
	switch (rate) {
	case 32000:
		lc[3] = rc[3] = 0x0C;	/* Level I, 32 kHz */
		break;
	case 44100:
		lc[3] = rc[3] = 0x00;	/* Level I, 44.1 kHz */
		break;
	case 48000:
		lc[3] = rc[3] = 0x04;	/* Level I, 48 kHz */
		break;
	default:
		/* not a valid IEC-958 sample rate */
		lc[3] = rc[3] = 0x10;	/* Level III, 44.1 kHz */
	}
	snd_rad1_set_aestx_subcode(chip, lc, rc);
}

static void snd_rad1_setrate_pipe(rad1_t *chip, int pidx, int rate)
{
	if (pidx == RAD1_ATOD)
		snd_rad1_set_cgms(chip, 0, rate);
	if (pidx == RAD1_DTOA)
		snd_rad1_set_cgms(chip, 1, rate);
	if (pidx == RAD1_AESTX) {
		snd_rad1_set_cgms(chip, 2, rate);
		snd_rad1_genset_aestx_subcode(chip, rate);
	}
}

static int snd_rad1_prepare_pipe(struct snd_pcm_substream *substream, int pidx)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	rad1_pipe_t *pipe = (chip->pipes + pidx);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	snd_rad1_run_pipe(chip, pidx, 0);
	spin_unlock_irqrestore(&chip->lock, flags);

	pipe->subs = substream;
	pipe->vma = (int *)runtime->dma_area;
	pipe->pma = runtime->dma_addr;
	pipe->pnum = runtime->periods;
	pipe->plen = frames_to_bytes(runtime, runtime->period_size);

	snd_rad1_setrate_pipe(chip, pidx, runtime->rate);

	pipe->hptr = 0;
	pipe->qptr = 0;
	snd_rad1_setup_dma_pipe(chip, pidx);

	return 0;
}

static void snd_rad1_detect_aesrx_rate(rad1_t *chip, struct snd_pcm_hardware *hw)
{
	int rate;
	unsigned sc = ((chip->mmio->chip_status0) >> 24) & 7;
	static int rates[8] = {0, 48000, 44100, 32000, 48000, 44100, 44056, 32000};

	if (!rates[sc]) {
		printk(KERN_INFO "Warning: Recording from an unlocked IEC958 source.\n");
		printk(KERN_INFO "         Assuming sample rate: %d.\n", chip->last_aesrx_rate);
		rate = chip->last_aesrx_rate;
	} else
		rate = rates[sc];

	chip->last_aesrx_rate = rate;
	hw->rate_min = hw->rate_max = rate;

	switch (rate) {
	case 32000:
		hw->rates = SNDRV_PCM_RATE_32000;
		break;
	case 44056:
		hw->rates = SNDRV_PCM_RATE_CONTINUOUS;
		break;
	case 48000:
		hw->rates = SNDRV_PCM_RATE_48000;
	case 44100:
	default:
		hw->rates = SNDRV_PCM_RATE_44100;
		break;
	}
}

static int snd_rad1_trigger_pipe(struct snd_pcm_substream *substream, int pidx, int cmd)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	int result = 0;

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
		snd_rad1_run_pipe(chip, pidx, 1);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		snd_rad1_run_pipe(chip, pidx, 0);
		break;
	default:
		result = -EINVAL;
	}
	return result;
}

static snd_pcm_uframes_t snd_rad1_pointer_pipe(struct snd_pcm_substream *substream, int pidx)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	return chip->pipes[pidx].qptr;
}

/* ATOD pipe */
static struct snd_pcm_hardware snd_rad1_atod_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S24_BE,
	.rates = SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_32000 |
		 SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 1048576,
	.period_bytes_min = 4096,
	.period_bytes_max = 4096,
	.periods_min = 4,
	.periods_max = 256,
};

static int snd_rad1_atod_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	runtime->hw = snd_rad1_atod_hw;
	return 0;
}

static int snd_rad1_atod_close(struct snd_pcm_substream *substream)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	snd_rad1_run_pipe(chip, RAD1_ATOD, 0);
	return 0;
}

static int snd_rad1_atod_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_rad1_atod_free(struct snd_pcm_substream *substream)
{
	return snd_rad1_free_pipe(substream, RAD1_ATOD);
}

static int snd_rad1_atod_prepare(struct snd_pcm_substream *substream)
{
	return snd_rad1_prepare_pipe(substream, RAD1_ATOD);
}

static int snd_rad1_atod_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return snd_rad1_trigger_pipe(substream, RAD1_ATOD, cmd);
}

static snd_pcm_uframes_t snd_rad1_atod_pointer(struct snd_pcm_substream *substream)
{
	return snd_rad1_pointer_pipe(substream, RAD1_ATOD);
}

static struct snd_pcm_ops snd_rad1_atod_ops = {
	.open = snd_rad1_atod_open,
	.close = snd_rad1_atod_close,
	.hw_params = snd_rad1_atod_params,
	.hw_free = snd_rad1_atod_free,
	.prepare = snd_rad1_atod_prepare,
	.trigger = snd_rad1_atod_trigger,
	.pointer = snd_rad1_atod_pointer,
	.ioctl = snd_pcm_lib_ioctl,
};

static void snd_rad1_atod_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int __devinit snd_rad1_new_atod(rad1_t *chip, int dev)
{
	struct snd_pcm *pcm;
	int err;

	if ((err = snd_pcm_new(chip->card, "RAD1 AtoD", dev, 0, 1, &pcm)) < 0)
		return err;

	pcm->private_data = chip;
	pcm->private_free = snd_rad1_atod_pcm_free;
	strcpy(pcm->name, "RAD1 AtoD");
	chip->pipes[RAD1_ATOD].pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_rad1_atod_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(chip->pci),
					      65536, 65536);

	return 0;
}

/* DTOA pipe */
static struct snd_pcm_hardware snd_rad1_dtoa_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S24_BE,
	.rates = SNDRV_PCM_RATE_CONTINUOUS | SNDRV_PCM_RATE_32000 |
		 SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 1048576,
	.period_bytes_min = 4096,
	.period_bytes_max = 4096,
	.periods_min = 4,
	.periods_max = 256,
};

static int snd_rad1_dtoa_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	runtime->hw = snd_rad1_dtoa_hw;
	return 0;
}

static int snd_rad1_dtoa_close(struct snd_pcm_substream *substream)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	snd_rad1_run_pipe(chip, RAD1_DTOA, 0);
	return 0;
}

static int snd_rad1_dtoa_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_rad1_dtoa_free(struct snd_pcm_substream *substream)
{
	return snd_rad1_free_pipe(substream, RAD1_DTOA);
}

static int snd_rad1_dtoa_prepare(struct snd_pcm_substream *substream)
{
	return snd_rad1_prepare_pipe(substream, RAD1_DTOA);
}

static int snd_rad1_dtoa_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return snd_rad1_trigger_pipe(substream, RAD1_DTOA, cmd);
}

static snd_pcm_uframes_t snd_rad1_dtoa_pointer(struct snd_pcm_substream *substream)
{
	return snd_rad1_pointer_pipe(substream, RAD1_DTOA);
}

static struct snd_pcm_ops snd_rad1_dtoa_ops = {
	.open = snd_rad1_dtoa_open,
	.close = snd_rad1_dtoa_close,
	.hw_params = snd_rad1_dtoa_params,
	.hw_free = snd_rad1_dtoa_free,
	.prepare = snd_rad1_dtoa_prepare,
	.trigger = snd_rad1_dtoa_trigger,
	.pointer = snd_rad1_dtoa_pointer,
	.ioctl = snd_pcm_lib_ioctl,
};

static void snd_rad1_dtoa_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int __devinit snd_rad1_new_dtoa(rad1_t *chip, int dev)
{
	struct snd_pcm *pcm;
	int err;

	if ((err = snd_pcm_new(chip->card, "RAD1 DtoA", dev, 1, 0, &pcm)) < 0)
		return err;

	pcm->private_data = chip;
	pcm->private_free = snd_rad1_dtoa_pcm_free;
	strcpy(pcm->name, "RAD1 DtoA");
	chip->pipes[RAD1_DTOA].pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_rad1_dtoa_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(chip->pci),
					      65536, 65536);

	return 0;
}

/* AESRX pipe */
static struct snd_pcm_hardware snd_rad1_aesrx_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S24_BE,
	.rates = SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000 | SNDRV_PCM_RATE_32000,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 1048576,
	.period_bytes_min = 4096,
	.period_bytes_max = 4096,
	.periods_min = 4,
	.periods_max = 256,
};

static int snd_rad1_aesrx_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	rad1_t *chip = snd_pcm_substream_chip(substream);
	runtime->hw = snd_rad1_aesrx_hw;
	snd_rad1_detect_aesrx_rate(chip, &runtime->hw);
	return 0;
}

static int snd_rad1_aesrx_close(struct snd_pcm_substream *substream)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	snd_rad1_run_pipe(chip, RAD1_AESRX, 0);
	return 0;
}

static int snd_rad1_aesrx_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_rad1_aesrx_free(struct snd_pcm_substream *substream)
{
	return snd_rad1_free_pipe(substream, RAD1_AESRX);
}

static int snd_rad1_aesrx_prepare(struct snd_pcm_substream *substream)
{
	return snd_rad1_prepare_pipe(substream, RAD1_AESRX);
}

static int snd_rad1_aesrx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return snd_rad1_trigger_pipe(substream, RAD1_AESRX, cmd);
}

static snd_pcm_uframes_t snd_rad1_aesrx_pointer(struct snd_pcm_substream *substream)
{
	return snd_rad1_pointer_pipe(substream, RAD1_AESRX);
}

static struct snd_pcm_ops snd_rad1_aesrx_ops = {
	.open = snd_rad1_aesrx_open,
	.close = snd_rad1_aesrx_close,
	.hw_params = snd_rad1_aesrx_params,
	.hw_free = snd_rad1_aesrx_free,
	.prepare = snd_rad1_aesrx_prepare,
	.trigger = snd_rad1_aesrx_trigger,
	.pointer = snd_rad1_aesrx_pointer,
	.ioctl = snd_pcm_lib_ioctl,
};

static void snd_rad1_aesrx_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int __devinit snd_rad1_new_aesrx(rad1_t *chip, int dev)
{
	struct snd_pcm *pcm;
	int err;

	if ((err = snd_pcm_new(chip->card, "RAD1 AES Rx", dev, 0, 1, &pcm)) < 0)
		return err;

	pcm->private_data = chip;
	pcm->private_free = snd_rad1_aesrx_pcm_free;
	strcpy(pcm->name, "RAD1 AES Rx");
	chip->pipes[RAD1_AESRX].pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &snd_rad1_aesrx_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(chip->pci),
					      65536, 65536);
	chip->last_aesrx_rate = 44100;

	return 0;
}

/* AESTX pipe */
static struct snd_pcm_hardware snd_rad1_aestx_hw = {
	.info = (SNDRV_PCM_INFO_INTERLEAVED | SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_MMAP_VALID | SNDRV_PCM_INFO_BLOCK_TRANSFER),
	.formats = SNDRV_PCM_FMTBIT_S24_BE,
	.rates = SNDRV_PCM_RATE_32000 | SNDRV_PCM_RATE_44100 | SNDRV_PCM_RATE_48000,
	.rate_min = 32000,
	.rate_max = 48000,
	.channels_min = 2,
	.channels_max = 2,
	.buffer_bytes_max = 1048576,
	.period_bytes_min = 4096,
	.period_bytes_max = 4096,
	.periods_min = 4,
	.periods_max = 256,
};

static int snd_rad1_aestx_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	runtime->hw = snd_rad1_aestx_hw;
	return 0;
}

static int snd_rad1_aestx_close(struct snd_pcm_substream *substream)
{
	rad1_t *chip = snd_pcm_substream_chip(substream);
	snd_rad1_run_pipe(chip, RAD1_AESTX, 0);
	return 0;
}

static int snd_rad1_aestx_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, params_buffer_bytes(hw_params));
}

static int snd_rad1_aestx_free(struct snd_pcm_substream *substream)
{
	return snd_rad1_free_pipe(substream, RAD1_AESTX);
}

static int snd_rad1_aestx_prepare(struct snd_pcm_substream *substream)
{
	return snd_rad1_prepare_pipe(substream, RAD1_AESTX);
}

static int snd_rad1_aestx_trigger(struct snd_pcm_substream *substream, int cmd)
{
	return snd_rad1_trigger_pipe(substream, RAD1_AESTX, cmd);
}

static snd_pcm_uframes_t snd_rad1_aestx_pointer(struct snd_pcm_substream *substream)
{
	return snd_rad1_pointer_pipe(substream, RAD1_AESTX);
}

static struct snd_pcm_ops snd_rad1_aestx_ops = {
	.open = snd_rad1_aestx_open,
	.close = snd_rad1_aestx_close,
	.hw_params = snd_rad1_aestx_params,
	.hw_free = snd_rad1_aestx_free,
	.prepare = snd_rad1_aestx_prepare,
	.trigger = snd_rad1_aestx_trigger,
	.pointer = snd_rad1_aestx_pointer,
	.ioctl = snd_pcm_lib_ioctl,
};

static void snd_rad1_aestx_pcm_free(struct snd_pcm *pcm)
{
	snd_pcm_lib_preallocate_free_for_all(pcm);
}

static int __devinit snd_rad1_new_aestx(rad1_t *chip, int dev)
{
	struct snd_pcm *pcm;
	int err;

	if ((err = snd_pcm_new(chip->card, "RAD1 AES Tx", dev, 1, 0, &pcm)) < 0)
		return err;

	pcm->private_data = chip;
	pcm->private_free = snd_rad1_aestx_pcm_free;
	strcpy(pcm->name, "RAD1 AES Tx");
	chip->pipes[RAD1_AESTX].pcm = pcm;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &snd_rad1_aestx_ops);
	snd_pcm_lib_preallocate_pages_for_all(pcm, SNDRV_DMA_TYPE_DEV,
					      snd_dma_pci_data(chip->pci),
					      65536, 65536);

	return 0;
}

/* Volume control */
static int snd_rad1_control_pv_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 255;
	return 0;
}

static int snd_rad1_control_pv_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int shift=kcontrol->private_value * 16;

	spin_lock_irqsave(&chip->lock, flags);
	u->value.integer.value[0] = (chip->attctrl >> shift) & 0xff;
	u->value.integer.value[1] = (chip->attctrl >> (8 + shift)) & 0xff;
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int snd_rad1_control_pv_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int change = 0, shift = kcontrol->private_value * 16;

	spin_lock_irqsave(&chip->lock, flags);
	if (u->value.integer.value[0] != ((chip->attctrl >> shift) & 0xff))
		change = 1;

	if (u->value.integer.value[1] != ((chip->attctrl >> (8 + shift)) & 0xff))
		change = 1;

	if (change) {
		chip->attctrl &= 0xffff << (16 - shift);
		chip->attctrl |= u->value.integer.value[0] << shift;
		chip->attctrl |= u->value.integer.value[1] << (8 + shift);
		chip->mmio->volume_control = chip->attctrl;
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	return change;
}

/* AES Tx route control */
static int snd_rad1_control_tr_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	static char *rts[2] = {"Optical", "Coaxial"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, rts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_rad1_control_tr_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	if (chip->rt_opto == 0x00001440)
		u->value.enumerated.item[0] = 1;
	else
		u->value.enumerated.item[0] = 0;
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int snd_rad1_control_tr_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int change = 0;

	spin_lock_irqsave(&chip->lock, flags);
	if (u->value.enumerated.item[0] && chip->rt_opto != 0x00001440)
		change = 1;
	if (!u->value.enumerated.item[0] && chip->rt_opto == 0x00001440)
		change = 1;
	if (change) {
		if (u->value.enumerated.item[0])
			chip->rt_opto = 0x00001440;
		else
			chip->rt_opto = 0x00001040;
		chip->mmio->misc_control = chip->rt_opto;
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	return change;
}

/* AES Rx route control */

static int snd_rad1_control_rr_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	if (chip->rt_aesr == 0x00000010)
		u->value.enumerated.item[0] = 1;
	else
		u->value.enumerated.item[0] = 0;
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int snd_rad1_control_rr_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int change = 0;

	spin_lock_irqsave(&chip->lock, flags);
	if (u->value.enumerated.item[0] && chip->rt_aesr != 0x00000010)
		change = 1;
	if (!u->value.enumerated.item[0] && chip->rt_aesr == 0x00000010)
		change = 1;
	if (change) {
		if (u->value.enumerated.item[0]) {
			chip->rt_aesr = 0x00000010;
			chip->rt_adat = 0xa0000018;
		} else {
			chip->rt_aesr = 0x00000018;
			chip->rt_adat = 0xa0000000;
		}
		chip->mmio->aes_rx_control = (chip->rt_aesr | chip->pipes[RAD1_AESRX].adma);
		chip->mmio->adat_rx_control = (chip->rt_adat | chip->pipes[RAD1_ADATRX].adma);
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	return change;
}


/* AtoD route control */
static int snd_rad1_control_ar_info(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info * uinfo)
{
	static char *rts[2] = {"Mic", "Line"};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = 2;
	if (uinfo->value.enumerated.item > 1)
		uinfo->value.enumerated.item = 1;
	strcpy(uinfo->value.enumerated.name, rts[uinfo->value.enumerated.item]);
	return 0;
}

static int snd_rad1_control_ar_get(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;

	spin_lock_irqsave(&chip->lock, flags);
	if (chip->rt_atod == 0x05000000)
		u->value.enumerated.item[0] = 1;
	else
		u->value.enumerated.item[0] = 0;
	spin_unlock_irqrestore(&chip->lock, flags);

	return 0;
}

static int snd_rad1_control_ar_put(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *u)
{
	rad1_t *chip = snd_kcontrol_chip(kcontrol);
	unsigned long flags;
	int change = 0;

	spin_lock_irqsave(&chip->lock, flags);
	if (u->value.enumerated.item[0] && chip->rt_atod != 0x05000000)
		change = 1;
	if (!u->value.enumerated.item[0] && chip->rt_atod == 0x05000000)
		change = 1;
	if (change) {
		if (u->value.enumerated.item[0])
			chip->rt_atod = 0x05000000;
		else
			chip->rt_atod = 0x03000000;
		chip->mmio->atod_control = (chip->rt_atod | chip->pipes[RAD1_ATOD].adma);
	}
	spin_unlock_irqrestore(&chip->lock, flags);

	return change;
}

static struct snd_kcontrol_new snd_rad1_controls[] = {
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Master Playback Volume",
	.info =		snd_rad1_control_pv_info,
	.get =		snd_rad1_control_pv_get,
	.put =		snd_rad1_control_pv_put,
	.private_value = 1
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Line Capture Volume",
	.info =		snd_rad1_control_pv_info,
	.get =		snd_rad1_control_pv_get,
	.put =		snd_rad1_control_pv_put,
	.private_value = 0
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"IEC958 Playback Routing",
	.info =		snd_rad1_control_tr_info,
	.get =		snd_rad1_control_tr_get,
	.put =		snd_rad1_control_tr_put
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"IEC958 Capture Routing",
	.info =		snd_rad1_control_tr_info, /* clone */
	.get =		snd_rad1_control_rr_get,
	.put =		snd_rad1_control_rr_put
},
{
	.iface =	SNDRV_CTL_ELEM_IFACE_MIXER,
	.name =		"Line Capture Routing",
	.info =		snd_rad1_control_ar_info,
	.get =		snd_rad1_control_ar_get,
	.put =		snd_rad1_control_ar_put
},
};

static int snd_rad1_add_controls(rad1_t *chip)
{
	int idx, err;

	for (idx = 0; idx < 5; idx++)
		if ((err = snd_ctl_add(chip->card, snd_ctl_new1(&snd_rad1_controls[idx], chip))) < 0)
			return err;
	return 0;
}

static int index[SNDRV_CARDS] = SNDRV_DEFAULT_IDX;
static char *id[SNDRV_CARDS] = SNDRV_DEFAULT_STR;
static int enable[SNDRV_CARDS] = SNDRV_DEFAULT_ENABLE_PNP;
static int ndev;

static int snd_rad1_free(rad1_t *chip)
{
	if (chip->mmio) {
		iounmap((void *)(chip->mmio));
		chip->mmio = NULL;
	}
	pci_release_regions(chip->pci);
	kfree(chip);
	return 0;
}

static int snd_rad1_dev_free(struct snd_device *device)
{
	rad1_t *chip = device->device_data;
	return snd_rad1_free(chip);
}

static int __devinit snd_rad1_create(struct snd_card *card, struct pci_dev *pci, rad1_t **rchip)
{
	rad1_t *chip;
	int err;
	static struct snd_device_ops ops = {
		.dev_free = snd_rad1_dev_free,
	};

	*rchip = NULL;

	if ((err = pci_enable_device(pci)) < 0)
		return err;

	chip = kcalloc(sizeof(rad1_t), 1, GFP_KERNEL);
	if (chip == NULL)
		return -ENOMEM;

	init_timer(&chip->timer);
	chip->timer.function = snd_rad1_poll_timer;
	chip->timer.data = (unsigned long)chip;

	chip->card = card;
	chip->pci = pci;

	spin_lock_init(&chip->lock);

	pci_set_master(pci);

	if ((err = pci_request_regions(pci, "RAD1")) < 0) {
		kfree(chip);
		return err;
	}

	chip->mmio_phys = pci_resource_start(pci, 0);
	chip->mmio = ioremap_nocache(chip->mmio_phys, pci_resource_len(pci, 0));

	snd_rad1_hw_init(chip);

	if ((err = snd_device_new(card, SNDRV_DEV_LOWLEVEL, chip, &ops)) < 0) {
		snd_rad1_free(chip);
		return err;
	}
	*rchip = chip;
	return 0;
}

static struct pci_device_id snd_rad1_ids[] = {
	{ PCI_VENDOR_ID_SGI, PCI_DEVICE_ID_SGI_RAD1, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0, },
	{ 0, },
};

static int __devinit snd_rad1_probe(struct pci_dev *pci, const struct pci_device_id *pci_id)
{
	struct snd_card *card;
	rad1_t *chip;
	int err;

	if (ndev >= SNDRV_CARDS)
		return -ENODEV;
	if (!enable[ndev]) {
		ndev++;
		return -ENOENT;
	}

	card = snd_card_new(index[ndev], id[ndev], THIS_MODULE, 0);
	if (card == NULL)
		return -ENOMEM;

	if ((err = snd_rad1_create(card, pci, &chip)) < 0) {
		snd_card_free(card);
		return err;
	}

	strcpy(card->driver, "RAD1");
	strcpy(card->shortname, "RADAudio");
	sprintf(card->longname, "SGI RAD Audio at 0x%lx", chip->mmio_phys);

	/* create pipes */
	snd_rad1_new_dtoa(chip, 0);
	snd_rad1_new_atod(chip, 1);
	snd_rad1_new_aestx(chip, 2);
	snd_rad1_new_aesrx(chip, 3);
	snd_rad1_add_controls(chip);

	if ((err = snd_card_register(card)) < 0) {
		snd_card_free(card);
		return err;
	}

	pci_set_drvdata(pci, card);
	ndev++;
	return 0;
}

static void __devexit snd_rad1_remove(struct pci_dev *pci)
{
	snd_card_free(pci_get_drvdata(pci));
	pci_set_drvdata(pci, NULL);
}

MODULE_DEVICE_TABLE(pci, snd_rad1_ids);

static struct pci_driver driver = {
	.name = "SGI RAD1",
	.id_table = snd_rad1_ids,
	.probe = snd_rad1_probe,
	.remove = __devexit_p(snd_rad1_remove),
};

static int __init alsa_card_rad1_init(void)
{
	return pci_register_driver(&driver);
}

static void __exit alsa_card_rad1_exit(void)
{
	pci_unregister_driver(&driver);
}

MODULE_AUTHOR("Stanislaw Skowronek <skylark@linux-mips.org>");
MODULE_DESCRIPTION("SGI Octane (IP30) RAD1 Alsa Audio Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("R28");

module_init(alsa_card_rad1_init)
module_exit(alsa_card_rad1_exit)
