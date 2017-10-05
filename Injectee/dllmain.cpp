// dllmain.cpp : Defines the entry point for the injectee.
#include "stdafx.h"
#include <easyhook.h>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <comdef.h>

int64_t(*gOriginalPrintChat)(void *, int, wchar_t*, int);

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

int64_t __fastcall PrintChatHook(void *a1, int a2, wchar_t* message, int a4) {
	WriteChatMsgToNamedPipe(message);
	ga1 = a1;
	ga2 = a2;
	ga4 = a4;
	return gOriginalPrintChat(a1, a2, message, a4);
}

DWORD WINAPI ListenPipeAndPrint(LPVOID lpParam) {
	int64_t(*bypassAddr)(void*, int, wchar_t*, int) = 0;
	LhGetHookBypassAddress(&ghHook, (PVOID **)&bypassAddr);
	Log("Client pipe listener ready");

	while (true) {
		short numBytes;
		bool didRead = ReadFile(ghPipe2, &numBytes, 2, NULL, NULL);
#ifdef _DEBUG
		*gLogFile << "numBytes: " << (int)numBytes << "\r\n";
		gLogFile->flush();
#endif
		if (!didRead) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}

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

		bypassAddr(ga1, ga2, (wchar_t *)buf, ga4);
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

	gOriginalPrintChat = (int64_t(*) (void *, int, wchar_t*, int))((int64_t)ba + 0x358cc0);
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