#pragma once

typedef enum {/* mW, */mA, C_Rate }LOAD_UNIT;

typedef struct _TEST_PREFERENCE
{
	LOAD_UNIT lUnit;
	UINT uBatteryCapaicty;
	UINT uFormerBatteryCapacity;
}TEST_PREFERENCE;

BOOL WINAPI SetTestPreference(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LPTSTR GetLoadUnit(TEST_PREFERENCE *pTestPreference);