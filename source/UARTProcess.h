#pragma once

#define MAX_TEMP_STRING_LENGTH 32

typedef struct 
{
	HANDLE *phTempMutex;
	HANDLE *phEventTemp;
	HANDLE *phCom;
	char szTempString[MAX_TEMP_STRING_LENGTH];
	BOOL bCloseCOMHandle;
}TAB;

DWORD WINAPI TempReadProcess(LPARAM lParam);
