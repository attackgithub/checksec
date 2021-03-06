// checksec.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

/***********************************************************
* Global Variable Needed For CheckSec Flags
***********************************************************/
#define ASLR_ENABLED     0x00000001
#define DEP_ENABLED      0x00000002
#define SAFESEH_ENABLED  0x00000004


/***********************************************************
* Global Variable Needed For Versioning
***********************************************************/
EXT_API_VERSION g_ExtApiVersion = {
	1 ,
	0 ,
	EXT_API_VERSION_NUMBER ,
	0
};


/***********************************************************
* Global Variable Needed For Functions
***********************************************************/
WINDBG_EXTENSION_APIS ExtensionApis = { 0 };


/***********************************************************
* ExtensionApiVersion
*
* Purpose: WINDBG will call this function to get the version
*          of the API
*
*  Parameters:
*     Void
*
*  Return Values:
*     Pointer to a EXT_API_VERSION structure.
*
***********************************************************/
LPEXT_API_VERSION WDBGAPI ExtensionApiVersion(void)
{
	return &g_ExtApiVersion;
}

/***********************************************************
* WinDbgExtensionDllInit
*
* Purpose: WINDBG will call this function to initialize
*          the API
*
*  Parameters:
*     Pointer to the API functions, Major Version, Minor Version
*
*  Return Values:
*     Nothing
*
***********************************************************/
VOID WDBGAPI WinDbgExtensionDllInit(PWINDBG_EXTENSION_APIS lpExtensionApis,
									USHORT usMajorVersion,
									USHORT usMinorVersion
								   )
{
	ExtensionApis = *lpExtensionApis;
	dprintf("[Checksec] Successfully loaded!\n");
}

/***********************************************************
* User Defined Function
*
***********************************************************/
int checksec(DWORD baseAddress) {
	int ret = 0;
	ULONG returnLength;

	IMAGE_OPTIONAL_HEADER image_optional_header;
	ReadMemory(
		(ULONG_PTR)baseAddress + sizeof(IMAGE_NT_HEADERS) - sizeof(IMAGE_OPTIONAL_HEADER),
		&image_optional_header,
		sizeof(image_optional_header),
		&returnLength
	);

	if (image_optional_header.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE) {
		ret |= ASLR_ENABLED;
	}

	if (image_optional_header.DllCharacteristics & IMAGE_DLLCHARACTERISTICS_NX_COMPAT) {
		ret |= DEP_ENABLED;
	}

	if (image_optional_header.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].VirtualAddress != 0 \
		&& image_optional_header.DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size != 0) {
		ret |= SAFESEH_ENABLED;
	}

	return ret;
}

int HelloWindbgExtensionMain(HANDLE hCurrentProcess) {
	HMODULE hModule;
	PROCESS_BASIC_INFORMATION basicInfo;
	ULONG returnLength;
	PEB peb;
	PEB_LDR_DATA ldrList;
	LIST_ENTRY listModules;
	LDR_DATA_TABLE_ENTRY module;
	DWORD firstModule;

	wchar_t wModuleName[1024] = { 0 };
	char aModuleName[1024] = { 0 };
	int protections = 0;

	hModule = GetModuleHandle(L"ntdll.dll");
	if (hModule == NULL)
	{
		dprintf("[-] checksec - Failed to GetModuleHandle\n");
		return 0;
	}

	// https://msdn.microsoft.com/en-us/library/windows/desktop/ms684280(v=vs.85).aspx
	pNtQueryInformationProcess NtQueryInformationProcess = (pNtQueryInformationProcess)GetProcAddress(
		hModule, "NtQueryInformationProcess"
	);

	NtQueryInformationProcess(hCurrentProcess, 0, &basicInfo, sizeof(basicInfo), &returnLength);

	ReadMemory((ULONG_PTR)basicInfo.PebBaseAddress, &peb, sizeof(peb), &returnLength);

	ReadMemory((ULONG_PTR)peb.Ldr, &ldrList, sizeof(ldrList), &returnLength);

	firstModule = (DWORD)ldrList.InLoadOrderModuleList.Flink;

	ReadMemory((ULONG_PTR)firstModule, &listModules, sizeof(listModules), &returnLength);
	ReadMemory((ULONG_PTR)firstModule, &module, sizeof(module), &returnLength);

	dprintf("Image Base \tSize \t\tASLR \tDEP \tSafe SEH \tModule Name \n");

	do {
		ReadMemory((ULONG_PTR)module.BaseDllName.Buffer, wModuleName, module.BaseDllName.Length + 2, &returnLength);
		WideCharToMultiByte(CP_ACP, 0, wModuleName, -1, aModuleName, sizeof(aModuleName), NULL, NULL);

		protections = checksec((DWORD)module.DllBase);
		dprintf("0x%0.8X \t0x%0.8X", module.DllBase, module.SizeOfImage);

		(protections & ASLR_ENABLED) ? dprintf("\tYes") : dprintf("\tNo");
		(protections & DEP_ENABLED) ? dprintf("\tYes") : dprintf("\tNo");
		(protections & SAFESEH_ENABLED) ? dprintf("\tYes") : dprintf("\tNo");

		dprintf("\t\t%s \n", aModuleName);

		ReadMemory((ULONG_PTR)listModules.Flink, &module, sizeof(module), &returnLength);
		ReadMemory((ULONG_PTR)listModules.Flink, &listModules, sizeof(listModules), &returnLength); // Next
	} while ((DWORD)listModules.Flink != firstModule);

	return 1;
}

/***********************************************************
* !help
*
* Purpose: WINDBG will call this API when the user types !help
*
*
*  Parameters:
*     N/A
*
*  Return Values:
*     N/A
*
***********************************************************/

void checksec_help()
{
	dprintf("Windbg Extensions: CheckSec - By Nixawk\n\n");
	dprintf("!checksec - Check modules ASLR/DEP/SafeSEH settings\n");
}

DECLARE_API(help)
{
	checksec_help();
	/* String Split so it is readable in this article. */
}

/***********************************************************
* !checksec
*
* Purpose: WINDBG will call this API when the user types !checksec
*
*
*  Parameters:
*     !checksec
*
*  Return Values:
*     N/A
*
***********************************************************/
DECLARE_API(checksec)
{
	if (strcmp(args, "help") == 0)
	{
		checksec_help();
	}
	else
	{
	    HelloWindbgExtensionMain(hCurrentProcess);
	}
}


// https://docs.microsoft.com/en-us/windows-hardware/drivers/debugger/debugger-engine-and-extension-apis
// https://blogs.msdn.microsoft.com/sgajjela/2013/03/02/how-to-develop-windbg-extension-dll/
// https://dimitrifourny.github.io/2014/02/28/make-plugin-for-windbg/
// https://github.com/rapid7/metasploit-framework/tree/master/external/source/byakugan
// https://www.codeproject.com/Articles/6522/Debug-Tutorial-Part-Writing-WINDBG-Extensions
// https://en.wikipedia.org/wiki/Address_space_layout_randomization
// https://en.wikipedia.org/wiki/DEP
// https://docs.microsoft.com/en-us/cpp/build/reference/safeseh-image-has-safe-exception-handlers
