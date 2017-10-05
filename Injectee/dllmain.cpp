// dllmain.cpp : Defines the entry point for the injectee.
#include "stdafx.h"
#include <easyhook.h>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <comdef.h>

int64_t(*gOriginalPrintChat)(void *, int, WCHAR*, int);

std::fstream* gLogFile;
HOOK_TRACE_INFO ghHook;
HANDLE ghPipe1, ghPipe2;
// Saved args for later printing
void *ga1;
int ga2, ga4;

void Log(char* msg, std::fstream* f = gLogFile) {
#ifdef _DEBUG
	*f << msg << "\r\n";
	f->flush();
#endif
}

void LogError(std::fstream* f = gLogFile) {
#ifdef _DEBUG
	char err[50];
	snprintf(err, 50, "Error: %ld", GetLastError());
	Log(err, f);
#endif
}

void WriteChatMsgToNamedPipe(char* msg) {
	if (!WriteFile(ghPipe1, msg, strlen(msg) + 1, NULL, NULL))
		LogError();
}

int64_t __fastcall PrintChatHook(void *a1, int a2, WCHAR* message, int a4) {
	_bstr_t narrow(message);
	Log(narrow);
	WriteChatMsgToNamedPipe(narrow);
	ga1 = a1;
	ga2 = a2;
	ga4 = a4;
	return gOriginalPrintChat(a1, a2, message, a4);
}

DWORD WINAPI ListenPipeAndPrint(LPVOID lpParam) {
	int64_t(*bypassAddr)(void*, int, WCHAR*, int) = 0;
	LhGetHookBypassAddress(&ghHook, &(PVOID *)bypassAddr);
	Log("Client pipe listener ready");

	while (true) {
		byte numBytes;
		bool didRead = ReadFile(ghPipe2, &numBytes, 1, NULL, NULL);
		if (!didRead) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}
		*gLogFile << "Have "<< (int)numBytes << " bytes in incoming pipe.\r\n";

		byte* buf = new byte[numBytes + 2];
		didRead = ReadFile(ghPipe2, buf, numBytes, NULL, NULL);
		if (!didRead) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}
		buf[numBytes] = 0;
		buf[numBytes + 1] = 0;

		Log("Message read from pipe successfully");
		size_t l = wcslen((wchar_t *)buf);
		
		*gLogFile << "Message length: " << l << "\r\n";
		gLogFile->flush();

		bypassAddr(ga1, ga2, (WCHAR *)buf, ga4);
		delete buf;
		Log("Injected translated message successfully");
	}

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

	HMODULE hClient = GetModuleHandle(L"client.dll");
	FARPROC ba = GetProcAddress(hClient, "BinaryProperties_GetValue");

	gOriginalPrintChat = (int64_t(*) (void *, int, WCHAR*, int))((int64_t)ba + 0x358cc0);
	CreateThread(0, 0, ListenPipeAndPrint, 0, 0, 0);
	ghHook = { NULL };

	Log("Hooking....");

	NTSTATUS didHook = LhInstallHook(gOriginalPrintChat, PrintChatHook, NULL, &ghHook);
	if (FAILED(didHook))
		Log("Failed to hook printChat :(");
	else
		Log("Hooked printChat successfully");

	ULONG ACLEntries[1] = { 0 };
	LhSetExclusiveACL(ACLEntries, 1, &ghHook);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}