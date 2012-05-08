/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2004-2007 Stanislaw Skowronek <skylark@linux-mips.org>
 */

#ifndef _SOUND_RAD1_H
#define _SOUND_RAD1_H

#include <linux/types.h>

#define RAD1_ADATRX	0
#define RAD1_AESRX	1
#define RAD1_ATOD	2
#define RAD1_ADATSUBRX	3
#define RAD1_AESSUBRX	4
#define RAD1_ADATTX	5
#define RAD1_AESTX	6
#define RAD1_DTOA	7
#define RAD1_STATUS	8

struct rad1regs {
	u32 pci_status;			/* 0x00000000 */
	u32 adat_rx_msc_ust; 		/* 0x00000004 */
	u32 adat_rx_msc0_submsc; 	/* 0x00000008 */
	u32 aes_rx_msc_ust;	 	/* 0x0000000c */
	u32 aes_rx_msc0_submsc; 	/* 0x00000010 */
	u32 atod_msc_ust;		/* 0x00000014 */
	u32 atod_msc0_submsc; 		/* 0x00000018 */
	u32 adat_tx_msc_ust; 		/* 0x0000001c */
	u32 adat_tx_msc0_submsc; 	/* 0x00000020 */
	u32 aes_tx_msc_ust; 		/* 0x00000024 */
	u32 aes_tx_msc0_submsc; 	/* 0x00000028 */
	u32 dtoa_msc_ust; 		/* 0x0000002c */
	u32 ust_register; 		/* 0x00000030 */
	u32 gpio_status;		/* 0x00000034 */
	u32 chip_status1; 		/* 0x00000038 */
	u32 chip_status0;		/* 0x0000003c */

	u32 ust_clock_control; 		/* 0x00000040 */
	u32 adat_rx_control; 		/* 0x00000044 */
	u32 aes_rx_control;	 	/* 0x00000048 */
	u32 atod_control; 		/* 0x0000004c */
	u32 adat_tx_control; 		/* 0x00000050 */
	u32 aes_tx_control; 		/* 0x00000054 */
	u32 dtoa_control; 		/* 0x00000058 */
	u32 status_timer; 		/* 0x0000005c */

	u32 _pad70[4];

	u32 misc_control; 		/* 0x00000070 */
	u32 pci_holdoff;		/* 0x00000074 */
	u32 pci_arb_control; 		/* 0x00000078 */

	u32 volume_control; 		/* 0x0000007c */

	u32 reset;			/* 0x00000080 */

	u32 gpio0;			/* 0x00000084 */
	u32 gpio1;			/* 0x00000088 */
	u32 gpio2;			/* 0x0000008c */
	u32 gpio3;			/* 0x00000090 */

	u32 _pada0[3];

	u32 clockgen_ictl; 		/* 0x000000a0 */
	u32 clockgen_rem;	 	/* 0x000000a4 */
	u32 freq_synth_mux_sel[4]; 	/* 0x000000a8 */
	u32 mpll0_lock_control;		/* 0x000000b8 */
	u32 mpll1_lock_control;	 	/* 0x000000bc */

	u32 _pad400[208];

	/* descriptor RAM */
	struct {
		u32 loadr; 		/* 0x00000400 + 12*idx */
		u32 hiadr; 		/* 0x00000404 + 12*idx */
		u32 control;		/* 0x00000408 + 12*idx */
	} pci_descr[16];

	/* running descriptors */
	struct {
		u32 loadr;
		u32 control;
	} pci_lc[9];
	u32 pci_hiadr[9];

	u32 _pad1000[693];

	u32 adat_subcode_txa_u[24];	/* 0x00001000 */
	u32 adat_subcode_txa_unused;	/* 0x00001060 */

	u32 _pad1080[7];

	u32 adat_subcode_txb_u[24];	/* 0x00001080 */
	u32 adat_subcode_txb_unused;	/* 0x000010e0 */

	u32 _pad1100[7];

	u32 aes_subcode_txa_lu[6];	/* 0x00001100 */
	u32 aes_subcode_txa_lc[6];	/* 0x00001118 */
	u32 aes_subcode_txa_lv[6];	/* 0x00001130 */
	u32 aes_subcode_txa_ru[6];	/* 0x00001148 */
	u32 aes_subcode_txa_rc[6];	/* 0x00001160 */
	u32 aes_subcode_txa_rv0[2];	/* 0x00001178 */
	u32 aes_subcode_txb_lu[6];	/* 0x00001180 */
	u32 aes_subcode_txb_lc[6];	/* 0x00001198 */
	u32 aes_subcode_txb_lv[6];	/* 0x000011b0 */
	u32 aes_subcode_txb_ru[6];	/* 0x000011c8 */
	u32 aes_subcode_txb_rc[6];	/* 0x000011e0 */
	u32 aes_subcode_txb_rv0[2];	/* 0x000011f8 */
	u32 aes_subcode_txa_rv2[4];	/* 0x00001200 */
	u32 aes_subcode_txb_rv2[4];	/* 0x00001210 */
	u32 aes_subcode_tx_unused; 	/* 0x00001220 */
};

#endif /* _SOUND_RAD1_H */
