##
#  Copyright (c) 2013, AppliedMicro Corp. All rights reserved.
#
#  This program and the accompanying materials
#  are licensed and made available under the terms and conditions of the BSD License
#  which accompanies this distribution.  The full text of the license may be found at
#  http://opensource.org/licenses/bsd-license.php
#
#  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
#  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
#
#  APM X-Gene TianoCore Makefile
#
##

all: clean basetools tianocore
.PHONY : all

# SHELL:=/bin/bash
EDK2DIR=$(shell pwd)
EDK2TOOLSDIR=$(EDK2DIR)/BaseTools
export EDK_TOOLS_PATH=$(EDK2TOOLSDIR)

export ARCH=
#export CROSS_COMPILE:=aarch64-apm-linux-gnu-
#export CROSS_COMPILER_PATH:=/tools/arm/armv8/Theobroma/opt/apm-aarch64/6.0.4/bin
export ASL_BIN_DIR:=$(shell pwd)/../tools/acpi

clean: clean_tianocore clean_tianocore_debug clean_tianocore_VHP_debug clean_tianocore_VHP clean_basetools
	@echo
	@echo "############################### Clean Binary Files ###########################"
	rm -rf $(EDK2DIR)/Build/APMXGene-Mustang

clean_basetools:
	@echo
	@echo "################################# Clean BaseTools ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	make -C $(EDK2TOOLSDIR) clean

clean_tianocore:
	@echo
	@echo "################################# Clean TianoCore ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -b RELEASE -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang.dsc clean

clean_tianocore_VHP:
	@echo
	@echo "################################# Clean TianoCore VHP ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -b RELEASE -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-VHP.dsc clean

clean_tianocore_VHP_debug:
	@echo
	@echo "################################# Clean TianoCore VHP Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-VHP.dsc clean

clean_tianocore_UHP_debug:
	@echo
	@echo "################################# Clean TianoCore UHP Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-UHP.dsc clean

clean_tianocore_debug:
	@echo
	@echo "################################# Clean TianoCore Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang.dsc clean

basetools:
	@echo
	@echo "############################## Build TianoCore Tools #########################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	make -C $(EDK2TOOLSDIR)

tianocore:
	@echo
	@echo "################################# Build TianoCore ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b RELEASE -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang.dsc

tianocore_UHP:
	@echo
	@echo "################################# Build TianoCore UHP ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b RELEASE -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-UHP.dsc

tianocore_VHP:
	@echo
	@echo "################################# Build TianoCore VHP ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b RELEASE -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-VHP.dsc

tianocore_VHP_debug:
	@echo
	@echo "################################# Build TianoCore Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-VHP.dsc

tianocore_debug:
	@echo
	@echo "################################# Build TianoCore Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang.dsc

tianocore_UHP_debug:
	@echo
	@echo "################################# Build TianoCore UHP Debug ############################"
	cd $(EDK2DIR) && \
	.  $(EDK2DIR)/edksetup.sh $(EDK2TOOLSDIR) && \
	AARCH64LINUXGCC_TOOLS=${CROSS_COMPILER_PATH}/${CROSS_COMPILE} build -v -D EDK2_ARMVE_UEFI2_SHELL -b DEBUG -a AARCH64 -t ARMLINUXGCC -p ArmPlatformPkg/APMXGenePkg/APMXGene-Mustang-UHP.dsc
