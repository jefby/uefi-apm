/*
 * sata_xgene_serdes.c - AppliedMicro X-Gene SATA PHY driver
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
#include "SataController.h"
#include "sata_apm88xxxx.h"

#define PHY_ERROR
#undef PHY_DEBUG
#undef PHYCSR_DEBUG

#ifdef PHY_DEBUG
#define PHYDEBUG(fmt, args...) 		\
	do { \
		DEBUG((EFI_D_BLKIO, "XGENESATA PHY CSR: " fmt "\n", ## args)); \
	} while (0)
#else
#define PHYDEBUG(fmt, args...)
#endif

#ifdef PHYCSR_DEBUG
#define PHYCSRDEBUG(fmt, args...) 	\
	do { \
		DEBUG((EFI_D_BLKIO, "XGENESATA PHY CSR: " fmt "\n", ## args)); \
	} while (0)
#else
#define PHYCSRDEBUG(fmt, args...)
#endif

#ifdef PHY_ERROR
#define PHYERROR(fmt, args...) 		\
	do { \
		DEBUG((EFI_D_BLKIO, "XGENESATA PHY ERROR: " fmt "\n", ## args)); \
	} while (0)
#else
#define PHYERROR(fmt, args...)
#endif

/* SATA PHY CSR block offset */
#define SATA_ETH_MUX_OFFSET		0x00007000
#define SATA_SERDES_OFFSET		0x0000A000
#define SATA_CLK_OFFSET			0x0000C000

/* SATA PHY common tunning parameters.
 *
 * These are the common tunning PHY parameter. This are here to quick
 * reference. They can be override from the control override registers.
 */
#define FBDIV_VAL_50M   		0x77
#define REFDIV_VAL_50M  		0x1
#define FBDIV_VAL_100M 			0x3B
#define REFDIV_VAL_100M 		0x0
#define FBDIV_VAL  			FBDIV_VAL_50M
#define REFDIV_VAL  			REFDIV_VAL_50M
#define SPD_SEL_GEN2                    0x3
#define SPD_SEL_GEN1	                0x1

/* SATA Clock/Reset CSR */
#define SATACLKENREG_ADDR                                            0x00000000
#define SATASRESETREG_ADDR                                           0x00000004
#define  SATA_MEM_RESET_MASK                                         0x00000020
#define  SATA_MEM_RESET_RD(src)                       (((src) & 0x00000020)>>5)
#define  SATA_SDS_RESET_MASK                                         0x00000004
#define  SATA_CSR_RESET_MASK                                         0x00000001
#define  SATA_CORE_RESET_MASK                                        0x00000002
#define  SATA_PMCLK_RESET_MASK                                       0x00000010
#define  SATA_PCLK_RESET_MASK                                        0x00000008

/* SATA SDS CSR */
#define SATA_ENET_SDS_PCS_CTL0_ADDR                                  0x00000000
#define  REGSPEC_CFG_I_TX_WORDMODE0_SET(dst,src) \
		(((dst) & ~0x00070000) | (((u32)(src)<<16) & 0x00070000))
#define  REGSPEC_CFG_I_RX_WORDMODE0_SET(dst,src) \
		(((dst) & ~0x00e00000) | (((u32)(src)<<21) & 0x00e00000))
#define SATA_ENET_SDS_CTL1_ADDR                                      0x00000010
#define  CFG_I_SPD_SEL_CDR_OVR1_SET(dst,src) \
		(((dst) & ~0x0000000f) | (((u32)(src)) & 0x0000000f))
#define SATA_ENET_SDS_CTL0_ADDR                                      0x0000000c
#define  REGSPEC_CFG_I_CUSTOMER_PIN_MODE0_SET(dst,src) \
		(((dst) & ~0x00007fff) | (((u32)(src)) & 0x00007fff))
#define SATA_ENET_SDS_RST_CTL_ADDR                                   0x00000024
#define SATA_ENET_SDS_IND_CMD_REG_ADDR                               0x0000003c
#define  CFG_IND_WR_CMD_MASK                                         0x00000001
#define  CFG_IND_RD_CMD_MASK                                         0x00000002
#define  CFG_IND_CMD_DONE_MASK                                       0x00000004
#define  CFG_IND_ADDR_SET(dst,src) \
		(((dst) & ~0x003ffff0) | (((u32)(src)<<4) & 0x003ffff0))
#define SATA_ENET_SDS_IND_RDATA_REG_ADDR                             0x00000040
#define SATA_ENET_SDS_IND_WDATA_REG_ADDR                             0x00000044
#define SATA_ENET_CLK_MACRO_REG_ADDR                                 0x0000004c
#define  I_RESET_B_SET(dst,src) \
		(((dst) & ~0x00000001) | (((u32)(src)) & 0x00000001))
#define  I_PLL_FBDIV_SET(dst,src) \
		(((dst) & ~0x001ff000) | (((u32)(src)<<12) & 0x001ff000))
#define  I_CUSTOMEROV_SET(dst,src) \
		(((dst) & ~0x00000f80) | (((u32)(src)<<7) & 0x00000f80))
#define  O_PLL_LOCK_RD(src)                          (((src) & 0x40000000)>>30)
#define  O_PLL_READY_RD(src)                         (((src) & 0x80000000)>>31)

/* SATA PHY clock CSR */
#define KC_CLKMACRO_CMU_REGS_CMU_REG0_ADDR                0x20000
#define  CMU_REG0_PDOWN_MASK					0x00004000
#define  CMU_REG0_CAL_COUNT_RESOL_SET(dst, src) \
		(((dst) & ~0x000000e0) | (((u32)(src) << 0x5) & 0x000000e0))
#define KC_CLKMACRO_CMU_REGS_CMU_REG1_ADDR                0x20002
#define  CMU_REG1_PLL_CP_SET(dst, src) \
		(((dst) & ~0x00003c00) | (((u32)(src) << 0xa) & 0x00003c00))
#define  CMU_REG1_PLL_MANUALCAL_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define  CMU_REG1_PLL_CP_SEL_SET(dst, src) \
		(((dst) & ~0x000003e0) | (((u32)(src) << 0x5) & 0x000003e0))
#define KC_CLKMACRO_CMU_REGS_CMU_REG2_ADDR                0x20004
#define  CMU_REG2_PLL_LFRES_SET(dst, src) \
		(((dst) & ~0x0000001e) | (((u32)(src) << 0x1) & 0x0000001e))
#define  CMU_REG2_PLL_FBDIV_SET(dst, src) \
		(((dst) & ~0x00003fe0) | (((u32)(src) << 0x5) & 0x00003fe0))
#define KC_CLKMACRO_CMU_REGS_CMU_REG3_ADDR                0x20006
#define  CMU_REG3_VCOVARSEL_SET(dst, src) \
		(((dst) & ~0x0000000f) | (((u32)(src) << 0x0) & 0x0000000f))
#define  CMU_REG3_VCO_MOMSEL_INIT_SET(dst, src) \
		(((dst) & ~0x000003f0) | (((u32)(src) << 0x4) & 0x000003f0))
#define KC_CLKMACRO_CMU_REGS_CMU_REG4_ADDR                0x20008
#define KC_CLKMACRO_CMU_REGS_CMU_REG5_ADDR                0x2000a
#define  CMU_REG5_PLL_LFSMCAP_SET(dst, src) \
		(((dst) & ~0x0000c000) | (((u32)(src) << 0xe) & 0x0000c000))
#define  CMU_REG5_PLL_LOCK_RESOLUTION_SET(dst, src) \
		(((dst) & ~0x0000000e) | (((u32)(src) << 0x1) & 0x0000000e))
#define  CMU_REG5_PLL_LFCAP_SET(dst, src) \
		(((dst) & ~0x00003000) | (((u32)(src) << 0xc) & 0x00003000))
#define KC_CLKMACRO_CMU_REGS_CMU_REG6_ADDR                0x2000c
#define  CMU_REG6_PLL_VREGTRIM_SET(dst, src) \
		(((dst) & ~0x00000600) | (((u32)(src) << 0x9) & 0x00000600))
#define  CMU_REG6_MAN_PVT_CAL_SET(dst, src) \
		(((dst) & ~0x00000004) | (((u32)(src) << 0x2) & 0x00000004))
#define KC_CLKMACRO_CMU_REGS_CMU_REG7_ADDR                0x2000e
#define  CMU_REG7_PLL_CALIB_DONE_RD(src) \
		((0x00004000 & (u32)(src)) >> 0xe)
#define  CMU_REG7_VCO_CAL_FAIL_RD(src) \
		((0x00000c00 & (u32)(src)) >> 0xa)
#define KC_CLKMACRO_CMU_REGS_CMU_REG8_ADDR                0x20010
#define KC_CLKMACRO_CMU_REGS_CMU_REG9_ADDR                0x20012
#define KC_CLKMACRO_CMU_REGS_CMU_REG10_ADDR               0x20014
#define KC_CLKMACRO_CMU_REGS_CMU_REG11_ADDR               0x20016
#define KC_CLKMACRO_CMU_REGS_CMU_REG12_ADDR               0x20018
#define KC_CLKMACRO_CMU_REGS_CMU_REG13_ADDR               0x2001a
#define KC_CLKMACRO_CMU_REGS_CMU_REG14_ADDR               0x2001c
#define KC_CLKMACRO_CMU_REGS_CMU_REG15_ADDR               0x2001e
#define KC_CLKMACRO_CMU_REGS_CMU_REG16_ADDR               0x20020
#define  CMU_REG16_PVT_DN_MAN_ENA_MASK				0x00000001
#define  CMU_REG16_PVT_UP_MAN_ENA_MASK				0x00000002
#define  CMU_REG16_VCOCAL_WAIT_BTW_CODE_SET(dst, src) \
		(((dst) & ~0x0000001c) | (((u32)(src) << 0x2) & 0x0000001c))
#define  CMU_REG16_CALIBRATION_DONE_OVERRIDE_SET(dst, src) \
		(((dst) & ~0x00000040) | (((u32)(src) << 0x6) & 0x00000040))
#define  CMU_REG16_BYPASS_PLL_LOCK_SET(dst, src) \
		(((dst) & ~0x00000020) | (((u32)(src) << 0x5) & 0x00000020))
#define KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR               0x20022
#define  CMU_REG17_PVT_CODE_R2A_SET(dst, src) \
		(((dst) & ~0x00007f00) | (((u32)(src) << 0x8) & 0x00007f00))
#define  CMU_REG17_RESERVED_7_SET(dst, src) \
		(((dst) & ~0x000000e0) | (((u32)(src) << 0x5) & 0x000000e0))
#define  CMU_REG17_PVT_TERM_MAN_ENA_MASK			0x00008000
#define KC_CLKMACRO_CMU_REGS_CMU_REG18_ADDR               0x20024
#define KC_CLKMACRO_CMU_REGS_CMU_REG19_ADDR               0x20026
#define KC_CLKMACRO_CMU_REGS_CMU_REG20_ADDR               0x20028
#define KC_CLKMACRO_CMU_REGS_CMU_REG21_ADDR               0x2002a
#define KC_CLKMACRO_CMU_REGS_CMU_REG22_ADDR               0x2002c
#define KC_CLKMACRO_CMU_REGS_CMU_REG23_ADDR               0x2002e
#define KC_CLKMACRO_CMU_REGS_CMU_REG24_ADDR               0x20030
#define KC_CLKMACRO_CMU_REGS_CMU_REG25_ADDR               0x20032
#define KC_CLKMACRO_CMU_REGS_CMU_REG26_ADDR               0x20034
#define  CMU_REG26_FORCE_PLL_LOCK_SET(dst, src) \
		(((dst) & ~0x00000001) | (((u32)(src) << 0x0) & 0x00000001))
#define KC_CLKMACRO_CMU_REGS_CMU_REG27_ADDR               0x20036
#define KC_CLKMACRO_CMU_REGS_CMU_REG28_ADDR               0x20038
#define KC_CLKMACRO_CMU_REGS_CMU_REG29_ADDR               0x2003a
#define KC_CLKMACRO_CMU_REGS_CMU_REG30_ADDR               0x2003c
#define  CMU_REG30_LOCK_COUNT_SET(dst, src) \
		(((dst) & ~0x00000006) | (((u32)(src) << 0x1) & 0x00000006))
#define  CMU_REG30_PCIE_MODE_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define KC_CLKMACRO_CMU_REGS_CMU_REG31_ADDR               0x2003e
#define KC_CLKMACRO_CMU_REGS_CMU_REG32_ADDR               0x20040
#define  CMU_REG32_FORCE_VCOCAL_START_MASK			0x00004000
#define  CMU_REG32_PVT_CAL_WAIT_SEL_SET(dst, src) \
		(((dst) & ~0x00000006) | (((u32)(src) << 0x1) & 0x00000006))
#define  CMU_REG32_IREF_ADJ_SET(dst, src) \
		(((dst) & ~0x00000180) | (((u32)(src) << 0x7) & 0x00000180))
#define KC_CLKMACRO_CMU_REGS_CMU_REG33_ADDR               0x20042
#define KC_CLKMACRO_CMU_REGS_CMU_REG34_ADDR               0x20044
#define  CMU_REG34_VCO_CAL_VTH_LO_MAX_SET(dst, src) \
		(((dst) & ~0x0000000f) | (((u32)(src) << 0x0) & 0x0000000f))
#define  CMU_REG34_VCO_CAL_VTH_HI_MAX_SET(dst, src) \
		(((dst) & ~0x00000f00) | (((u32)(src) << 0x8) & 0x00000f00))
#define  CMU_REG34_VCO_CAL_VTH_LO_MIN_SET(dst, src) \
		(((dst) & ~0x000000f0) | (((u32)(src) << 0x4) & 0x000000f0))
#define  CMU_REG34_VCO_CAL_VTH_HI_MIN_SET(dst, src) \
		(((dst) & ~0x0000f000) | (((u32)(src) << 0xc) & 0x0000f000))
#define KC_SERDES_CMU_REGS_CMU_REG35_ADDR                 0x46
#define  CMU_REG35_PLL_SSC_MOD_SET(dst, src) \
		(((dst) & ~0x0000fe00) | (((u32)(src) << 0x9) & 0x0000fe00))
#define KC_SERDES_CMU_REGS_CMU_REG36_ADDR                 0x48
#define  CMU_REG36_PLL_SSC_EN_SET(dst, src) \
		(((dst) & ~0x00000010) | (((u32)(src) << 0x4) & 0x00000010))
#define  CMU_REG36_PLL_SSC_VSTEP_SET(dst, src) \
		(((dst) & ~0x0000ffc0) | (((u32)(src) << 0x6) & 0x0000ffc0))
#define  CMU_REG36_PLL_SSC_DSMSEL_SET(dst, src) \
		(((dst) & ~0x00000020) | (((u32)(src) << 0x5) & 0x00000020))
#define KC_CLKMACRO_CMU_REGS_CMU_REG35_ADDR               0x20046
#define KC_CLKMACRO_CMU_REGS_CMU_REG36_ADDR               0x20048
#define KC_CLKMACRO_CMU_REGS_CMU_REG37_ADDR               0x2004a
#define KC_CLKMACRO_CMU_REGS_CMU_REG38_ADDR               0x2004c
#define KC_CLKMACRO_CMU_REGS_CMU_REG39_ADDR               0x2004e

/* SATA PHY RXTX CSR */
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR         0x400
#define  CH0_RXTX_REG0_CTLE_EQ_HR_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define  CH0_RXTX_REG0_CTLE_EQ_QR_SET(dst, src) \
		(((dst) & ~0x000007c0) | (((u32)(src) << 0x6) & 0x000007c0))
#define  CH0_RXTX_REG0_CTLE_EQ_FR_SET(dst, src) \
		(((dst) & ~0x0000003e) | (((u32)(src) << 0x1) & 0x0000003e))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG1_ADDR         0x402
#define  CH0_RXTX_REG1_RXACVCM_SET(dst, src) \
		(((dst) & ~0x0000f000) | (((u32)(src) << 0xc) & 0x0000f000))
#define  CH0_RXTX_REG1_CTLE_EQ_SET(dst, src) \
		(((dst) & ~0x00000f80) | (((u32)(src) << 0x7) & 0x00000f80))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG2_ADDR         0x404
#define  CH0_RXTX_REG2_VTT_ENA_SET(dst, src) \
		(((dst) & ~0x00000100) | (((u32)(src) << 0x8) & 0x00000100))
#define  CH0_RXTX_REG2_TX_FIFO_ENA_SET(dst, src) \
		(((dst) & ~0x00000020) | (((u32)(src) << 0x5) & 0x00000020))
#define  CH0_RXTX_REG2_VTT_SEL_SET(dst, src) \
		(((dst) & ~0x000000c0) | (((u32)(src) << 0x6) & 0x000000c0))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR         0x408
#define  CH0_RXTX_REG4_TX_LOOPBACK_BUF_EN_MASK			0x00000040
#define  CH0_RXTX_REG4_TX_DATA_RATE_SET(dst, src) \
		(((dst) & ~0x0000c000) | (((u32)(src) << 0xe) & 0x0000c000))
#define  CH0_RXTX_REG4_TX_WORD_MODE_SET(dst, src) \
		(((dst) & ~0x00003800) | (((u32)(src) << 0xb) & 0x00003800))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG5_ADDR         0x40a
#define  CH0_RXTX_REG5_TX_CN1_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define  CH0_RXTX_REG5_TX_CP1_SET(dst, src) \
		(((dst) & ~0x000007e0) | (((u32)(src) << 0x5) & 0x000007e0))
#define  CH0_RXTX_REG5_TX_CN2_SET(dst, src) \
		(((dst) & ~0x0000001f) | (((u32)(src) << 0x0) & 0x0000001f))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG6_ADDR         0x40c
#define  CH0_RXTX_REG6_TXAMP_CNTL_SET(dst, src) \
		(((dst) & ~0x00000780) | (((u32)(src) << 0x7) & 0x00000780))
#define  CH0_RXTX_REG6_TXAMP_ENA_SET(dst, src) \
		(((dst) & ~0x00000040) | (((u32)(src) << 0x6) & 0x00000040))
#define  CH0_RXTX_REG6_RX_BIST_ERRCNT_RD_SET(dst, src) \
		(((dst) & ~0x00000001) | (((u32)(src) << 0x0) & 0x00000001))
#define  CH0_RXTX_REG6_TX_IDLE_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define  CH0_RXTX_REG6_RX_BIST_RESYNC_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR         0x40e
#define  CH0_RXTX_REG7_RESETB_RXD_MASK			0x00000100
#define  CH0_RXTX_REG7_RESETB_RXA_MASK			0x00000080
#define  CH0_RXTX_REG7_BIST_ENA_RX_SET(dst, src) \
		(((dst) & ~0x00000040) | (((u32)(src) << 0x6) & 0x00000040))
#define  CH0_RXTX_REG7_RX_WORD_MODE_SET(dst, src) \
		(((dst) & ~0x00003800) | (((u32)(src) << 0xb) & 0x00003800))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG8_ADDR         0x410
#define  CH0_RXTX_REG8_CDR_LOOP_ENA_SET(dst, src) \
		(((dst) & ~0x00004000) | (((u32)(src) << 0xe) & 0x00004000))
#define  CH0_RXTX_REG8_CDR_BYPASS_RXLOS_SET(dst, src) \
		(((dst) & ~0x00000800) | (((u32)(src) << 0xb) & 0x00000800))
#define  CH0_RXTX_REG8_SSC_ENABLE_SET(dst, src) \
		(((dst) & ~0x00000200) | (((u32)(src) << 0x9) & 0x00000200))
#define  CH0_RXTX_REG8_SD_VREF_SET(dst, src) \
		(((dst) & ~0x000000f0) | (((u32)(src) << 0x4) & 0x000000f0))
#define  CH0_RXTX_REG8_SD_DISABLE_SET(dst, src) \
		(((dst) & ~0x00000100) | (((u32)(src) << 0x8) & 0x00000100))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR         0x40e
#define  CH0_RXTX_REG7_RESETB_RXD_SET(dst, src) \
		(((dst) & ~0x00000100) | (((u32)(src) << 0x8) & 0x00000100))
#define  CH0_RXTX_REG7_RESETB_RXA_SET(dst, src) \
		(((dst) & ~0x00000080) | (((u32)(src) << 0x7) & 0x00000080))
#define  CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_MASK			0x00004000
#define  CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_SET(dst, src) \
		(((dst) & ~0x00004000) | (((u32)(src) << 0xe) & 0x00004000))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG11_ADDR        0x416
#define  CH0_RXTX_REG11_PHASE_ADJUST_LIMIT_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR        0x418
#define  CH0_RXTX_REG12_LATCH_OFF_ENA_SET(dst, src) \
		(((dst) & ~0x00002000) | (((u32)(src) << 0xd) & 0x00002000))
#define  CH0_RXTX_REG12_SUMOS_ENABLE_SET(dst, src) \
		(((dst) & ~0x00000004) | (((u32)(src) << 0x2) & 0x00000004))
#define  CH0_RXTX_REG12_RX_DET_TERM_ENABLE_MASK		0x00000002
#define  CH0_RXTX_REG12_RX_DET_TERM_ENABLE_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG13_ADDR        0x41a
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG14_ADDR        0x41c
#define CH0_RXTX_REG14_CLTE_LATCAL_MAN_PROG_SET(dst, src) \
		(((dst) & ~0x0000003f) | (((u32)(src) << 0x0) & 0x0000003f))
#define CH0_RXTX_REG14_CTLE_LATCAL_MAN_ENA_SET(dst, src) \
		(((dst) & ~0x00000040) | (((u32)(src) << 0x6) & 0x00000040))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG26_ADDR        0x434
#define  CH0_RXTX_REG26_PERIOD_ERROR_LATCH_SET(dst, src) \
		(((dst) & ~0x00003800) | (((u32)(src) << 0xb) & 0x00003800))
#define  CH0_RXTX_REG26_BLWC_ENA_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG21_ADDR        0x42a
#define  CH0_RXTX_REG21_DO_LATCH_CALOUT_RD(src) \
		((0x0000fc00 & (u32)(src)) >> 0xa)
#define  CH0_RXTX_REG21_XO_LATCH_CALOUT_RD(src) \
		((0x000003f0 & (u32)(src)) >> 0x4)
#define  CH0_RXTX_REG21_LATCH_CAL_FAIL_ODD_RD(src) \
		((0x0000000f & (u32)(src)))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG22_ADDR        0x42c
#define  CH0_RXTX_REG22_SO_LATCH_CALOUT_RD(src) \
		((0x000003f0 & (u32)(src)) >> 0x4)
#define  CH0_RXTX_REG22_EO_LATCH_CALOUT_RD(src) \
		((0x0000fc00 & (u32)(src)) >> 0xa)
#define  CH0_RXTX_REG22_LATCH_CAL_FAIL_EVEN_RD(src) \
		((0x0000000f & (u32)(src)))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG23_ADDR        0x42e
#define  CH0_RXTX_REG23_DE_LATCH_CALOUT_RD(src) \
		((0x0000fc00 & (u32)(src)) >> 0xa)
#define  CH0_RXTX_REG23_XE_LATCH_CALOUT_RD(src) \
		((0x000003f0 & (u32)(src)) >> 0x4)
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG24_ADDR        0x430
#define  CH0_RXTX_REG24_EE_LATCH_CALOUT_RD(src) \
		((0x0000fc00 & (u32)(src)) >> 0xa)
#define  CH0_RXTX_REG24_SE_LATCH_CALOUT_RD(src) \
		((0x000003f0 & (u32)(src)) >> 0x4)
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG28_ADDR        0x438
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG31_ADDR        0x43e
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG38_ADDR        0x44c
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG39_ADDR        0x44e
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG40_ADDR        0x450
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG41_ADDR        0x452
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG42_ADDR        0x454
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG43_ADDR        0x456
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG44_ADDR        0x458
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG45_ADDR        0x45a
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG46_ADDR        0x45c
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG47_ADDR        0x45e
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG48_ADDR        0x460
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG49_ADDR        0x462
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG50_ADDR        0x464
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG51_ADDR        0x466
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG52_ADDR        0x468
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG53_ADDR        0x46a
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG54_ADDR        0x46c
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG55_ADDR        0x46e
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG61_ADDR        0x47a
#define  CH0_RXTX_REG61_ISCAN_INBERT_SET(dst, src) \
		(((dst) & ~0x00000010) | (((u32)(src) << 0x4) & 0x00000010))
#define  CH0_RXTX_REG61_LOADFREQ_SHIFT_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define  CH0_RXTX_REG61_EYE_COUNT_WIDTH_SEL_SET(dst, src) \
		(((dst) & ~0x000000c0) | (((u32)(src) << 0x6) & 0x000000c0))
#define  CH0_RXTX_REG61_SPD_SEL_CDR_SET(dst, src) \
		(((dst) & ~0x00003c00) | (((u32)(src) << 0xa) & 0x00003c00))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG62_ADDR        0x47c
#define  CH0_RXTX_REG62_PERIOD_H1_QLATCH_SET(dst, src) \
		(((dst) & ~0x00003800) | (((u32)(src) << 0xb) & 0x00003800))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG81_ADDR        0x4a2

#define  CH0_RXTX_REG89_MU_TH7_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define  CH0_RXTX_REG89_MU_TH8_SET(dst, src) \
		(((dst) & ~0x000007c0) | (((u32)(src) << 0x6) & 0x000007c0))
#define  CH0_RXTX_REG89_MU_TH9_SET(dst, src) \
		(((dst) & ~0x0000003e) | (((u32)(src) << 0x1) & 0x0000003e))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG96_ADDR        0x4c0
#define  CH0_RXTX_REG96_MU_FREQ1_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define  CH0_RXTX_REG96_MU_FREQ2_SET(dst, src) \
		(((dst) & ~0x000007c0) | (((u32)(src) << 0x6) & 0x000007c0))
#define  CH0_RXTX_REG96_MU_FREQ3_SET(dst, src) \
		(((dst) & ~0x0000003e) | (((u32)(src) << 0x1) & 0x0000003e))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG99_ADDR        0x4c6
#define  CH0_RXTX_REG99_MU_PHASE1_SET(dst, src) \
		(((dst) & ~0x0000f800) | (((u32)(src) << 0xb) & 0x0000f800))
#define  CH0_RXTX_REG99_MU_PHASE2_SET(dst, src) \
		(((dst) & ~0x000007c0) | (((u32)(src) << 0x6) & 0x000007c0))
#define  CH0_RXTX_REG99_MU_PHASE3_SET(dst, src) \
		(((dst) & ~0x0000003e) | (((u32)(src) << 0x1) & 0x0000003e))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG102_ADDR       0x4cc
#define  CH0_RXTX_REG102_FREQLOOP_LIMIT_SET(dst, src) \
		(((dst) & ~0x00000060) | (((u32)(src) << 0x5) & 0x00000060))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG114_ADDR       0x4e4
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG121_ADDR       0x4f2
#define  CH0_RXTX_REG121_SUMOS_CAL_CODE_RD(src) \
		((0x0000003e & (u32)(src)) >> 0x1)
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG125_ADDR       0x4fa
#define  CH0_RXTX_REG125_PQ_REG_SET(dst, src) \
		(((dst) & ~0x0000fe00) | (((u32)(src) << 0x9) & 0x0000fe00))
#define  CH0_RXTX_REG125_SIGN_PQ_SET(dst, src) \
		(((dst) & ~0x00000100) | (((u32)(src) << 0x8) & 0x00000100))
#define  CH0_RXTX_REG125_SIGN_PQ_2C_SET(dst, src) \
		(((dst) & ~0x00000080) | (((u32)(src) << 0x7) & 0x00000080))
#define  CH0_RXTX_REG125_PHZ_MANUALCODE_SET(dst, src) \
		(((dst) & ~0x0000007c) | (((u32)(src) << 0x2) & 0x0000007c))
#define  CH0_RXTX_REG125_PHZ_MANUAL_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR       0x4fe
#define  CH0_RXTX_REG127_FORCE_SUM_CAL_START_MASK		0x00000002
#define  CH0_RXTX_REG127_FORCE_LAT_CAL_START_MASK		0x00000004
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG127_ADDR       0x6fe
#define  CH1_RXTX_REG127_FORCE_SUM_CAL_START_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define  CH1_RXTX_REG127_FORCE_LAT_CAL_START_SET(dst, src) \
		(((dst) & ~0x00000004) | (((u32)(src) << 0x2) & 0x00000004))
#define  CH0_RXTX_REG127_LATCH_MAN_CAL_ENA_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define  CH0_RXTX_REG127_DO_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x0000fc00) | (((u32)(src) << 0xa) & 0x0000fc00))
#define  CH0_RXTX_REG127_XO_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x000003f0) | (((u32)(src) << 0x4) & 0x000003f0))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG128_ADDR       0x500
#define  CH0_RXTX_REG128_LATCH_CAL_WAIT_SEL_SET(dst, src) \
		(((dst) & ~0x0000000c) | (((u32)(src) << 0x2) & 0x0000000c))
#define  CH0_RXTX_REG128_EO_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x0000fc00) | (((u32)(src) << 0xa) & 0x0000fc00))
#define CH0_RXTX_REG128_SO_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x000003f0) | (((u32)(src) << 0x4) & 0x000003f0))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG129_ADDR       0x502
#define CH0_RXTX_REG129_DE_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x0000fc00) | (((u32)(src) << 0xa) & 0x0000fc00))
#define CH0_RXTX_REG129_XE_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x000003f0) | (((u32)(src) << 0x4) & 0x000003f0))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG130_ADDR       0x504
#define  CH0_RXTX_REG130_EE_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x0000fc00) | (((u32)(src) << 0xa) & 0x0000fc00))
#define  CH0_RXTX_REG130_SE_LATCH_MANCAL_SET(dst, src) \
		(((dst) & ~0x000003f0) | (((u32)(src) << 0x4) & 0x000003f0))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG145_ADDR       0x522
#define  CH0_RXTX_REG145_TX_IDLE_SATA_SET(dst, src) \
		(((dst) & ~0x00000001) | (((u32)(src) << 0x0) & 0x00000001))
#define  CH0_RXTX_REG145_RXES_ENA_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define  CH0_RXTX_REG145_RXDFE_CONFIG_SET(dst, src) \
		(((dst) & ~0x0000c000) | (((u32)(src) << 0xe) & 0x0000c000))
#define  CH0_RXTX_REG145_RXVWES_LATENA_SET(dst, src) \
		(((dst) & ~0x00000004) | (((u32)(src) << 0x2) & 0x00000004))
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG147_ADDR       0x526
#define KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG148_ADDR       0x528

#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG4_ADDR         0x608
#define  CH1_RXTX_REG4_TX_LOOPBACK_BUF_EN_SET(dst, src) \
		(((dst) & ~0x00000040) | (((u32)(src) << 0x6) & 0x00000040))
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG7_ADDR         0x60e
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG13_ADDR        0x61a
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG38_ADDR        0x64c
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG39_ADDR        0x64e
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG40_ADDR        0x650
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG41_ADDR        0x652
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG42_ADDR        0x654
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG43_ADDR        0x656
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG44_ADDR        0x658
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG45_ADDR        0x65a
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG46_ADDR        0x65c
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG47_ADDR        0x65e
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG48_ADDR        0x660
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG49_ADDR        0x662
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG50_ADDR        0x664
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG51_ADDR        0x666
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG52_ADDR        0x668
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG53_ADDR        0x66a
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG54_ADDR        0x66c
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG55_ADDR        0x66e
#define KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG121_ADDR       0x6f2

/* SATA/ENET Shared CSR */
#define SATA_ENET_CONFIG_REG_ADDR                                    0x00000000
#define  CFG_SATA_ENET_SELECT_MASK                                   0x00000001

/* SATA SERDES CMU CSR */
#define KC_SERDES_CMU_REGS_CMU_REG0_ADDR                  0x0
#define  CMU_REG0_PLL_REF_SEL_MASK		0x00002000
#define CMU_REG0_PLL_REF_SEL_SHIFT_MASK		0xd
#define CMU_REG0_PLL_REF_SEL_SET(dst, src)	\
         (((dst) & ~0x00002000) | (((unsigned int)(src) << 0xd) & 0x00002000))
#define KC_SERDES_CMU_REGS_CMU_REG1_ADDR                  0x2
#define  CMU_REG1_REFCLK_CMOS_SEL_MASK		0x00000001
#define CMU_REG1_REFCLK_CMOS_SEL_SHIFT_MASK		0x0
#define CMU_REG1_REFCLK_CMOS_SEL_SET(dst, src)	\
		(((dst) & ~0x00000001) | (((unsigned int)(src) << 0x0 ) & 0x00000001))
#define KC_SERDES_CMU_REGS_CMU_REG2_ADDR                  0x4
#define  CMU_REG2_PLL_REFDIV_SET(dst, src) \
		(((dst) & ~0x0000c000) | (((u32)(src) << 0xe) & 0x0000c000))
#define KC_SERDES_CMU_REGS_CMU_REG3_ADDR                  0x6
#define  CMU_REG3_VCO_MANMOMSEL_SET(dst, src) \
		(((dst) & ~0x0000fc00) | (((u32)(src) << 0xa) & 0x0000fc00))
#define KC_SERDES_CMU_REGS_CMU_REG5_ADDR                  0xa
#define  CMU_REG5_PLL_RESETB_MASK			0x00000001
#define KC_SERDES_CMU_REGS_CMU_REG6_ADDR                  0xc
#define KC_SERDES_CMU_REGS_CMU_REG7_ADDR                  0xe
#define KC_SERDES_CMU_REGS_CMU_REG9_ADDR                  0x12
#define  CMU_REG9_TX_WORD_MODE_CH1_SET(dst, src) \
		(((dst) & ~0x00000380) | (((u32)(src) << 0x7) & 0x00000380))
#define  CMU_REG9_TX_WORD_MODE_CH0_SET(dst, src) \
		(((dst) & ~0x00000070) | (((u32)(src) << 0x4) & 0x00000070))
#define  CMU_REG9_PLL_POST_DIVBY2_SET(dst, src) \
		(((dst) & ~0x00000008) | (((u32)(src) << 0x3) & 0x00000008))
#define KC_SERDES_CMU_REGS_CMU_REG12_ADDR                 0x18
#define  CMU_REG12_STATE_DELAY9_SET(dst, src) \
		(((dst) & ~0x000000f0) | (((u32)(src) << 0x4) & 0x000000f0))
#define KC_SERDES_CMU_REGS_CMU_REG13_ADDR                 0x1a
#define KC_SERDES_CMU_REGS_CMU_REG14_ADDR                 0x1c
#define KC_SERDES_CMU_REGS_CMU_REG15_ADDR                 0x1e
#define KC_SERDES_CMU_REGS_CMU_REG16_ADDR                 0x20
#define KC_SERDES_CMU_REGS_CMU_REG17_ADDR                 0x22
#define KC_SERDES_CMU_REGS_CMU_REG26_ADDR                 0x34
#define KC_SERDES_CMU_REGS_CMU_REG30_ADDR                 0x3c
#define KC_SERDES_CMU_REGS_CMU_REG31_ADDR                 0x3e
#define KC_SERDES_CMU_REGS_CMU_REG32_ADDR                 0x40
#define  CMU_REG32_FORCE_VCOCAL_START_MASK		0x00004000
#define KC_SERDES_CMU_REGS_CMU_REG34_ADDR                 0x44
#define KC_SERDES_CMU_REGS_CMU_REG37_ADDR                 0x4a

/* PCIE SATA Serdes CSR (CSR shared with the PCIe) */
#define SM_PCIE_CLKRST_CSR_PCIE_SRST_ADDR                 0xc000
#define SM_PCIE_CLKRST_CSR_PCIE_CLKEN_ADDR                0xc008
#define SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_WDATA_REG_ADDR 	0xa01c
#define SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_CMD_REG_ADDR 	0xa014
#define  PCIE_SDS_IND_CMD_REG_CFG_IND_ADDR_SET(dst, src) \
		(((dst) & ~0x003ffff0) | (((u32)(src) << 0x4) & 0x003ffff0))
#define  PCIE_SDS_IND_CMD_REG_CFG_IND_WR_CMD_SET(dst, src) \
		(((dst) & ~0x00000001) | (((u32)(src) << 0x0) & 0x00000001))
#define SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_RDATA_REG_ADDR 	0xa018
#define SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR   0xa094
#define  PCIE_SDS_IND_CMD_REG_CFG_IND_CMD_DONE_RD(src) \
		((0x00000004 & (uint32_t)(src)) >> 0x2)
#define  PCIE_SDS_IND_CMD_REG_CFG_IND_CMD_DONE_SET(dst, src) \
		(((dst) & ~0x00000004) | (((u32)(src) << 0x2) & 0x00000004))
#define  PCIE_SDS_IND_CMD_REG_CFG_IND_RD_CMD_SET(dst, src) \
		(((dst) & ~0x00000002) | (((u32)(src) << 0x1) & 0x00000002))
#define PCIE_CLK_MACRO_REG_I_RESET_B_SET(dst, src) \
		(((dst) & ~0x00000001) | (((u32)(src) << 0x0) & 0x00000001))
#define PCIE_CLK_MACRO_REG_I_CUSTOMEROV_SET(dst, src) \
		(((dst) & ~0x00000f80) | (((u32)(src) << 0x7) & 0x00000f80))
#define PCIE_CLK_MACRO_REG_O_PLL_READY_RD(src) \
		((0x80000000 & (u32)(src)) >> 0x1f)
#define PCIE_CLK_MACRO_REG_I_PLL_FBDIV_SET(dst, src) \
		(((dst) & ~0x001ff000) | (((u32)(src) << 0xc) & 0x001ff000))
#define PCIE_CLK_MACRO_REG_O_PLL_LOCK_RD(src)	((0x40000000 & (u32)(src)) >> 0x1e)

static void xgene_serdes_wr(void *csr_base, u32 indirect_cmd_reg,
	u32 indirect_data_reg, u32 addr, u32 data)
{
	u32 val;
	u32 cmd;

	cmd = CFG_IND_WR_CMD_MASK | CFG_IND_CMD_DONE_MASK;
	cmd = (addr << 4) | cmd;
	xgene_sata_out32(csr_base + indirect_data_reg, data);
	xgene_sata_out32(csr_base + indirect_cmd_reg, cmd);
	/* Very important please don't remove*/
	wmb();
	xgene_sata_in32(csr_base + indirect_cmd_reg, &val);
	xgene_sata_delayus(1000);
	do {
		xgene_sata_in32(csr_base + indirect_cmd_reg, &val);
	} while (!(val & CFG_IND_CMD_DONE_MASK));
}

static void xgene_serdes_rd(void *csr_base, u32 indirect_cmd_reg,
	u32 indirect_data_reg, u32 addr, u32 *data)
{
	u32 val;
	u32 cmd;

	cmd = CFG_IND_RD_CMD_MASK | CFG_IND_CMD_DONE_MASK;
	cmd = (addr << 4) | cmd;
	xgene_sata_out32(csr_base + indirect_cmd_reg, cmd);
	/* Very important please don't remove*/
	wmb();
	xgene_sata_in32(csr_base + indirect_cmd_reg, &val);
	xgene_sata_delayus(1000);
	do {
		xgene_sata_in32(csr_base + indirect_cmd_reg, &val);
	} while (!(val & CFG_IND_CMD_DONE_MASK));
	xgene_sata_in32(csr_base + indirect_data_reg, data);
}

/* X-Gene Serdes write helper for SATA port 0, 1, 2, and 3 */
static void xgene_sds_wr(void *csr_base, u32 addr, u32 data)
{
	u32 val;
	xgene_serdes_wr(csr_base, SATA_ENET_SDS_IND_CMD_REG_ADDR,
		SATA_ENET_SDS_IND_WDATA_REG_ADDR, addr, data);
	xgene_serdes_rd(csr_base, SATA_ENET_SDS_IND_CMD_REG_ADDR,
		SATA_ENET_SDS_IND_RDATA_REG_ADDR, addr, &val);
	PHYCSRDEBUG("SDS WR addr 0x%X value 0x%08X <-> 0x%08X", addr, data,
		val);
}

/* X-Gene Serdes read helper for SATA port 0, 1, 2, and 3 */
static void xgene_sds_rd(void *csr_base, u32 addr, u32 *data)
{
	xgene_serdes_rd(csr_base, SATA_ENET_SDS_IND_CMD_REG_ADDR,
		SATA_ENET_SDS_IND_RDATA_REG_ADDR, addr, data);
	PHYCSRDEBUG("SDS RD addr 0x%X value 0x%08X", addr, *data);
}

/* X-Gene Serdes toggle helper for SATA port 0, 1, 2, and 3 */
static void xgene_sds_toggle1to0(void *csr_base, u32 addr, u32 bits,
	u32 delayus)
{
	u32 val;
	xgene_sds_rd(csr_base, addr, &val);
	val |= bits;
	xgene_sds_wr(csr_base, addr, val);
	xgene_sata_delayus(delayus);
	xgene_sds_rd(csr_base, addr, &val);
	val &= ~bits;
	xgene_sds_wr(csr_base, addr, val);
}

/* X-Gene Serdes clear bits helper for SATA port 0, 1, 2, and 3 */
static void xgene_sds_clrbits(void *csr_base, u32 addr, u32 bits)
{
	u32 val;
	xgene_sds_rd(csr_base, addr, &val);
	val &= ~bits;
	xgene_sds_wr(csr_base, addr, val);
}

/* X-Gene Serdes set bits helper for SATA port 0, 1, 2, and 3 */
static void xgene_sds_setbits(void *csr_base, u32 addr, u32 bits)
{
	u32 val;
	xgene_sds_rd(csr_base, addr, &val);
	val |= bits;
	xgene_sds_wr(csr_base, addr, val);
}

/* X-Gene Serdes write helper for SATA port 4 and 5 */
static void xgene_sds_pcie_wr(void *csr_base, u32 addr, u32 data)
{
	u32 val;
	xgene_serdes_wr(csr_base,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_CMD_REG_ADDR,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_WDATA_REG_ADDR,
		addr, data);
	xgene_serdes_rd(csr_base,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_CMD_REG_ADDR,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_RDATA_REG_ADDR,
		addr, &val);
	xgene_sata_delayus(122); /* FIXME */
	PHYCSRDEBUG("PCIE SDS WR addr 0x%X value 0x%08X <-> 0x%08X", addr,
		data, val);
}

/* X-Gene Serdes read helper for SATA port 4 and 5 */
static void xgene_sds_pcie_rd(void *csr_base, u32 addr, u32 *data)
{
	xgene_serdes_rd(csr_base,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_CMD_REG_ADDR,
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_SDS_IND_RDATA_REG_ADDR,
		addr, data);
	PHYCSRDEBUG("PCIE SDS RD addr 0x%X value 0x%08X", addr, *data);
}

static void xgene_sds_pcie_toggle1to0(void *csr_base, u32 addr, u32 bits,
	u32 delayus)
{
	u32 val;
	xgene_sds_pcie_rd(csr_base, addr, &val);
	val |= bits;
	xgene_sds_pcie_wr(csr_base, addr, val);
	if (delayus > 0)
		xgene_sata_delayus(delayus);
	xgene_sds_pcie_rd(csr_base, addr, &val);
	val &= ~bits;
	xgene_sds_pcie_wr(csr_base, addr, val);
}

static int xgene_serdes_calib_ready_check(void *csr_serdes,
	void (*serdes_rd)(void *, u32, u32 *),
	void (*serdes_wr)(void *, u32, u32),
	void (*serdes_toggle1to0)(void *, u32, u32, u32))
{
	int loopcount;
	u32 val;

	/* TERM CALIBRATION CH0*/
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x0d);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, val);
	serdes_toggle1to0(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR,
			CMU_REG17_PVT_TERM_MAN_ENA_MASK, 0);
	/* DOWN CALIBRATION CH0*/
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x26);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, val);
	serdes_toggle1to0(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG16_ADDR,
			CMU_REG16_PVT_DN_MAN_ENA_MASK, 0);
	/* UP CALIBRATION CH0 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x28);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG17_ADDR, val);
	serdes_toggle1to0(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG16_ADDR,
			CMU_REG16_PVT_UP_MAN_ENA_MASK, 0);

	/* Check for PLL calibration for 1ms */
	loopcount = 10;
	do {
		serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG7_ADDR, &val);
		if (CMU_REG7_PLL_CALIB_DONE_RD(val))
			return 0;
		xgene_sata_delayus(222); /* FIXME */
	} while (!CMU_REG7_PLL_CALIB_DONE_RD(val) && --loopcount > 0);

	return -1;
}

/* SATA port 0 - 3 PLL initialization */
int kc_macro_calib_ready_check(struct xgene_sata_context *ctx)
{
	void *csr_serdes = ctx->csr_base + SATA_SERDES_OFFSET;
	u32 val;

	xgene_serdes_calib_ready_check(csr_serdes, xgene_sds_rd, xgene_sds_wr,
		xgene_sds_toggle1to0);

	/* Check if PLL calibration complete sucessfully */
	xgene_sds_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG7_ADDR, &val);
	if (CMU_REG7_PLL_CALIB_DONE_RD(val) == 0x1)
		PHYDEBUG("CLKMACRO PLL calibration done");
	/* Check for VCO FAIL */
	if (CMU_REG7_VCO_CAL_FAIL_RD(val) == 0x0) {
		PHYDEBUG("CLKMACRO VCO calibration successful");
		return 0;
	}
	/* Assert SDS reset for recall calibration if required */
	xgene_sata_in32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, &val);
	xgene_sata_out32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, val);
	PHYERROR("CLKMACRO calibration failed due to VCO failure");
	return -1;
}

/* SATA port 0 - 3 force power down VCO */
void kc_macro_pdown_force_vco(struct xgene_sata_context *ctx)
{
	xgene_sds_toggle1to0(ctx->csr_base + SATA_SERDES_OFFSET,
		KC_CLKMACRO_CMU_REGS_CMU_REG0_ADDR, CMU_REG0_PDOWN_MASK,
		1000); /* FIXME */
	xgene_sds_toggle1to0(ctx->csr_base + SATA_SERDES_OFFSET,
		KC_CLKMACRO_CMU_REGS_CMU_REG32_ADDR,
		CMU_REG32_FORCE_VCOCAL_START_MASK, 0);
}

/* SATA port 4 - 5 PLL initialization */
int kc_sata45_macro_calib_ready_check(struct xgene_sata_context *ctx)
{
	void *pcie_base = ctx->pcie_base;
	u32 val;

	xgene_serdes_calib_ready_check(pcie_base, xgene_sds_pcie_rd,
		xgene_sds_pcie_wr, xgene_sds_pcie_toggle1to0);

	/* PLL Calibration DONE */
	xgene_sds_pcie_rd(pcie_base, KC_CLKMACRO_CMU_REGS_CMU_REG7_ADDR, &val);
	if (CMU_REG7_PLL_CALIB_DONE_RD(val) == 0x1)
		PHYDEBUG("CLKMACRO PLL CALIB done");
	/* Check for VCO FAIL */
	if (CMU_REG7_VCO_CAL_FAIL_RD(val) == 0x0) {
		PHYDEBUG("CLKMACRO CALIB successful");
		return 0;
	}
	PHYERROR("CLKMACRO CALIB failed due to VCO failure");
	return -1;
}

int xgene_serdes_macro_cfg(void *csr_serdes,
	void (*serdes_rd)(void *, u32, u32 *),
	void (*serdes_wr)(void *, u32, u32))
{
	u32 val;

	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG34_ADDR, &val);
	val = CMU_REG34_VCO_CAL_VTH_LO_MAX_SET(val, 0x7);
	val = CMU_REG34_VCO_CAL_VTH_HI_MAX_SET(val, 0xd);
	val = CMU_REG34_VCO_CAL_VTH_LO_MIN_SET(val, 0x2);
	val = CMU_REG34_VCO_CAL_VTH_HI_MIN_SET(val, 0x8);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG34_ADDR, val);
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG0_ADDR, &val);
  	val = CMU_REG0_CAL_COUNT_RESOL_SET(val, 0x4);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG0_ADDR, val);
	/* CMU_REG1 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG1_ADDR, &val);
        val = CMU_REG1_PLL_CP_SET(val, 0x1);
        val = CMU_REG1_PLL_CP_SEL_SET(val, 0x5);
        val = CMU_REG1_PLL_MANUALCAL_SET(val, 0x0);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG1_ADDR, val);
	/* CMU_REG2 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG2_ADDR, &val);
  	val = CMU_REG2_PLL_LFRES_SET(val, 0xa);
   	val = CMU_REG2_PLL_FBDIV_SET(val, 0x27);	/* 100Mhz refclk */
   	val = CMU_REG2_PLL_FBDIV_SET(val, 0x4f);	/* 50Mhz refclk */
	val = CMU_REG2_PLL_REFDIV_SET(val , 0x1);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG2_ADDR, val);
	/* CMU_REG3 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG3_ADDR, &val);
 	val = CMU_REG3_VCOVARSEL_SET(val, 0xf);
	val = CMU_REG3_VCO_MOMSEL_INIT_SET(val, 0x10);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG3_ADDR, val);
	/* CMU_REG26  */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG26_ADDR, &val);
        val = CMU_REG26_FORCE_PLL_LOCK_SET(val, 0x0);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG26_ADDR, val);
	/* CMU_REG5 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG5_ADDR, &val);
	val = CMU_REG5_PLL_LFSMCAP_SET(val, 0x3);
	val = CMU_REG5_PLL_LFCAP_SET(val, 0x3);
	val = CMU_REG5_PLL_LOCK_RESOLUTION_SET(val, 0x7);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG5_ADDR, val);
	/* CMU_reg6 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG6_ADDR, &val);
	val = CMU_REG6_PLL_VREGTRIM_SET(val, 0x0);
	val = CMU_REG6_MAN_PVT_CAL_SET(val, 0x1);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG6_ADDR, val);
	/* CMU_reg16 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG16_ADDR, &val);
	val = CMU_REG16_CALIBRATION_DONE_OVERRIDE_SET(val, 0x1);
        val = CMU_REG16_BYPASS_PLL_LOCK_SET(val, 0x1);
	val = CMU_REG16_VCOCAL_WAIT_BTW_CODE_SET(val, 0x4);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG16_ADDR, val);
	/* CMU_reg30 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG30_ADDR, &val);
	val = CMU_REG30_PCIE_MODE_SET(val, 0x0);
	val = CMU_REG30_LOCK_COUNT_SET(val, 0x3);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG30_ADDR, val);
	/* CMU reg31 */
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG31_ADDR, 0xF);
	/* CMU_reg32 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG32_ADDR, &val);
	val = CMU_REG32_PVT_CAL_WAIT_SEL_SET(val, 0x3);
	val = CMU_REG32_IREF_ADJ_SET(val, 0x3);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG32_ADDR, val);
	/* CMU_reg34 */
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG34_ADDR, 0x8d27);
	/* CMU_reg37 */
	serdes_rd(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG37_ADDR, &val);
	serdes_wr(csr_serdes, KC_CLKMACRO_CMU_REGS_CMU_REG37_ADDR, 0xF00F);

	return 0;
}

/* SATA port 0 - 3 macro configuration */
int kc_macro_cfg(struct xgene_sata_context *ctx)
{
  	void *csr_serdes = ctx->csr_base + SATA_SERDES_OFFSET;
     	int calib_loop_count = 0;
	u32 val;

  	xgene_sata_in32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, &val);
  	val = I_RESET_B_SET(val, 0x0);
  	val = I_PLL_FBDIV_SET(val, 0x27);
  	val = I_CUSTOMEROV_SET(val, 0x0);
  	xgene_sata_out32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, val);

	xgene_serdes_macro_cfg(csr_serdes, xgene_sds_rd, xgene_sds_wr);

 	xgene_sata_in32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, &val);
  	val = I_RESET_B_SET(val, 0x1);
  	val = I_CUSTOMEROV_SET(val, 0x0);
 	xgene_sata_out32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, val);

  	xgene_sata_delayus(800000); /* FIXME */
      	while (++calib_loop_count <= 5) {
	  	if (kc_macro_calib_ready_check(ctx) == 0)
			break;
		kc_macro_pdown_force_vco(ctx);
	}
	if (calib_loop_count > 5)
		return -1;
 	xgene_sata_in32(csr_serdes + SATA_ENET_CLK_MACRO_REG_ADDR, &val);
	PHYDEBUG("PLL CLKMACRO %sLOOKED...", O_PLL_LOCK_RD(val) ? "" : "UN");
	PHYDEBUG("PLL CLKMACRO %sREADY...",
		O_PLL_READY_RD(val) ? "" : "NOT");

	return 0;
}

/* SATA port 4 - 5 force power down VCO */
void kc_sata45_macro_pdown_force_vco(struct xgene_sata_context *ctx)
{
	xgene_sds_pcie_toggle1to0(ctx->pcie_base,
		KC_CLKMACRO_CMU_REGS_CMU_REG0_ADDR, CMU_REG0_PDOWN_MASK,
		1000); /* FIXME */
	xgene_sds_pcie_toggle1to0(ctx->pcie_base,
		KC_CLKMACRO_CMU_REGS_CMU_REG32_ADDR,
		CMU_REG32_FORCE_VCOCAL_START_MASK, 0);
}

void sata45_config_internal_clk(struct xgene_sata_context * ctx)
{
	void *pcie_base = ctx->pcie_base;

	xgene_sata_out32_flush(pcie_base + SM_PCIE_CLKRST_CSR_PCIE_CLKEN_ADDR,
		0xff);
	xgene_sata_out32_flush(pcie_base + SM_PCIE_CLKRST_CSR_PCIE_SRST_ADDR,
		0x00);
}

void sata45_reset_cmos(struct xgene_sata_context *ctx)
{
	void *pcie_base = ctx->pcie_base;

	xgene_sata_out32(pcie_base + SM_PCIE_CLKRST_CSR_PCIE_CLKEN_ADDR, 0x00);
	xgene_sata_out32(pcie_base + SM_PCIE_CLKRST_CSR_PCIE_SRST_ADDR, 0xff);
}

/* SATA port 4 - 5 macro configuration */
int sata45_kc_macro_cfg(struct xgene_sata_context *ctx)
{
   	void *pcie_base = ctx->pcie_base;
     	int calib_loop_count;
	u32 val;

   	sata45_config_internal_clk(ctx);

  	xgene_sata_in32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, &val);
  	val = PCIE_CLK_MACRO_REG_I_RESET_B_SET(val, 0x0);
  	val = PCIE_CLK_MACRO_REG_I_CUSTOMEROV_SET(val, 0x0);
 	xgene_sata_out32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, val);
  	xgene_sata_in32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, &val);

	xgene_serdes_macro_cfg(pcie_base, xgene_sds_pcie_rd, xgene_sds_pcie_wr);

  	xgene_sata_in32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, &val);
        val = PCIE_CLK_MACRO_REG_I_RESET_B_SET(val, 0x1);
        val = PCIE_CLK_MACRO_REG_I_CUSTOMEROV_SET(val, 0x0);
        xgene_sata_out32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, val);

	xgene_sata_delayus(8000); /* FIXME */
	calib_loop_count = 5;
	do {
		if (kc_sata45_macro_calib_ready_check(ctx) == 0)
			break;
		kc_sata45_macro_pdown_force_vco(ctx);
		xgene_sata_delayus(8000); /* FIXME */
	} while (--calib_loop_count > 0);
	if (calib_loop_count <= 0)
		return -1;
	xgene_sata_in32(pcie_base +
		SM_PCIE_X8_SDS_CSR_REGS_PCIE_CLK_MACRO_REG_ADDR, &val);
	PHYDEBUG("PLL CLKMACRO %sLOOKED...",
		PCIE_CLK_MACRO_REG_O_PLL_LOCK_RD(val) ? "" : "UN");
	PHYDEBUG("PLL CLKMACRO %sREADY...",
		PCIE_CLK_MACRO_REG_O_PLL_READY_RD(val) ? "" : "NOT ");

	return 0;
}

static void apm_sata_clk_rst_pre(struct xgene_sata_context *ctx)
{
        u32 val;
        void *clkcsr_base = ctx->csr_base + SATA_CLK_OFFSET;

	xgene_sata_in32(clkcsr_base + SATACLKENREG_ADDR, &val);
	PHYDEBUG("SATA%d controller clock enable", ctx->cid);
	/* disable all reset */
	xgene_sata_out32_flush(clkcsr_base + SATASRESETREG_ADDR, 0x00);

	/* Enable all resets */
	xgene_sata_out32_flush(clkcsr_base + SATASRESETREG_ADDR, 0xff);

	/* Disable all clks */
	xgene_sata_out32_flush(clkcsr_base + SATACLKENREG_ADDR, 0x00);

	/* Enable all clks */
	xgene_sata_out32_flush(clkcsr_base + SATACLKENREG_ADDR, 0xf9);

	/* Get out of reset for:
	 * 	SDS, CSR
	 *
	 * CORE & MEM are still reset
	 */
	xgene_sata_in32(clkcsr_base + SATASRESETREG_ADDR, &val);
	if (SATA_MEM_RESET_RD(val) == 1) {
		val &= ~(SATA_CSR_RESET_MASK | SATA_SDS_RESET_MASK );
		val |= SATA_CORE_RESET_MASK | SATA_PCLK_RESET_MASK |
			SATA_PMCLK_RESET_MASK | SATA_MEM_RESET_MASK;
	}
	xgene_sata_out32_flush(clkcsr_base + SATASRESETREG_ADDR, val);
}

void xgene_sata_serdes_reset_rxa_rxd(struct xgene_sata_context *ctx, int chan)
{
	void *csr_base = ctx->csr_base + SATA_SERDES_OFFSET;

	xgene_sds_clrbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + chan * 0x200,
		CH0_RXTX_REG7_RESETB_RXD_MASK);
	xgene_sds_clrbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + chan * 0x200,
		CH0_RXTX_REG7_RESETB_RXA_MASK);
	xgene_sds_setbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + chan * 0x200,
		CH0_RXTX_REG7_RESETB_RXA_MASK);
	xgene_sds_setbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + chan * 0x200,
		CH0_RXTX_REG7_RESETB_RXD_MASK);
}

void  sata_sm_deass_pclk_reset(struct xgene_sata_context *ctx)
{
	void *clkcsr_base = ctx->csr_base + SATA_CLK_OFFSET;
	u32 val;

	xgene_sata_in32(clkcsr_base + SATASRESETREG_ADDR, &val);
	val &= ~SATA_PCLK_RESET_MASK;
	xgene_sata_out32(clkcsr_base + SATASRESETREG_ADDR, val);
}

void sata_sm_dis_sds_pmclk_core_reset(struct xgene_sata_context *ctx)
{
	void *clkcsr_base = ctx->csr_base + SATA_CLK_OFFSET;
	u32 val;

	xgene_sata_in32(clkcsr_base + SATASRESETREG_ADDR, &val);
	val &= ~(SATA_CORE_RESET_MASK |
		SATA_PMCLK_RESET_MASK |
		SATA_SDS_RESET_MASK);
	xgene_sata_out32(clkcsr_base + SATASRESETREG_ADDR, val);
}

void xgene_sata_force_lat_summer_cal(struct xgene_sata_context *ctx,
	int channel)
{
  	void *csr_base = ctx->csr_base + SATA_SERDES_OFFSET;
	u32 os = channel * 0x200;
	int i;
	struct {
		u32 reg;
		u32 val;
	} serdes_reg[] = {
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG38_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG39_ADDR, 0xff00 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG40_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG41_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG42_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG43_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG44_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG45_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG46_ADDR, 0xffff },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG47_ADDR, 0xfffc },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG48_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG49_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG50_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG51_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG52_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG53_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG54_ADDR, 0x0 },
		{ KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG55_ADDR, 0x0 },
		{ 0, 0x0 },
	};

	/* SUMMER CALIBRATION CH0/CH1 */
	/* SUMMER calib toggle CHX */
	xgene_sds_toggle1to0(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os,
		CH0_RXTX_REG127_FORCE_SUM_CAL_START_MASK, 0);
	/* latch calib toggle CHX */
	xgene_sds_toggle1to0(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os,
		CH0_RXTX_REG127_FORCE_LAT_CAL_START_MASK, 0);
	/* CHX */
	xgene_sds_wr(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG28_ADDR + os, 0x7);
	xgene_sds_wr(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG31_ADDR + os, 0x7e00);

	xgene_sds_clrbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR + os,
		CH0_RXTX_REG4_TX_LOOPBACK_BUF_EN_MASK);
	xgene_sds_clrbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + os,
		CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_MASK);

	/* RXTX_REG38-55 */
	for (i = 0; serdes_reg[i].reg != 0; i++)
		xgene_sds_wr(csr_base, serdes_reg[i].reg + os,
			serdes_reg[i].val);
}

void force_lat_summer_cal_get_avg(struct xgene_sata_context *ctx, int chan)
{
	void *csr_serdes_base = ctx->csr_base + SATA_SERDES_OFFSET;
	u32 os = chan * 0x200;

	/* SUMMer calib toggle */
	xgene_sds_toggle1to0(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os,
		CH0_RXTX_REG127_FORCE_SUM_CAL_START_MASK, 0);
	xgene_sds_toggle1to0(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os,
		CH0_RXTX_REG127_FORCE_LAT_CAL_START_MASK, 0);
	xgene_sds_clrbits(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR + os,
		CH0_RXTX_REG4_TX_LOOPBACK_BUF_EN_MASK);
	xgene_sds_clrbits(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + os,
		CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_MASK);

	/* removing loopback after calibration cycle */
	xgene_sds_clrbits(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR + os,
		CH0_RXTX_REG4_TX_LOOPBACK_BUF_EN_MASK);
	xgene_sds_clrbits(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + os,
		CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_MASK);
	/* RXTX_REG38 */
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG38_ADDR + os, 0x0);
}

int get_avg(int accum,int samples)
{
	return ((accum + (samples / 2)) / samples);
}

static void xgene_serdes_reset_rxd(struct xgene_sata_context *ctx, int channel)
{
	void *csr_base = ctx->csr_base + SATA_SERDES_OFFSET;

	xgene_sds_clrbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + channel*0x200,
		CH0_RXTX_REG7_RESETB_RXD_MASK);
	xgene_sds_setbits(csr_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + channel*0x200,
		CH0_RXTX_REG7_RESETB_RXD_MASK);
}

void xgene_sata_gen_avg_val(struct xgene_sata_context *ctx, int channel)
{
	void *csr_serdes_base = ctx->csr_base + SATA_SERDES_OFFSET;
	int avg_loop = 10;
	int MAX_LOOP = 10;
	int lat_do = 0, lat_xo = 0, lat_eo = 0, lat_so = 0;
	int lat_de = 0, lat_xe = 0, lat_ee = 0, lat_se = 0;
	int sum_cal = 0;
	int lat_do_itr = 0, lat_xo_itr = 0, lat_eo_itr = 0, lat_so_itr = 0;
	int lat_de_itr = 0, lat_xe_itr = 0, lat_ee_itr = 0, lat_se_itr = 0;
	int sum_cal_itr = 0;
	int fail_even = 0;
	int fail_odd = 0;
	u32 val;
	u32 os;

	PHYDEBUG("Generating average calibration value for port %d", channel);

	os = channel * 0x200;

	/* Enable RX Hi-Z termination enable */
	xgene_sds_setbits(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR + os,
		CH0_RXTX_REG12_RX_DET_TERM_ENABLE_MASK);
	/* Turn off DFE */
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG28_ADDR + os, 0x0);
	/* DFE Presets to zero */
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG31_ADDR + os, 0x0);

	while (avg_loop > 0) {
		xgene_sata_force_lat_summer_cal(ctx, channel);

		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG21_ADDR + os, &val);
		lat_do_itr = CH0_RXTX_REG21_DO_LATCH_CALOUT_RD(val);
		lat_xo_itr = CH0_RXTX_REG21_XO_LATCH_CALOUT_RD(val);
		fail_odd   = CH0_RXTX_REG21_LATCH_CAL_FAIL_ODD_RD(val);

		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG22_ADDR + os, &val);
		lat_eo_itr = CH0_RXTX_REG22_EO_LATCH_CALOUT_RD(val);
		lat_so_itr = CH0_RXTX_REG22_SO_LATCH_CALOUT_RD(val);
		fail_even  = CH0_RXTX_REG22_LATCH_CAL_FAIL_EVEN_RD(val);

		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG23_ADDR + os, &val);
		lat_de_itr = CH0_RXTX_REG23_DE_LATCH_CALOUT_RD(val);
		lat_xe_itr = CH0_RXTX_REG23_XE_LATCH_CALOUT_RD(val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG24_ADDR + os, &val);
		lat_ee_itr = CH0_RXTX_REG24_EE_LATCH_CALOUT_RD(val);
		lat_se_itr = CH0_RXTX_REG24_SE_LATCH_CALOUT_RD(val);

		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG121_ADDR + os, &val);
		sum_cal_itr = CH0_RXTX_REG121_SUMOS_CAL_CODE_RD(val);

		if ((fail_even == 0 || fail_even == 1) &&
		    (fail_odd == 0 || fail_odd == 1)) {
			lat_do += lat_do_itr;
			lat_xo += lat_xo_itr;
			lat_eo += lat_eo_itr;
			lat_so += lat_so_itr;
			lat_de += lat_de_itr;
			lat_xe += lat_xe_itr;
			lat_ee += lat_ee_itr;
			lat_se += lat_se_itr;
			sum_cal += sum_cal_itr;

			PHYDEBUG("Interation Value: %d", avg_loop);
			PHYDEBUG("DO 0x%x XO 0x%x EO 0x%x SO 0x%x", lat_do_itr,
				lat_xo_itr, lat_eo_itr, lat_so_itr);
			PHYDEBUG("DE 0x%x XE 0x%x EE 0x%x SE 0x%x", lat_de_itr,
				lat_xe_itr, lat_ee_itr, lat_se_itr);
			PHYDEBUG("sum_cal 0x%x", sum_cal_itr);
			avg_loop--;
		} else {
			PHYDEBUG("Interation Failed %d", avg_loop);
		}
		xgene_serdes_reset_rxd(ctx, channel);
    	}

	/* Update with Average Value */
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, &val);
	val = CH0_RXTX_REG127_DO_LATCH_MANCAL_SET(val,
		get_avg(lat_do, MAX_LOOP));
	val = CH0_RXTX_REG127_XO_LATCH_MANCAL_SET(val,
		get_avg(lat_xo, MAX_LOOP));
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, val);

	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG128_ADDR + os, &val);
	val = CH0_RXTX_REG128_EO_LATCH_MANCAL_SET(val,
		get_avg(lat_eo, MAX_LOOP));
	val = CH0_RXTX_REG128_SO_LATCH_MANCAL_SET(val,
		get_avg(lat_so, MAX_LOOP));
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG128_ADDR + os, val);
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG129_ADDR + os, &val);
	val = CH0_RXTX_REG129_DE_LATCH_MANCAL_SET(val,
		get_avg(lat_de, MAX_LOOP));
	val = CH0_RXTX_REG129_XE_LATCH_MANCAL_SET(val,
		get_avg(lat_xe, MAX_LOOP));
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG129_ADDR + os, val);

	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG130_ADDR + os, &val);
	val = CH0_RXTX_REG130_EE_LATCH_MANCAL_SET(val,
		get_avg(lat_ee, MAX_LOOP));
	val = CH0_RXTX_REG130_SE_LATCH_MANCAL_SET(val,
		get_avg(lat_se, MAX_LOOP));
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG130_ADDR + os, val);
	/* Summer Calibration Value */
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG14_ADDR + os, &val);
	val = CH0_RXTX_REG14_CLTE_LATCAL_MAN_PROG_SET(val,
		get_avg(sum_cal, MAX_LOOP));
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG14_ADDR + os, val);

	PHYDEBUG("Average Value:");
	PHYDEBUG("DO 0x%x XO 0x%x EO 0x%x SO 0x%x", get_avg(lat_do, MAX_LOOP),
		get_avg(lat_xo, MAX_LOOP), get_avg(lat_eo,MAX_LOOP),
		get_avg(lat_so, MAX_LOOP));
	PHYDEBUG("DE 0x%x XE 0x%x EE 0x%x SE 0x%x", get_avg(lat_de, MAX_LOOP),
		get_avg(lat_xe, MAX_LOOP), get_avg(lat_ee, MAX_LOOP),
		get_avg(lat_se, MAX_LOOP));
	PHYDEBUG("sum_cal 0x%x", get_avg(sum_cal, MAX_LOOP));

	/* Manual Summer Calibration */
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG14_ADDR + os, &val);
	val = CH0_RXTX_REG14_CTLE_LATCAL_MAN_ENA_SET(val, 0x1);
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG14_ADDR + os, val);

	PHYDEBUG("Manual Summer calibration enabled");
	xgene_sata_delayus(122);

	/* Manual Latch Calibration */
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, &val);
	val = CH0_RXTX_REG127_LATCH_MAN_CAL_ENA_SET(val, 0x1);
	PHYDEBUG("Manual Latch Calibration Enabled");
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, val);
	xgene_sata_delayus(122);

	/* Disable RX Hi-Z termination enable */
	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR + os, &val);
	val = CH0_RXTX_REG12_RX_DET_TERM_ENABLE_SET(val, 0);
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR + os, val);

	/* Turn on DFE */
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG28_ADDR + os, 0x0000);

	/* DFE Presets to 0 */
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG31_ADDR + os, 0x0000);
}

static int apm_serdes_host_sata_select(struct xgene_sata_context *ctx)
{
	void *muxcsr_base = ctx->csr_base + SATA_ETH_MUX_OFFSET;
	u32 val;

	PHYDEBUG("SATA%d select SATA MUX", ctx->cid);
        xgene_sata_in32(muxcsr_base + SATA_ENET_CONFIG_REG_ADDR, &val);
        val &= ~CFG_SATA_ENET_SELECT_MASK;
        xgene_sata_out32(muxcsr_base + SATA_ENET_CONFIG_REG_ADDR, val);
        xgene_sata_in32(muxcsr_base + SATA_ENET_CONFIG_REG_ADDR, &val);
        return val & CFG_SATA_ENET_SELECT_MASK ? -1 : 0;
}

static void xgene_serdes_validation_CMU_cfg(struct xgene_sata_context *ctx)
{
	void *csr_base = ctx->csr_base + SATA_SERDES_OFFSET;
        u32 val;
	char *chip_revision = apm88xxx_chip_revision();

	/* CMU_reg0[0] = 0      - pciegen3
	 * CMU_reg0[7:5] = 111	- cal_count_resol
         */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG0_ADDR, &val);
        val = CMU_REG0_CAL_COUNT_RESOL_SET(val, 0x4);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG0_ADDR, val);

	/* CMU_reg1[13:10] = 0xF = 1111   -  pll_cp[3:0]
	 * CMU_reg1[9:5]   = 0xC = 0 1100 -  pll_cp_sel[4:0]
	 * CMU_reg1[3]     = 0            -  pll_manualcal
	 */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG1_ADDR, &val);
	val= CMU_REG1_PLL_CP_SET(val, 0x1);
	val= CMU_REG1_PLL_CP_SEL_SET(val, 0x5);
	val = CMU_REG1_PLL_MANUALCAL_SET(val, 0x0);
	xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG1_ADDR, val);

	/* CMU_reg2[15:14] = 00   		- pll_refdiv[1:0]
	 * CMU_reg2[13:5]  = 0x3B = 00011 1011 	- pll_fbdiv[8:0]
	 * CMU_reg2[4:1]   = 0x2  = 0010  	- pll_lfres[3:0]
	 */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG2_ADDR, &val);
	val=CMU_REG2_PLL_LFRES_SET(val, 0xa);
	val=CMU_REG2_PLL_FBDIV_SET(val, FBDIV_VAL);
	val=CMU_REG2_PLL_REFDIV_SET(val, REFDIV_VAL);
	xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG2_ADDR, val);

	/* CMU_reg3[15:10] = 0x1C = 01 1100  vco_manmomsel[5:0]
	 * CMU_reg3[9:4]   = 0x10 = 01 0000  vco_momsel_init[5:0]
	 * CMU_reg3[3:0]   = 0x1  = 0001     vcovarsel[3:0]
	 * CMU_reg3[15:0]  = 0111 0001 0000 0001 = 0x7101
	 */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG3_ADDR, &val);
	val = CMU_REG3_VCOVARSEL_SET(val, 0xF);
	val = CMU_REG3_VCO_MOMSEL_INIT_SET(val, 0x15);
	val = CMU_REG3_VCO_MANMOMSEL_SET(val, 0x15);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG3_ADDR, val);

	xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG26_ADDR, &val);
	val = CMU_REG26_FORCE_PLL_LOCK_SET(val, 0x0);
	xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG26_ADDR,val);

	/* CMU_reg5[15:14] = 00  pll_lfsmcap[1:0]
	 * CMU_reg5[13:12] = 00  pll_lfcap[1:0]
 	 */
	xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG5_ADDR, &val);
	val = CMU_REG5_PLL_LFSMCAP_SET(val, 0x3);
	val = CMU_REG5_PLL_LFCAP_SET(val, 0x3);
	if (strcmp(chip_revision, "A1") == 0)
		val = CMU_REG5_PLL_LOCK_RESOLUTION_SET(val, 0x7);
	else
		val = CMU_REG5_PLL_LOCK_RESOLUTION_SET(val, 0x4);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG5_ADDR, val);

	/* CMU_reg6[10:9] = 00 pll_vregtrim[1:0] */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG6_ADDR, &val);
	val = CMU_REG6_PLL_VREGTRIM_SET(val, 0x0);
	val = CMU_REG6_MAN_PVT_CAL_SET(val, 0x1);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG6_ADDR, val);

	/* CMU_reg9[3]   = 1  pll_post_divby2 */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG9_ADDR, &val);
	val = CMU_REG9_TX_WORD_MODE_CH1_SET(val, 0x3);
	val = CMU_REG9_TX_WORD_MODE_CH0_SET(val, 0x3);
	val = CMU_REG9_PLL_POST_DIVBY2_SET(val, 0x1);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG9_ADDR, val);

	/* CMU_reg16[4:2] = 111   vcocal_wait_btw_code[2:0] */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG16_ADDR, &val);
	val = CMU_REG16_CALIBRATION_DONE_OVERRIDE_SET(val, 0x1);
	val = CMU_REG16_BYPASS_PLL_LOCK_SET(val, 0x1);
	val = CMU_REG16_VCOCAL_WAIT_BTW_CODE_SET(val, 0x4);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG16_ADDR, val);

	/* CMU_reg30[3] = 0   pciegen3
	 * CMU_reg30[2:1] = 11  lock_count[1:0]
	 */
        xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG30_ADDR, &val);
	val = CMU_REG30_PCIE_MODE_SET(val, 0x0);
	val = CMU_REG30_LOCK_COUNT_SET(val, 0x3);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG30_ADDR, val);

	/* CMU_reg31[3:0] = 1111 los_override_ch0-ch3 */
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG31_ADDR, 0xF);

	/* CMU_reg32[2:1] = 11  pvt_cal_wait_sel[1:0]
	 * CMU_reg32[8:7] = 11  iref_adj[1:0]
	 */
	xgene_sds_rd(csr_base, KC_SERDES_CMU_REGS_CMU_REG32_ADDR, &val);
	if (strcmp(chip_revision, "A1") == 0)
		val |= 0x0006 | 0x0180;
	val = CMU_REG32_PVT_CAL_WAIT_SEL_SET(val, 0x3);
	val = CMU_REG32_IREF_ADJ_SET(val, 0x3);
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG32_ADDR, val);

	/* CMU_reg34[15:12] = 0010
	 * CMU_reg34[11:8]  = 1010  vco_cal_vth_hi_max[3:0]
	 * CMU_reg34[7:4]   = 0010  vco_cal_vth_lo_min[3:0]
	 * CMU_reg34[3:0]   = 1010  vco_cal_vth_lo_max[3:0]
	 */
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG34_ADDR, 0x8d27);

	/* CMU_reg37[15:12] = 1111  CTLE_cal_done_ovr[3:0]
	 * CMU_reg37[3:0]   = 1111  FT_search_done_ovr[3:0]
	 */
        xgene_sds_wr(csr_base, KC_SERDES_CMU_REGS_CMU_REG37_ADDR, 0xF00F);
}

static void xgene_serdes_validation_rxtx_cfg(struct xgene_sata_context *ctx,
	int gen_sel)
{
	void *csr_base = ctx->csr_base + SATA_SERDES_OFFSET;
	char *chip_revision = apm88xxx_chip_revision();
        u32 val;
        u32 reg;
        int i;
	int chan;

	for (chan = 0; chan < 2; chan++) {
		u32 os = chan * 0x200;

		if (strcmp(chip_revision, "A1") == 0) {
			xgene_sds_wr(csr_base,
				KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG38_ADDR + os,
				0x40);
			xgene_sds_wr(csr_base,
				KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG38_ADDR + os,
				0x41);
		}

	        /* rxtx_reg147[15:0] =666666    STMC_OVERRIDE[15:0] */
		xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG147_ADDR + os, 0x6);

                /* rxtx_reg0[15:11] = 0x10        CTLE_EQ_HR[4:0]
                 * rxtx_reg0[10:6]  = 0x10        CTLE_EQ_QR[4:0]
                 * rxtx_reg0[5:1]   = 0x10        CTLE_EQ_FR[4:0]
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + os, &val);
                val = CH0_RXTX_REG0_CTLE_EQ_HR_SET(val, 0x10);
		val = CH0_RXTX_REG0_CTLE_EQ_QR_SET(val, 0x10);
		val = CH0_RXTX_REG0_CTLE_EQ_FR_SET(val, 0x10);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + os, val);

                /* rxtx_reg1[15:12] = 0x7   rxacvcm[3:0]
                 * rxtx_reg1[11:7]  = 0x1C  CTLE_EQ[4:0]
                 */
		xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG1_ADDR + os, &val);
             	val = CH0_RXTX_REG1_RXACVCM_SET(val, 0x7);
		if (strcmp(chip_revision, "A1") == 0)
			val = CH0_RXTX_REG1_CTLE_EQ_SET(val, ctx->ctrl_eq_A1);
		else
			val = CH0_RXTX_REG1_CTLE_EQ_SET(val, ctx->ctrl_eq);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG1_ADDR + os, val);

                /* rxtx_reg2[14]  = 0  'Resetb_term'
                 * rxtx_reg2[12]  = 1 (default) Resetb_TXD
                 * rxtx_reg2[11]  = 0 (default) tx_fifo_ena
                 * rxtx_reg2[10]  = 0 (default) tx_inv
                 * rxtx_reg2[8]   = 1   vtt_ena
                 *
                 * Termination to Ground
                 * rxtx_reg2[7:6] = 1  vtt_sel
                 *
                 * rxtx_reg2[5]   = 1  tx_fifo_ena
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG2_ADDR + os, &val);
        	val = CH0_RXTX_REG2_VTT_ENA_SET(val, 0x1);
		val = CH0_RXTX_REG2_VTT_SEL_SET(val, 0x1);
		val = CH0_RXTX_REG2_TX_FIFO_ENA_SET(val, 0x1);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG2_ADDR + os, val);

                /* rxtx_reg4[15:14] = 2 for GEN 1 (Quarter rate)
                 * 		    = 1 for GEN 2 (Half rate)
                 * 		    = 0 for GEN 3 (Full rate)
                 * rxtx_reg4[13:11] = 3
                 * rxtx_reg4[10:8]  = 4 (default) TX_PRBS_sel[2:0]
                 * rxtx_reg4[6]     = 0     tx_loopback_buf_en
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR + os, &val);
		val = CH0_RXTX_REG4_TX_WORD_MODE_SET(val, 0x3);
		if (strcmp(chip_revision, "A1") == 0)
			val = CH1_RXTX_REG4_TX_LOOPBACK_BUF_EN_SET(val,
				ctx->loopback_buf_en_A1);
		xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG4_ADDR + os, val);

                /* rxtx_reg5[15:11] = 6   tx_cn1[4:0]
                 * rxtx_reg5[10:5]  = 4   tx_cp1[5:0]
                 * rxtx_reg5[4:0]   = 0   tx_cn2[4:0]
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG5_ADDR + os, &val);
	        val = CH0_RXTX_REG5_TX_CN1_SET(val, 0x0);
		val = CH0_RXTX_REG5_TX_CP1_SET(val, 0xF);
		val = CH0_RXTX_REG5_TX_CN2_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG5_ADDR + os, val);

                /* rxtx_reg6[10:7]  = 8   txamp_cntl[3:0]
                 * rxtx_reg6[6]  = 1    txamp_ena
                 * rxtx_reg6[3] = 0
                 * rxtx_reg6[1] = 0
                 * rxtx_reg6[0] = 0   rx_bist_errcnt_rd
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG6_ADDR + os, &val);
            	val = CH0_RXTX_REG6_TXAMP_CNTL_SET(val, 0xf);
		val = CH0_RXTX_REG6_TXAMP_ENA_SET(val, 0x1);
		val = CH0_RXTX_REG6_TX_IDLE_SET (val, 0x0);
		val = CH0_RXTX_REG6_RX_BIST_RESYNC_SET(val, 0x0);
		val = CH0_RXTX_REG6_RX_BIST_ERRCNT_RD_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG6_ADDR + os, val);

                /* rxtx_reg7[14]    = 0  Loop_back_ena_ctle
                 * rxtx_reg7[13:11] = 3  RX_word_mode[2:0]
                 *
                 * RX Rate divider select
                 * rxtx_reg7[10:9] 	= 2 for GEN 1 (Quarter rate)
                 * 			= 1 for GEN 2 (Half rate)
                 * 			= 0 for GEN 3 (Full rate)
                 *
                 * rxtx_reg7[8]  	= 1  resetb_rxd
                 * rxtx_reg7[6]  	= 0  st_ena_rx
                 *
                 * RX Word modes select (20bit)
                 * rxtx_reg7[5:3]  	= 4 RX_PRBS_sel[2:0]
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + os, &val);
		val = CH0_RXTX_REG7_BIST_ENA_RX_SET(val, 0x0);
		if (strcmp(chip_revision, "A1") == 0)
			val = CH0_RXTX_REG7_LOOP_BACK_ENA_CTLE_SET(val,
				ctx->loopback_ena_ctle_A1);
		val = CH0_RXTX_REG7_RX_WORD_MODE_SET(val, 0x3);
		xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG7_ADDR + os, val);

                /* rxtx_reg8[14] = 1    CDR_Loop_ena
                 * rxtx_reg8[11] = 0    cdr_bypass_rxlos
                 * rxtx_reg8[9]  = 0    SSC_enable
                 * rxtx_reg8[8]  = 1    sd_disable => set to 0 sata working => Tao changed 31/1/2013
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG8_ADDR + os, &val);
                val = CH0_RXTX_REG8_CDR_LOOP_ENA_SET(val, 0x1);
		val = CH0_RXTX_REG8_CDR_BYPASS_RXLOS_SET(val, 0x0);
		val = CH0_RXTX_REG8_SSC_ENABLE_SET(val, 0x1);
		val = CH0_RXTX_REG8_SD_DISABLE_SET(val,0x0);
		val = CH0_RXTX_REG8_SD_VREF_SET(val, 0x4);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG8_ADDR + os, val);

                /* rxtx_reg11[15:11] = 0    phase_adjust_limit[4:0] */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG11_ADDR + os, &val);
                val = CH0_RXTX_REG11_PHASE_ADJUST_LIMIT_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG11_ADDR + os, val);

                /* rxtx_reg12[13] = 1  Latch_off_ena
                 * rxtx_reg12[11] = 0  rx_inv
                 * rxtx_reg12[2]  = 0  sumos_enable
                 * rxtx_reg12[1]  = 0  rx_det_term_enable
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR + os, &val);
                val = CH0_RXTX_REG12_LATCH_OFF_ENA_SET(val, 0x1);
		val = CH0_RXTX_REG12_SUMOS_ENABLE_SET(val, 0x0);
		val = CH0_RXTX_REG12_RX_DET_TERM_ENABLE_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG12_ADDR + os, val);

                 /* rxtx_reg26[3] = 1   blwc_ena */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG26_ADDR + os, &val);
		val = CH0_RXTX_REG26_PERIOD_ERROR_LATCH_SET(val, 0x0);
		val = CH0_RXTX_REG26_BLWC_ENA_SET (val, 0x1);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG26_ADDR + os, val);

                 /* rxtx_reg28[15:0] = 0xFFFF   DFE_tap_ena[15:0] */
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG28_ADDR + os, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG31_ADDR + os, 0x0);

                /* RXTX REG39-55 */
		if (strcmp(chip_revision, "A1") == 0) {
			for (i = 0; i < 17; i++)
				xgene_sds_wr(csr_base,
					KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG39_ADDR + os + i*2,
					0x0000);
		}
		/* Speed Select for Different Data Standards.
                 * rxtx_reg61[13:10]  	= 2 for GEN 1SPD_sel_cdr[3:0]
                 * 			= 4 for GEN 2
                 * 			= 7 for GEN 3
                 * Reset bert logic (bert_resetb)
                 * rxtx_reg61[5] = 0   bert_resetb
                 *
                 * rxtx_reg61[3] = 0
                 *
                 * Pll Select for RX/TX
                 * rxtx_reg61[15] = Don't Care 	rx_hsls_pll_SELECT
                 * rxtx_reg61[14] = Don't Care
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG61_ADDR + os, &val);
		val = CH0_RXTX_REG61_ISCAN_INBERT_SET(val, 0x1);
		if (strcmp(chip_revision, "A1") == 0)
			val = CH0_RXTX_REG61_SPD_SEL_CDR_SET(val,
				ctx->spd_sel_cdr_A1);
		val = CH0_RXTX_REG61_LOADFREQ_SHIFT_SET(val, 0x0);
		val = CH0_RXTX_REG61_EYE_COUNT_WIDTH_SEL_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG61_ADDR + os, val);

                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG62_ADDR + os, &val);
          	val = CH0_RXTX_REG62_PERIOD_H1_QLATCH_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG62_ADDR + os, val);

                /* rxtx_reg81-89[15:11] = 0xE
                 * rxtx_reg81-89[10:6] 	= 0xE
                 * rxtx_reg81-89[5:1] 	= 0xE
                 */
                for (i = 0; i < 9; i++) {
			reg = KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG81_ADDR +
				os + i * 2;
			xgene_sds_rd(csr_base, reg, &val);
                        val = CH0_RXTX_REG89_MU_TH7_SET(val, 0xe);
                        val = CH0_RXTX_REG89_MU_TH8_SET(val, 0xe);
                        val = CH0_RXTX_REG89_MU_TH9_SET(val, 0xe);
                        xgene_sds_wr(csr_base, reg, val);
                }

                /* rxtx_reg96-98[15:11] = 0x10
                 * rxtx_reg96-98[10:6]	= 0x10
                 * rxtx_reg96-98[5:1] 	= 0x10
                 */
                for (i = 0; i < 3; i++) {
                        reg = KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG96_ADDR +
				os + i * 2;
                        xgene_sds_rd(csr_base, reg, &val);
                        val = CH0_RXTX_REG96_MU_FREQ1_SET(val, 0x10);
                        val = CH0_RXTX_REG96_MU_FREQ2_SET(val, 0x10);
                        val = CH0_RXTX_REG96_MU_FREQ3_SET(val, 0x10);
                        xgene_sds_wr(csr_base, reg, val);
                }

                /* rxtx_reg99-101[15:11] = 0x7
                 * rxtx_reg99-101[10:6]	 = 0x7
                 * rxtx_reg99-101[5:1] 	 = 0x7
                 */
                for (i = 0; i < 3; i++) {
                        reg = KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG99_ADDR +
				os + i * 2;
                        xgene_sds_rd(csr_base, reg, &val);
                       	val = CH0_RXTX_REG99_MU_PHASE1_SET(val, 0x7);
			val = CH0_RXTX_REG99_MU_PHASE2_SET(val, 0x7);
			val = CH0_RXTX_REG99_MU_PHASE3_SET(val, 0x7);
                        xgene_sds_wr(csr_base, reg, val);
                }

                /* rxtx_reg102[6:5] = 2  freqloop_limit[1:0] */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG102_ADDR + os, &val);
		val = CH0_RXTX_REG102_FREQLOOP_LIMIT_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG102_ADDR + os, val);

                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG114_ADDR + os,
			0xffe0);

                /* rxtx_reg125[15:9] = 0xA  pq_reg[6:0]
                 * Manual phase program mode
                 * rxtx_reg125[1] = 1  phz_manual
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG125_ADDR + os, &val);
                val = CH0_RXTX_REG125_SIGN_PQ_SET(val, ctx->pq_sign);
		if (strcmp(chip_revision, "A1") == 0)
	               	val = CH0_RXTX_REG125_PQ_REG_SET(val, ctx->pq_A1);
		else
	               	val = CH0_RXTX_REG125_PQ_REG_SET(val, ctx->pq);
		val = CH0_RXTX_REG125_PHZ_MANUAL_SET(val, 0x1);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG125_ADDR + os, val);

                /* rxtx_reg127[3] = 0 latch_man_cal_ena */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, &val);
                val = CH0_RXTX_REG127_LATCH_MAN_CAL_ENA_SET(val, 0x0);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG127_ADDR + os, val);

                /* rxtx_reg128[3:2] = 3 latch_cal_wait_sel[1:0] */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG128_ADDR + os, &val);
               	val = CH0_RXTX_REG128_LATCH_CAL_WAIT_SEL_SET(val, 0x3);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG128_ADDR + os, val);

                /* rxtx_reg145[15:14] 	= 3   rxdfe_config[1:0]
                 * rxtx_reg145[0] 	= 0
                 */
                xgene_sds_rd(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG145_ADDR + os, &val);
                val = CH0_RXTX_REG145_RXDFE_CONFIG_SET(val, 0x3);
		val = CH0_RXTX_REG145_TX_IDLE_SATA_SET(val, 0x0);
		val = CH0_RXTX_REG145_RXES_ENA_SET(val, 0x1);
		val = CH0_RXTX_REG145_RXVWES_LATENA_SET(val, 0x1);
                xgene_sds_wr(csr_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG145_ADDR + os, val);

                /* rxtx_reg148-151[15:0] = 0xFFFF */
                for (i = 0; i < 4; i++) {
                        reg = KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG148_ADDR +
				os + i * 2;
                        xgene_sds_wr(csr_base, reg, 0xFFFF);
                }
	}
}

static int serdes_calib_ready_check(struct xgene_sata_context *ctx)
{
	void *csr_serdes = ctx->csr_base + SATA_SERDES_OFFSET;
	char *chip_revision = apm88xxx_chip_revision();
	int loopcount;
	u32 val;

	/* 4. relasase serdes main reset */
	xgene_sata_out32_flush(csr_serdes + SATA_ENET_SDS_RST_CTL_ADDR,
			0x000000DF);
	xgene_sata_delayus(8000); /* FIXME */

        /* TERM CALIBRATION KC_SERDES_CMU_REGS_CMU_REG17__ADDR */
        /* TERM calibration for channel 0 */
	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x0d);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, val);
	xgene_sds_toggle1to0(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR,
			CMU_REG17_PVT_TERM_MAN_ENA_MASK, 0);
        /* DOWN CALIBRATION for channel zero */
	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x26);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, val);
	xgene_sds_toggle1to0(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG16_ADDR,
			CMU_REG16_PVT_DN_MAN_ENA_MASK, 0);
        /* UP CALIBRATION for channel 0 */
	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, &val);
	val = CMU_REG17_PVT_CODE_R2A_SET(val, 0x28);
	val = CMU_REG17_RESERVED_7_SET(val, 0x0);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG17_ADDR, val);
	xgene_sds_toggle1to0(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG16_ADDR,
			CMU_REG16_PVT_UP_MAN_ENA_MASK, 0);

	loopcount = 10;
	do {
		xgene_sds_rd(csr_serdes,
			KC_SERDES_CMU_REGS_CMU_REG7_ADDR, &val);
        	if (CMU_REG7_PLL_CALIB_DONE_RD(val))
			break;
		xgene_sata_delayus(2000);
	} while (--loopcount > 0);

	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG7_ADDR, &val);
        if (CMU_REG7_PLL_CALIB_DONE_RD(val) == 1)
                PHYDEBUG("SATA%d SERDES PLL calibration done", ctx->cid);
        if (CMU_REG7_VCO_CAL_FAIL_RD(val) == 0x0) {
        	PHYDEBUG("SERDES CALIB successful");
	} else {
       		/* Assert SDS reset and recall calib function */
		PHYERROR("SERDES CALIB FAILED due to VCO FAIL");
		return -1;
        }
	if (strcmp(chip_revision, "A1") != 0)
		xgene_sata_out32_flush(csr_serdes + SATA_ENET_SDS_RST_CTL_ADDR,
			0x000000DF);

        PHYDEBUG("SATA%d Checking TX ready", ctx->cid);
	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG15_ADDR, &val);
	PHYDEBUG("SERDES TX is %sready", val & 0x0300 ? "" : "NOT ");
	return 0;
}

void serdes_pdown_force_vco(struct xgene_sata_context *ctx)
{
      	void *csr_serdes = ctx->csr_base + SATA_SERDES_OFFSET;
	u32 val;

	PHYDEBUG("serdes power down VCO");
	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG16_ADDR, &val);
	val = CMU_REG16_VCOCAL_WAIT_BTW_CODE_SET(val, 0x5);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG16_ADDR, val);

	xgene_sds_toggle1to0(csr_serdes,
		KC_SERDES_CMU_REGS_CMU_REG0_ADDR, CMU_REG0_PDOWN_MASK,
		1000); /* FIXME */
	xgene_sds_toggle1to0(csr_serdes,
		KC_SERDES_CMU_REGS_CMU_REG32_ADDR,
		CMU_REG32_FORCE_VCOCAL_START_MASK, 0);
}

void xgene_serdes_tx_ssc_enable(struct xgene_sata_context *ctx)
{
        void *csr_serdes = ctx->csr_base + SATA_SERDES_OFFSET;
	u32 val;

	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG35_ADDR, &val);
	val = CMU_REG35_PLL_SSC_MOD_SET(val, 0x5f);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG35_ADDR, val);

	xgene_sds_rd(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG36_ADDR, &val);
	val = CMU_REG36_PLL_SSC_VSTEP_SET(val, 33);  /* Gen3 == 33 */
     	val = CMU_REG36_PLL_SSC_EN_SET(val, 1);
     	val = CMU_REG36_PLL_SSC_DSMSEL_SET(val, 1);
	xgene_sds_wr(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG36_ADDR, val);

	xgene_sds_clrbits(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG5_ADDR,
		CMU_REG5_PLL_RESETB_MASK);
	xgene_sata_delayus(1000);	/* FIXME */
	xgene_sds_setbits(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG5_ADDR,
		CMU_REG5_PLL_RESETB_MASK);
	xgene_sds_toggle1to0(csr_serdes, KC_SERDES_CMU_REGS_CMU_REG32_ADDR,
		CMU_REG32_FORCE_VCOCAL_START_MASK, 0);
}

int xgene_sata_serdes_init(struct xgene_sata_context *ctx,
	int gen_sel, int clk_type, int rxwclk_inv)
{
	u32 val;
	u32 ssc_enable = 0;
	int calib_loop_count;
	int rc = 0;
	void *csr_base = ctx->csr_base;
	void *csr_serdes_base = csr_base + SATA_SERDES_OFFSET;
	void *clkcsr_base = ctx->csr_base + SATA_CLK_OFFSET;
	char *revision = apm88xxx_chip_revision();

	PHYDEBUG("SATA%d PHY init speed %d clk type %d inv clk %d",
		ctx->cid, gen_sel, clk_type, rxwclk_inv);
	PHYDEBUG("SATA%d ctrl_eq %d %d pq %d %d loopback %d %d ena_ctle %d %d "
		"spd_sel_cdr %d %d use_gen_avg %d", ctx->cid,
		ctx->ctrl_eq_A1, ctx->ctrl_eq,
		ctx->pq_A1, ctx->pq,
		ctx->loopback_buf_en_A1, ctx->loopback_buf_en,
		ctx->loopback_ena_ctle_A1, ctx->loopback_ena_ctle,
		ctx->spd_sel_cdr_A1, ctx->spd_sel_cdr, ctx->use_gen_avg);

	if (ctx->cid == 2 && (clk_type == SATA_CLK_INT_DIFF ||
		clk_type == SATA_CLK_INT_SING)){
		sata45_kc_macro_cfg(ctx);
	}

	/* Select SATA mux for SATA port 0 - 3 which shared with SGMII ETH */
        if (ctx->cid < 2) {
                if (apm_serdes_host_sata_select(ctx) != 0) {
                        PHYERROR("SATA%d can not select SATA MUX", ctx->cid);
                        return -1;
                }
        }

        /* Clock reset */
	PHYDEBUG("SATA%d enable clock", ctx->cid);
	apm_sata_clk_rst_pre(ctx);

	if (ctx->cid != 2 && (clk_type == SATA_CLK_INT_DIFF ||
		clk_type == SATA_CLK_INT_SING)) {
		kc_macro_cfg(ctx);
	}

	xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_RST_CTL_ADDR, 0x00);
	xgene_sata_delayus(1000); /* FIXME */
        PHYDEBUG("SATA%d reset Serdes", ctx->cid);
	/* 1. Serdes main reset and Controller also under reset */
        xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_RST_CTL_ADDR,
			0x00000020);

	/* Release all resets except  main reset */
        xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_RST_CTL_ADDR,
                        0x000000DE);

	xgene_sata_in32(csr_serdes_base + SATA_ENET_SDS_CTL1_ADDR,  &val);
	val = CFG_I_SPD_SEL_CDR_OVR1_SET(val, SPD_SEL);
	xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_CTL1_ADDR, val);

	PHYDEBUG("SATA%d Setting the customer pin mode", ctx->cid);
        /*
	 * Clear customer pins mode[13:0] = 0
	 * Set customer pins mode[14] = 1
	 */
        xgene_sata_in32(csr_serdes_base + SATA_ENET_SDS_CTL0_ADDR, &val);
	val = REGSPEC_CFG_I_CUSTOMER_PIN_MODE0_SET(val, 0x4421);
        xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_CTL0_ADDR, val);

	/* CMU_REG12 tx ready delay 0x2 */
	xgene_sds_rd(csr_serdes_base, KC_SERDES_CMU_REGS_CMU_REG12_ADDR, &val);
	if (strcmp(revision, "A1") == 0)
		val = CMU_REG12_STATE_DELAY9_SET(val, 0x2);
	else
		val = CMU_REG12_STATE_DELAY9_SET(val, 0x1);
	xgene_sds_wr(csr_serdes_base, KC_SERDES_CMU_REGS_CMU_REG12_ADDR, val);
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_CMU_REGS_CMU_REG13_ADDR, 0xF222);
        xgene_sds_wr(csr_serdes_base,
		KC_SERDES_CMU_REGS_CMU_REG14_ADDR, 0x2225);
	if (clk_type == SATA_CLK_EXT_DIFF) {
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG0_ADDR,
			&val);
		val =  CMU_REG0_PLL_REF_SEL_SET(val, 0x0);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG0_ADDR,
			val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			&val);
		val =  CMU_REG1_REFCLK_CMOS_SEL_SET(val, 0x0);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			val);
		PHYDEBUG("SATA%d Setting REFCLK EXTERNAL DIFF CML0",ctx->cid );
    	} else if (clk_type == SATA_CLK_INT_DIFF) {
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG0_ADDR,
			&val);
		val =  CMU_REG0_PLL_REF_SEL_SET(val, 0x1);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG0_ADDR,
			val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			&val);
		val =  CMU_REG1_REFCLK_CMOS_SEL_SET(val, 0x1);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			val);

	   PHYDEBUG("SATA%d Setting REFCLK INTERNAL DIFF CML1", ctx->cid);
    	} else if (clk_type == SATA_CLK_INT_SING) {
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			&val);
		val =  CMU_REG1_REFCLK_CMOS_SEL_SET(val, 0x1);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			val);
		PHYDEBUG("SATA%d Setting REFCLK INTERNAL CMOS", ctx->cid);
	}
	/* SATA4/5 no support for CML1 */
	if (ctx->cid == 2 && clk_type == SATA_CLK_INT_DIFF)
		xgene_sds_setbits(csr_serdes_base,
			KC_SERDES_CMU_REGS_CMU_REG1_ADDR,
			CMU_REG1_REFCLK_CMOS_SEL_MASK);
        /* Setup clock inversion */
	if (strcmp(revision, "A1") == 0) {
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG13_ADDR, &val);
		val &= ~(rxwclk_inv << 13);
		val |=  (rxwclk_inv << 13);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG13_ADDR, val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG13_ADDR, &val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG13_ADDR, &val);
		val &= ~(rxwclk_inv << 13);
		val |=  (rxwclk_inv << 13);
		xgene_sds_wr(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG13_ADDR, val);
		xgene_sds_rd(csr_serdes_base,
			KC_SERDES_X2_RXTX_REGS_CH1_RXTX_REG13_ADDR, &val);
	}
        /* 2. Program serdes registers */
        xgene_serdes_validation_CMU_cfg(ctx);
	if (ssc_enable)
		xgene_serdes_tx_ssc_enable(ctx);
        xgene_serdes_validation_rxtx_cfg(ctx, gen_sel);

	xgene_sata_in32(csr_serdes_base + SATA_ENET_SDS_PCS_CTL0_ADDR, &val);
	val = REGSPEC_CFG_I_RX_WORDMODE0_SET(val, 0x3);
	val = REGSPEC_CFG_I_TX_WORDMODE0_SET(val, 0x3);
	xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_PCS_CTL0_ADDR, val);

	calib_loop_count = 10;
	do {
		rc = serdes_calib_ready_check(ctx);
		if (rc == 0)
			break;
		serdes_pdown_force_vco(ctx);
	} while (++calib_loop_count > 0);
	if (calib_loop_count <= 0)
		return -1;

	xgene_sata_out32_flush(clkcsr_base + SATACLKENREG_ADDR, 0xff);

	xgene_sata_delayms(3); /* FIXME */
	sata_sm_dis_sds_pmclk_core_reset(ctx);

	xgene_sata_delayms(3); /* FIXME */
	sata_sm_deass_pclk_reset(ctx);

	PHYDEBUG("SATA%d initialized PHY", ctx->cid);
	return 0;
}

void xgene_sata_force_gen2(struct xgene_sata_context *ctx, int chan)
{
	void *csr_base = ctx->csr_base;
        void *csr_serdes = csr_base + SATA_SERDES_OFFSET;
	u32 val;

	xgene_sata_in32(csr_serdes + SATA_ENET_SDS_CTL1_ADDR,  &val);
	val = CFG_I_SPD_SEL_CDR_OVR1_SET(val, SPD_SEL_GEN2);
	xgene_sata_out32(csr_serdes + SATA_ENET_SDS_CTL1_ADDR, val);

 	xgene_sds_rd(csr_serdes,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + chan*0x200, &val);
	val = CH0_RXTX_REG0_CTLE_EQ_HR_SET(val, 0x1c);
	val = CH0_RXTX_REG0_CTLE_EQ_QR_SET(val, 0x1c);
	val = CH0_RXTX_REG0_CTLE_EQ_FR_SET(val, 0x1c);
	xgene_sds_wr(csr_serdes,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + chan*0x200, val);
}

void xgene_sata_force_gen1(struct xgene_sata_context *ctx, int chan)
{
	void *csr_base = ctx->csr_base;
        void *csr_serdes_base = csr_base + SATA_SERDES_OFFSET;
	u32 val;

	xgene_sata_in32(csr_serdes_base + SATA_ENET_SDS_CTL1_ADDR, &val);
	val = CFG_I_SPD_SEL_CDR_OVR1_SET(val, SPD_SEL_GEN1);
	xgene_sata_out32(csr_serdes_base + SATA_ENET_SDS_CTL1_ADDR, val);

 	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + chan*0x200, &val);
	val = CH0_RXTX_REG0_CTLE_EQ_HR_SET(val, 0x1c);
	val = CH0_RXTX_REG0_CTLE_EQ_QR_SET(val, 0x1c);
	val = CH0_RXTX_REG0_CTLE_EQ_FR_SET(val, 0x1c);
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG0_ADDR + chan*0x200, val);
}

void xgene_change_serdes_val(struct xgene_sata_context * ctx , int channel , int data)
{
	void *csr_base = ctx->csr_base;
	void *csr_serdes_base = csr_base + SATA_SERDES_OFFSET;
	unsigned int val;

	xgene_sds_rd(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG125_ADDR + channel*0x200, &val);
	val = CH0_RXTX_REG125_SIGN_PQ_SET(val , data);
	if (data)
		val = CH0_RXTX_REG125_PQ_REG_SET(val , 3);
	else
		val = CH0_RXTX_REG125_PQ_REG_SET(val , PQ_REG_A2);
	xgene_sds_wr(csr_serdes_base,
		KC_SERDES_X2_RXTX_REGS_CH0_RXTX_REG125_ADDR + channel*0x200, val);
}
