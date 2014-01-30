/** @file

Copyright (c) 2010 - 2011, Intel Corporation. All rights reserved.<BR>
Portions copyright (c) 2013, ARM Ltd. All rights reserved.<BR>

This program and the accompanying materials are licensed and made available
under the terms and conditions of the BSD License which accompanies this
distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "WinNtInclude.h"

#ifndef __GNUC__
#include <windows.h>
#include <io.h>
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>

#include <Common/UefiBaseTypes.h>
#include <IndustryStandard/PeImage.h>

#include "PeCoffLib.h"
#include "EfiUtilityMsgs.h"

#include "GenFw.h"
#include "ElfConvert.h"
#include "Elf64Convert.h"

STATIC
VOID
ScanSections64 (
  VOID
  );

STATIC
BOOLEAN
WriteSections64 (
  SECTION_FILTER_TYPES  FilterType
  );

STATIC
VOID
WriteRelocations64 (
  VOID
  );

STATIC
VOID
WriteDebug64 (
  VOID
  );

STATIC
VOID
SetImageSize64 (
  VOID
  );

STATIC
VOID
CleanUp64 (
  VOID
  );

//
// Rename ELF32 strucutres to common names to help when porting to ELF64.
//
typedef Elf64_Shdr Elf_Shdr;
typedef Elf64_Ehdr Elf_Ehdr;
typedef Elf64_Rel Elf_Rel;
typedef Elf64_Rela Elf_Rela;
typedef Elf64_Sym Elf_Sym;
typedef Elf64_Phdr Elf_Phdr;
typedef Elf64_Dyn Elf_Dyn;
#define ELFCLASS ELFCLASS64
#define ELF_R_TYPE(r) ELF64_R_TYPE(r)
#define ELF_R_SYM(r) ELF64_R_SYM(r)

//
// Well known ELF structures.
//
STATIC Elf_Ehdr *mEhdr;
STATIC Elf_Shdr *mShdrBase;
STATIC Elf_Phdr *mPhdrBase;

//
// Coff information
//
STATIC const UINT32 mCoffAlignment = 0x20;

//
// PE section alignment.
//
STATIC const UINT16 mCoffNbrSections = 5;

//
// ELF sections to offset in Coff file.
//
STATIC UINT32 *mCoffSectionsOffset = NULL;

//
// Offsets in COFF file
//
STATIC UINT32 mNtHdrOffset;
STATIC UINT32 mTextOffset;
STATIC UINT32 mDataOffset;
STATIC UINT32 mHiiRsrcOffset;
STATIC UINT32 mRelocOffset;

//
// Initialization Function
//
BOOLEAN
InitializeElf64 (
  UINT8               *FileBuffer,
  ELF_FUNCTION_TABLE  *ElfFunctions
  )
{
  //
  // Initialize data pointer and structures.
  //
  VerboseMsg ("Set EHDR");
  mEhdr = (Elf_Ehdr*) FileBuffer;

  //
  // Check the ELF64 specific header information.
  //
  VerboseMsg ("Check ELF64 Header Information");
  if (mEhdr->e_ident[EI_CLASS] != ELFCLASS64) {
    Error (NULL, 0, 3000, "Unsupported", "ELF EI_DATA not ELFCLASS64");
    return FALSE;
  }
  if (mEhdr->e_ident[EI_DATA] != ELFDATA2LSB) {
    Error (NULL, 0, 3000, "Unsupported", "ELF EI_DATA not ELFDATA2LSB");
    return FALSE;
  }
  if ((mEhdr->e_type != ET_EXEC) && (mEhdr->e_type != ET_DYN)) {
    Error (NULL, 0, 3000, "Unsupported", "ELF e_type not ET_EXEC or ET_DYN");
    return FALSE;
  }
  if (!((mEhdr->e_machine == EM_X86_64) || (mEhdr->e_machine == EM_AARCH64))) {
    Error (NULL, 0, 3000, "Unsupported", "ELF e_machine not EM_X86_64 or EM_AARCH64");
    return FALSE;
  }
  if (mEhdr->e_version != EV_CURRENT) {
    Error (NULL, 0, 3000, "Unsupported", "ELF e_version (%u) not EV_CURRENT (%d)", (unsigned) mEhdr->e_version, EV_CURRENT);
    return FALSE;
  }

  //
  // Update section header pointers
  //
  VerboseMsg ("Update Header Pointers");
  mShdrBase  = (Elf_Shdr *)((UINT8 *)mEhdr + mEhdr->e_shoff);
  mPhdrBase = (Elf_Phdr *)((UINT8 *)mEhdr + mEhdr->e_phoff);

  //
  // Create COFF Section offset buffer and zero.
  //
  VerboseMsg ("Create COFF Section Offset Buffer");
  mCoffSectionsOffset = (UINT32 *)malloc(mEhdr->e_shnum * sizeof (UINT32));
  memset(mCoffSectionsOffset, 0, mEhdr->e_shnum * sizeof(UINT32));

  //
  // Fill in function pointers.
  //
  VerboseMsg ("Fill in Function Pointers");
  ElfFunctions->ScanSections = ScanSections64;
  ElfFunctions->WriteSections = WriteSections64;
  ElfFunctions->WriteRelocations = WriteRelocations64;
  ElfFunctions->WriteDebug = WriteDebug64;
  ElfFunctions->SetImageSize = SetImageSize64;
  ElfFunctions->CleanUp = CleanUp64;

  return TRUE;
}


//
// Header by Index functions
//
STATIC
Elf_Shdr*
GetShdrByIndex (
  UINT32 Num
  )
{
  if (Num >= mEhdr->e_shnum)
    return NULL;
  return (Elf_Shdr*)((UINT8*)mShdrBase + Num * mEhdr->e_shentsize);
}

STATIC
UINT32
CoffAlign (
  UINT32 Offset
  )
{
  return (Offset + mCoffAlignment - 1) & ~(mCoffAlignment - 1);
}

//
// filter functions
//
STATIC
BOOLEAN
IsTextShdr (
  Elf_Shdr *Shdr
  )
{
  return (BOOLEAN) ((Shdr->sh_flags & (SHF_WRITE | SHF_ALLOC)) == SHF_ALLOC);
}

STATIC
BOOLEAN
IsHiiRsrcShdr (
  Elf_Shdr *Shdr
  )
{
  Elf_Shdr *Namedr = GetShdrByIndex(mEhdr->e_shstrndx);

  return (BOOLEAN) (strcmp((CHAR8*)mEhdr + Namedr->sh_offset + Shdr->sh_name, ELF_HII_SECTION_NAME) == 0);
}

STATIC
BOOLEAN
IsDataShdr (
  Elf_Shdr *Shdr
  )
{
  if (IsHiiRsrcShdr(Shdr)) {
    return FALSE;
  }
  return (BOOLEAN) (Shdr->sh_flags & (SHF_WRITE | SHF_ALLOC)) == (SHF_ALLOC | SHF_WRITE);
}

//
// Elf functions interface implementation
//

STATIC
VOID
ScanSections64 (
  VOID
  )
{
  UINT32                          i;
  EFI_IMAGE_DOS_HEADER            *DosHdr;
  EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHdr;
  UINT32                          CoffEntry;
  UINT32                          SectionCount;
  BOOLEAN                         FoundText;

  CoffEntry = 0;
  mCoffOffset = 0;
  mTextOffset = 0;
  FoundText = FALSE;

  //
  // Coff file start with a DOS header.
  //
  mCoffOffset = sizeof(EFI_IMAGE_DOS_HEADER) + 0x40;
  mNtHdrOffset = mCoffOffset;
  switch (mEhdr->e_machine) {
  case EM_X86_64:
  case EM_IA_64:
  case EM_AARCH64:
    mCoffOffset += sizeof (EFI_IMAGE_NT_HEADERS64);
  break;
  default:
    VerboseMsg ("%s unknown e_machine type. Assume X64", (UINTN)mEhdr->e_machine);
    mCoffOffset += sizeof (EFI_IMAGE_NT_HEADERS64);
  break;
  }

  mTableOffset = mCoffOffset;
  mCoffOffset += mCoffNbrSections * sizeof(EFI_IMAGE_SECTION_HEADER);

  //
  // First text sections.
  //
  mCoffOffset = CoffAlign(mCoffOffset);
  SectionCount = 0;
  for (i = 0; i < mEhdr->e_shnum; i++) {
    Elf_Shdr *shdr = GetShdrByIndex(i);
    if (IsTextShdr(shdr)) {
      if ((shdr->sh_addralign != 0) && (shdr->sh_addralign != 1)) {
        // the alignment field is valid
        if ((shdr->sh_addr & (shdr->sh_addralign - 1)) == 0) {
          // if the section address is aligned we must align PE/COFF
          mCoffOffset = (UINT32) ((mCoffOffset + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1));
        } else if ((shdr->sh_addr % shdr->sh_addralign) != (mCoffOffset % shdr->sh_addralign)) {
          // ARM RVCT tools have behavior outside of the ELF specification to try
          // and make images smaller.  If sh_addr is not aligned to sh_addralign
          // then the section needs to preserve sh_addr MOD sh_addralign.
          // Normally doing nothing here works great.
          Error (NULL, 0, 3000, "Invalid", "Unsupported section alignment.");
        }
      }

      /* Relocate entry.  */
      if ((mEhdr->e_entry >= shdr->sh_addr) &&
          (mEhdr->e_entry < shdr->sh_addr + shdr->sh_size)) {
        CoffEntry = (UINT32) (mCoffOffset + mEhdr->e_entry - shdr->sh_addr);
      }

      //
      // Set mTextOffset with the offset of the first '.text' section
      //
      if (!FoundText) {
        mTextOffset = mCoffOffset;
        FoundText = TRUE;
      }

      mCoffSectionsOffset[i] = mCoffOffset;
      mCoffOffset += (UINT32) shdr->sh_size;
      SectionCount ++;
    }
  }

  if (!FoundText) {
    Error (NULL, 0, 3000, "Invalid", "Did not find any '.text' section.");
    assert (FALSE);
  }

  if (mEhdr->e_machine != EM_ARM) {
    mCoffOffset = CoffAlign(mCoffOffset);
  }

  if (SectionCount > 1 && mOutImageType == FW_EFI_IMAGE) {
    Warning (NULL, 0, 0, NULL, "Mulitple sections in %s are merged into 1 text section. Source level debug might not work correctly.", mInImageName);
  }

  //
  //  Then data sections.
  //
  mDataOffset = mCoffOffset;
  SectionCount = 0;
  for (i = 0; i < mEhdr->e_shnum; i++) {
    Elf_Shdr *shdr = GetShdrByIndex(i);
    if (IsDataShdr(shdr)) {
      if ((shdr->sh_addralign != 0) && (shdr->sh_addralign != 1)) {
        // the alignment field is valid
        if ((shdr->sh_addr & (shdr->sh_addralign - 1)) == 0) {
          // if the section address is aligned we must align PE/COFF
          mCoffOffset = (UINT32) ((mCoffOffset + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1));
        } else if ((shdr->sh_addr % shdr->sh_addralign) != (mCoffOffset % shdr->sh_addralign)) {
          // ARM RVCT tools have behavior outside of the ELF specification to try
          // and make images smaller.  If sh_addr is not aligned to sh_addralign
          // then the section needs to preserve sh_addr MOD sh_addralign.
          // Normally doing nothing here works great.
          Error (NULL, 0, 3000, "Invalid", "Unsupported section alignment.");
        }
      }
      mCoffSectionsOffset[i] = mCoffOffset;
      mCoffOffset += (UINT32) shdr->sh_size;
      SectionCount ++;
    }
  }
  mCoffOffset = CoffAlign(mCoffOffset);

  if (SectionCount > 1 && mOutImageType == FW_EFI_IMAGE) {
    Warning (NULL, 0, 0, NULL, "Mulitple sections in %s are merged into 1 data section. Source level debug might not work correctly.", mInImageName);
  }

  //
  //  The HII resource sections.
  //
  mHiiRsrcOffset = mCoffOffset;
  for (i = 0; i < mEhdr->e_shnum; i++) {
    Elf_Shdr *shdr = GetShdrByIndex(i);
    if (IsHiiRsrcShdr(shdr)) {
      if ((shdr->sh_addralign != 0) && (shdr->sh_addralign != 1)) {
        // the alignment field is valid
        if ((shdr->sh_addr & (shdr->sh_addralign - 1)) == 0) {
          // if the section address is aligned we must align PE/COFF
          mCoffOffset = (UINT32) ((mCoffOffset + shdr->sh_addralign - 1) & ~(shdr->sh_addralign - 1));
        } else if ((shdr->sh_addr % shdr->sh_addralign) != (mCoffOffset % shdr->sh_addralign)) {
          // ARM RVCT tools have behavior outside of the ELF specification to try
          // and make images smaller.  If sh_addr is not aligned to sh_addralign
          // then the section needs to preserve sh_addr MOD sh_addralign.
          // Normally doing nothing here works great.
          Error (NULL, 0, 3000, "Invalid", "Unsupported section alignment.");
        }
      }
      if (shdr->sh_size != 0) {
        mCoffSectionsOffset[i] = mCoffOffset;
        mCoffOffset += (UINT32) shdr->sh_size;
        mCoffOffset = CoffAlign(mCoffOffset);
        SetHiiResourceHeader ((UINT8*) mEhdr + shdr->sh_offset, mHiiRsrcOffset);
      }
      break;
    }
  }

  mRelocOffset = mCoffOffset;

  //
  // Allocate base Coff file.  Will be expanded later for relocations.
  //
  mCoffFile = (UINT8 *)malloc(mCoffOffset);
  memset(mCoffFile, 0, mCoffOffset);

  //
  // Fill headers.
  //
  DosHdr = (EFI_IMAGE_DOS_HEADER *)mCoffFile;
  DosHdr->e_magic = EFI_IMAGE_DOS_SIGNATURE;
  DosHdr->e_lfanew = mNtHdrOffset;

  NtHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION*)(mCoffFile + mNtHdrOffset);

  NtHdr->Pe32Plus.Signature = EFI_IMAGE_NT_SIGNATURE;

  switch (mEhdr->e_machine) {
  case EM_X86_64:
    NtHdr->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_X64;
    NtHdr->Pe32Plus.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    break;
  case EM_IA_64:
    NtHdr->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_IPF;
    NtHdr->Pe32Plus.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    break;
  case EM_AARCH64:
    NtHdr->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_AARCH64;
    NtHdr->Pe32Plus.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
    break;
  default:
    VerboseMsg ("%s unknown e_machine type. Assume X64", (UINTN)mEhdr->e_machine);
    NtHdr->Pe32Plus.FileHeader.Machine = EFI_IMAGE_MACHINE_X64;
    NtHdr->Pe32Plus.OptionalHeader.Magic = EFI_IMAGE_NT_OPTIONAL_HDR64_MAGIC;
  }

  NtHdr->Pe32Plus.FileHeader.NumberOfSections = mCoffNbrSections;
  NtHdr->Pe32Plus.FileHeader.TimeDateStamp = (UINT32) time(NULL);
  mImageTimeStamp = NtHdr->Pe32Plus.FileHeader.TimeDateStamp;
  NtHdr->Pe32Plus.FileHeader.PointerToSymbolTable = 0;
  NtHdr->Pe32Plus.FileHeader.NumberOfSymbols = 0;
  NtHdr->Pe32Plus.FileHeader.SizeOfOptionalHeader = sizeof(NtHdr->Pe32Plus.OptionalHeader);
  NtHdr->Pe32Plus.FileHeader.Characteristics = EFI_IMAGE_FILE_EXECUTABLE_IMAGE
    | EFI_IMAGE_FILE_LINE_NUMS_STRIPPED
    | EFI_IMAGE_FILE_LOCAL_SYMS_STRIPPED
    | EFI_IMAGE_FILE_LARGE_ADDRESS_AWARE;

  NtHdr->Pe32Plus.OptionalHeader.SizeOfCode = mDataOffset - mTextOffset;
  NtHdr->Pe32Plus.OptionalHeader.SizeOfInitializedData = mRelocOffset - mDataOffset;
  NtHdr->Pe32Plus.OptionalHeader.SizeOfUninitializedData = 0;
  NtHdr->Pe32Plus.OptionalHeader.AddressOfEntryPoint = CoffEntry;

  NtHdr->Pe32Plus.OptionalHeader.BaseOfCode = mTextOffset;

  NtHdr->Pe32Plus.OptionalHeader.ImageBase = 0;
  NtHdr->Pe32Plus.OptionalHeader.SectionAlignment = mCoffAlignment;
  NtHdr->Pe32Plus.OptionalHeader.FileAlignment = mCoffAlignment;
  NtHdr->Pe32Plus.OptionalHeader.SizeOfImage = 0;

  NtHdr->Pe32Plus.OptionalHeader.SizeOfHeaders = mTextOffset;
  NtHdr->Pe32Plus.OptionalHeader.NumberOfRvaAndSizes = EFI_IMAGE_NUMBER_OF_DIRECTORY_ENTRIES;

  //
  // Section headers.
  //
  if ((mDataOffset - mTextOffset) > 0) {
    CreateSectionHeader (".text", mTextOffset, mDataOffset - mTextOffset,
            EFI_IMAGE_SCN_CNT_CODE
            | EFI_IMAGE_SCN_MEM_EXECUTE
            | EFI_IMAGE_SCN_MEM_READ);
  } else {
    // Don't make a section of size 0.
    NtHdr->Pe32Plus.FileHeader.NumberOfSections--;
  }

  if ((mHiiRsrcOffset - mDataOffset) > 0) {
    CreateSectionHeader (".data", mDataOffset, mHiiRsrcOffset - mDataOffset,
            EFI_IMAGE_SCN_CNT_INITIALIZED_DATA
            | EFI_IMAGE_SCN_MEM_WRITE
            | EFI_IMAGE_SCN_MEM_READ);
  } else {
    // Don't make a section of size 0.
    NtHdr->Pe32Plus.FileHeader.NumberOfSections--;
  }

  if ((mRelocOffset - mHiiRsrcOffset) > 0) {
    CreateSectionHeader (".rsrc", mHiiRsrcOffset, mRelocOffset - mHiiRsrcOffset,
            EFI_IMAGE_SCN_CNT_INITIALIZED_DATA
            | EFI_IMAGE_SCN_MEM_READ);

    NtHdr->Pe32Plus.OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = mRelocOffset - mHiiRsrcOffset;
    NtHdr->Pe32Plus.OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = mHiiRsrcOffset;
  } else {
    // Don't make a section of size 0.
    NtHdr->Pe32Plus.FileHeader.NumberOfSections--;
  }

}

#if defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
/**
 * AARCH64
 */
typedef UINT64 u64;
typedef UINT32 u32;
typedef INT64 s64;
typedef INT32 s32;
typedef INT16 s16;
#define ERANGE	34
#define BIT(nr)                    (1ULL << (nr))

enum aarch64_reloc_op {
	RELOC_OP_NONE,
	RELOC_OP_ABS,
	RELOC_OP_PREL,
	RELOC_OP_PAGE,
};

static u64 do_reloc(enum aarch64_reloc_op reloc_op, u64 place, u64 val)
{
	switch (reloc_op) {
	case RELOC_OP_ABS:
		return val;
	case RELOC_OP_PREL:
		return val - place;
	case RELOC_OP_PAGE:
		return (val & ~0xfff) - (place & ~0xfff);
	case RELOC_OP_NONE:
		return 0;
	}

        Error(NULL, 0, 3000, "Invalid",
              "do_reloc: unknown relocation operation %d\n", reloc_op);
	return 0;
}

enum aarch64_imm_type {
	INSN_IMM_MOVNZ,
	INSN_IMM_MOVK,
	INSN_IMM_ADR,
	INSN_IMM_26,
	INSN_IMM_19,
	INSN_IMM_16,
	INSN_IMM_14,
	INSN_IMM_12,
	INSN_IMM_9,
};

static u32 encode_insn_immediate(enum aarch64_imm_type type, u32 insn, u64 imm)
{
	u32 immlo, immhi, lomask, himask, mask;
	int shift;

	switch (type) {
	case INSN_IMM_MOVNZ:
		/*
		 * For signed MOVW relocations, we have to manipulate the
		 * instruction encoding depending on whether or not the
		 * immediate is less than zero.
		 */
		insn &= ~(3 << 29);
		if ((s64)imm >= 0) {
			/* >=0: Set the instruction to MOVZ (opcode 10b). */
			insn |= 2 << 29;
		} else {
			/*
			 * <0: Set the instruction to MOVN (opcode 00b).
			 *     Since we've masked the opcode already, we
			 *     don't need to do anything other than
			 *     inverting the new immediate field.
			 */
			imm = ~imm;
		}
	case INSN_IMM_MOVK:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case INSN_IMM_ADR:
		lomask = 0x3;
		himask = 0x7ffff;
		immlo = imm & lomask;
		imm >>= 2;
		immhi = imm & himask;
		imm = (immlo << 24) | (immhi);
		mask = (lomask << 24) | (himask);
		shift = 5;
		break;
	case INSN_IMM_26:
		mask = BIT(26) - 1;
		shift = 0;
		break;
	case INSN_IMM_19:
		mask = BIT(19) - 1;
		shift = 5;
		break;
	case INSN_IMM_16:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case INSN_IMM_14:
		mask = BIT(14) - 1;
		shift = 5;
		break;
	case INSN_IMM_12:
		mask = BIT(12) - 1;
		shift = 10;
		break;
	case INSN_IMM_9:
		mask = BIT(9) - 1;
		shift = 12;
		break;
	default:
                Error(NULL, 0, 3000, "Invalid",
		      "encode_insn_immediate: unknown immediate encoding %d\n",
		      type);
		return 0;
	}

	/* Update the immediate field. */
	insn &= ~(mask << shift);
	insn |= (imm & mask) << shift;

	return insn;
}

#if 0
static int reloc_insn_movw(enum aarch64_reloc_op op, void *place, u64 loc, u64 val,
			   int lsb, enum aarch64_imm_type imm_type)
{
	u64 imm, limit = 0;
	s64 sval;
	u32 insn = *(u32 *)place;

	sval = do_reloc(op, loc, val);
	sval >>= lsb;
	imm = sval & 0xffff;

	/* Update the instruction with the new encoding. */
	*(u32 *)place = encode_insn_immediate(imm_type, insn, imm);

	/* Shift out the immediate field. */
	sval >>= 16;

	/*
	 * For unsigned immediates, the overflow check is straightforward.
	 * For signed immediates, the sign bit is actually the bit past the
	 * most significant bit of the field.
	 * The INSN_IMM_16 immediate type is unsigned.
	 */
	if (imm_type != INSN_IMM_16) {
		sval++;
		limit++;
	}

	/* Check the upper bits depending on the sign of the immediate. */
	if ((u64)sval > limit)
		return -ERANGE;

	return 0;
}
#endif

static UINT32 reloc_insn_imm(enum aarch64_reloc_op op, void *place, u64 loc, u64 val,
			  int lsb, int len, enum aarch64_imm_type imm_type)
{
	u64 imm, imm_mask;
	s64 sval;
	u32 insn = *(u32 *)place;
        u32 ret;

	/* Calculate the relocation value. */
	ret = sval = do_reloc(op, loc, val);
	sval >>= lsb;

	/* Extract the value bits and shift them to bit 0. */
	imm_mask = (BIT(lsb + len) - 1) >> lsb;
	imm = sval & imm_mask;

	/* Update the instruction's immediate field. */
	*(u32 *)place = encode_insn_immediate(imm_type, insn, imm);

	return ret;
}

static UINT64 reloc_insn_extract(enum aarch64_imm_type type, u32 insn)
{
	u32 immlo, immhi, lomask, himask, mask;
	int shift;

	switch (type) {
	case INSN_IMM_MOVNZ:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case INSN_IMM_MOVK:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case INSN_IMM_ADR:
		lomask = 0x3;
		himask = 0x7ffff;
		mask = (lomask << 24) | himask;
                shift = 5;

                /* Extract the field */
                insn >>= shift;
                insn &= mask;
                immlo = (insn >> 24) & lomask;
                immhi = insn & himask;
                immhi <<= 2;
                return immhi | immlo;
	case INSN_IMM_26:
		mask = BIT(26) - 1;
		shift = 0;
		break;
	case INSN_IMM_19:
		mask = BIT(19) - 1;
		shift = 5;
		break;
	case INSN_IMM_16:
		mask = BIT(16) - 1;
		shift = 5;
		break;
	case INSN_IMM_14:
		mask = BIT(14) - 1;
		shift = 5;
		break;
	case INSN_IMM_12:
		mask = BIT(12) - 1;
		shift = 10;
		break;
	case INSN_IMM_9:
		mask = BIT(9) - 1;
		shift = 12;
		break;
	default:
                Error(NULL, 0, 3000, "Invalid",
		      "encode_insn_immediate: unknown immediate encoding %d\n",
		      type);
		return 0;
	}
	/* Extract the immediate field. */
        insn >>= shift;
        insn &= mask;
	return insn;
}
#endif

STATIC
BOOLEAN
WriteSections64 (
  SECTION_FILTER_TYPES  FilterType
  )
{
  UINT32      Idx;
  Elf_Shdr    *SecShdr;
  UINT32      SecOffset;
  BOOLEAN     (*Filter)(Elf_Shdr *);

  //
  // Initialize filter pointer
  //
  switch (FilterType) {
    case SECTION_TEXT:
      Filter = IsTextShdr;
      break;
    case SECTION_HII:
      Filter = IsHiiRsrcShdr;
      break;
    case SECTION_DATA:
      Filter = IsDataShdr;
      break;
    default:
      return FALSE;
  }

  //
  // First: copy sections.
  //
  for (Idx = 0; Idx < mEhdr->e_shnum; Idx++) {
    Elf_Shdr *Shdr = GetShdrByIndex(Idx);
    if ((*Filter)(Shdr)) {
      switch (Shdr->sh_type) {
      case SHT_PROGBITS:
        /* Copy.  */
        memcpy(mCoffFile + mCoffSectionsOffset[Idx],
              (UINT8*)mEhdr + Shdr->sh_offset,
              (size_t) Shdr->sh_size);
        break;

      case SHT_NOBITS:
        memset(mCoffFile + mCoffSectionsOffset[Idx], 0, (size_t) Shdr->sh_size);
        break;

      default:
        //
        //  Ignore for unkown section type.
        //
        VerboseMsg ("%s unknown section type %x. We directly copy this section into Coff file", mInImageName, (unsigned)Shdr->sh_type);
        break;
      }
    }
  }

  //
  // Second: apply relocations.
  //
  VerboseMsg ("Applying Relocations...");
  for (Idx = 0; Idx < mEhdr->e_shnum; Idx++) {
    //
    // Determine if this is a relocation section.
    //
    Elf_Shdr *RelShdr = GetShdrByIndex(Idx);
    if ((RelShdr->sh_type != SHT_REL) && (RelShdr->sh_type != SHT_RELA)) {
      continue;
    }

    //
    // Relocation section found.  Now extract section information that the relocations
    // apply to in the ELF data and the new COFF data.
    //
    SecShdr = GetShdrByIndex(RelShdr->sh_info);
    SecOffset = mCoffSectionsOffset[RelShdr->sh_info];

    //
    // Only process relocations for the current filter type.
    //
    if (RelShdr->sh_type == SHT_RELA && (*Filter)(SecShdr)) {
      UINT64 RelIdx;

      //
      // Determine the symbol table referenced by the relocation data.
      //
      Elf_Shdr *SymtabShdr = GetShdrByIndex(RelShdr->sh_link);
      UINT8 *Symtab = (UINT8*)mEhdr + SymtabShdr->sh_offset;

      //
      // Process all relocation entries for this section.
      //
      for (RelIdx = 0; RelIdx < RelShdr->sh_size; RelIdx += (UINT32) RelShdr->sh_entsize) {

        //
        // Set pointer to relocation entry
        //
        Elf_Rela *Rel = (Elf_Rela *)((UINT8*)mEhdr + RelShdr->sh_offset + RelIdx);

        //
        // Set pointer to symbol table entry associated with the relocation entry.
        //
        Elf_Sym  *Sym = (Elf_Sym *)(Symtab + ELF_R_SYM(Rel->r_info) * SymtabShdr->sh_entsize);

        Elf_Shdr *SymShdr;
        UINT8    *Targ;

        //
        // Check section header index found in symbol table and get the section
        // header location.
        //
        if (Sym->st_shndx == SHN_UNDEF
            || Sym->st_shndx == SHN_ABS
            || Sym->st_shndx > mEhdr->e_shnum) {
          Error (NULL, 0, 3000, "Invalid", "%s bad symbol definition.", mInImageName);
        }
        SymShdr = GetShdrByIndex(Sym->st_shndx);

        //
        // Convert the relocation data to a pointer into the coff file.
        //
        // Note:
        //   r_offset is the virtual address of the storage unit to be relocated.
        //   sh_addr is the virtual address for the base of the section.
        //
        //   r_offset in a memory address.
        //   Convert it to a pointer in the coff file.
         //
        Targ = mCoffFile + SecOffset + (Rel->r_offset - SecShdr->sh_addr);

        //
        // Determine how to handle each relocation type based on the machine type.
        //
        if (mEhdr->e_machine == EM_X86_64) {
          switch (ELF_R_TYPE(Rel->r_info)) {
          case R_X86_64_NONE:
            break;
          case R_X86_64_64:
            //
            // Absolute relocation.
            //
            VerboseMsg ("R_X86_64_64");
            VerboseMsg ("Offset: 0x%08X, Addend: 0x%016LX", 
              (UINT32)(SecOffset + (Rel->r_offset - SecShdr->sh_addr)), 
              *(UINT64 *)Targ);
            *(UINT64 *)Targ = *(UINT64 *)Targ - SymShdr->sh_addr + mCoffSectionsOffset[Sym->st_shndx];
            VerboseMsg ("Relocation:  0x%016LX", *(UINT64*)Targ);
            break;
          case R_X86_64_32:
            VerboseMsg ("R_X86_64_32");
            VerboseMsg ("Offset: 0x%08X, Addend: 0x%08X", 
              (UINT32)(SecOffset + (Rel->r_offset - SecShdr->sh_addr)), 
              *(UINT32 *)Targ);
            *(UINT32 *)Targ = (UINT32)((UINT64)(*(UINT32 *)Targ) - SymShdr->sh_addr + mCoffSectionsOffset[Sym->st_shndx]);
            VerboseMsg ("Relocation:  0x%08X", *(UINT32*)Targ);
            break;
          case R_X86_64_32S:
            VerboseMsg ("R_X86_64_32S");
            VerboseMsg ("Offset: 0x%08X, Addend: 0x%08X", 
              (UINT32)(SecOffset + (Rel->r_offset - SecShdr->sh_addr)), 
              *(UINT32 *)Targ);
            *(INT32 *)Targ = (INT32)((INT64)(*(INT32 *)Targ) - SymShdr->sh_addr + mCoffSectionsOffset[Sym->st_shndx]);
            VerboseMsg ("Relocation:  0x%08X", *(UINT32*)Targ);
            break;
          case R_X86_64_PC32:
            //
            // Relative relocation: Symbol - Ip + Addend
            //
            VerboseMsg ("R_X86_64_PC32");
            VerboseMsg ("Offset: 0x%08X, Addend: 0x%08X", 
              (UINT32)(SecOffset + (Rel->r_offset - SecShdr->sh_addr)), 
              *(UINT32 *)Targ);
            *(UINT32 *)Targ = (UINT32) (*(UINT32 *)Targ
              + (mCoffSectionsOffset[Sym->st_shndx] - SymShdr->sh_addr)
              - (SecOffset - SecShdr->sh_addr));
            VerboseMsg ("Relocation:  0x%08X", *(UINT32 *)Targ);
            break;
          default:
            Error (NULL, 0, 3000, "Invalid", "%s unsupported ELF EM_X86_64 relocation 0x%x.", mInImageName, (unsigned) ELF_R_TYPE(Rel->r_info));
          }
        } else if (mEhdr->e_machine == EM_AARCH64) {
#if 0
          /* TODO AArch64 :
             - Add relocs for building with ARMCC (ARMCC uses REL not RELA like GCC).
          */
          // AARCH64 GCC uses RELA relocation. All relocations have to be fixed up.
          // BOOLEAN val64bit = 0;
          // UINT64 Instr64 = 0;
#if defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
          UINT32 Instr32 = *(UINT32 *) Targ;
          UINT64 P = 0;
          UINT64 S = 0;
          UINT64 addend = 0;
          UINT32 RelVal = 0;
#endif
          switch (ELF_R_TYPE(Rel->r_info)) {
#if defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
          case R_AARCH64_LD_PREL_LO19:
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_26, Instr32);
            S = (P & ~0xFFF) + addend;
            RelVal = reloc_insn_imm(RELOC_OP_PREL, Targ, P, S, 2, 19, INSN_IMM_19);
            break;
          case R_AARCH64_CALL26:
          case R_AARCH64_JUMP26:
            if (ELF64_ST_TYPE(Sym->st_info) != 0x3) { /* STT_FUNC */
              P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
              addend = Sym->st_value + Rel->r_addend - SymShdr->sh_addr;
              S = mCoffSectionsOffset[Sym->st_shndx] + addend;
              RelVal = reloc_insn_imm(RELOC_OP_PREL, Targ, P, S, 2, 26, INSN_IMM_26);
            }
            break;
          case R_AARCH64_LDST32_ABS_LO12_NC:
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_12, Instr32);
            addend <<= 2;
            S = (P & ~0xFFF) + addend - (P & ~0xFFF);
            RelVal = reloc_insn_imm(RELOC_OP_ABS, Targ, P, S, 2, 10, INSN_IMM_12);
            break;
          case R_AARCH64_LDST8_ABS_LO12_NC:
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_12, Instr32);
            S = (P & ~0xFFF) + addend - (P & ~0xFFF);
            RelVal = reloc_insn_imm(RELOC_OP_ABS, Targ, P, S, 0, 12, INSN_IMM_12);
            break;
          case R_AARCH64_LDST64_ABS_LO12_NC:
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_12, Instr32);
            addend <<= 3;
            S = (P & ~0xFFF) + addend - (P & ~0xFFF);
            RelVal = reloc_insn_imm(RELOC_OP_ABS, Targ, P, S, 3, 9, INSN_IMM_12);
            break;
          case R_AARCH64_LDST16_ABS_LO12_NC:
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_12, Instr32);
            addend <<= 1;
            S = (P & ~0xFFF) + addend - (P & ~0xFFF);
            RelVal = reloc_insn_imm(RELOC_OP_ABS, Targ, P, S, 1, 11, INSN_IMM_12);
            break;
          case R_AARCH64_ADR_PREL_PG_HI21:
            // Due to the fact that the symbol location is lost by the linker
            // when create the DLL file, we need to extract the imm value from
            // the actual instruction.
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_ADR, Instr32) << 12;
            S = (P & ~0xFFF) + addend;
            RelVal = reloc_insn_imm(RELOC_OP_PAGE, Targ, P, S, 12, 21, INSN_IMM_ADR);
            break;
          case R_AARCH64_ADD_ABS_LO12_NC:
            // Due to the fact that the symbol location is lost by the linker
            // when create the DLL file, we need to extract the imm value from
            // the actual instruction.
            P = SecOffset + Rel->r_offset - SecShdr->sh_addr;
            addend = reloc_insn_extract(INSN_IMM_12, Instr32);
            S = (P & ~0xFFF) + addend - (P & ~0xFFF);
            RelVal = reloc_insn_imm(RELOC_OP_ABS, Targ, P, S, 0, 12, INSN_IMM_12);
            break;
          case R_AARCH64_ADR_GOT_PAGE:
//            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADR_GOT_PAGE.", mInImageName);
            break;
          case R_AARCH64_LD64_GOT_LO12_NC:
//            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_LD64_GOT_LO12_NC.", mInImageName);
            break;
#else
          case R_AARCH64_LD_PREL_LO19:
            if  (Rel->r_addend != 0 ) { /* TODO */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_LD_PREL_LO19 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_CALL26:
            if  (Rel->r_addend != 0 ) { /* TODO */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_CALL26 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_JUMP26:
            if  (Rel->r_addend != 0 ) { /* TODO : AArch64 '-O2' optimisation. */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_JUMP26 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_ADR_PREL_PG_HI21:
            // TODO : AArch64 'small' memory model.
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADR_PREL_PG_HI21.", mInImageName);
            break;

          case R_AARCH64_ADD_ABS_LO12_NC:
            // TODO : AArch64 'small' memory model.
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADD_ABS_LO12_NC.", mInImageName);
            break;
#endif
          case R_AARCH64_ABS64: // Absolute relocations.
            // Instr64 = *(UINT64 *) Targ;
            *(UINT64 *)Targ = *(UINT64 *)Targ - SymShdr->sh_addr + mCoffSectionsOffset[Sym->st_shndx];
            // val64bit = 1;
            break;

          default:
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation 0x%x.", mInImageName, (unsigned) ELF_R_TYPE(Rel->r_info));
          }
#if defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
#if 0
            VerboseMsg("\nSection[%d] Relocate Entry[%d]: Target Section %d "
                     "Section addr 0x%08X offset 0x%016LX "
                     "Sym Idx %d Type %d Addend 0x%016LX Target Addend 0x%016LX",
                   Idx, RelIdx / RelShdr->sh_entsize, RelShdr->sh_info,
                   SecShdr->sh_addr, Rel->r_offset,
                   ELF_R_SYM(Rel->r_info), ELF_R_TYPE(Rel->r_info),
                   Rel->r_addend, addend);
            VerboseMsg("Symbol: name %s info 0x%02X sectidx %d value 0x%016LX size %d",
                   (UINT8*)mEhdr + mStrNameOffset + Sym->st_name,
                   Sym->st_info, Sym->st_shndx, Sym->st_value, Sym->st_size);
            VerboseMsg("Symbol Section %d section addr 0x%08X P 0x%016LX S 0x%016LX",
                   Sym->st_shndx, SymShdr->sh_addr, P, S);
            if (!val64bit) {
              VerboseMsg("File Offset: 0x%08X Target 0x%08X Value 0x%08X (0x%08X) Relative 0x%08X",
                   SecOffset, SecOffset + (Rel->r_offset - SecShdr->sh_addr),
                   Instr32, *(UINT32 *)Targ, RelVal);
            } else {
              VerboseMsg("File Offset: 0x%08X Target 0x%08X Value 0x%016LX (0x%016LX) Relative 0x%08X",
                   SecOffset, SecOffset + (Rel->r_offset - SecShdr->sh_addr),
                   Instr64, *(UINT64 *)Targ, RelVal);
            }
#endif
#endif
#endif
          // AARCH64 GCC uses RELA relocation, so all relocations have to be fixed up.
          // As opposed to ARM32 using REL.

          switch (ELF_R_TYPE(Rel->r_info)) {

          case R_AARCH64_LD_PREL_LO19:
            if  (Rel->r_addend != 0 ) { /* TODO */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_LD_PREL_LO19 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_CALL26:
            if  (Rel->r_addend != 0 ) { /* TODO */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_CALL26 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_JUMP26:
            if  (Rel->r_addend != 0 ) { /* TODO : AArch64 '-O2' optimisation. */
              Error (NULL, 0, 3000, "Invalid", "AArch64: R_AARCH64_JUMP26 Need to fixup with addend!.");
            }
            break;

          case R_AARCH64_ADR_PREL_PG_HI21:
            // TODO : AArch64 'small' memory model.
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADR_PREL_PG_HI21.", mInImageName);
            break;

          case R_AARCH64_ADD_ABS_LO12_NC:
            // TODO : AArch64 'small' memory model.
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADD_ABS_LO12_NC.", mInImageName);
            break;

          // Absolute relocations.
          case R_AARCH64_ABS64:
            *(UINT64 *)Targ = *(UINT64 *)Targ - SymShdr->sh_addr + mCoffSectionsOffset[Sym->st_shndx];
            break;

          default:
            Error (NULL, 0, 3000, "Invalid", "WriteSections64(): %s unsupported ELF EM_AARCH64 relocation 0x%x.", mInImageName, (unsigned) ELF_R_TYPE(Rel->r_info));
          }
        } else {
          Error (NULL, 0, 3000, "Invalid", "Not a supported machine type");
        }
      }
    }
  }

  return TRUE;
}

STATIC
VOID
WriteRelocations64 (
  VOID
  )
{
  UINT32                           Index;
  EFI_IMAGE_OPTIONAL_HEADER_UNION  *NtHdr;
  EFI_IMAGE_DATA_DIRECTORY         *Dir;

  for (Index = 0; Index < mEhdr->e_shnum; Index++) {
    Elf_Shdr *RelShdr = GetShdrByIndex(Index);
    if ((RelShdr->sh_type == SHT_REL) || (RelShdr->sh_type == SHT_RELA)) {
      Elf_Shdr *SecShdr = GetShdrByIndex (RelShdr->sh_info);
      if (IsTextShdr(SecShdr) || IsDataShdr(SecShdr)) {
        UINT64 RelIdx;

        for (RelIdx = 0; RelIdx < RelShdr->sh_size; RelIdx += RelShdr->sh_entsize) {
          Elf_Rela *Rel = (Elf_Rela *)((UINT8*)mEhdr + RelShdr->sh_offset + RelIdx);

          if (mEhdr->e_machine == EM_X86_64) {
            switch (ELF_R_TYPE(Rel->r_info)) {
            case R_X86_64_NONE:
            case R_X86_64_PC32:
              break;
            case R_X86_64_64:
              VerboseMsg ("EFI_IMAGE_REL_BASED_DIR64 Offset: 0x%08X", 
                mCoffSectionsOffset[RelShdr->sh_info] + (Rel->r_offset - SecShdr->sh_addr));
              CoffAddFixup(
                (UINT32) ((UINT64) mCoffSectionsOffset[RelShdr->sh_info]
                + (Rel->r_offset - SecShdr->sh_addr)),
                EFI_IMAGE_REL_BASED_DIR64);
              break;
            case R_X86_64_32S:
            case R_X86_64_32:
              VerboseMsg ("EFI_IMAGE_REL_BASED_HIGHLOW Offset: 0x%08X", 
                mCoffSectionsOffset[RelShdr->sh_info] + (Rel->r_offset - SecShdr->sh_addr));
              CoffAddFixup(
                (UINT32) ((UINT64) mCoffSectionsOffset[RelShdr->sh_info]
                + (Rel->r_offset - SecShdr->sh_addr)),
                EFI_IMAGE_REL_BASED_HIGHLOW);
              break;
            default:
              Error (NULL, 0, 3000, "Invalid", "%s unsupported ELF EM_X86_64 relocation 0x%x.", mInImageName, (unsigned) ELF_R_TYPE(Rel->r_info));
            }
          } else if (mEhdr->e_machine == EM_AARCH64) {
            // AArch64 GCC uses RELA relocation, so all relocations has to be fixed up. ARM32 uses REL.
            switch (ELF_R_TYPE(Rel->r_info)) {
            case R_AARCH64_LD_PREL_LO19:
		break;

            case R_AARCH64_CALL26:
		break;

            case R_AARCH64_JUMP26:
		break;

            case R_AARCH64_ADR_PREL_PG_HI21:
#if !defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
              // TODO : AArch64 'small' memory model.
              Error (NULL, 0, 3000, "Invalid", "WriteRelocations64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADR_PREL_PG_HI21.", mInImageName);
#endif
              break;
            case R_AARCH64_ADD_ABS_LO12_NC:
#if !defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
              // TODO : AArch64 'small' memory model.
              Error (NULL, 0, 3000, "Invalid", "WriteRelocations64(): %s unsupported ELF EM_AARCH64 relocation R_AARCH64_ADD_ABS_LO12_NC.", mInImageName);
#endif
              break;
#if defined(ENABLE_AARCH64_RELOCATION_SUPPORT)
            case R_AARCH64_ADR_GOT_PAGE:
            case R_AARCH64_LD64_GOT_LO12_NC:
            case R_AARCH64_LDST32_ABS_LO12_NC:
            case R_AARCH64_LDST8_ABS_LO12_NC:
            case R_AARCH64_LDST64_ABS_LO12_NC:
            case R_AARCH64_LDST16_ABS_LO12_NC:
              break;
#endif
            case R_AARCH64_ABS64:
              CoffAddFixup(
                (UINT32) ((UINT64) mCoffSectionsOffset[RelShdr->sh_info]
                + (Rel->r_offset - SecShdr->sh_addr)),
                EFI_IMAGE_REL_BASED_DIR64);
              break;

            case R_AARCH64_ABS32:
              CoffAddFixup(
                (UINT32) ((UINT64) mCoffSectionsOffset[RelShdr->sh_info]
                + (Rel->r_offset - SecShdr->sh_addr)),
                EFI_IMAGE_REL_BASED_HIGHLOW);
             break;

            default:
                Error (NULL, 0, 3000, "Invalid", "WriteRelocations64(): %s unsupported ELF EM_AARCH64 relocation 0x%x.", mInImageName, (unsigned) ELF_R_TYPE(Rel->r_info));
            }
          } else {
            Error (NULL, 0, 3000, "Not Supported", "This tool does not support relocations for ELF with e_machine %u (processor type).", (unsigned) mEhdr->e_machine);
          }
        }
      }
    }
  }

  //
  // Pad by adding empty entries.
  //
  while (mCoffOffset & (mCoffAlignment - 1)) {
    CoffAddFixupEntry(0);
  }

  NtHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(mCoffFile + mNtHdrOffset);
  Dir = &NtHdr->Pe32Plus.OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_BASERELOC];
  Dir->Size = mCoffOffset - mRelocOffset;
  if (Dir->Size == 0) {
    // If no relocations, null out the directory entry and don't add the .reloc section
    Dir->VirtualAddress = 0;
    NtHdr->Pe32Plus.FileHeader.NumberOfSections--;
  } else {
    Dir->VirtualAddress = mRelocOffset;
    CreateSectionHeader (".reloc", mRelocOffset, mCoffOffset - mRelocOffset,
            EFI_IMAGE_SCN_CNT_INITIALIZED_DATA
            | EFI_IMAGE_SCN_MEM_DISCARDABLE
            | EFI_IMAGE_SCN_MEM_READ);
  }
}

STATIC
VOID
WriteDebug64 (
  VOID
  )
{
  UINT32                              Len;
  UINT32                              DebugOffset;
  EFI_IMAGE_OPTIONAL_HEADER_UNION     *NtHdr;
  EFI_IMAGE_DATA_DIRECTORY            *DataDir;
  EFI_IMAGE_DEBUG_DIRECTORY_ENTRY     *Dir;
  EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY *Nb10;

  Len = strlen(mInImageName) + 1;
  DebugOffset = mCoffOffset;

  mCoffOffset += sizeof(EFI_IMAGE_DEBUG_DIRECTORY_ENTRY)
    + sizeof(EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY)
    + Len;
  mCoffOffset = CoffAlign(mCoffOffset);

  mCoffFile = realloc(mCoffFile, mCoffOffset);
  memset(mCoffFile + DebugOffset, 0, mCoffOffset - DebugOffset);

  Dir = (EFI_IMAGE_DEBUG_DIRECTORY_ENTRY*)(mCoffFile + DebugOffset);
  Dir->Type = EFI_IMAGE_DEBUG_TYPE_CODEVIEW;
  Dir->SizeOfData = sizeof(EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY) + Len;
  Dir->RVA = DebugOffset + sizeof(EFI_IMAGE_DEBUG_DIRECTORY_ENTRY);
  Dir->FileOffset = DebugOffset + sizeof(EFI_IMAGE_DEBUG_DIRECTORY_ENTRY);

  Nb10 = (EFI_IMAGE_DEBUG_CODEVIEW_NB10_ENTRY*)(Dir + 1);
  Nb10->Signature = CODEVIEW_SIGNATURE_NB10;
  strcpy ((char *)(Nb10 + 1), mInImageName);


  NtHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(mCoffFile + mNtHdrOffset);
  DataDir = &NtHdr->Pe32Plus.OptionalHeader.DataDirectory[EFI_IMAGE_DIRECTORY_ENTRY_DEBUG];
  DataDir->VirtualAddress = DebugOffset;
  DataDir->Size = mCoffOffset - DebugOffset;
  if (DataDir->Size == 0) {
    // If no debug, null out the directory entry and don't add the .debug section
    DataDir->VirtualAddress = 0;
    NtHdr->Pe32Plus.FileHeader.NumberOfSections--;
  } else {
    DataDir->VirtualAddress = DebugOffset;
    CreateSectionHeader (".debug", DebugOffset, mCoffOffset - DebugOffset,
            EFI_IMAGE_SCN_CNT_INITIALIZED_DATA
            | EFI_IMAGE_SCN_MEM_DISCARDABLE
            | EFI_IMAGE_SCN_MEM_READ);

  }
}

STATIC
VOID
SetImageSize64 (
  VOID
  )
{
  EFI_IMAGE_OPTIONAL_HEADER_UNION *NtHdr;

  //
  // Set image size
  //
  NtHdr = (EFI_IMAGE_OPTIONAL_HEADER_UNION *)(mCoffFile + mNtHdrOffset);
  NtHdr->Pe32Plus.OptionalHeader.SizeOfImage = mCoffOffset;
}

STATIC
VOID
CleanUp64 (
  VOID
  )
{
  if (mCoffSectionsOffset != NULL) {
    free (mCoffSectionsOffset);
  }
}


