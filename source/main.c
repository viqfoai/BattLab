#include <windows.h>
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
#include "SetTestPreference.h"

/*  Declare Windows procedure  */
char lpszValidMacAddr[13] = { 0 };

HICON hAppIcon = NULL;
const UINT64 ui64MacAddr[] = { 0x8CEC4BF7D6E7, 0x00FFA073DA07, 0xF85971AF69FB, 0x00FFA033F908, 0x6C88148B985D, \
							0xA44CC8AAA124, 0x00FFa0883FA1, 0x00FFA0538208, 0xD89EF375E42A, 0xB46BFCC9AAD2,\
							0x00FF60D7DD74, 0x00FFA0B36008, 0x00FF40C4EB3F, 0x00FF5055F407, 0x00FFB05B1D08, \
							0x00FFD0F18008, 0x6C88148B985C, 0xF01FAF5E0277, 0xF85971AF69FA, 0x00FFA0D3DD07, 0x00FF20747AAE, \
							0xD89EF375E522, 0x00FFB0E31808, 0x00FFB0FBF9B4, 0x00FFB09B8108, 0xD4BED906B9FE, 0xCB5B76BF156A, 
							0x34F39ABC5DDB, 0xD89EF37FA1B4, 0};

int WINAPI WinMain(HINSTANCE hThisInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpszArgument,
	int nFunsterStil)
{
	LRESULT ErrorCode, Result;
	STRING strMAC_ADDR = { 0 };
	int iPCMacIndex = 0, iAuthenticMacIndex = 0;
	BOOL bAuthenticationPassed = FALSE;

	strMAC_ADDR.uiLength = MAC_ADDR_LENGTH;
	strMAC_ADDR.lpszBuffer = (char *)malloc(MAC_ADDR_LENGTH);

	while (GetMacByCmd(&strMAC_ADDR, iPCMacIndex))
	{
		iAuthenticMacIndex = 0;
		while (0 != ui64MacAddr[iAuthenticMacIndex])
		{
			sprintf(lpszValidMacAddr, "%012I64X", ui64MacAddr[iAuthenticMacIndex]);
			if (!strcmp(strMAC_ADDR.lpszBuffer, lpszValidMacAddr))
			{
				bAuthenticationPassed = TRUE;
				break;
			}
			else
				iAuthenticMacIndex++;
		}

		if (!bAuthenticationPassed)
			iPCMacIndex++;
		else
			break;
	}

	if (!bAuthenticationPassed)			//validate if this is a illegal PC
	{
		MessageBox(NULL, "This is not a valid PC to run this program!", NULL, MB_OK);
		return 0;
	}

	InitCommonControls();
	hAppIcon = LoadIcon(hThisInstance, MAKEINTRESOURCE(IDI_ICON1));
//	HINSTANCE hinst = LoadLibrary("InstrDrv.dll");
#ifdef RELEASE_DEBUG
	OutputDebugString("BMW: in WinMain");
#endif
	Result = DialogBox(hThisInstance, MAKEINTRESOURCE(IDD_MAIN), NULL, BatCycleController);
	
	ErrorCode = GetLastError();


	return 0;
}