// dllmain.cpp : Defines the entry point for the injectee.
#include "stdafx.h"
#include <easyhook.h>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <comdef.h>
#include <Psapi.h>

// The function prologue for CUserMessageSayText2::MergeFrom
const byte gcSignature[] = { 0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x41,0x56,0x41,0x57,0xB8,0x30,0x18,0x00,0x00 };

int64_t(*gOriginalMergeFrom)(void*, void*, char);
int64_t(*gBypassAddr)(void*, void*);

struct SavedMergeFromParams {
	void* a1;
	void* a2;
	char a3;
};

std::fstream* gLogFile;
HOOK_TRACE_INFO ghHook;
HANDLE ghPipe1, ghPipe2;

void Log(char* msg) {
#ifdef _DEBUG
	*gLogFile << msg << "\r\n";
	gLogFile->flush();
#endif
}

void LogError() {
#ifdef _DEBUG
	char err[50];
	snprintf(err, 50, "Error: %ld", GetLastError());
	Log(err);
#endif
}

void WriteChatMsgToNamedPipe(wchar_t* msg) {
	short len = (short)(wcslen(msg) * sizeof(wchar_t));
#ifdef _DEBUG
	*gLogFile << "Byte length: " << (int)len << "\r\n";
	gLogFile->flush();
#endif
	if (!WriteFile(ghPipe1, &len, 2, NULL, NULL))
		LogError();
	if (!WriteFile(ghPipe1, msg, (int)len, NULL, NULL))
		LogError();
}

void WriteMessageParamsToNamedPipe(void *a1, void *a2, char a3) {
	SavedMergeFromParams params;
	short size = sizeof(params);

	params.a1 = a1;
	params.a2 = a2;
	params.a3 = a3;

	if (!WriteFile(ghPipe1, &size, sizeof(size), NULL, NULL))
		LogError();
	if (!WriteFile(ghPipe1, &params, size, NULL, NULL))
		LogError();
}

int64_t __fastcall MergeFromHook(void *a1, void *a2, char a3) {
	//WriteChatMsgToNamedPipe(message);
	//WriteMessageParamsToNamedPipe(a1, a2, a4);
#ifdef _DEBUG
	*gLogFile << "Called!\r\n";
	*gLogFile << "Status at send:\r\n";
	*gLogFile << "a1: " << std::hex << a1;
	*gLogFile << "\r\na2: " << std::hex << a2;
	gLogFile->flush();
#endif
	return gOriginalMergeFrom(a1, a2, a3);
}

DWORD WINAPI ListenPipeAndPrint(LPVOID lpParam) {
	Log("Client pipe listener ready");
	if (!gBypassAddr) {
		Log("Bad hook bypass address");
		return 0;
	}


	while (true) {
		short numBytes;
		if (!ReadFile(ghPipe2, &numBytes, 2, NULL, NULL)) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}
#ifdef _DEBUG
		*gLogFile << "numBytes: " << (int)numBytes << "\r\n";
		gLogFile->flush();
#endif

		byte* buf = new byte[numBytes + 2];
		if (!ReadFile(ghPipe2, buf, numBytes, NULL, NULL)) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}
		buf[numBytes] = 0;
		buf[numBytes + 1] = 0;

		Log("Message read from pipe successfully");
		size_t l = wcslen((wchar_t *)buf);

		// Retrive the message parameters
		SavedMergeFromParams params;
		if (!ReadFile(ghPipe2, &params, sizeof(params), NULL, NULL))
			LogError();

		Log("Message parameters read from pipe successfully");
#ifdef _DEBUG
		*gLogFile << "Status at recv:\r\n";
		*gLogFile << "a1: " << std::hex << params.a1;
		*gLogFile << "\r\na2: " << std::hex << params.a2;
		gLogFile->flush();
#endif

//		gBypassAddr(params.a1, params.a2, (wchar_t *)buf, params.a4);
		delete buf;
		Log("Injected translated message successfully");
	}

	return 0;
}

void *FindPrintChatAddr(MODULEINFO *mi) {
	byte *baseAddr = (byte *)mi->lpBaseOfDll;
	DWORD dllSize = mi->SizeOfImage;

	if (!baseAddr || !dllSize) {
		Log("Invalid MODULEINFO");
		return 0;
	}

	for (DWORD i = 0; i < dllSize; i++) {
		for (int j = 0; j < sizeof(gcSignature); j++) {
			if (*(baseAddr + j) != gcSignature[j])
				break;

			if (j == sizeof(gcSignature) - 1)
				return (void *)baseAddr;
		}
		baseAddr++;
	}

	Log("Could not find PrintChat!");
	return 0;
}

extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo) {
#ifdef _DEBUG
	gLogFile = new std::fstream("dt.log", std::ios::out);
#endif

	ghPipe1 = CreateFile(L"\\\\.\\Pipe\\DotATranslator1", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	ghPipe2 = CreateFile(L"\\\\.\\Pipe\\DotATranslator2", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (ghPipe1 == INVALID_HANDLE_VALUE || ghPipe2 == INVALID_HANDLE_VALUE) {
		Log("Can't connect to the pipes");
		LogError();
	}
	else
		Log("Connected to named pipes");

	HMODULE hClientDll = GetModuleHandle(L"client.dll");
	HANDLE hProcess = GetCurrentProcess();
	MODULEINFO mi;
	if (!GetModuleInformation(hProcess, hClientDll, &mi, sizeof(mi))) {
		Log("Could not get module information");
		return;
	}

	gOriginalMergeFrom = (int64_t(*) (void*, void*, char))FindPrintChatAddr(&mi);
	if (!gOriginalMergeFrom)
		return;

	ghHook = { NULL };

#ifdef _DEBUG
	*gLogFile << "Hooking printChat at: 0x" << std::hex << gOriginalMergeFrom << "\r\n";
#endif

	NTSTATUS didHook = LhInstallHook(gOriginalMergeFrom, MergeFromHook, NULL, &ghHook);
	if (FAILED(didHook))
		Log("Failed to hook printChat :(");
	else
		Log("Hooked printChat successfully");

	ULONG ACLEntries[1] = { 0 };
	LhSetExclusiveACL(ACLEntries, 1, &ghHook);
	LhGetHookBypassAddress(&ghHook, (void ***)&gBypassAddr);
	//CreateThread(0, 0, ListenPipeAndPrint, 0, 0, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}