#ifndef _WINCOMPAT_H_
#define _WINCOMPAT_H_

/*
 * This file is used to help the Unix clients understand some windows'isms
 */

#ifndef _WIN32

#include "wxvera.h"

#include <stdint.h>
#include <stdio.h>

#define BYTE                                uint8_t
#define WORD                                uint16_t
#define DWORD                               uint32_t
#define HANDLE                              uint32_t
#define LONG                                uint32_t

// PE File signatures 
#define IMAGE_DOS_SIGNATURE                 0x5A4D      // MZ
#define IMAGE_NT_SIGNATURE                  0x00004550  // PE00  

// Constants from http://doxygen.reactos.org/d8/dcb/pefixup_8c_a75892b738729f0de64146086f462defe.html
#define IMAGE_NUMBEROF_DIRECTORY_ENTRIES    16
#define IMAGE_SIZEOF_SHORT_NAME             8

// Start with the DOS header
typedef struct _IMAGE_DOS_HEADER
{
	WORD e_magic;
	WORD e_cblp;
	WORD e_cp;
	WORD e_crlc;
	WORD e_cparhdr;
	WORD e_minalloc;
	WORD e_maxalloc;
	WORD e_ss;
	WORD e_sp;
	WORD e_csum;
	WORD e_ip;
	WORD e_cs;
	WORD e_lfarlc;
	WORD e_ovno;
	WORD e_res[4];
	WORD e_oemid;
	WORD e_oeminfo;
	WORD e_res2[10];
	LONG e_lfanew;
} __attribute__((__packed__)) Image_DOS_HEADER, *PIMAGE_DOS_HEADER;

typedef struct _IMAGE_FILE_HEADER
{
	WORD  Machine;
	WORD  NumberOfSections;
	DWORD TimeDateStamp;
	DWORD PointerToSymbolTable;
	DWORD NumberOfSymbols;
	WORD  SizeOfOptionalHeader;
	WORD  Characteristics;
}  __attribute__((__packed__)) IMAGE_FILE_HEADER, *PIMAGE_FILE_HEADER;

typedef struct _IMAGE_DATA_DIRECTORY
{
	DWORD VirtualAddress;
	DWORD Size;
} __attribute__((__packed__)) IMAGE_DATA_DIRECTORY, *PIMAGE_DATA_DIRECTORY;

typedef struct _IMAGE_SECTION_HEADER {
  BYTE  Name[IMAGE_SIZEOF_SHORT_NAME];
  union {
    DWORD PhysicalAddress;
    DWORD VirtualSize;
  } Misc;
  DWORD VirtualAddress;
  DWORD SizeOfRawData;
  DWORD PointerToRawData;
  DWORD PointerToRelocations;
  DWORD PointerToLinenumbers;
  WORD  NumberOfRelocations;
  WORD  NumberOfLinenumbers;
  DWORD Characteristics;
} __attribute__((__packed__)) IMAGE_SECTION_HEADER, *PIMAGE_SECTION_HEADER;

typedef struct _IMAGE_OPTIONAL_HEADER
{
	WORD                 Magic;
	BYTE                 MajorLinkerVersion;
	BYTE                 MinorLinkerVersion;
	DWORD                SizeOfCode;
	DWORD                SizeOfInitializedData;
	DWORD                SizeOfUninitializedData;
	DWORD                AddressOfEntryPoint;
	DWORD                BaseOfCode;
	DWORD                BaseOfData;
	DWORD                ImageBase;
	DWORD                SectionAlignment;
	DWORD                FileAlignment;
	WORD                 MajorOperatingSystemVersion;
	WORD                 MinorOperatingSystemVersion;
	WORD                 MajorImageVersion;
	WORD                 MinorImageVersion;
	WORD                 MajorSubsystemVersion;
	WORD                 MinorSubsystemVersion;
	DWORD                Win32VersionValue;
	DWORD                SizeOfImage;
	DWORD                SizeOfHeaders;
	DWORD                CheckSum;
	WORD                 Subsystem;
	WORD                 DllCharacteristics;
	DWORD                SizeOfStackReserve;
	DWORD                SizeOfStackCommit;
	DWORD                SizeOfHeapReserve;
	DWORD                SizeOfHeapCommit;
	DWORD                LoaderFlags;
	DWORD                NumberOfRvaAndSizes;
	IMAGE_DATA_DIRECTORY DataDirectory[IMAGE_NUMBEROF_DIRECTORY_ENTRIES];
} __attribute__((__packed__)) IMAGE_OPTIONAL_HEADER, *PIMAGE_OPTIONAL_HEADER;

typedef struct _IMAGE_NT_HEADERS
{
	DWORD                 Signature;
	IMAGE_FILE_HEADER     FileHeader;
	IMAGE_OPTIONAL_HEADER OptionalHeader;
} __attribute__((__packed__)) IMAGE_NT_HEADERS, *PIMAGE_NT_HEADERS;

#endif // _WIN32 check
#endif // _WINCOMPAT_H_
