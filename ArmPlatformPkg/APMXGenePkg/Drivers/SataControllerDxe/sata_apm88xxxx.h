/*
 * sata_apm88xxxx.h - AppliedMicro X-Gene SATA PHY driver
 *
 * Copyright (c) 2013, Applied Micro Circuits Corporation
 * Author: Loc Ho <lho@apm.com>
 *         Tuan Phan <tphan@apm.com>
 *         Suman Tripathi <stripathi@apm.com>
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 */
#ifndef __SATA_APM88XXXX_H__
#define __SATA_APM88XXXX_H__

#define XGENE_SERDES_VAL_NOT_SET	~0x0
#define CTLE_EQ 			0x9
#define PQ_REG  			0x8
#define CTLE_EQ_A2 			0x2
#define PQ_REG_A2  			0xa
#define SPD_SEL 			0x5

/*
 * Configure Reference clock (clock type):
 *  External differential 0
 *  Internal differential 1
 *  Internal single ended 2
 */
#define SATA_CLK_EXT_DIFF		0
#define SATA_CLK_INT_DIFF		1
#define SATA_CLK_INT_SING		2

struct xgene_sata_context {
	u8 cid;			/* Controller ID */
	int irq;		/* IRQ */
	void *csr_base;		/* CSR base address of IP - serdes */
	void *mmio_base;	/* AHCI I/O base address */
	void *pcie_base;	/* Shared Serdes CSR in PCIe 4/5 domain */
	u64 csr_phys;		/* Physical address of CSR base address */
	u64 mmio_phys;		/* Physical address of MMIO base address */
	u64 pcie_phys;		/* Physical address of PCIE base address */

	/* Override Serdes parameters */
	u32 ctrl_eq_A1; /* Serdes Reg 1 RX/TX ctrl_eq value for A1 */
	u32 ctrl_eq;	/* Serdes Reg 1 RX/TX ctrl_eq value */
	u32 pq_A1; 	/* Serdes Reg 125 pq value for A1 */
	u32 pq;	   	/* Serdes Reg 125 pq value */
	u32 pq_sign;	/* Serdes Reg 125 pq sign */
	u32 loopback_buf_en_A1; /* Serdes Reg 4 Tx loopback buffer enable for A1 */
	u32 loopback_buf_en;    /* Serdes Reg 4 Tx loopback buffer enable */
	u32 loopback_ena_ctle_A1; /* Serdes Reg 7 loopback enable ctrl value for A1 */
	u32 loopback_ena_ctle;    /* Serdes Reg 7 loopback enable ctrl value */
	u32 spd_sel_cdr_A1;	/* Serdes Reg 61 spd sel cdr value for A1*/
	u32 spd_sel_cdr;	/* Serdes Reg 61 spd sel cdr value */
	u32 use_gen_avg;	/* Use generate average value */
};

int xgene_sata_in32(void *addr, u32 *val);
int xgene_sata_out32(void *addr, u32 val);
int xgene_sata_out32_flush(void *addr, u32 val);
int xgene_sata_serdes_init(struct xgene_sata_context *ctx,
	int gen_sel, int clk_type, int rxwclk_inv);
void xgene_sata_gen_avg_val(struct xgene_sata_context *ctx, int channel);
void xgene_sata_force_lat_summer_cal(struct xgene_sata_context *ctx,
	int channel);
void xgene_sata_serdes_reset_rxa_rxd(struct xgene_sata_context *ctx,
	int channel);
void xgene_sata_force_gen2(struct xgene_sata_context *ctx, int channel);
void xgene_sata_force_gen1(struct xgene_sata_context *ctx, int channel);
void xgene_sata_delayus(unsigned long us);
void xgene_sata_delayms(unsigned long ms);
void xgene_change_serdes_val(struct xgene_sata_context * ctx , int channel , int data);
char *apm88xxx_chip_revision(void);
#endif
