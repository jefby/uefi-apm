/** @file
  This driver module produces IDE_CONTROLLER_INIT protocol for Sata Controllers.

  Copyright (c) 2011, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "SataController.h"
#include "sata_apm88xxxx.h"

#undef ENABLE_SATA0
#undef ENABLE_SATA1

extern int xgene_ahci_init(UINT64 serdes_base, UINT64 ahci_base, UINT64 pcie_clk_base,
	u32 serdes_diff_clk, u32 gen_sel, int cid, int irq,
	u32 ctrl_eq_A1, u32 ctrl_eq, u32 pq_A1, u32 pq,
	u32 loopback_buf_en_A1, u32 loopback_buf_en,
	u32 loopback_ena_ctle_A1, u32 loopback_ena_ctle,
	u32 spd_sel_cdr_A1, u32 spd_sel_cdr, u32 use_gen_avg);

/**
  Read AHCI Operation register.

  @param Base       Pointer to base address.
  @param Offset     The operation register offset.

  @return The register content read.

**/
UINT32
EFIAPI
AhciReadReg (
  IN VOID                   *Base,
  IN UINT32                 Offset
  )
{
  ASSERT (Base != NULL);

  return *(volatile UINT32 *)(Base + Offset);
}

/**
  Write AHCI Operation register.

  @param Base      Pointer to base address.
  @param Offset     The operation register offset.
  @param Data       The data used to write down.

**/
VOID
EFIAPI
AhciWriteReg (
  IN VOID                  *Base,
  IN UINT32                 Offset,
  IN UINT32                 Data
  )
{
  ASSERT (Base != NULL);

  *(volatile UINT32 *)(Base + Offset) = Data;

  return ;
}

/**
  This function is used to calculate the best PIO mode supported by specific IDE device

  @param IdentifyData   The identify data of specific IDE device.
  @param DisPioMode     Disqualified PIO modes collection.
  @param SelectedMode   Available PIO modes collection.

  @retval EFI_SUCCESS       Best PIO modes are returned.
  @retval EFI_UNSUPPORTED   The device doesn't support PIO mode,
                            or all supported modes have been disqualified.
**/
EFI_STATUS
CalculateBestPioMode (
  IN EFI_IDENTIFY_DATA  *IdentifyData,
  IN UINT16             *DisPioMode OPTIONAL,
  OUT UINT16            *SelectedMode
  )
{
  UINT16    PioMode;
  UINT16    AdvancedPioMode;
  UINT16    Temp;
  UINT16    Index;
  UINT16    MinimumPioCycleTime;

  Temp = 0xff;

  PioMode = (UINT8) (((ATA5_IDENTIFY_DATA *) (&(IdentifyData->AtaData)))->pio_cycle_timing >> 8);
  DEBUG ((EFI_D_INFO, "CalculateBestPioMode: PioMode = %x\n", PioMode));
  //
  // See whether Identify Data word 64 - 70 are valid
  //
  if ((IdentifyData->AtaData.field_validity & 0x02) == 0x02) {

    AdvancedPioMode = IdentifyData->AtaData.advanced_pio_modes;
    DEBUG ((EFI_D_INFO, "CalculateBestPioMode: AdvancedPioMode = %x\n", AdvancedPioMode));

    for (Index = 0; Index < 8; Index++) {
      if ((AdvancedPioMode & 0x01) != 0) {
        Temp = Index;
      }

      AdvancedPioMode >>= 1;
    }

    //
    // If Temp is modified, mean the advanced_pio_modes is not zero;
    // if Temp is not modified, mean there is no advanced PIO mode supported,
    // the best PIO Mode is the value in pio_cycle_timing.
    //
    if (Temp != 0xff) {
      AdvancedPioMode = (UINT16) (Temp + 3);
    } else {
      AdvancedPioMode = PioMode;
    }

    //
    // Limit the PIO mode to at most PIO4.
    //
    PioMode = (UINT16) MIN (AdvancedPioMode, 4);

    MinimumPioCycleTime = IdentifyData->AtaData.min_pio_cycle_time_with_flow_control;

    if (MinimumPioCycleTime <= 120) {
      PioMode = (UINT16) MIN (4, PioMode);
    } else if (MinimumPioCycleTime <= 180) {
      PioMode = (UINT16) MIN (3, PioMode);
    } else if (MinimumPioCycleTime <= 240) {
      PioMode = (UINT16) MIN (2, PioMode);
    } else {
      PioMode = 0;
    }

    //
    // Degrade the PIO mode if the mode has been disqualified
    //
    if (DisPioMode != NULL) {
      if (*DisPioMode < 2) {
        return EFI_UNSUPPORTED; // no mode below ATA_PIO_MODE_BELOW_2
      }

      if (PioMode >= *DisPioMode) {
        PioMode = (UINT16) (*DisPioMode - 1);
      }
    }

    if (PioMode < 2) {
      *SelectedMode = 1;        // ATA_PIO_MODE_BELOW_2;
    } else {
      *SelectedMode = PioMode;  // ATA_PIO_MODE_2 to ATA_PIO_MODE_4;
    }

  } else {
    //
    // Identify Data word 64 - 70 are not valid
    // Degrade the PIO mode if the mode has been disqualified
    //
    if (DisPioMode != NULL) {
      if (*DisPioMode < 2) {
        return EFI_UNSUPPORTED; // no mode below ATA_PIO_MODE_BELOW_2
      }

      if (PioMode == *DisPioMode) {
        PioMode--;
      }
    }

    if (PioMode < 2) {
      *SelectedMode = 1;        // ATA_PIO_MODE_BELOW_2;
    } else {
      *SelectedMode = 2;        // ATA_PIO_MODE_2;
    }

  }
  DBG("CalculateBestPioMode: Return selected mode:%d\n", *SelectedMode);
  return EFI_SUCCESS;
}

/**
  This function is used to calculate the best UDMA mode supported by specific IDE device

  @param IdentifyData   The identify data of specific IDE device.
  @param DisUDmaMode     Disqualified UDMA modes collection.
  @param SelectedMode   Available UDMA modes collection.

  @retval EFI_SUCCESS       Best UDMA modes are returned.
  @retval EFI_UNSUPPORTED   The device doesn't support UDMA mode,
                            or all supported modes have been disqualified.
**/
EFI_STATUS
CalculateBestUdmaMode (
  IN EFI_IDENTIFY_DATA  *IdentifyData,
  IN UINT16             *DisUDmaMode OPTIONAL,
  OUT UINT16            *SelectedMode
  )
{
  UINT16    TempMode;
  UINT16    DeviceUDmaMode;

  DeviceUDmaMode = 0;

  //
  // Check whether the WORD 88 (supported UltraDMA by drive) is valid
  //
  if ((IdentifyData->AtaData.field_validity & 0x04) == 0x00) {
    return EFI_UNSUPPORTED;
  }

  DeviceUDmaMode = IdentifyData->AtaData.ultra_dma_mode;
  DEBUG ((EFI_D_INFO, "CalculateBestUdmaMode: DeviceUDmaMode = %x\n", DeviceUDmaMode));
  DeviceUDmaMode &= 0x7f;
  TempMode = 0;                 // initialize it to UDMA-0

  while ((DeviceUDmaMode >>= 1) != 0) {
    TempMode++;
  }

  //
  // Degrade the UDMA mode if the mode has been disqualified
  //
  if (DisUDmaMode != NULL) {
    if (*DisUDmaMode == 0) {
      *SelectedMode = 0;
      return EFI_UNSUPPORTED;   // no mode below ATA_UDMA_MODE_0
    }

    if (TempMode >= *DisUDmaMode) {
      TempMode = (UINT16) (*DisUDmaMode - 1);
    }
  }

  //
  // Possible returned mode is between ATA_UDMA_MODE_0 and ATA_UDMA_MODE_5
  //
  *SelectedMode = TempMode;
  DBG("CalculateBestUdmaMode: return SelectedMode %d\n", *SelectedMode);
  return EFI_SUCCESS;
}

/**
  The Entry Point of module. It follows the standard UEFI driver model.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS   The entry point is executed successfully.
  @retval other         Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
InitializeSataControllerDriver (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS	Status = 0;
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  UINT32        Data32;
  UINTN         ChannelDeviceCount;
  EFI_HANDLE    Handle = NULL;
  UINT64 	serdes_base;
  UINT64 	ahci_base;
  UINT64 	pcie_base;
  UINT32 	gen_sel = 3;
  INTN 		cid = 0;
  INTN 		irq;

  for (cid = 0; cid < 3; cid++) {
    // Init serdes stuffs
    switch (cid) {
    case 0:
#if !defined(ENABLE_SATA0)
      continue;
#else
      if (PcdGet64(PcdSata1SerdesRegisterBase) == 0)
        continue;
      serdes_base = PcdGet64(PcdSata1SerdesRegisterBase);
      ahci_base = PcdGet64(PcdAHCI1RegisterBase);
      pcie_base = 0;
      gen_sel = PcdGet32(PcdSata1GenMax);
      irq = PcdGet32(PcdSata1Irq);
      break;
#endif
    case 1:
#if !defined(ENABLE_SATA1)
      continue;
#else
      if (PcdGet64(PcdSata2SerdesRegisterBase) == 0)
        continue;
      serdes_base = PcdGet64(PcdSata2SerdesRegisterBase);
      ahci_base = PcdGet64(PcdAHCI2RegisterBase);
      pcie_base = 0;
      gen_sel = PcdGet32(PcdSata2GenMax);
      irq = PcdGet32(PcdSata2Irq);
      break;
#endif
    case 2:
      if (PcdGet64(PcdSata3SerdesRegisterBase) == 0)
        continue;
      serdes_base = PcdGet64(PcdSata3SerdesRegisterBase);
      ahci_base = PcdGet64(PcdAHCI3RegisterBase);
      pcie_base = 0x1f2d0000;
      gen_sel = PcdGet32(PcdSata3GenMax);
      irq = PcdGet32(PcdSata3Irq);
      break;
    default:
      continue;
    }

#ifdef APM_XGENE_NOSATA
    return 0;
#endif

    if (xgene_ahci_init(serdes_base, ahci_base, pcie_base, SATA_CLK_EXT_DIFF,
		gen_sel, cid, irq,
		XGENE_SERDES_VAL_NOT_SET, XGENE_SERDES_VAL_NOT_SET,
		XGENE_SERDES_VAL_NOT_SET, XGENE_SERDES_VAL_NOT_SET,
		XGENE_SERDES_VAL_NOT_SET, XGENE_SERDES_VAL_NOT_SET,
		XGENE_SERDES_VAL_NOT_SET, XGENE_SERDES_VAL_NOT_SET,
		XGENE_SERDES_VAL_NOT_SET, XGENE_SERDES_VAL_NOT_SET,
		XGENE_SERDES_VAL_NOT_SET)) {
      SATA_ERROR("Controller %d Initialize Serdes failed\n", cid);
      Status = EFI_DEVICE_ERROR;
      return Status;
    }
    DBG("InitializeSataControllerDriver CID %d START\n", cid);

    // Allocate Sata Private Data structure
    SataPrivateData = AllocateZeroPool(sizeof(EFI_SATA_CONTROLLER_PRIVATE_DATA));
    if (SataPrivateData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    // Initialize Sata Private Data
    SataPrivateData->Signature = SATA_CONTROLLER_SIGNATURE;
    SataPrivateData->IdeInit.GetChannelInfo = IdeInitGetChannelInfo;
    SataPrivateData->IdeInit.NotifyPhase = IdeInitNotifyPhase;
    SataPrivateData->IdeInit.SubmitData = IdeInitSubmitData;
    SataPrivateData->IdeInit.DisqualifyMode = IdeInitDisqualifyMode;
    SataPrivateData->IdeInit.CalculateMode = IdeInitCalculateMode;
    SataPrivateData->IdeInit.SetTiming = IdeInitSetTiming;
    SataPrivateData->IdeInit.EnumAll = SATA_ENUMER_ALL;
    SataPrivateData->IdeInit.GetMappedBaseAddress = IdeGetBaseAddress;
    SataPrivateData->IdeInit.BuildDevicePath = IdeInitBuildDevicePath;
    SataPrivateData->AHCIBaseAddress = ahci_base;

    // Read Host Capability Register(CAP) to get Number of Ports(NPS) and Supports Port Multiplier(SPM)
    //   NPS is 0's based value indicating the maximum number of ports supported by the HBA silicon.
    //   A maximum of 32 ports can be supported. A value of '0h', indicating one port, is the minimum requirement.
    //
    Data32 = AhciReadReg((VOID *) SataPrivateData->AHCIBaseAddress, R_AHCI_CAP);
    SataPrivateData->IdeInit.ChannelCount = (UINT8) ((Data32 & B_AHCI_CAP_NPS) + 1);
    SataPrivateData->DeviceCount = AHCI_MAX_DEVICES;
    /* Don't support SPM now */
//  if ((Data32 & B_AHCI_CAP_SPM) == B_AHCI_CAP_SPM) {
//    SataPrivateData->DeviceCount = AHCI_MULTI_MAX_DEVICES;
//  }

    ChannelDeviceCount = (UINTN) (SataPrivateData->IdeInit.ChannelCount) * (UINTN) (SataPrivateData->DeviceCount);
    SataPrivateData->DisqulifiedModes = AllocateZeroPool ((sizeof (EFI_ATA_COLLECTIVE_MODE)) * ChannelDeviceCount);
    if (SataPrivateData->DisqulifiedModes == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    SataPrivateData->IdentifyData = AllocateZeroPool ((sizeof (EFI_IDENTIFY_DATA)) * ChannelDeviceCount);
    if (SataPrivateData->IdentifyData == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    SataPrivateData->IdentifyValid = AllocateZeroPool ((sizeof (BOOLEAN)) * ChannelDeviceCount);
    if (SataPrivateData->IdentifyValid == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      goto Done;
    }

    // Install IDE Controller Init Protocol to this instance
    Status = gBS->InstallMultipleProtocolInterfaces (
                  &Handle,
                  &gEfiIdeControllerInitProtocolGuid, &(SataPrivateData->IdeInit),
                  NULL
                  );

Done:
    if (EFI_ERROR (Status)) {
      if (SataPrivateData != NULL) {
        if (SataPrivateData->DisqulifiedModes != NULL)
          FreePool(SataPrivateData->DisqulifiedModes);
        if (SataPrivateData->IdentifyData != NULL)
          FreePool(SataPrivateData->IdentifyData);
        if (SataPrivateData->IdentifyValid != NULL)
          FreePool(SataPrivateData->IdentifyValid);
        FreePool(SataPrivateData);
      }
    } else {
      DBG("InitializeSataControllerDriver CID %d status %r\n", cid, Status);
    }
    DBG("InitializeSataControllerDriver END status = %r\n", Status);
  }
  return Status;
}

//
// Interface functions of IDE_CONTROLLER_INIT protocol
//
/**
  Returns the information about the specified IDE channel.

  This function can be used to obtain information about a particular IDE channel.
  The driver entity uses this information during the enumeration process.

  If Enabled is set to FALSE, the driver entity will not scan the channel. Note
  that it will not prevent an operating system driver from scanning the channel.

  For most of today's controllers, MaxDevices will either be 1 or 2. For SATA
  controllers, this value will always be 1. SATA configurations can contain SATA
  port multipliers. SATA port multipliers behave like SATA bridges and can support
  up to 16 devices on the other side. If a SATA port out of the IDE controller
  is connected to a port multiplier, MaxDevices will be set to the number of SATA
  devices that the port multiplier supports. Because today's port multipliers
  support up to fifteen SATA devices, this number can be as large as fifteen. The IDE
  bus driver is required to scan for the presence of port multipliers behind an SATA
  controller and enumerate up to MaxDevices number of devices behind the port
  multiplier.

  In this context, the devices behind a port multiplier constitute a channel.

  @param[in]  This         The pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in]  Channel      Zero-based channel number.
  @param[out] Enabled      TRUE if this channel is enabled.  Disabled channels
                           are not scanned to see if any devices are present.
  @param[out] MaxDevices   The maximum number of IDE devices that the bus driver
                           can expect on this channel.  For the ATA/ATAPI
                           specification, version 6, this number will either be
                           one or two. For Serial ATA (SATA) configurations with a
                           port multiplier, this number can be as large as fifteen.

  @retval EFI_SUCCESS             Information was returned without any errors.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).

**/
EFI_STATUS
EFIAPI
IdeInitGetChannelInfo (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN UINT8                              Channel,
  OUT BOOLEAN                           *Enabled,
  OUT UINT8                             *MaxDevices
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  ASSERT (SataPrivateData != NULL);

  if (Channel < This->ChannelCount) {
    *Enabled = TRUE;
    *MaxDevices = SataPrivateData->DeviceCount;
    return EFI_SUCCESS;
  }

  *Enabled = FALSE;
  return EFI_INVALID_PARAMETER;
}

/**
  The notifications from the driver entity that it is about to enter a certain
  phase of the IDE channel enumeration process.

  This function can be used to notify the IDE controller driver to perform
  specific actions, including any chipset-specific initialization, so that the
  chipset is ready to enter the next phase. Seven notification points are defined
  at this time.

  More synchronization points may be added as required in the future.

  @param[in] This      The pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in] Phase     The phase during enumeration.
  @param[in] Channel   Zero-based channel number.

  @retval EFI_SUCCESS             The notification was accepted without any errors.
  @retval EFI_UNSUPPORTED         Phase is not supported.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_NOT_READY           This phase cannot be entered at this time; for
                                  example, an attempt was made to enter a Phase
                                  without having entered one or more previous
                                  Phase.

**/
EFI_STATUS
EFIAPI
IdeInitNotifyPhase (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN EFI_IDE_CONTROLLER_ENUM_PHASE      Phase,
  IN UINT8                              Channel
  )
{
  return EFI_SUCCESS;
}

/**
  Submits the device information to the IDE controller driver.

  This function is used by the driver entity to pass detailed information about
  a particular device to the IDE controller driver. The driver entity obtains
  this information by issuing an ATA or ATAPI IDENTIFY_DEVICE command. IdentifyData
  is the pointer to the response data buffer. The IdentifyData buffer is owned
  by the driver entity, and the IDE controller driver must make a local copy
  of the entire buffer or parts of the buffer as needed. The original IdentifyData
  buffer pointer may not be valid when

    - EFI_IDE_CONTROLLER_INIT_PROTOCOL.CalculateMode() or
    - EFI_IDE_CONTROLLER_INIT_PROTOCOL.DisqualifyMode() is called at a later point.

  The IDE controller driver may consult various fields of EFI_IDENTIFY_DATA to
  compute the optimum mode for the device. These fields are not limited to the
  timing information. For example, an implementation of the IDE controller driver
  may examine the vendor and type/mode field to match known bad drives.

  The driver entity may submit drive information in any order, as long as it
  submits information for all the devices belonging to the enumeration group
  before EFI_IDE_CONTROLLER_INIT_PROTOCOL.CalculateMode() is called for any device
  in that enumeration group. If a device is absent, EFI_IDE_CONTROLLER_INIT_PROTOCOL.SubmitData()
  should be called with IdentifyData set to NULL.  The IDE controller driver may
  not have any other mechanism to know whether a device is present or not. Therefore,
  setting IdentifyData to NULL does not constitute an error condition.
  EFI_IDE_CONTROLLER_INIT_PROTOCOL.SubmitData() can be called only once for a
  given (Channel, Device) pair.

  @param[in] This           A pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in] Channel        Zero-based channel number.
  @param[in] Device         Zero-based device number on the Channel.
  @param[in] IdentifyData   The device's response to the ATA IDENTIFY_DEVICE command.

  @retval EFI_SUCCESS             The information was accepted without any errors.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_INVALID_PARAMETER   Device is invalid.

**/
EFI_STATUS
EFIAPI
IdeInitSubmitData (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN UINT8                              Channel,
  IN UINT8                              Device,
  IN EFI_IDENTIFY_DATA                  *IdentifyData
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  ASSERT (SataPrivateData != NULL);

  if ((Channel >= This->ChannelCount) || (Device >= SataPrivateData->DeviceCount)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Make a local copy of device's IdentifyData and mark the valid flag
  //
  if (IdentifyData != NULL) {
    CopyMem (
      &(SataPrivateData->IdentifyData[Channel * Device]),
      IdentifyData,
      sizeof (EFI_IDENTIFY_DATA)
      );

    SataPrivateData->IdentifyValid[Channel * Device] = TRUE;
  } else {
    SataPrivateData->IdentifyValid[Channel * Device] = FALSE;
  }

  return EFI_SUCCESS;
}

/**
  Disqualifies specific modes for an IDE device.

  This function allows the driver entity or other drivers (such as platform
  drivers) to reject certain timing modes and request the IDE controller driver
  to recalculate modes. This function allows the driver entity and the IDE
  controller driver to negotiate the timings on a per-device basis. This function
  is useful in the case of drives that lie about their capabilities. An example
  is when the IDE device fails to accept the timing modes that are calculated
  by the IDE controller driver based on the response to the Identify Drive command.

  If the driver entity does not want to limit the ATA timing modes and leave that
  decision to the IDE controller driver, it can either not call this function for
  the given device or call this function and set the Valid flag to FALSE for all
  modes that are listed in EFI_ATA_COLLECTIVE_MODE.

  The driver entity may disqualify modes for a device in any order and any number
  of times.

  This function can be called multiple times to invalidate multiple modes of the
  same type (e.g., Programmed Input/Output [PIO] modes 3 and 4). See the ATA/ATAPI
  specification for more information on PIO modes.

  For Serial ATA (SATA) controllers, this member function can be used to disqualify
  a higher transfer rate mode on a given channel. For example, a platform driver
  may inform the IDE controller driver to not use second-generation (Gen2) speeds
  for a certain SATA drive.

  @param[in] This       The pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in] Channel    The zero-based channel number.
  @param[in] Device     The zero-based device number on the Channel.
  @param[in] BadModes   The modes that the device does not support and that
                        should be disqualified.

  @retval EFI_SUCCESS             The modes were accepted without any errors.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_INVALID_PARAMETER   Device is invalid.
  @retval EFI_INVALID_PARAMETER   IdentifyData is NULL.

**/
EFI_STATUS
EFIAPI
IdeInitDisqualifyMode (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN UINT8                              Channel,
  IN UINT8                              Device,
  IN EFI_ATA_COLLECTIVE_MODE            *BadModes
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  ASSERT (SataPrivateData != NULL);

  if ((Channel >= This->ChannelCount) || (BadModes == NULL) || (Device >= SataPrivateData->DeviceCount)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Record the disqualified modes per channel per device. From ATA/ATAPI spec,
  // if a mode is not supported, the modes higher than it is also not supported.
  //
  CopyMem (
    &(SataPrivateData->DisqulifiedModes[Channel * Device]),
    BadModes,
    sizeof (EFI_ATA_COLLECTIVE_MODE)
    );

  return EFI_SUCCESS;
}

/**
  Returns the information about the optimum modes for the specified IDE device.

  This function is used by the driver entity to obtain the optimum ATA modes for
  a specific device.  The IDE controller driver takes into account the following
  while calculating the mode:
    - The IdentifyData inputs to EFI_IDE_CONTROLLER_INIT_PROTOCOL.SubmitData()
    - The BadModes inputs to EFI_IDE_CONTROLLER_INIT_PROTOCOL.DisqualifyMode()

  The driver entity is required to call EFI_IDE_CONTROLLER_INIT_PROTOCOL.SubmitData()
  for all the devices that belong to an enumeration group before calling
  EFI_IDE_CONTROLLER_INIT_PROTOCOL.CalculateMode() for any device in the same group.

  The IDE controller driver will use controller- and possibly platform-specific
  algorithms to arrive at SupportedModes.  The IDE controller may base its
  decision on user preferences and other considerations as well. This function
  may be called multiple times because the driver entity may renegotiate the mode
  with the IDE controller driver using EFI_IDE_CONTROLLER_INIT_PROTOCOL.DisqualifyMode().

  The driver entity may collect timing information for various devices in any
  order. The driver entity is responsible for making sure that all the dependencies
  are satisfied. For example, the SupportedModes information for device A that
  was previously returned may become stale after a call to
  EFI_IDE_CONTROLLER_INIT_PROTOCOL.DisqualifyMode() for device B.

  The buffer SupportedModes is allocated by the callee because the caller does
  not necessarily know the size of the buffer. The type EFI_ATA_COLLECTIVE_MODE
  is defined in a way that allows for future extensibility and can be of variable
  length. This memory pool should be deallocated by the caller when it is no
  longer necessary.

  The IDE controller driver for a Serial ATA (SATA) controller can use this
  member function to force a lower speed (first-generation [Gen1] speeds on a
  second-generation [Gen2]-capable hardware).  The IDE controller driver can
  also allow the driver entity to stay with the speed that has been negotiated
  by the physical layer.

  @param[in]  This             The pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in]  Channel          A zero-based channel number.
  @param[in]  Device           A zero-based device number on the Channel.
  @param[out] SupportedModes   The optimum modes for the device.

  @retval EFI_SUCCESS             SupportedModes was returned.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_INVALID_PARAMETER   Device is invalid.
  @retval EFI_INVALID_PARAMETER   SupportedModes is NULL.
  @retval EFI_NOT_READY           Modes cannot be calculated due to a lack of
                                  data.  This error may happen if
                                  EFI_IDE_CONTROLLER_INIT_PROTOCOL.SubmitData()
                                  and EFI_IDE_CONTROLLER_INIT_PROTOCOL.DisqualifyData()
                                  were not called for at least one drive in the
                                  same enumeration group.

**/
EFI_STATUS
EFIAPI
IdeInitCalculateMode (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN UINT8                              Channel,
  IN UINT8                              Device,
  OUT EFI_ATA_COLLECTIVE_MODE           **SupportedModes
  )
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;
  EFI_IDENTIFY_DATA                 *IdentifyData;
  BOOLEAN                           IdentifyValid;
  EFI_ATA_COLLECTIVE_MODE           *DisqulifiedModes;
  UINT16                            SelectedMode;
  EFI_STATUS                        Status;

  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  ASSERT (SataPrivateData != NULL);

  if ((Channel >= This->ChannelCount) || (SupportedModes == NULL) || (Device >= SataPrivateData->DeviceCount)) {
    return EFI_INVALID_PARAMETER;
  }

  *SupportedModes = AllocateZeroPool (sizeof (EFI_ATA_COLLECTIVE_MODE));
  if (*SupportedModes == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  IdentifyData = &(SataPrivateData->IdentifyData[Channel * Device]);
  IdentifyValid = SataPrivateData->IdentifyValid[Channel * Device];
  DisqulifiedModes = &(SataPrivateData->DisqulifiedModes[Channel * Device]);

  //
  // Make sure we've got the valid identify data of the device from SubmitData()
  //
  if (!IdentifyValid) {
    return EFI_NOT_READY;
  }

  Status = CalculateBestPioMode (
            IdentifyData,
            (DisqulifiedModes->PioMode.Valid ? ((UINT16 *) &(DisqulifiedModes->PioMode.Mode)) : NULL),
            &SelectedMode
            );
  if (!EFI_ERROR (Status)) {
    (*SupportedModes)->PioMode.Valid = TRUE;
    (*SupportedModes)->PioMode.Mode = SelectedMode;

  } else {
    (*SupportedModes)->PioMode.Valid = FALSE;
  }
  DEBUG ((EFI_D_INFO, "IdeInitCalculateMode: PioMode = %x\n", (*SupportedModes)->PioMode.Mode));

  Status = CalculateBestUdmaMode (
            IdentifyData,
            (DisqulifiedModes->UdmaMode.Valid ? ((UINT16 *) &(DisqulifiedModes->UdmaMode.Mode)) : NULL),
            &SelectedMode
            );

  if (!EFI_ERROR (Status)) {
    (*SupportedModes)->UdmaMode.Valid = TRUE;
    (*SupportedModes)->UdmaMode.Mode  = SelectedMode;

  } else {
    (*SupportedModes)->UdmaMode.Valid = FALSE;
  }

  DEBUG ((EFI_D_INFO, "IdeInitCalculateMode: UdmaMode = %x\n", (*SupportedModes)->UdmaMode.Mode));

  //
  // The modes other than PIO and UDMA are not supported
  //
  return EFI_SUCCESS;
}

/**
  Commands the IDE controller driver to program the IDE controller hardware
  so that the specified device can operate at the specified mode.

  This function is used by the driver entity to instruct the IDE controller
  driver to program the IDE controller hardware to the specified modes. This
  function can be called only once for a particular device. For a Serial ATA
  (SATA) Advanced Host Controller Interface (AHCI) controller, no controller-
  specific programming may be required.

  @param[in] This      Pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[in] Channel   Zero-based channel number.
  @param[in] Device    Zero-based device number on the Channel.
  @param[in] Modes     The modes to set.

  @retval EFI_SUCCESS             The command was accepted without any errors.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_INVALID_PARAMETER   Device is invalid.
  @retval EFI_NOT_READY           Modes cannot be set at this time due to lack of data.
  @retval EFI_DEVICE_ERROR        Modes cannot be set due to hardware failure.
                                  The driver entity should not use this device.

**/
EFI_STATUS
EFIAPI
IdeInitSetTiming (
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL   *This,
  IN UINT8                              Channel,
  IN UINT8                              Device,
  IN EFI_ATA_COLLECTIVE_MODE            *Modes
  )
{
  return EFI_SUCCESS;
}

/* FIXME: Currently support only 1 controller (Sata45) */
EFI_GUID gIdeInitDevicePathGuid = {0xf9dd58fc, 0xc9ac, 0x4c50, 0x9f, 0x78, 0xa7, 0x63, 0x1e, 0x79, 0xb2, 0x96};

EFI_STATUS
EFIAPI
IdeInitBuildDevicePath (
  IN  EFI_IDE_CONTROLLER_INIT_PROTOCOL     *This,
  OUT EFI_DEVICE_PATH_PROTOCOL             **DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *NewDevicePathNode;
  NewDevicePathNode = CreateDeviceNode(HARDWARE_DEVICE_PATH,HW_VENDOR_DP,sizeof(VENDOR_DEVICE_PATH));
  CopyGuid(&(((VENDOR_DEVICE_PATH*)NewDevicePathNode)->Guid),&gIdeInitDevicePathGuid);
  *DevicePath = NewDevicePathNode;
  return EFI_SUCCESS;
}

/**
  For some IDE controller that base address (AHCI) is mapped. This function is used to
  get the address.

  @param[in]  This          Pointer to the EFI_IDE_CONTROLLER_INIT_PROTOCOL instance.
  @param[OUT] BaseAddress   Pointer to mapped base address.

  @retval EFI_SUCCESS             The command was accepted without any errors.
  @retval EFI_INVALID_PARAMETER   Channel is invalid (Channel >= ChannelCount).
  @retval EFI_INVALID_PARAMETER   Device is invalid.
  @retval EFI_NOT_READY           Modes cannot be set at this time due to lack of data.
  @retval EFI_DEVICE_ERROR        Modes cannot be set due to hardware failure.
                                  The driver entity should not use this device.

**/
EFI_STATUS
EFIAPI IdeGetBaseAddress(
  IN EFI_IDE_CONTROLLER_INIT_PROTOCOL  *This,
  OUT UINT64                           *BaseAddress)
{
  EFI_SATA_CONTROLLER_PRIVATE_DATA  *SataPrivateData;

  SataPrivateData = SATA_CONTROLLER_PRIVATE_DATA_FROM_THIS (This);
  ASSERT (SataPrivateData != NULL);

  *BaseAddress = SataPrivateData->AHCIBaseAddress;

  return EFI_SUCCESS;
}
