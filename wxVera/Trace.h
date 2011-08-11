/* Copyright (C) 2011 Danny Quist. All rights reserved. */
#ifndef _TRACE_H_
#define _TRACE_H_

#include <map>
#include "wx/string.h"

// Special rules for compiling outside of wxVera

#ifndef _WXVERA_H_

#ifdef _WIN32
#include <windows.h>
#endif
#include "util.h"

#endif

// Windows Compatibility headers
#ifndef _WIN32
#include "wincompat.h"
#endif

using namespace std;

#define INSTLEN 		256
#define ADDRLEN 		32
#define LINELEN			1024
#define FILELEN			256
#define APILEN			256
#define START_ADDR		0xDEADBEEF

// Colors for output. Thanks for Kasimir Gilbert for helping pick these out
// These appear to be safe for people who are red-green color blind

//Blue 84, 141, 212
//Light Green 0, 206, 172  // no section for data
//Yellowish Brown 198, 189, 2
//Yellow 247, 252, 54 // Normal
//Light Blue, 142, 252, 249
//weird Purple 194, 140, 254 // SizeOfRawData = 0
//Magenta 255, 51, 153 // Instruction not in packed executable
//Darker Green 0, 153, 0
//Light Grey 178, 178, 178 // opcodes don't match

#define CB_START_ADDR_COLOR				0x0000bb // Dark Blue
#define CB_NORMAL_COLOR					0xf7fc36 // Yellow
#define CB_NO_SECTION_COLOR				0x00cfac // Light green
#define CB_SIZEOF_RAWDATA_ZERO_COLOR	0xc28cfe // weird purple
#define CB_INST_NOT_IN_PACKED_EXE_COLOR	0xff3399 // Magenta
#define CB_OPCODES_DONT_MATCH_COLOR		0xb2b2b2 // light grey
#define CB_HIGH_ENTROPY_COLOR			0x009900 // Darker Green

#define START_ADDR_COLOR				0x0000bb // Dark Blue
#define NORMAL_COLOR					0xffd700
#define NO_SECTION_COLOR				0x00bb00
#define SIZEOF_RAWDATA_ZERO_COLOR		0xbbbbff
#define INST_NOT_IN_PACKED_EXE_COLOR	0xff4444
#define OPCODES_DONT_MATCH_COLOR		0x44ff44
#define HIGH_ENTROPY_COLOR				0xbb0000


enum GRAPH_COLORING_ALG
{
	GRAPH_COLOR_PACKER,
	GRAPH_COLOR_PACKER_COLORBLIND,
	GRAPH_COLOR_BY_MODULE
};

// Structures

typedef struct Trace_Address {
	uint32_t addr;
	union {
		char inst[INSTLEN];
		char api[APILEN];
	} info;
	DWORD count;
	uint32_t num;
	bool isApi;
} trace_address_t;

typedef struct Trace_Edge {
	uint32_t src;
	uint32_t dst;
	uint32_t count;
	uint32_t num;
} trace_edge_t;

// Maps and what-not
typedef map<uint32_t, trace_address_t> bblMap_t; // Possible 64 bit conversion problem
typedef map<uint64_t, trace_edge_t> edgeMap_t;

// Prototypes
void initEdge(trace_edge_t *edge, uint32_t src, uint32_t dst, uint32_t count, uint32_t inum);
void initAddress(trace_address_t *address, uint32_t addr, char *inst, DWORD count, uint32_t inum);

class Trace
{
private:
	char 						tracefile[FILELEN];
	char 						orig_exe_file[FILELEN];
	char 						outputfile[FILELEN];
	uint32_t				        instnum;
	float *						sectionEntropy;
	float **					sectionCharProb;
	HANDLE						hFile;
	HANDLE						hMmap;
	DWORD						dwFileSize;
	//OFSTRUCT					of;
	unsigned char *				        origData;
	PIMAGE_DOS_HEADER			        pDosHeader;
	PIMAGE_NT_HEADERS			        pPeHeader;
	PIMAGE_SECTION_HEADER		                pSectionHeader;
	PIMAGE_OPTIONAL_HEADER		                pOptHeader;
	enum GRAPH_COLORING_ALG		                graphColoringAlgorithm;
	bool						doColorBlind;

	uint32_t	        addrColor(uint32_t addr);
	float			calcEntropy(unsigned char *data, size_t len);
	void			parseFiles(void);
	uint32_t	        packerAddrColor(uint32_t addr);

public:
	Trace(char *tracefile, char *orig_exe_file, char *outputfile);
	Trace(wxString tracefile, wxString orig_exe_file, wxString outputfile);
	Trace();
	bblMap_t					bblMap;
	edgeMap_t					edgeMap;
	void			process(bool doBasicBlocks);
	void 			processEther(bool doBasicBlocks);
	void			processVeraPin(bool doBasicBlocks);
	void 			writeGmlFile(char *gmlfile);
	void 			writeGmlFile(wxString infile);
	void 			writeGmlFile(void);
	void 			layoutGraph(const char *infile, const char *outfile);
	void 			layoutGraph(wxString infile, wxString outfile);
	void 			layoutGraph(wxString infile);
	void 			layoutGraph(const char* infile);
	void			setGraphColoringAlg(enum GRAPH_COLORING_ALG);
	~Trace(void);
};

#endif
