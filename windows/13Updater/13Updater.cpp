#include "stdafx.h"

char* WinHttpErrorAsStr(DWORD error)
{
	char *msgBuf = NULL;
	char *copy;
	DWORD err = GetLastError();
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_IGNORE_INSERTS,
		GetModuleHandleA("winhttp.dll"), error, 0,
		(LPSTR) &msgBuf, 0, NULL);
	if (!msgBuf) {
		err = GetLastError();
		// 317 - can't find message
		return NULL;
	}
	copy = _strdup(msgBuf);
	LocalFree(msgBuf);
	return copy;
}

char* LastErrorAsStr(DWORD err)
{
	char *msgBuf = NULL;
	char *copy;
	if (-1 == err)
		err = GetLastError();
	FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, 0,
		(LPSTR) &msgBuf, 0, NULL);
	if (!msgBuf) {
		return WinHttpErrorAsStr(err);
	}
	copy = _strdup(msgBuf);
	LocalFree(msgBuf);
	return copy;
}

int WINAPI _tWinMain(HINSTANCE /*hInstance*/, HINSTANCE /*hPrevInstance*/, LPTSTR /*cmdLine*/, int /* nCmdShow */)
{

    return 0;
}
