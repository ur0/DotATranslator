// dllmain.cpp : Defines the entry point for the injectee.
#include "stdafx.h"
#include <easyhook.h>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <comdef.h>
#include <Psapi.h>

// The function prologue for CDOTAChatChannelBase::ProcessMsg
const byte gcProcessMsgSignature[] = { 0x48,0x89,0x5C,0x24,0x10,0x48,0x89,0x6C,0x24,0x18,0x48,0x89,0x74,0x24,0x20,0x57,0x41,0x56,0x41,0x57,0xB8,0x30,0x18,0x00,0x00 };
// and the the function prologue for CDOTA_SF_Hud_Chat::MessagePrintf
const byte gcPrintChatSignature[] = { 0x48,0x8B,0xC4,0x55,0x56,0x41,0x55,0x41,0x56,0x41,0x57 };

// Unknown areas are represented as byte arrays
struct CMsgDOTAChatMessage {
	void* vtable;
	byte u1[40];
	char* message;
	byte u4[96];
	byte u5[128];
};

void *gDotaChatAreas;
int64_t(*gOriginalProcessMsg)(void*, CMsgDOTAChatMessage*, char);
int64_t(*gProcessMsgBypassAddr)(void*, CMsgDOTAChatMessage*, char);
int64_t(*gOriginalPrintChat)(void*, int, wchar_t *, int);
int64_t(*gPrintChatBypassAddr)(void*, int, wchar_t *, int);


std::fstream* gLogFile;
HOOK_TRACE_INFO ghHook1, ghHook2;
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

void WriteChatMsgToNamedPipe(char* msg) {
	short len = (short)(strlen(msg) * sizeof(char));
#ifdef _DEBUG
	*gLogFile << "Byte length: " << (int)len << "\r\n";
	gLogFile->flush();
#endif
	if (!WriteFile(ghPipe1, &len, 2, NULL, NULL))
		LogError();
	if (!WriteFile(ghPipe1, msg, (int)len, NULL, NULL))
		LogError();
}

int64_t __fastcall ProcessMsgHook(void *a1, CMsgDOTAChatMessage *message, char a3) {
#ifdef _DEBUG
	*gLogFile << "ProcessMsg Called!\r\n";
	*gLogFile << "Message: ";
	*gLogFile << message->message << "\r\n";
	gLogFile->flush();
#endif
	// Don't translate translated messages
	if (strstr(message->message, "(Translated from "))
		return 0;

	WriteChatMsgToNamedPipe(message->message);
	return gOriginalProcessMsg(a1, message, a3);
}

int64_t __fastcall PrintChatHook(void *a1, int a2, wchar_t *a3, int a4) {
	Log("PrintChat called!");
#ifdef _DEBUG
	*gLogFile << "a2: " << a2 << "\r\n";
	*gLogFile << "a4: " << a4 << "\r\n";
	gLogFile->flush();
#endif
	gDotaChatAreas = a1;
	return gPrintChatBypassAddr(a1, a2, a3, a4);
}

DWORD WINAPI ListenPipeAndPrint(LPVOID lpParam) {
	Log("Client pipe listener ready");
	if (!gPrintChatBypassAddr) {
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

		size_t len = (numBytes + 2) / sizeof(wchar_t);
		wchar_t* buf = new wchar_t[len];
		if (!ReadFile(ghPipe2, buf, numBytes, NULL, NULL)) {
			Log("Can't read from pipe");
			LogError();
			return 0;
		}
		buf[len - 1] = (wchar_t)0;
		Log("Message read from pipe successfully");

		if (gDotaChatAreas) {
			gPrintChatBypassAddr(gDotaChatAreas, 0, buf, 0xffffffff);
			Log("Translated message injected successfully.");
		}
		else
			Log("gDotaChatAreas not initialized yet, skipping injection.");

		delete buf;
	}

	return 0;
}

void *FindAddr(MODULEINFO *mi, const byte* sig, size_t len) {
	byte *baseAddr = (byte *)mi->lpBaseOfDll;
	DWORD dllSize = mi->SizeOfImage;

	if (!baseAddr || !dllSize) {
		Log("Invalid MODULEINFO");
		return 0;
	}

	for (DWORD i = 0; i < dllSize; i++) {
		for (int j = 0; j < len; j++) {
			if (*(baseAddr + j) != sig[j])
				break;

			if (j == len - 1)
				return (void *)baseAddr;
		}
		baseAddr++;
	}

	Log("Could not find by signature!");
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

	gOriginalProcessMsg = (int64_t(*) (void*, CMsgDOTAChatMessage*, char))FindAddr(&mi, gcProcessMsgSignature, sizeof(gcProcessMsgSignature));
	gOriginalPrintChat = (int64_t(*) (void*, int, wchar_t*, int))FindAddr(&mi, gcPrintChatSignature, sizeof(gcPrintChatSignature));

	if (!gOriginalProcessMsg || !gOriginalPrintChat)
		return;

	ghHook1 = { NULL };
	ghHook2 = { NULL };

#ifdef _DEBUG
	*gLogFile << "Hooking CDOTAChatChannelBase::ProcessMsg at: 0x" << std::hex << gOriginalProcessMsg << "\r\n";
	*gLogFile << "Hooking CDOTA_SF_Hud_Chat::MessagePrintf at: 0x" << std::hex << gOriginalPrintChat << "\r\n";
#endif

	NTSTATUS didHook = LhInstallHook(gOriginalProcessMsg, ProcessMsgHook, NULL, &ghHook1);
	if (FAILED(didHook))
		Log("Failed to hook ProcessMsg");
	else
		Log("Hooked ProcessMsg successfully");
	didHook = LhInstallHook(gOriginalPrintChat, PrintChatHook, NULL, &ghHook2);
	if (FAILED(didHook))
		Log("Failed to hook PrintChat");
	else
		Log("Hooked PrintChat successfully");

	ULONG ACLEntries[1] = { 0 };
	LhSetExclusiveACL(ACLEntries, 1, &ghHook1);
	LhSetExclusiveACL(ACLEntries, 1, &ghHook2);
	LhGetHookBypassAddress(&ghHook1, (void ***)&gProcessMsgBypassAddr);
	LhGetHookBypassAddress(&ghHook2, (void ***)&gPrintChatBypassAddr);
	CreateThread(0, 0, ListenPipeAndPrint, 0, 0, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule,
	DWORD  ul_reason_for_call,
	LPVOID lpReserved
)
{
	return TRUE;
}