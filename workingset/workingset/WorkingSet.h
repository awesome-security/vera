/*
 *
 * Copyright (C) 2011 Daniel Quist, All Rights Reserved
 */

#pragma once

#include <windows.h>
#include <vector>
#include <hash_map>
#include <map>

#define MODULE_LEN				260
#define PAGE_SIZE				4096
#define NO_ADDRESS_FOUND		-1;
#define MAX_API_LEN				256
#define MAX_API_STRING_LEN		4096
#define VALID_MEMORY_NO_DLL		((void *) 0xFFFFFFFF)

typedef  PIMAGE_DOS_HEADER         PIDH;
typedef  PIMAGE_NT_HEADERS         PINH;
typedef  PIMAGE_SECTION_HEADER     PISH;
typedef  PIMAGE_DATA_DIRECTORY     PIDD;
typedef  PIMAGE_IMPORT_DESCRIPTOR  PIID;
typedef  PIMAGE_IMPORT_BY_NAME     PIBN;

typedef struct workingsettree 
{
	DWORD address;
	wchar_t module[MODULE_LEN];
	struct workingsettree *left;
	struct workingsettree *right;
} workingSetTree_t;

typedef struct dlllist {
	wchar_t module[MODULE_LEN];
	DWORD startAddress;
	DWORD size;
} dllList_t;

typedef struct apiinfo {
	wchar_t name[MAX_API_LEN];
	dllList_t *dll;
	WORD ordinal;
	DWORD address;
} apiInfo_t;

typedef stdext::hash_map<DWORD,apiInfo_t *> ApiMapType;
typedef std::vector<dllList_t *> DllVectorType;

WORD GetApiNameOrdinal(DWORD ApiAddress, char *DllName, char *ApiName);

class WorkingSet
{

public:
	WorkingSet();
	~WorkingSet();
	void					printWorkingSet(FILE *fout);
	void					printDlls(void);
	apiInfo_t *				findAddr(DWORD addr);
	workingSetTree_t *		findTreeAddr(DWORD addr);
	dllList_t *				findDllList(wchar_t *module);
	//WORD					GetApiNameOrdinal(DWORD ApiAddress, char *DllName, char *ApiName);

private:
	workingSetTree_t *		addTree(workingSetTree_t *tree, DWORD addr, wchar_t *modulename);
	void					printWorkingSetTree(workingSetTree_t *tree, FILE *fout);
	bool					buildNewWorkingSetInfo(void);
	void					freeTree(workingSetTree_t *tree);
	void					addDllExports(wchar_t *);
	void					collapseWSInfo(workingSetTree_t *tree);
	void collapseWSInfo(void);
	void buildApiMaps(void);
	workingSetTree_t *		findAddr(workingSetTree_t *, DWORD addr);

	bool						isWorkingSetLoaded;
	bool						isApiMapBuilt;
	workingSetTree_t *			root;
	workingSetTree_t *			lastFound;
	DllVectorType				dlls;
	ApiMapType					apis;
};