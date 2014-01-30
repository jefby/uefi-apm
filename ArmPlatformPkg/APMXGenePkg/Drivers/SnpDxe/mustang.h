/*
 * Configuration for APM Storm based Ref board
 * This is the OCM uboot build
 *
 * Copyright (c) 2010 Applied Micro Circuits Corporation.
 * All rights reserved. Keyur Chudgar <kchudgar@apm.com>.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifndef __CONFIG_H
#define __CONFIG_H
/* Fabric Map */
#define CONFIG_SYS_QML_FABRIC_BASE	0x10000000
#define CONFIG_SYS_QM0_FABRIC_BASE	0x18000000
#define CONFIG_SYS_QM1_FABRIC_BASE	0x1B000000
#define CONFIG_SYS_QM2_FABRIC_BASE	0x1E000000

/* Address Map */
#define CONFIG_SYS_STANDBY_CSR_BASE	0x17000000

#define CONFIG_SYS_ETH4_CSR_BASE	(CONFIG_SYS_STANDBY_CSR_BASE + 0x20000)
#define CONFIG_SYS_CLE4_CSR_BASE	(CONFIG_SYS_STANDBY_CSR_BASE + 0x26000)
#define CONFIG_SYS_QML_CSR_BASE		(CONFIG_SYS_STANDBY_CSR_BASE + 0x30000)

#define CONFIG_SYS_CSR_BASE		0x1F200000

#define CONFIG_SYS_QM1_CSR_BASE		(CONFIG_SYS_CSR_BASE + 0x000000)
#define CONFIG_SYS_ETH01_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x010000)
#define CONFIG_SYS_CLE01_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x016000)
#define CONFIG_SYS_ETH23_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x020000)
#define CONFIG_SYS_CLE23_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x026000)

#define CONFIG_SYS_QM0_CSR_BASE		(CONFIG_SYS_CSR_BASE + 0x400000)
#define CONFIG_SYS_XGE0_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x410000)
#define CONFIG_SYS_XGC0_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x416000)
#define CONFIG_SYS_XGE1_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x420000)
#define CONFIG_SYS_XGC1_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x426000)

#define CONFIG_SYS_QM2_CSR_BASE		(CONFIG_SYS_CSR_BASE + 0x500000)
#define CONFIG_SYS_XGE2_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x510000)
#define CONFIG_SYS_XGC2_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x516000)
#define CONFIG_SYS_XGE3_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x520000)
#define CONFIG_SYS_XGC3_CSR_BASE	(CONFIG_SYS_CSR_BASE + 0x526000)

/*****************************************************************************
 * Ethernet
 *****************************************************************************/
#define ENABLE_ENET
#ifdef ENABLE_ENET
#define CONFIG_NET_MULTI
#define CONFIG_CMD_NET
#undef CONFIG_NETCONSOLE

#define CONFIG_APM88XXXX_ENET
//#define CONFIG_APM88XXXX_XGENET

#ifdef CONFIG_APM88XXXX_ENET
#define CONFIG_HAS_ETH0
#undef CONFIG_HAS_ETH1
//#define CONFIG_HAS_ETH1
#undef CONFIG_HAS_ETH2
//#define CONFIG_HAS_ETH2
#undef CONFIG_HAS_ETH3
//#define CONFIG_HAS_ETH3
//#define CONFIG_HAS_ETH8
#define	CONFIG_CMD_APM88XXXX_ENET
#endif

#ifdef CONFIG_APM88XXXX_XGENET
#undef CONFIG_HAS_XG0
#undef CONFIG_HAS_XG1
#undef CONFIG_HAS_XG2
#undef CONFIG_HAS_XG3
#endif
#endif

#endif	/* __CONFIG_H */
