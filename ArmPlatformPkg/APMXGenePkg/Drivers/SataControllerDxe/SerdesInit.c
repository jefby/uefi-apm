/**
 * This driver module produces IDE_CONTROLLER_INIT protocol for Sata Controllers.
 *
 * Copyright (c) 2013, AppliedMicro Corp. All rights reserved.
 *
 * This program and the accompanying materials
 * are licensed and made available under the terms and conditions of the BSD License
 * which accompanies this distribution.  The full text of the license may be found at
 * http://opensource.org/licenses/bsd-license.php
 *
 * THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
 * WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
 *
 * Integration Note:
 *  Merged as of 10/17/2013
 **/
#include "SataController.h"
#include "sata_apm88xxxx.h"

/* Max # of disk per a controller */
#define MAX_AHCI_CHN_PERCTR	 	2

#define SATA_CLK_OFFSET			0x0000D000
#define SATA_DIAG_OFFSET		0x0000D000
#define SATA_GLB_OFFSET			0x0000D850
#define SATA_SHIM_OFFSET		0x0000E000
#define SATA_MASTER_OFFSET		0x0000F000
#define SATA_PORT0_OFFSET		0x00000100
#define SATA_PORT1_OFFSET		0x00000180

#define HOST_IRQ_STAT			0x08	/* interrupt status */

/* SATA host controller CSR */
#define SLVRDERRATTRIBUTES_ADDR                                      0x00000000
#define SLVWRERRATTRIBUTES_ADDR                                      0x00000004
#define MSTRDERRATTRIBUTES_ADDR                                      0x00000008
#define MSTWRERRATTRIBUTES_ADDR                                      0x0000000c
#define BUSCTLREG_ADDR                                               0x00000014
#define  MSTAWAUX_COHERENT_BYPASS_SET(dst,src) \
		(((dst) & ~0x00000002) | (((u32)(src)<<1) & 0x00000002))
#define  MSTARAUX_COHERENT_BYPASS_SET(dst,src) \
		(((dst) & ~0x00000001) | (((u32)(src)) & 0x00000001))
#define IOFMSTRWAUX_ADDR                                             0x00000018
#define INTSTATUSMASK_ADDR                                           0x0000002c
#define ERRINTSTATUS_ADDR                                            0x00000030
#define ERRINTSTATUSMASK_ADDR                                        0x00000034

/* SATA host AHCI CSR */
#define PORTCFG_ADDR                                                 0x000000a4
#define  PORTADDR_SET(dst,src) \
		(((dst) & ~0x0000003f) | (((u32)(src)) & 0x0000003f))
#define PORTPHY1CFG_ADDR                                             0x000000a8
#define PORTPHY1CFG_FRCPHYRDY_SET(dst, src) \
		(((dst) & ~0x00100000) | (((u32)(src) << 0x14) & 0x00100000))
#define PORTPHY2CFG_ADDR                                             0x000000ac
#define PORTPHY3CFG_ADDR                                             0x000000b0
#define PORTPHY4CFG_ADDR                                             0x000000b4
#define PORTPHY5CFG_ADDR                                             0x000000b8
#define SCTL0_ADDR                                                   0x0000012C
#define PORTPHY5CFG_RTCHG_SET(dst, src) \
		(((dst) & ~0xfff00000) | (((u32)(src) << 0x14) & 0xfff00000))
#define PORTAXICFG_EN_CONTEXT_SET(dst, src) \
		(((dst) & ~0x01000000) | (((u32)(src) << 0x18) & 0x01000000))
#define PORTAXICFG_ADDR                                              0x000000bc
#define PORTAXICFG_OUTTRANS_SET(dst, src) \
		(((dst) & ~0x00f00000) | (((u32)(src) << 0x14) & 0x00f00000))

/* SATA host controller slave CSR */
#define INT_SLV_TMOMASK_ADDR                                         0x00000010

/* SATA global diagnostic CSR */
#define REGSPEC_CFG_MEM_RAM_SHUTDOWN_ADDR                            0x00000070
#define REGSPEC_BLOCK_MEM_RDY_ADDR                                   0x00000074

#define readl(x)		*((volatile u32 *) (x))
#define writel(x, v)		*((volatile u32 *) (x)) = (v)

int xgene_sata_in32(void *addr, u32 *val)
{
        *val = readl(addr);
        XGENE_CSRDBG("SATAPHY CSR RD: 0x%llx value: 0x%08x\n", addr, *val);
        return 0;
}

int xgene_sata_out32(void *addr, u32 val)
{
	writel(addr, val);
        XGENE_CSRDBG("SATAPHY CSR WR: 0x%llx value: 0x%08x\n", addr, val);
        return 0;
}

int xgene_sata_out32_flush(void *addr, u32 val)
{
	writel(addr, val);
	XGENE_CSRDBG("SATAPHY CSR WR: 0x%llx value: 0x%08x\n", addr, val);
        readl(addr);
	return 0;
}

void xgene_sata_delayus(unsigned long us)
{
	MicroSecondDelay(us);
}

void xgene_sata_delayms(unsigned long us)
{
	MicroSecondDelay(us*1000);
}

static void xgene_ahci_init_memram(struct xgene_sata_context *ctx)
{
	void *diagcsr = ctx->csr_base + SATA_DIAG_OFFSET;
	int timeout;
	u32 val;
	#define SATA_RESET_MEM_RAM_TO		100000

	xgene_sata_in32(diagcsr + REGSPEC_CFG_MEM_RAM_SHUTDOWN_ADDR, &val);
	if (val == 0) {
		DBG("already clear memory shutdown\n");
		return;
	}
	DBG("clear controller %d memory shutdown\n", ctx->cid);
	/* SATA controller memory in shutdown. Remove from shutdown. */
        xgene_sata_out32_flush(diagcsr + REGSPEC_CFG_MEM_RAM_SHUTDOWN_ADDR,
				0x00);
	timeout = SATA_RESET_MEM_RAM_TO;
        xgene_sata_in32(diagcsr + REGSPEC_BLOCK_MEM_RDY_ADDR, &val);
        while (val != 0xFFFFFFFF && timeout-- > 0) {
        	xgene_sata_in32(diagcsr + REGSPEC_BLOCK_MEM_RDY_ADDR, &val);
			if (val != 0xFFFFFFFF)
				xgene_sata_delayus(1);
        }
}

char *apm88xxx_chip_revision(void)
{
	#define EFUSE0_SHADOW_VERSION_SHIFT     28
	#define EFUSE0_SHADOW_VERSION_MASK      0xF
	volatile void *efuse = (volatile void *) 0x1054A000ULL;
	volatile void *jtagid = (volatile void *) 0x17000004ULL;
        u32 val;

	val = (readl(efuse) >> EFUSE0_SHADOW_VERSION_SHIFT)
		& EFUSE0_SHADOW_VERSION_MASK;
	switch (val) {
	case 0x00:
		val = readl(jtagid);
		return val & 0x10000000 ? "A2": "A1";
	case 0x01:
		return "A2";
	case 0x02:
		return "A3";
	default:
	        return "Unknown";
	}
}

static void xgene_sata_set_portphy_cfg(struct xgene_sata_context *ctx,
	int channel)
{
	void * mmio = ctx->mmio_base;
        u32 val;

	DBG("SATA%d.%d port configure mmio 0x%p channel %d\n",
		ctx->cid, channel, mmio, channel);
        xgene_sata_in32(mmio + PORTCFG_ADDR, &val);
	val = PORTADDR_SET(val, channel == 0 ? 2 : 3);
        xgene_sata_out32_flush(mmio + PORTCFG_ADDR, val);
	/* disable fix rate */
        xgene_sata_out32_flush(mmio + PORTPHY1CFG_ADDR, 0x0001fffe);
        xgene_sata_out32_flush(mmio + PORTPHY2CFG_ADDR, 0x5018461c);
        xgene_sata_out32_flush(mmio + PORTPHY3CFG_ADDR, 0x1c081907);
        xgene_sata_out32_flush(mmio + PORTPHY4CFG_ADDR, 0x1c080815);
	xgene_sata_in32(mmio + PORTPHY5CFG_ADDR, &val);
	/* window negotiation 0x800 t0 0x400 */
	val = PORTPHY5CFG_RTCHG_SET(val, 0x300);
	xgene_sata_out32(mmio + PORTPHY5CFG_ADDR, val);
        xgene_sata_in32(mmio + PORTAXICFG_ADDR, &val);
	/* enable context management */
        val = PORTAXICFG_EN_CONTEXT_SET(val,0x1);
	/* Outstanding */
        val = PORTAXICFG_OUTTRANS_SET(val, 0xe);
	xgene_sata_out32_flush(mmio + PORTAXICFG_ADDR, val);
}

int xgene_ahci_init(UINT64 serdes_base, UINT64 ahci_base, UINT64 pcie_clk_base,
	u32 serdes_diff_clk, u32 gen_sel, int cid, int irq,
	u32 ctrl_eq_A1, u32 ctrl_eq, u32 pq_A1, u32 pq,
	u32 loopback_buf_en_A1, u32 loopback_buf_en,
	u32 loopback_ena_ctle_A1, u32 loopback_ena_ctle,
	u32 spd_sel_cdr_A1, u32 spd_sel_cdr, u32 use_gen_avg)
{
	struct xgene_sata_context *hpriv;
	INTN rc = 0;
	INTN i;
	UINT32 val;
	u32 rxclk_inv;
	char *chip_revision = apm88xxx_chip_revision();

	hpriv = AllocateZeroPool(sizeof(struct xgene_sata_context));
	if (!hpriv) {
		SATA_ERROR("can't alloc host context\n");
		return -1;
	}
	hpriv->cid = cid;
	hpriv->irq = irq;

  	hpriv->csr_phys = serdes_base;
  	hpriv->csr_base = (void *) serdes_base;
	if (!hpriv->csr_base) {
    		SATA_ERROR("can't map PHY CSR resource\n");
		rc  = -1;
		goto error;
	}

	hpriv->mmio_phys = ahci_base;
	hpriv->mmio_base = (VOID *) ahci_base;
	if (!hpriv->mmio_base) {
		SATA_ERROR("can't map MMIO resource\n");
		rc  = -1;
		goto error;
	}

	if (cid == 2) {
		hpriv->pcie_phys = pcie_clk_base;
		hpriv->pcie_base = (void *) pcie_clk_base;
		if (!hpriv->pcie_base) {
			SATA_ERROR("can't ma pcie resource\n");
			rc  = -1;
			goto error;
		}
	}

	DBG("SATA%d PHY PAddr 0x%016LX VAddr 0x%p Mmio PAddr "
		"0x%016LX VAddr 0x%p Rev %s\n", cid, hpriv->csr_phys, hpriv->csr_base,
		hpriv->mmio_phys, hpriv->mmio_base, chip_revision);

	/* Custom Serdes override paraemter */
	if (ctrl_eq_A1 == XGENE_SERDES_VAL_NOT_SET)
		hpriv->ctrl_eq_A1 = CTLE_EQ;
	else
		hpriv->ctrl_eq_A1 = ctrl_eq_A1;
	if (ctrl_eq == XGENE_SERDES_VAL_NOT_SET)
		hpriv->ctrl_eq = CTLE_EQ_A2;
	else
		hpriv->ctrl_eq = ctrl_eq;
	DBG("SATA%d ctrl_eq %d %d\n", cid, hpriv->ctrl_eq_A1, hpriv->ctrl_eq);
	if (use_gen_avg == XGENE_SERDES_VAL_NOT_SET)
		hpriv->use_gen_avg = AsciiStrCmp(chip_revision, "A1") == 0 ? 0 : 1;
	else
		hpriv->use_gen_avg = use_gen_avg;
	DBG("SATA%d use avg %d\n", cid, hpriv->use_gen_avg);
	if (loopback_buf_en_A1 == XGENE_SERDES_VAL_NOT_SET)
		hpriv->loopback_buf_en_A1 = 1;
	else
		hpriv->loopback_buf_en_A1 = loopback_buf_en_A1;
	if (loopback_buf_en == XGENE_SERDES_VAL_NOT_SET)
		hpriv->loopback_buf_en = 0;
	else
		hpriv->loopback_buf_en = loopback_buf_en;
	DBG("SATA%d loopback_buf_en %d %d\n", cid,
		hpriv->loopback_buf_en_A1, hpriv->loopback_buf_en);
	if (loopback_ena_ctle_A1 == XGENE_SERDES_VAL_NOT_SET)
		hpriv->loopback_ena_ctle_A1 = 1;
	else
		hpriv->loopback_ena_ctle_A1 = loopback_ena_ctle_A1;
	if (loopback_ena_ctle == XGENE_SERDES_VAL_NOT_SET)
		hpriv->loopback_ena_ctle = 0;
	else
		hpriv->loopback_ena_ctle = loopback_ena_ctle;
	DBG("SATA%d loopback_ena_ctle %d %d\n", cid,
		hpriv->loopback_ena_ctle_A1, hpriv->loopback_ena_ctle);
	if (spd_sel_cdr_A1 == XGENE_SERDES_VAL_NOT_SET)
		hpriv->spd_sel_cdr_A1 = SPD_SEL;
	else
		hpriv->spd_sel_cdr_A1 = spd_sel_cdr_A1;
	if (spd_sel_cdr == XGENE_SERDES_VAL_NOT_SET)
		hpriv->spd_sel_cdr = SPD_SEL;
	else
		hpriv->spd_sel_cdr = spd_sel_cdr;
	DBG("SATA%d spd_sel_cdr %d %d\n", cid,
		hpriv->spd_sel_cdr_A1, hpriv->spd_sel_cdr);
	if (pq_A1 == XGENE_SERDES_VAL_NOT_SET)
		hpriv->pq_A1 = PQ_REG;
	else
		hpriv->pq_A1 = pq_A1;
	if (pq == XGENE_SERDES_VAL_NOT_SET)
		hpriv->pq = PQ_REG_A2;
	else
		hpriv->pq = pq;
	hpriv->pq_sign = 0;
	DBG("SATA%d pq %d %d %d\n", cid, hpriv->pq_A1, hpriv->pq, hpriv->pq_sign);

	rxclk_inv = AsciiStrCmp(chip_revision, "A1") == 0 ? 1 : 0;
	rc = xgene_sata_serdes_init(hpriv, gen_sel, serdes_diff_clk, rxclk_inv);
	if (rc != 0) {
		SATA_ERROR("SATA%d PHY initialize failed %d\n", cid, rc);
		rc = -1;
		goto error;
	}

	if (hpriv->use_gen_avg) {
		xgene_sata_gen_avg_val(hpriv, 1);
		xgene_sata_gen_avg_val(hpriv, 0);
	} else {
		xgene_sata_force_lat_summer_cal(hpriv, 0);
		xgene_sata_force_lat_summer_cal(hpriv, 1);
	}

	if (AsciiStrCmp(chip_revision, "A1") == 0) {
		xgene_sata_serdes_reset_rxa_rxd(hpriv, 0);
		xgene_sata_serdes_reset_rxa_rxd(hpriv, 1);
	}
	for (i = 0; i < MAX_AHCI_CHN_PERCTR; i++)
		xgene_sata_set_portphy_cfg(hpriv, i);

	/* Now enable top level interrupt. Otherwise, port interrupt will
	   not work. */
	/* AXI disable Mask */
	xgene_sata_out32_flush(hpriv->mmio_base + HOST_IRQ_STAT, 0xffffffff);
        xgene_sata_out32(hpriv->csr_base + INTSTATUSMASK_ADDR, 0);
	xgene_sata_in32(hpriv->csr_base + INTSTATUSMASK_ADDR, &val);
	DBG("SATA%d top level interrupt mask 0x%X value 0x%08X\n",
		cid, INTSTATUSMASK_ADDR, val);
	xgene_sata_out32_flush(hpriv->csr_base + ERRINTSTATUSMASK_ADDR, 0x0);
	xgene_sata_out32_flush(hpriv->csr_base + SATA_SHIM_OFFSET +
			INT_SLV_TMOMASK_ADDR, 0x0);
	/* Enable AXI Interrupt */
	xgene_sata_out32(hpriv->csr_base + SLVRDERRATTRIBUTES_ADDR, 0xffffffff);
	xgene_sata_out32(hpriv->csr_base + SLVWRERRATTRIBUTES_ADDR, 0xffffffff);
	xgene_sata_out32(hpriv->csr_base + MSTRDERRATTRIBUTES_ADDR, 0xffffffff);
	xgene_sata_out32(hpriv->csr_base + MSTWRERRATTRIBUTES_ADDR, 0xffffffff);

	/* Enable coherency as Linux uses cache. */
	xgene_sata_in32(hpriv->csr_base + BUSCTLREG_ADDR, &val);
	val= MSTAWAUX_COHERENT_BYPASS_SET(val, 0);
	val= MSTARAUX_COHERENT_BYPASS_SET(val, 0);
	xgene_sata_out32(hpriv->csr_base+ BUSCTLREG_ADDR, val);

        xgene_sata_in32(hpriv->csr_base + IOFMSTRWAUX_ADDR, &val);
	val |= (1 << 3);	/* Enable read coherency */
	val |= (1 << 9);	/* Enable write coherency */
        xgene_sata_out32_flush(hpriv->csr_base + IOFMSTRWAUX_ADDR, val);
        xgene_sata_in32(hpriv->csr_base + IOFMSTRWAUX_ADDR, &val);
	DBG("SATA%d coherency 0x%X value 0x%08X\n",
		cid, IOFMSTRWAUX_ADDR, val);

	/* Take memory ram out of shutdown */
	xgene_ahci_init_memram(hpriv);

	DBG("SATA%d PHY initialized\n", cid);
	return 0;

error:
	FreePool(hpriv);
	return rc;
}
