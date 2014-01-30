/**
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
 **/

#ifndef __APM_PRECLASS_BASE_H__
#define __APM_PRECLASS_BASE_H__

/**
 * @brief   Patricia tree node read routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   node_index - Offset of node (0 - 127).
 * @param   *node_value - Pointer to 32 bit array to read data into.
 * @return  APM_RC_OK or APM_RC_ERROR
 *
 */
int apm_preclass_ptram_read(u32 port, u8 node_index,
		   	u32* node_value);

/**
 * @brief   Patricia tree node write routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   node_index - Offset of node (0 - 127).
 * @param   *node_value - Pointer to 32 bit array containing data to write.
 * @return  APM_RC_OK or APM_RC_ERROR
 */
int apm_preclass_ptram_write(u32 port, u8 node_index,
		      	u32* node_value);

/**
 * @brief   Packet ram user data read routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   data_hi - return hi value of read data.
 * @param   data_lo - return low value of read data.
 * @return  APM_RC_OK or APM_RC_ERROR
 *
 */
int apm_preclass_pktram_read(u32 port, u32 *data_hi,
	   		u32 *data_lo);

/**
 * @brief   Packet ram user data write routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   data_hi - hi value to write.
 * @param   data_lo - low value to write.
 * @return  APM_RC_OK or APM_RC_ERROR
 *
 */
int apm_preclass_pktram_write(u32 port, u32 data_hi,
		       	u32 data_lo);

/**
 * @brief   Classification database read routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   db_index - Database entry index
 * @param   * data - Pointer to DB entry data.
 * @return  APM_RC_OK or APM_RC_ERROR
 *
 */
int apm_preclass_cldb_read(u32 port, u16 db_index, u32* data);

/**
 * @brief   Classification database write routine.
 * @param   port - Inline-GE/LAC Port number
 * @param   db_index - Database entry index
 * @param   * data - Pointer to DB entry data.
 * @return  APM_RC_OK or APM_RC_ERROR
 *
 */
int apm_preclass_cldb_write(u32 port, u16 db_index, u32* data);

#endif	/* __APM_PRECLASS_BASE_H__ */

