// dllmain.cpp : Defines the entry point for the injectee.
#include "stdafx.h"
#include <easyhook.h>
#include <iostream>

int64_t(*gOriginalPrintChat)(void *, int, char*, int);

int64_t __fastcall printChatHook(void *a1, int a2, char* message, int a4) {
	std::cout << message;
	return gOriginalPrintChat(a1, a2, message, a4);
}

extern "C" void __declspec(dllexport) __stdcall NativeInjectionEntryPoint(REMOTE_ENTRY_INFO* inRemoteInfo) {
	HMODULE hClient = GetModuleHandle(TEXT("client.dll"));
	gOriginalPrintChat = (int64_t(*) (void *, int, char*, int))(hClient + 0x123);
	HOOK_TRACE_INFO hHook = { NULL };

	std::cout << "Found printChat at: " << std::hex << gOriginalPrintChat << std::endl;
	std::cout << "Hooking...." << std::endl;

	NTSTATUS didHook = LhInstallHook(gOriginalPrintChat, printChatHook, NULL, &hHook);
	if (FAILED(didHook))
		std::cout << "Failed to hook printChat :(" << std::endl;
	else
		std::cout << "Hooked printChat successfully" << std::endl;

	LhSetExclusiveACL({}, 0, &hHook);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}