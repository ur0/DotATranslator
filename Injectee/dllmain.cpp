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
HANDLE ghPipe;

void Log(char* msg) {
#ifdef DEBUG
	*gLogFile << msg << "\r\n";
	gLogFile->flush();
#endif
}

void WriteChatMsgToNamedPipe(char* msg) {
	WriteFile(ghPipe, msg, strlen(msg), NULL, NULL);
}

int64_t __fastcall printChatHook(void *a1, int a2, WCHAR* message, int a4) {
	_bstr_t narrow(message);
	WriteChatMsgToNamedPipe(narrow);
	return gOriginalPrintChat(a1, a2, message, a4);
}

extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo) {
#ifdef DEBUG
	gLogFile = new std::fstream("dt.log", std::ios::out);
#endif

	ghPipe = CreateFile(TEXT("\\\\.\\pipe\\DotATranslator"), GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if (ghPipe == INVALID_HANDLE_VALUE)
		Log("Can't connect to the pipe");
	else
		Log("Connected to named pipe");

	HMODULE hClient = GetModuleHandle(TEXT("client.dll"));
	FARPROC ba = GetProcAddress(hClient, "BinaryProperties_GetValue");

	gOriginalPrintChat = (int64_t(*) (void *, int, WCHAR*, int))((int64_t)ba + 0x358cc0);
	ghHook = { NULL };

	Log("Hooking....");

	NTSTATUS didHook = LhInstallHook(gOriginalPrintChat, printChatHook, NULL, &ghHook);
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