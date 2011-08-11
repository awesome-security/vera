/*
 *
 * Copyright (C) 2011 Daniel Quist, All Rights Reserved
 *
 * Portions from the OllyDump Import reconstruction code by Gigapede
 */

#include "stdafx.h"

#include <windows.h>
#include <psapi.h>

#include "WorkingSet.h"


WorkingSet::WorkingSet()
{
	isWorkingSetLoaded			= false;
	isApiMapBuilt				= false;
	root						= NULL;
	lastFound					= NULL;

	if (buildNewWorkingSetInfo() == false)
		throw "Could not build working set info";

	collapseWSInfo();
	buildApiMaps();
}

WorkingSet::~WorkingSet()
{
	if (root != NULL)
	{
		freeTree(root);
		root = NULL;
	}

	size_t len = dlls.size();
	
	for (size_t i = 0 ; i < len ; i++)
	{
		free(dlls[i]);
	}

	dlls.clear();

	ApiMapType::iterator end = apis.end();
	for (ApiMapType::iterator it = apis.begin() ; it != end ; it++)
		free(it->second);
	
	apis.clear();
}


workingSetTree_t * WorkingSet::addTree(workingSetTree_t *tree, DWORD addr, wchar_t *modulename)
{
	if (root == NULL && tree == NULL)
	{
		size_t foo = sizeof(workingSetTree_t);

		root = (workingSetTree_t *) malloc(sizeof(workingSetTree_t));

		if (root == NULL)
		{
			printf("Could not allocate memory\n");
			return NULL;
		}

		memset(root, 0, sizeof(workingSetTree_t));

		root->address = addr;
		lstrcpynW(root->module, modulename, wcslen(root->module));
		return root;
	}

	if (tree == NULL) 
	{
		printf("Bad tree\n");
		return NULL;
	}

	if (addr < tree->address)
	{
		if (tree->left == NULL) // Add to branch
		{
			tree->left = (workingSetTree_t *) malloc(sizeof(workingSetTree_t));
			if (tree->left == NULL)
			{
				printf("Could not allocate memory\n");
				return NULL;
			}
			memset(tree->left, 0, sizeof(workingSetTree_t));
			tree->left->address = addr;
			lstrcpynW(tree->left->module, modulename, wcslen(tree->left->module)-1);
			return tree->left;
		}
		else
		{
			return addTree(tree->left, addr, modulename);
		}
	}
	else if (addr > tree->address)
	{
		if (tree->right == NULL) // Add to branch
		{
			tree->right = (workingSetTree_t *) malloc(sizeof(workingSetTree_t));
			if (tree->right == NULL)
			{
				printf("Could not allocate memory\n");
				return NULL;
			}
			memset(tree->right, 0, sizeof(workingSetTree_t));
			tree->right->address = addr;
			lstrcpynW(tree->right->module, modulename, wcslen(tree->right->module)-1);
			return tree->right;
		}
		else
		{
			return addTree(tree->right, addr, modulename);
		}
	}

	return NULL;
}

void WorkingSet::printWorkingSet(FILE *fout)
{
	printWorkingSetTree(root, fout);
}

void WorkingSet::printWorkingSetTree(workingSetTree_t *tree, FILE *fout)
{
	if (tree == NULL || fout == NULL)
		return;

	if (tree->left != NULL)
		printWorkingSetTree(tree->left, fout);
	
	fwprintf(fout, L"%8.8x %s\n", tree->address, tree->module);

	if (tree->right != NULL)
		printWorkingSetTree(tree->right, fout);

}

/* Builds the working set tree */
bool WorkingSet::buildNewWorkingSetInfo(void)
{
	HANDLE myHandle = GetCurrentProcess();
	
	PSAPI_WORKING_SET_INFORMATION		psapiSize	= {0};
	PSAPI_WORKING_SET_INFORMATION *		pWs			= NULL;
	SYSTEM_INFO							si			= {0};
	size_t								setSize		= 0;

	GetSystemInfo(&si);
	QueryWorkingSet(myHandle, &psapiSize, sizeof(psapiSize));
	
	setSize = sizeof(PSAPI_WORKING_SET_INFORMATION) + ((psapiSize.NumberOfEntries + 1000) * sizeof(PSAPI_WORKING_SET_BLOCK));
	pWs = (PSAPI_WORKING_SET_INFORMATION *) malloc(setSize);

	if (pWs == NULL)
	{
		printf("Memory allocation error\n");
		return false;
	}

	memset(pWs, 0, setSize);

	if (! QueryWorkingSet(myHandle, pWs, setSize ) )
	{
		printf("QueryWorkingSet failed %u\n", GetLastError());
		return false;
	}

	for (size_t i = 0 ; i < psapiSize.NumberOfEntries ; i++)
	{
		wchar_t fname[MODULE_LEN] = {0};
		DWORD addr = pWs->WorkingSetInfo[i].VirtualPage * si.dwPageSize;
		GetMappedFileNameW(myHandle, (LPVOID) addr, fname, sizeof(fname));

		addTree(root, addr, fname );
	}

	free(pWs);

	return true;
}

workingSetTree_t *WorkingSet::findTreeAddr(DWORD addr)
{
	return findAddr(root, addr);
}

dllList_t *WorkingSet::findDllList(wchar_t *module)
{ 
	if (module == NULL)
		return NULL;

	size_t len = dlls.size();

	for (size_t i = 0 ; i < len ; i++)
		if (wcsstr(dlls[i]->module, module))
			return dlls[i];

	return NULL;
}

workingSetTree_t *WorkingSet::findAddr(workingSetTree_t *tree, DWORD addr)
{
	if (tree == NULL)
		return NULL;

	if ( (tree->address + PAGE_SIZE) < addr)
		return findAddr(tree->right, addr);
	else if (tree->address > addr)
		return findAddr(tree->left, addr);
	else // Should only match when they are equal
		return tree;

}

void WorkingSet::freeTree(workingSetTree_t *tree)
{
	if (tree->left != NULL)
	{
		freeTree(tree->left);
		tree->left = NULL;
	}
	
	if (tree->right != NULL)
	{
		freeTree(tree->right);
		tree->right = NULL;
	}

	if (tree->right == NULL && tree->left == NULL)
		free(tree);
}

void WorkingSet::collapseWSInfo(void)
{
	collapseWSInfo(root);
}

// This function works under the assumption that the tree is in order
void WorkingSet::collapseWSInfo(workingSetTree_t *tree)
{	
	if (tree == NULL)
		return;

	if (tree->left != NULL)
		collapseWSInfo(tree->left);

	// Case 1 no lastFound yet
	if (lstrlenW(tree->module) > 0)
	{
		if (lastFound == NULL)
		{

			//wprintf(L"%8.8x Found first module %s \n", tree->address, tree->module);
			lastFound = tree;
			dllList_t *d = (dllList_t *) malloc(sizeof(dllList_t));
			lstrcpynW(d->module, tree->module, sizeof(d->module));
			d->startAddress = tree->address;
			d->size = PAGE_SIZE;
			dlls.push_back(d);
		}
		else 
		{
			// Case 2 lastFound, address is in same module and next in page order
			if ( lstrcmpW(tree->module, lastFound->module) == 0)
			{
				lastFound = tree;
				dlls.back()->size = (tree->address + PAGE_SIZE) - dlls.back()->startAddress;
				
			}
			// Case 3 lastFound, address is not in same module
			else
			{
				//wprintf(L"%8.8x New module %s\n", tree->address, tree->module);
				lastFound = tree;
				dllList_t *d = (dllList_t *) malloc(sizeof(dllList_t));
				lstrcpynW(d->module, tree->module, sizeof(d->module));
				d->startAddress = tree->address;
				d->size = PAGE_SIZE;
				dlls.push_back(d);
			}
		}
	}

	if (tree->right != NULL)
		collapseWSInfo(tree->right);
}

void WorkingSet::printDlls(void)
{
	size_t len = dlls.size();

	for (size_t i = 0 ; i < len ; i++)
		wprintf(L"%s: start %8.8x size: %u\n", dlls[i]->module, dlls[i]->startAddress, dlls[i]->size);
}

/**
 ** WorkingSet::buildApiMaps
 **
 ** Input:  The working set of all the mapped DLLs, the base location of those DLLs in memory, the mapped
 **		    DLL information in memory.
 ** Output: A populated apis data structure
 **		   
 ** 
 ** This code was adapted from the OllyDump rebuild import code. It was modified in a couple of ways.
 ** First, the general idea of finding the export data was taken to find a listing of all the 
 ** exported functions within the DLL. From there, the actual code for extracting the DLL associated
 ** the file uses the original import rebuilding with code to extract the API information. The end
 ** result is that all the data is stored in the apis map_hash data structure keyed by the 
 ** address.
 **
 ** These things need to be done:
 **   * Clean up so that this function can be used to recover all the information available
 **   * Remove or at least reduce the reliance upon the Windows API to recover the DLL and Api 
 **     information
 **   * Finish removing the portions of the OllyDump code
 **/
void WorkingSet::buildApiMaps(void)
{

	size_t len = dlls.size();

	for (size_t i = 0 ; i < len ; i++)
	{
		addDllExports(dlls[i]->module);
	}
}

void WorkingSet::addDllExports(wchar_t *dlls)
{
	PIDH dosh;
	PINH peh;
	DWORD *pDW;
	DWORD expbase;
	DWORD functnum;
	DWORD functaddr;
	DWORD namenum;
	DWORD nameaddr;
	DWORD ordinaladdr;
	IMAGE_DATA_DIRECTORY dir;
	size_t j = 0;

	if (wcsstr(dlls, L".dll") == NULL)
		return;

	// find the DLL name
	for (j = lstrlenW(dlls) ; j >= 0 ; j--)
		if (dlls[j] == '\\')
		{
			j++;
			break;
		}

	wchar_t *dllname = &(dlls[j]);
	//DWORD modulebase = dlls[i]->startAddress;
	DWORD modulebase = (DWORD) LoadLibraryW(dllname);

	if (modulebase == 0) // The lookup failed somehow, skip it.
		return;

	//wprintf(L"%s: base %8.8x start %8.8x size: %u\n", dllname, modulebase, dllp[i]->startAddress, dllp[i]->size);

	dosh = (PIDH)modulebase;
	if(dosh->e_magic != IMAGE_DOS_SIGNATURE) 
		return;

	peh = (PINH)((DWORD)dosh + dosh->e_lfanew);

	if(peh->Signature != IMAGE_NT_SIGNATURE) 
		return;

	if (peh->FileHeader.Machine != 0x014c) // We only support i386 machine type DLLs
		return;

	dir = peh->OptionalHeader.DataDirectory[0];
	pDW = (DWORD*)(dir.VirtualAddress + 0x10 + (DWORD)dosh); // go fast to the base

	    // get the export values
	expbase     = *pDW;
	pDW++;
	functnum    = *pDW;
	pDW++;
	namenum     = *pDW;
	pDW++;
	functaddr   = *pDW;
	pDW++;
	nameaddr    = *pDW;
	pDW++;
	ordinaladdr = *pDW;

	if (functnum == 0)
		return;

	DWORD *nameptr  = (DWORD *) (nameaddr+modulebase);
	WORD *ordptr   = (WORD *) (ordinaladdr+modulebase);
	DWORD *functptr = (DWORD *) (functaddr+modulebase);
	
	for (DWORD k = 0 ; k < functnum ; k++)
	{
		//ordinal = (*(ordptr+k) + modulebase) &0xFFFF;
		//DWORD addr = (*(functptr+k*sizeof(DWORD)) + modulebase);

		DWORD functionentry = *functptr;

		char dll[MODULE_LEN];
		char apiname[MAX_API_LEN];
		wcstombs(dll, dllname, MODULE_LEN);

		WORD ordinal = GetApiNameOrdinal(functionentry + modulebase, dll, apiname);

		apiInfo_t * api = (apiInfo_t *) malloc(sizeof(apiInfo_t));

		if (api == NULL)
		{
			printf("Memory allocation error\n");
			return;
		}

		ZeroMemory(api, sizeof(apiInfo_t));

		// Find the dll pointer
		dllList_t *dllp = NULL;

		size_t len = this->dlls.size();
		for (size_t i = 0 ; i < len ; i++)
		{
			if (wcsstr(this->dlls[i]->module, dllname) != NULL)
			{
				dllp = NULL;
				break;
			}
		}

		api->dll = dllp;
		api->address = functionentry + modulebase;
		api->ordinal = ordinal;

		mbstowcs(api->name, apiname, MAX_API_LEN);
		
		//if (k < namenum) // If there is a name available, set it up
		//	mbstowcs(api->name, (char *)(*(nameptr+k) + modulebase), MAX_API_LEN);

		apis[api->address] = api;
		functptr++;
	}

	// Decrement the reference to the DLL so it can be unloaded if necessary
	FreeLibrary(GetModuleHandleW(dllname));
}

/**
 ** WORD GetApiNameOrdinal(DWORD ApiAddress, char *DllName, char *ApiName) 
 **
 ** This is code from the OllyDump import rebuilding code.
 **/

WORD GetApiNameOrdinal(DWORD ApiAddress, char *DllName, char *ApiName)
{
  DWORD functionentry;
  DWORD *pDW;
  WORD  *pWO;
  DWORD i;
  DWORD functposition,nameposition;
  DWORD modulebase;
  // export table values
  DWORD expbase;
  DWORD functnum;
  DWORD functaddr;
  DWORD namenum;
  DWORD nameaddr;
  DWORD ordinaladdr;
  WORD  ordinal;
  // PE structs
  PIDH dosh;
  PINH peh;
  IMAGE_DATA_DIRECTORY dir;

  functposition = 0xFFFFFFFF;

  __try {
    // load the dll
    modulebase = (DWORD)GetModuleHandleA(DllName);
    if(modulebase == 0) {
      modulebase = (DWORD)LoadLibraryA(DllName);
      if(modulebase == 0) {
        return((WORD)(functposition&0xFFFF));
      }
    }
    if(ApiAddress <= modulebase) {
      return((WORD)(functposition&0xFFFF));
    }
    functionentry = ApiAddress - modulebase;
    // check whether hmodule is a valid PE file
    dosh = (PIDH)modulebase;
    if(dosh->e_magic != IMAGE_DOS_SIGNATURE) {
      return((WORD)(functposition&0xFFFF));
    }
    peh = (PINH)((DWORD)dosh + dosh->e_lfanew);
    if(peh->Signature != IMAGE_NT_SIGNATURE) {
      return((WORD)(functposition&0xFFFF));
    }

    dir = peh->OptionalHeader.DataDirectory[0];
    pDW = (DWORD*)(dir.VirtualAddress + 0x10 + (DWORD)dosh); // go fast to the base
  
    // get the export values
    expbase     = *pDW;
    pDW++;
    functnum    = *pDW;
    pDW++;
    namenum     = *pDW;
    pDW++;
    functaddr   = *pDW;
    pDW++;
    nameaddr    = *pDW;
    pDW++;
    ordinaladdr = *pDW;
    // search the entry in the RVA array of the export table
    pDW = (DWORD*)((DWORD)dosh + functaddr);
    functposition = 0xFFFFFFFF;
    for(i=0; i<functnum; i++) {				// find the function position
      if(functionentry == *pDW) { 
        functposition = i;
        break;
      }
      pDW++;
    }
    if(functposition != 0xFFFFFFFF) {
      nameposition = 0xFFFFFFFF;
      pWO = (WORD*)(ordinaladdr + modulebase); // Match the function position in the ordinal table to get the name position
      for(i=0; i<namenum; i++) {
        if(functposition == (DWORD)*pWO) {
          nameposition = i;
          break;
        }
        pWO++;
      }
      if(nameposition != 0xFFFFFFFF) {
        pDW = (DWORD*)(nameaddr + modulebase);
        pDW += nameposition;
        sprintf(ApiName,"%s",(char*)(*pDW+modulebase));
      }
      else {
        sprintf(ApiName,"");
      }
    }
    ordinal = (WORD)((functposition + ((functposition == 0xFFFFFFFF) ? 0 : expbase))&0xFFFF);
  }
  __except(1) {
    sprintf(ApiName,"");
    return(0xFFFF);
  }
  return(ordinal);
}

apiInfo_t * WorkingSet::findAddr(DWORD addr)
{
	ApiMapType::iterator ii = apis.find(addr);

	if (ii == apis.end())
	{
		if (findTreeAddr(addr) == NULL)
		{
			wchar_t fname[MODULE_LEN] = {0};
			if (GetMappedFileNameW(GetCurrentProcess(), (LPVOID) addr, fname, sizeof(fname)) > 0)
			{ // Found something
				workingSetTree_t *dllNode = addTree(root, addr, fname);

				if (dllNode == NULL)
					return NULL;

				collapseWSInfo(dllNode);

				addDllExports(fname);

				ii = apis.find(addr);
				
				if (ii == apis.end())
					return NULL;
				
				return ii->second;
			}
			else
				return NULL; // Still nothing, return NULL (not found)
		}
		else
			return (apiInfo_t *) VALID_MEMORY_NO_DLL;
	}
	else
		return ii->second;

}

int _tmain(int argc, _TCHAR* argv[])
{
	//Sleep(100);
	int									testval2	= 301;
	int	*								testval3	= NULL;
	testval3 = (int *) malloc(sizeof(int));
	*testval3 = 302;
	
	// The internal Windows method of loading DLL and API information
	DWORD addr = (DWORD) GetProcAddress(LoadLibraryW(L"ws2_32.dll"), "WSAStartup");

	// The OllyDump Import Recovery Code
	char dll[256] = "ws2_32.dll";
	char api[256] = {0};
	wchar_t wapi[256] = {0};
	WORD ord = GetApiNameOrdinal(addr, dll, api);
	
	mbstowcs(wapi, api, 256);
	
	// The new WorkingSet recovery code
	WorkingSet *ws = new WorkingSet();
	apiInfo_t *node = ws->findAddr((DWORD) addr);

	if (node == NULL)
	{
		wprintf(L"WSAStartup FAILED address not found\n");
	}
	else if (addr == node->address && ord == node->ordinal && lstrcmpW(wapi, node->name) == 0)
	{
		wprintf(L"WSAStartup resolution test %p found (%s) %p %u\n", addr, node->name, node->address, node->ordinal);
	}
	else
	{
		wprintf(L"WSAStartup FAILED Resolved APIs: OllyDump/%s WSClass/%s ord: %u %u addr: %8.8x %8.8x\n",
			wapi, node->name,
			ord, node->ordinal,
			addr, node->address);
	}

	// Test a later load of a DLL
	addr = (DWORD) GetProcAddress(LoadLibraryW(L"msfeeds.dll"), "MsfeedsCreateInstance");
	
	// Test for proper failure
	node = ws->findAddr((DWORD) &addr);
	if (node == VALID_MEMORY_NO_DLL)
		wprintf(L"Delayed DLL load test passed\n");
	else
		wprintf(L"Delayed DLL load test FAILED!\n");

	ZeroMemory(dll, sizeof(dll));
	ZeroMemory(api, sizeof(api));
	ZeroMemory(wapi, sizeof(wapi));

	strncpy(dll, "msfeeds.dll", sizeof(dll));

	ord = GetApiNameOrdinal(addr, dll, api);
	mbstowcs(wapi, api, 256);

	// We need to add to the workingset copy
	/*delete ws;
	ws = new WorkingSet();*/

	node = ws->findAddr((DWORD) addr);
	if (node == NULL)
	{
		wprintf(L"BOINK Expected!\n");
	}
	else if ( node == VALID_MEMORY_NO_DLL)
		wprintf(L"Valid memory found, but no DLL was there.");
	else if (addr == node->address && ord == node->ordinal && lstrcmpW(wapi, node->name) == 0)
	{
		wprintf(L"MsfeedsCreateInstance resolution test %p found (%s) %p %u\n", addr, node->name, node->address, node->ordinal);
	}
	else
	{
		wprintf(L"MsfeedsCreateInstance FAILED Resolved APIs: OllyDump/%s WSClass/%s ord: %u %u addr: %8.8x %8.8x\n",
			wapi, node->name,
			ord, node->ordinal,
			addr, node->address);
	}


	// Test the findtree portion
	workingSetTree_t *tree = ws->findTreeAddr((DWORD) GetApiNameOrdinal);

	if (tree == NULL)
		wprintf(L"Tree lookup FAIL\n");
	else
	{
		wprintf(L"Tree lookup passed %s %8.8x\n", tree->module, tree->address);
	}
	
	// Find the DLLs
	dllList_t *dllt = ws->findDllList(L"workingset.exe");

	if (dllt == NULL)
		wprintf(L"DLL lookup FAIL\n");
	else 
		wprintf(L"DLL lookup passed %s %8.8x %x\n", dllt->module, dllt->startAddress, dllt->size);

	delete ws;
	ws = NULL;

	free(testval3);
	testval3 = NULL;

	wprintf(L"Finished.\n");
	getchar();
	return 0;
}

