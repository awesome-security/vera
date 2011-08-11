//#include "wxvera.h"

#include "Trace.h"
#include "ogdf/energybased/FMMMLayout.h"

// The edge keys contain a unique value of the combination of the source key and
// the destination edge key. It is stored in a 64 bit integer that contains as so:
// edgekey = (src << 32) + (dst & 0xFFFFFFFF);
//
// I'm not sure why I did it this way, I guess I was feeling clever.

#define MAKE_EDGE_KEY(src,dst) (((uint64_t) (src) << 32) | (dst))
#define GET_SRC_FROM_EDGE_KEY(edgekey) ( (edgekey) >> 32)
#define GET_DST_FROM_EDGE_KEY(edgekey) ( (edgekey) & (uint64_t) 0x00000000FFFFFFFF)

Trace::Trace()
{
}

Trace::Trace(wxString tracefile, wxString orig_exe_file, wxString outputfile)
{
	char tfile[FILELEN] = {0};
	char ogfile[FILELEN] = {0};
	char opfile[FILELEN] = {0};

	doColorBlind = false;
	graphColoringAlgorithm = GRAPH_COLOR_PACKER;

	strncpy(this->tracefile, tracefile.ToAscii(), sizeof(this->tracefile) - 1);
	strncpy(this->orig_exe_file, orig_exe_file.ToAscii(), sizeof(this->orig_exe_file) - 1);
	strncpy(this->outputfile, outputfile.ToAscii(), sizeof(this->outputfile) -1);

	this->parseFiles();
}

Trace::Trace(char *tracefile, char *orig_exe_file, char *outputfile)
{
	strncpy(this->tracefile, tracefile, sizeof(this->tracefile) - 1);
	strncpy(this->orig_exe_file, orig_exe_file, sizeof(this->orig_exe_file) - 1);
	strncpy(this->outputfile, outputfile, sizeof(this->outputfile) - 1);

	this->parseFiles();
}

Trace::~Trace(void)
{	
	// Free sectionEntropy
	free(sectionEntropy);
	sectionEntropy = NULL;

	// Free character probability of =each section
	for(WORD i = 0 ; i < pPeHeader->FileHeader.NumberOfSections ; i++)
	{
		free(sectionCharProb[i]);
		sectionCharProb[i] = NULL;
	}

	// Free the dynamic array of all the memory
	free(sectionCharProb);
	sectionCharProb = NULL;

	// Free the original data
	if (origData)
	{
		free(origData);
		origData = NULL;
	}
}

inline void initEdge(trace_edge_t *edge, uint32_t src, uint32_t dst, uint32_t count, uint32_t inum)
{
	// Copy the data
	edge->src = src;
	edge->dst = dst;
	edge->count = count;
	edge->num = inum;
}

inline void initAddress(trace_address_t *address, uint32_t addr, char *inst, uint32_t count, uint32_t inum)
{
	// Copy the data
	address->addr = addr;
	strncpy(address->info.inst, inst, INSTLEN);
	size_t len = strlen(address->info.inst);
	if (address->info.inst[len-1] == '\n')
		address->info.inst[len-1] = '\0';
	address->count = count;
	address->num = inum;
}

void Trace::parseFiles(void)
{
	struct stat st = {0};

	stat(this->orig_exe_file, &st);
	
	FILE *fin = fopen(this->orig_exe_file, "rb");

	if (fin == NULL)
	{
		throw "Could not open executable file for reading";
	}
	
	// Allocate and initialize to zero
	origData = (unsigned char *) malloc(st.st_size);

	if (origData == NULL)
	{
		throw "Could not allocate data";
	}
	
	memset(origData, 0, st.st_size);

	// Read the file data
	size_t readData = fread(origData, st.st_size, 1, fin);

	// Should return 1 for one block read
	if (readData != 1)
	{
		throw "Could not read original file data";
	}
	
	fclose(fin);
	
	if (!origData)
	{
		//wxLogDebug(wxT("Could not MapViewOfFile for original file")); 
		throw "Could not MapViewOfFile for original file";
	}

	// Get the DOS headers
	pDosHeader = (PIMAGE_DOS_HEADER) origData;
	
	// Check for a valid header
	if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE)
		throw "Not a valid DOS header";

	// Get the NT headers
	// 
	// WARNING FROM THE PAST:
	// This code is most likely going to be the cause of problems when porting to 64-bit.
	// The reason is because either the e_lfanew structure member doesn't get converted properly
	// in the GCC compiler or the msvc compiler doesn't convert the pointers properly. at any
	// rate they don't agree.
	// Danny, August 2011
	pPeHeader = (PIMAGE_NT_HEADERS) (origData + pDosHeader->e_lfanew);

	if(pPeHeader->Signature != IMAGE_NT_SIGNATURE)
	{
		//wxLogDebug(wxT("Not a valid NT header"));
		throw "Not a valid NT header";
	}

	pOptHeader = &(pPeHeader->OptionalHeader);

	pSectionHeader = (PIMAGE_SECTION_HEADER) ( (PIMAGE_SECTION_HEADER) pOptHeader + (DWORD) (pPeHeader->FileHeader.SizeOfOptionalHeader));

	//wxLogDebug(wxT("%s has been successfully loaded and parsed\n"), orig_exe_file);

	// Calculate entropy of all the sections
	// This is the standard Shannon entropy algorithm, and was adopted from Ero Carrera's
	// Pefile code
	
	sectionEntropy = (float *) malloc(sizeof(float) * pPeHeader->FileHeader.NumberOfSections);

	if (sectionEntropy == NULL)
	{
		//wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}

	sectionCharProb = (float **) malloc(sizeof(float*) * pPeHeader->FileHeader.NumberOfSections);
	
	if (sectionCharProb == NULL)
	{
		//wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
		return;
	}
	
	PIMAGE_SECTION_HEADER pHeaderTmp = pSectionHeader;

	for(WORD i = 0 ; i < pPeHeader->FileHeader.NumberOfSections ; i++)
	{
		sectionEntropy[i] = 0.0;
		sectionCharProb[i] = (float *) malloc(sizeof(float) * 256);

		if (sectionCharProb[i] == NULL)
		{
			//wxLogDebug(wxString::Format(wxT("Could not allocate memory: %s:%u"), __FILE__, __LINE__));
			return;
		}

		memset(sectionCharProb[i], 0, sizeof(float) * 256);
		
		//wxLogDebug(wxT("Processing section %s\n"), pSectionHeader[i].Name);
		
		// Get the character count from the section data
		if(pSectionHeader[i].SizeOfRawData > 0)
		{
			for (DWORD j = 0 ;
			     j < pSectionHeader[i].SizeOfRawData && (pSectionHeader[i].PointerToRawData + j) < dwFileSize ;
			     j++)
			{
				unsigned char byte = origData[pSectionHeader[i].PointerToRawData + j];
				sectionCharProb[i][byte] += 1.0;
			}

			// Calculate the shannon entropy
			for(DWORD j = 0 ; j < 256 ; j++)
			{
				sectionCharProb[i][j] = sectionCharProb[i][j] / pSectionHeader[i].SizeOfRawData;
				if( sectionCharProb[i][j] > 0.0)
					sectionEntropy[i] += -1 * sectionCharProb[i][j] * ( log(sectionCharProb[i][j]) / log((float)2.0) );
			}
			
		}

		//wxLogDebug(wxT("Entropy is %2.2f\n"), sectionEntropy[i]);

	}
}

inline bool branchMatch(char *inst)
{
	// check for the presence of any jump instruction
	if (inst[0] != 'j')
	{
		return false;
	}

	// Look for branch instructions
	// Recreate this: ^j([acbgl]?n?[ez]?|mp).*$
	switch(inst[1])
	{
	case 'a': // above
	case 'c': // jcxz, Jump if CX is zero
	case 'b': // below
	case 'g': // greater than
	case 'l': // less than
	case 'n': // not
	case 'm': // jmp, no comparison
	case 'p': // Parity
	case 'e': // Equal / Zero
	case 'z': // Zero / Equal
		return true;
	default:
		return false;
	}
}

bool isAddressLine(char *in)
{
	if (in == NULL)
		return false;

	size_t len = strlen(in);

	if (len <= 0)
		return false;

	for(size_t i = 0 ; i < len ; i++)
	{
		if (in[i] == ':')
			break;

		if (!isxdigit(in[i])) // If the current character is not a hex digit, break
			return false;
	}

	return true; // If we've gotten to this point we're not a hex digit
}

void Trace::process(bool doBasicBlocks)
{
	// Decide what kind of trace file it is
	FILE *fin = fopen(this->tracefile, "r");
	
	if (fin == 0)
	{
		char errstr[128] = {0};
		sprintf(errstr, "Could not open trace file %s", this->tracefile);
		throw errstr;
	}
	
	wchar_t linew[LINELEN] = {0};
	char line[LINELEN] = {0};
	
	fgetws(linew, sizeof(linew), fin);
	fseek(fin, 0, 0);
	fgets(line, sizeof(line), fin);

	fclose(fin);

	if ( (wcsstr(linew, L"vera_trace_version=0.1") == linew) ||
		 (wcsstr(linew, L"n") == linew) ||
		 (wcsstr(linew, L"m") == linew) ||
		 (wcsstr(linew, L"i") == linew) )
		processVeraPin(doBasicBlocks);
	else
		processEther(doBasicBlocks);
}

void Trace::processVeraPin(bool doBasicBlocks)
{
	//wxLogDebug(wxT("Trace::processVeraPin stub handler\n"));

	if (doBasicBlocks)
		return;

	wchar_t widein[LINELEN] = {0};
	char in[LINELEN] = {0};
	char addr[ADDRLEN] = {0};
	char api[APILEN] = {0};
	char inst[APILEN] = {0};
	FILE *fin = NULL;

	size_t charconverted = 0;
	uint32_t daddr = 0;
	uint32_t lastbbl = 0;
	unsigned long lines = 0;
	uint32_t inum = 0;
	uint32_t bblnum = 0;
	uint32_t edgenum = 0;

	bool isFirstAddr = true;
	bool isImport = false;

	fin = fopen(this->tracefile, "r");
	
	if (fin == 0)
		return;

	initAddress(&bblMap[START_ADDR], START_ADDR, "START", 1, bblnum++);

	while (fgetws(widein, sizeof(widein)-1, fin) != NULL)
	{
		wcstombs(in, widein, sizeof(in));

		switch (in[0])
		{
		case 'm':
		case 'n':
			// Just get the normal instructions
			sscanf(&in[1], "%[^:\n]:%[^\n]", addr, inst);
			isImport = false;
			break;
		case 'i':
			// Import
			sscanf(&in[1], "%[^:\n]:%[^\n]", addr, api);
			isImport = true;
			break;
		default:
			// normal
			break;
		}

		if (wcsstr(widein, L"vera_trace_version=") == widein) // skip the version string
			continue;
		
		xtoi(addr, &daddr);

		if (doBasicBlocks)
		{} // Todo
		else
		{
			if (isFirstAddr)
			{
				initAddress(&bblMap[daddr], daddr, (isImport == true ? api : inst), 0, bblnum++);
				initEdge(&edgeMap[MAKE_EDGE_KEY(START_ADDR, daddr)], START_ADDR, daddr, 1, edgenum++);
				isFirstAddr = false;
				lastbbl = daddr;
			}
			else
			{
				if(bblMap.find(daddr) != bblMap.end())
				{
					bblMap[daddr].count++;
				}
				else
				{
					initAddress(&bblMap[daddr], daddr, (isImport == true ? api : inst), 0, bblnum++);
					bblMap[daddr].isApi = isImport;
				}
				
				uint64_t edgekey = 0;
				edgekey = MAKE_EDGE_KEY(lastbbl, daddr);
				
				// Add the edge to the list
				if(edgeMap.find(edgekey) != edgeMap.end())
					edgeMap[edgekey].count++;
				else
					initEdge(&edgeMap[edgekey],
						 (uint32_t) GET_SRC_FROM_EDGE_KEY(edgekey),
						 (uint32_t) GET_DST_FROM_EDGE_KEY(edgekey),
						 (uint32_t) 1,
						 edgenum++);

				lastbbl = daddr;
			}
		}

		inum++;
		lines++;
		
	}

	fclose(fin);
}

void Trace::processEther(bool doBasicBlocks)
{
	FILE *fin = NULL;
	char in[LINELEN];
	char lastline[LINELEN];
	char addr[ADDRLEN], inst[INSTLEN];
	uint32_t daddr = 0;
	unsigned long lines = 0;
	uint32_t inum = 0;
	uint32_t bblnum = 0;
	uint32_t lastbbl = 0;
	uint32_t edgenum = 0;

	memset(in,       0, sizeof(in));
	memset(lastline, 0, sizeof(lastline));
	memset(addr,     0, sizeof(addr));
	memset(inst,     0, sizeof(inst));
	
	bool inInstructions = false;
	bool isFirstAddr = true;
	bool inBranch = false;

	fin = fopen(this->tracefile, "r");
	
	if(fin == 0)
	{
		//wxLogDebug(wxT("Error opening file %s\n"), this->tracefile);
		return;
	}

	//wxLogDebug(wxT("%s opened successfully\n"), this->tracefile);

	// Add the start basic block and edge
	initAddress(&bblMap[START_ADDR], START_ADDR, "START", 1, bblnum++);
	lastbbl = START_ADDR;

	while(fgets(in, sizeof(in)-1, fin) != NULL)
	{
		// if the lines are the same, skip it. Ether is weird like that
		if(strcmp(in, lastline) == 0)
			continue;
		else
			strncpy(lastline, in, sizeof(lastline));

		// This marks the beginning of the instructions
		if(!inInstructions && (strstr(in, "Entry Point:") ))
		{
			inInstructions = true;
			sscanf(in, "%*s%*s%s", addr);
			//wxLogDebug(wxT("Entry point: %s\n"), addr);
			continue;
		}
		else if (!inInstructions && isAddressLine(in))
		{
			inInstructions = true;
			sscanf(in, "%s:", addr);
			continue;
		}
		else if (!inInstructions)
		{
			continue;
		}

		// Check for an end condition
		if(strstr(in, "Handling sigint"))
			break;

		// If we are over the limit on instruction size then cap it at INSTLEN
		size_t inlen = strlen(in) > INSTLEN ? INSTLEN : strlen(in);
		size_t j = 0;
		size_t i;

		// read in the address
		for(i = 0 ; i < inlen ; i++)
		{
			if (in[i] == ':' || in[i] == ' ' || in[i] == '\t' || in[i] == '\0')
				break;
			if (in[i] != ':' && j < inlen)
				addr[j++] = in[i];
		}

		addr[j] = '\0';

		// skip the ": "
		i += 2;
		j = 0;

		// read in the instruction
		while(i < inlen)
		{
			// Fix the tab to be a space
			if( in[i] == '\t' ) 
				in[i] = ' ';

			inst[j++] = in[i++];
		}

		inst[j] = '\0';

		xtoi(addr, &daddr);

		if(doBasicBlocks)
		{
			if (isFirstAddr)
			{
				// The first address is always the beginning of a basic block. 
				// It stays that way until a branch is seen
				// What about if the first instruction is a branch instruction?
				initAddress(&bblMap[daddr], daddr, inst, 0, bblnum++);
				initEdge(&edgeMap[MAKE_EDGE_KEY(START_ADDR, daddr)], START_ADDR, daddr, 1, edgenum++);
				isFirstAddr = false;
				lastbbl = daddr;
			}
			else if (!inBranch && branchMatch(inst))
			{
				// If we're not presently in a branch and come to a branch instruction
				// setup for a new basic block
				inBranch = true;
			}
			else if (inBranch)			
			{
				// In a branch and we come to another branch instruction.
				// allocate the memory and end it off
				// Example: 
				// jmp ADDR1
				// ADDR1:
				// jmp ADDR2 <-- this is where we'd be at
				if(bblMap.find(daddr) != bblMap.end())
				{
					bblMap[daddr].count++;
				}
				else
				{
					initAddress(&bblMap[daddr], daddr, inst, 0, bblnum++);
				}

				// Add an edge to the edge list
				// current = daddr
				// last = lastbbl
				unsigned long long edgekey = 0;
				edgekey = MAKE_EDGE_KEY(lastbbl, daddr);
				
				// Add the edge to the list
				if(edgeMap.find(edgekey) != edgeMap.end())
					edgeMap[edgekey].count++;
				else
					initEdge(&edgeMap[edgekey], GET_SRC_FROM_EDGE_KEY(edgekey),
						GET_DST_FROM_EDGE_KEY(edgekey),
						1,
						edgenum++);

				lastbbl = daddr;

				// Setup the instruction states
				if (branchMatch(inst))
					inBranch = true;
				else
					inBranch = false;
			}
		}
		else
		{
			if (isFirstAddr)
			{
				initAddress(&bblMap[daddr], daddr, inst, 0, bblnum++);
				initEdge(&edgeMap[MAKE_EDGE_KEY(START_ADDR, daddr)], START_ADDR, daddr, 1, edgenum++);
				isFirstAddr = false;
				lastbbl = daddr;
			}
			else
			{
				if(bblMap.find(daddr) != bblMap.end())
				{
					bblMap[daddr].count++;
				}
				else
				{
					initAddress(&bblMap[daddr], daddr, inst, 0, bblnum++);
				}
				
				unsigned long long edgekey = 0;
				edgekey = MAKE_EDGE_KEY(lastbbl, daddr);
				
				// Add the edge to the list
				if(edgeMap.find(edgekey) != edgeMap.end())
					edgeMap[edgekey].count++;
				else
					initEdge(&edgeMap[edgekey],
					GET_SRC_FROM_EDGE_KEY(edgekey),
					GET_DST_FROM_EDGE_KEY(edgekey),
					1,
					edgenum++);

				lastbbl = daddr;
			}
		}
		inum++;
		lines++;
	}

	fclose(fin);
	//wxLogDebug(wxT("Analyzed %u instructions with %u basic blocks.\n"), inum, bblnum);
}

// Wrapper for wxWidgets
void Trace::writeGmlFile(wxString infile)
{
	char gmlfile[FILELEN] = {0};

	strncpy(gmlfile, infile.ToAscii(), sizeof(gmlfile));
	this->writeGmlFile(gmlfile);
}

void Trace::writeGmlFile(void)
{
	this->writeGmlFile(this->outputfile);
}

void Trace::writeGmlFile(char *gmlfile)
{
	FILE *fout = NULL;
	const trace_address_t *addr = NULL;
	const trace_edge_t *edge = NULL;
	uint32_t width = 0;
	size_t addrCharLen = 0;

	//wxLogDebug(wxT("Writing output to %s\n"), gmlfile);

	fout = fopen(gmlfile, "w");

	if (fout == NULL)
	{
		//wxLogDebug(wxT("Could not open gml file for output"));
		throw "Could not open gml file for output";
	}

	// GML header info
	fprintf(fout, "Creator \"ogdf::GraphAttributes::writeGML\"\n");
	fprintf(fout, "directed 1\n");
	fprintf(fout, "graph [\n");

	for(bblMap_t::const_iterator it = bblMap.begin() ; it != bblMap.end() ; it++)
	{
		addr = &it->second;
		fprintf(fout, "node [\n");
		fprintf(fout, "id %u\n", addr->num);
		//fprintf(fout, "template \"oreas:std:rect simple\"\n");
		if (it->first == START_ADDR)
		{
			fprintf(fout, "label \"<label font=\\\"Arial,10,-1,5,50,0,0,0,0,0\\\" align=\\\"68\\\" textColor=\\\"FFFFFFFF\\\">%8.8x</label>\"\n", addr->addr);
			addrCharLen = 0;
		}
		else if (addr->isApi == true)
		{
			fprintf(fout, "label \"<label font=\\\"Arial,10,-1,5,50,0,0,0,0,0\\\" align=\\\"68\\\" textColor=\\\"00000000\\\">%s</label>\"\n", addr->info.api);
			addrCharLen = strlen(addr->info.api);
		}
		else
		{
			fprintf(fout, "label \"<label font=\\\"Arial,10,-1,5,50,0,0,0,0,0\\\" align=\\\"68\\\" textColor=\\\"00000000\\\">%8.8x</label>\"\n", addr->addr);
			addrCharLen = 0;
		}
		fprintf(fout, "graphics [\n");
		fprintf(fout, "x 51.00000000\n");
		fprintf(fout, "y 31.00000000\n");
		fprintf(fout, "w %u.00000000\n", (addrCharLen <= 8 ? 80 : 10 * (unsigned int) addrCharLen));
		fprintf(fout, "h 20.00000000\n");
		fprintf(fout, "fill \"#%6.6x\"\n", addrColor(addr->addr));
		fprintf(fout, "line \"#000000\"\n");
		fprintf(fout, "pattern \"1\"\n");
		fprintf(fout, "stipple 1\n");
		fprintf(fout, "lineWidth 1.000000000\n");
		fprintf(fout, "type \"rectangle\"\n");
		fprintf(fout, "width 1.0\n");
		fprintf(fout, "]\n"); // end graphics
		fprintf(fout, "]\n"); // end node
	}

	for(edgeMap_t::const_iterator it = edgeMap.begin() ; it != edgeMap.end() ; it++)
	{
		edge = &it->second;
		fprintf(fout, "edge [\n");
		fprintf(fout, "source %u\n", bblMap[edge->src].num); 
		fprintf(fout, "target %u\n", bblMap[edge->dst].num);
		fprintf(fout, "generalization 0\n");
		fprintf(fout, "graphics [\n");
		fprintf(fout, "type \"line\"\n");
		fprintf(fout, "arrow \"none\"\n");
		fprintf(fout, "stipple 1\n");

		if (edge->count >= 1 && edge->count <= 10)
			width = edge->count;
		else
			width = 12;

		fprintf(fout, "lineWidth %u.000000000\n", width);
		fprintf(fout, "Line [\n");
		fprintf(fout, "point [ x 41.00000000 y 31.00000000 ]\n");
		fprintf(fout, "point [ x 41.00000000 y 31.00000000 ]\n");
		
		if (edge->count > 10000)
			fprintf(fout, "fill \"#0000bb\"\n");
		else
			fprintf(fout, "fill \"#000000\"\n");

		fprintf(fout, "]\n"); // end line
		fprintf(fout, "]\n"); // end graphics
		fprintf(fout, "]\n"); // end edge
	}
     
	fprintf(fout, "]\n"); // end graph

	fprintf(fout, "rootcluster [\n");

	for(bblMap_t::const_iterator it = bblMap.begin() ; it != bblMap.end() ; it++)
	{
		addr = &it->second;
        fprintf(fout, "vertex \"%u\"\n", addr->num);
	}

	fprintf(fout, "]\n"); // end root cluster
	
	fclose(fout);
}

void Trace::layoutGraph(const char *infile)
{
	layoutGraph(infile, (const char *) outputfile);
}

void Trace::layoutGraph(wxString infile)
{
	layoutGraph(infile.ToAscii(), (const char *) outputfile);
}

void Trace::layoutGraph(wxString infile, wxString outfile)
{
	char tmpinfile[FILELEN] = {0};
	char tmpoutfile[FILELEN] = {0};

	strncpy(tmpinfile, infile.ToAscii(), sizeof(tmpinfile) - 1);
	strncpy(tmpoutfile, outfile.ToAscii(), sizeof(tmpoutfile) - 1);

	this->layoutGraph(tmpinfile, tmpoutfile);
}

void Trace::layoutGraph(const char *infile, const char *outfile)
{
	if (!infile)
	{
		throw "Specify input file";
	}

	if (!outfile)
	{
		throw "Specify output file";
	}

	using namespace ogdf;
	
	Graph G;
	GraphAttributes GA(G,
			   GraphAttributes::nodeGraphics |
			   GraphAttributes::edgeGraphics |
			   GraphAttributes::nodeLabel |
			   GraphAttributes::edgeStyle |
			   GraphAttributes::nodeColor
		);

	if( !GA.readGML(G, infile) )
	{
		throw "Could not read GML file\n";
	}
	
	FMMMLayout fmmm;
	
	fmmm.useHighLevelOptions(true);
	fmmm.unitEdgeLength(300.0);
	fmmm.newInitialPlacement(true);
	fmmm.qualityVersusSpeed(FMMMLayout::qvsGorgeousAndEfficient);
	
	fmmm.call(GA);
	GA.writeGML(outfile);
	
}

uint32_t Trace::packerAddrColor(uint32_t addr)
{
	DWORD rva = addr - pOptHeader->ImageBase;
	PIMAGE_SECTION_HEADER pSection = pSectionHeader;
	bool foundSection = false;
	uint32_t i = 0;

	// Color the start address blue
	if (START_ADDR == addr)
		return doColorBlind ? CB_START_ADDR_COLOR : START_ADDR_COLOR;

	// Get the Section
	for(i = 0 ; i < pPeHeader->FileHeader.NumberOfSections ; i++, pSection++)
	{
		if ( rva >= pSection->VirtualAddress && rva < pSection->VirtualAddress + pSection->Misc.VirtualSize)
		{
			foundSection = true;
			break;
		}
	}

	if (!foundSection)
		return doColorBlind ? CB_NO_SECTION_COLOR : NO_SECTION_COLOR;

	if (pSection->SizeOfRawData == 0)
		return doColorBlind ? CB_SIZEOF_RAWDATA_ZERO_COLOR : SIZEOF_RAWDATA_ZERO_COLOR;

	if (sectionEntropy[i] > 7.5)
		return doColorBlind ? CB_HIGH_ENTROPY_COLOR : HIGH_ENTROPY_COLOR;

	return doColorBlind ? CB_NORMAL_COLOR : NORMAL_COLOR;
}

uint32_t Trace::addrColor(uint32_t addr)
{
	switch (graphColoringAlgorithm)
	{
	case GRAPH_COLOR_PACKER_COLORBLIND:
		doColorBlind = true;
	case GRAPH_COLOR_PACKER:
		return packerAddrColor(addr);
		break;
	default:
		return NORMAL_COLOR;
		break;
	}
}


