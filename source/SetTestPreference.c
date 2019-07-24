#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <visa.h>
#include "TempChamberDriver.h"
#include "UARTTransceiver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "resource.h"
#include "UARTProcess.h"
#include "AddTestStep.h"
#include "DeleteTestStep.h"
#include "EditTestStep.h"
#include "TestExecute.h"
#include "SetTestPreference.h"

char *szLoadUnit[] = { /*"mW",*/ "mA", "C-Rate" };

BOOL WINAPI SetTestPreference(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static TEST_PREFERENCE *pTestPreference;
	static HWND hCurrentUnit, hCapacity, hDisplayCounter;
	char szTempString[MAX_STRING_LENGTH] = { 0 };

	switch (uMsg)
	{
	case WM_INITDIALOG:
		pTestPreference = (TEST_PREFERENCE *)lParam;
		hCurrentUnit = GetDlgItem(hWnd, IDC_COMBO_CURRENT_UNIT);
		//ComboBox_InsertString(hCurrentUnit, -1, szLoadUnit[mW]);
		ComboBox_InsertString(hCurrentUnit, -1, szLoadUnit[mA]);
		ComboBox_InsertString(hCurrentUnit, -1, szLoadUnit[C_Rate]);
		ComboBox_SetCurSel(hCurrentUnit, pTestPreference->lUnit);

		hCapacity = GetDlgItem(hWnd, IDC_EDIT_BATTERY_CAPICTY);
		sprintf(szTempString, "%d", pTestPreference->uBatteryCapaicty);
		SetWindowText(hCapacity, szTempString);

		hDisplayCounter = GetDlgItem(hWnd, IDC_CHECK_DISPLAY_COUNTER);

		break;
	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
			GetWindowText(hCapacity, szTempString, MAX_STRING_LENGTH);

			if (!IsNumerical(szTempString))
			{
				MessageBox(NULL, "Incorrect Battery Capacity", NULL, MB_OK);
				break;
			}
			
			sscanf(szTempString, "%d", &(pTestPreference->uBatteryCapaicty));

			pTestPreference->lUnit = ComboBox_GetCurSel(hCurrentUnit);
			
			EndDialog(hWnd, ERROR_SUCCESS);
			break;
		case IDCANCEL:
			EndDialog(hWnd, ERROR_FAIL);
			break;
		default:
			break;
		}
	default:
		break;
	}

	return FALSE;
}

LPTSTR GetLoadUnit(TEST_PREFERENCE *pTestPreference)
{
	LPTSTR pUnitStringPointer;

	pUnitStringPointer = szLoadUnit[pTestPreference->lUnit];

	return pUnitStringPointer;
}