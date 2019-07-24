#include <Windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <visa.h>
#include "resource.h"
#include "TempChamberDriver.h"
#include "BatCycleController.h"
#include "BasicFunction.h"
#include "AddTestStep.h"
#include "TestExecute.h"
#include <DebugPrint.h>
#include <InstrDrv.h>

//#pragma comment(lib,"InstrDrv.lib")

BOOL bTestThreadMsgQueueCreated = FALSE;

extern char szPowerIDString[];
extern char szLoadIDString[];
extern char szVMeterIDString[];
extern char szIMeterIDString[];

DWORD WINAPI TestExecuteThread(LPARAM lParam)
{
	MSG Msg;
	INST_HANDLE_SET *pInstHandleSet;
	TEST_STEP *pTestStep;
	REC_ITEM *pRecordItem;
	BOOL bResult = FALSE;
	static REL_STATUS ChgRelStatus = OFF, DsgRelStatus = OFF;

	pInstHandleSet = (INST_HANDLE_SET *)lParam;

	PeekMessage(&Msg, (HWND)-1, WM_USER, WM_USER, PM_NOREMOVE);		//enforce system to create a message queue for this thread
	bTestThreadMsgQueueCreated = TRUE;

	while (GetMessage(&Msg, (HWND)-1, 0, 0))
	{

		GetRelayStatus(pInstHandleSet->hCOMCtrl, &ChgRelStatus, &DsgRelStatus);
		switch (Msg.message)
		{
		case START_TEST_STEP:

			pTestStep = *(TEST_STEP **)(Msg.lParam);
			if (NULL == pTestStep)
				return ERROR_INVALID_PARAMETER;

			pRecordItem = (REC_ITEM *)(Msg.wParam);
			if (NULL == pRecordItem)
				return ERROR_INVALID_PARAMETER;

			pRecordItem->uiStepTimerStart = GetTickCount() / 1000;
			pRecordItem->uiStepTimer = 0;

			pRecordItem->uiConditionTimerStart = 0;		//initialize condition time at the beginning of the test step. modified on 18-5-28
			switch (pTestStep->StepOperation)
			{
			case CHARGE:
				if (!ChgRelStatus)
					DisconnectLoad(pInstHandleSet->hCOMCtrl);
				DllSetCharger(pInstHandleSet->viVoltSrc, pTestStep, szPowerIDString);
				TurnOnChargePath(pInstHandleSet->hCOMCtrl);
				pInstHandleSet->pRecordItem->bChargerConnected = TRUE;
				pInstHandleSet->pRecordItem->bLoadConnected = FALSE;
				break;
			case DISCHARGE:
				if (!DsgRelStatus)
					DisconnectCharge(pInstHandleSet->hCOMCtrl);

				DllSetLoad(pInstHandleSet->viLoad, pTestStep, szLoadIDString);
				TurnOnDischargePath(pInstHandleSet->hCOMCtrl);
				pInstHandleSet->pRecordItem->bChargerConnected = FALSE;
				pInstHandleSet->pRecordItem->bLoadConnected = TRUE;
				break;
			case SET_TEMP:
				if (pTestStep->iMinTempTime > MAX_TIME || pTestStep->iMinTempTime < 0 || pTestStep->iTemperature > MAX_TEMP || pTestStep->iTemperature < MIN_TEMP)
					break;

				DllSetTemperature(pInstHandleSet->hTempChamber, pTestStep->iTemperature, pTestStep->iMinTempTime);
				break;
			case SHTDN_TCHMBR:
				DllShutdownTempChmaber(pInstHandleSet->hTempChamber);
				break;
			case GOTO:
				bResult = CheckConditionTrueOrFalse(pTestStep->pTerminateCondition, pRecordItem);
				if (bResult)
					PostMessage(pInstHandleSet->hWnd, WM_TEST_STEP_FINISHED, pTestStep->iJumpStepNumber, 0);
				else
					PostMessage(pInstHandleSet->hWnd, WM_TEST_STEP_FINISHED, pTestStep->uiStepNumber + 1, 0);
				break;
			case REST:
				if (pRecordItem->uiHighFrequenceyReadCounter)
					Sleep(200);
				DllGetTimer(pRecordItem->bLogStarted, &(pRecordItem->uiBatDisconnectTimer));
				DisconnectCharge(pInstHandleSet->hCOMCtrl);
				DisconnectLoad(pInstHandleSet->hCOMCtrl);
				pInstHandleSet->pRecordItem->bChargerConnected = FALSE;
				pInstHandleSet->pRecordItem->bLoadConnected = FALSE;
				ShutdownLoad(pInstHandleSet->viLoad);
				ShutdownCharger(pInstHandleSet->viVoltSrc);
				break;
			case NOP:
				DisconnectCharge(pInstHandleSet->hCOMCtrl);
				DisconnectLoad(pInstHandleSet->hCOMCtrl);
				pInstHandleSet->pRecordItem->bChargerConnected = FALSE;
				pInstHandleSet->pRecordItem->bLoadConnected = FALSE;
				ShutdownLoad(pInstHandleSet->viLoad);
				ShutdownCharger(pInstHandleSet->viVoltSrc);
				break;
			case PASSEDCHARGE_ON:
				pRecordItem->bPassedChargeOn = TRUE;
				break;
			case PASSEDCHARGE_OFF:
				pRecordItem->bPassedChargeOn = FALSE;
				break;
			case PASSEDCHARGE_CLEAR:
				pRecordItem->uiPassedChargeCounter = 0;
				pRecordItem->dblPassedCharge = 0;
				pRecordItem->dblPresentPassedCharge = 0;
				pRecordItem->llPassedCharge = 0;
				pRecordItem->llPresentPassedCharge = 0;
				break;
			case START_LOG:
				if (!pRecordItem->bLogStarted)
				{
					pRecordItem->uiLogTimer = 0;
					pRecordItem->bLogStarted = TRUE;
				}
				break;
			case STOP_LOG:
				if (NULL != pRecordItem->hLogFile)					//if log has started, do nothing
				{
					CloseHandle(pRecordItem->hLogFile);
					pRecordItem->hLogFile = NULL;
				}
				pRecordItem->bLogStarted = FALSE;
				break;
			case CNTR_INC:
				pRecordItem->iCounter++;
				break;
			case CNTR_DEC:
				pRecordItem->iCounter--;
				break;
			case CNTR_CLR:
				pRecordItem->iCounter = 0;
				break;
			}
			break;
		case CHECK_TEST_STEP_1S:

			pTestStep = *(TEST_STEP **)(Msg.lParam);
			if (NULL == pTestStep)
				return ERROR_INVALID_PARAMETER;

			pRecordItem = (REC_ITEM *)(Msg.wParam);
			if (NULL == pRecordItem)
				return ERROR_INVALID_PARAMETER;

			if (GOTO != pTestStep->StepOperation)
				pRecordItem->bTimerCountOnConditionCheck = TRUE;
			else
				pRecordItem->bTimerCountOnConditionCheck = FALSE;

			bResult = CheckConditionTrueOrFalse(pTestStep->pTerminateCondition, pRecordItem);

			if (bResult && NULL != pTestStep->pNextStep)
			{
				PostMessage(pInstHandleSet->hWnd, WM_TEST_STEP_FINISHED, pTestStep->pNextStep->uiStepNumber, 0);
				pRecordItem->bTimerCountOnConditionCheck = FALSE;
				pRecordItem->uiStepTimer = pRecordItem->uiStepTimerStart = 0;
				break;
			}
			
			if (bResult && NULL == pTestStep->pNextStep)
			{
				
				pRecordItem->bTimerCountOnConditionCheck = FALSE;
				pRecordItem->uiStepTimer = pRecordItem->uiStepTimerStart = 0;
				DisconnectLoad(pInstHandleSet->hCOMCtrl);
				DisconnectCharge(pInstHandleSet->hCOMCtrl);
				ShutdownLoad(pInstHandleSet->viLoad);
				ShutdownCharger(pInstHandleSet->viVoltSrc);
				PostMessage(pInstHandleSet->hWnd, WM_TEST_SCHEDULE_DONE, 0, 0);
				break;
			}

			break;
		case STOP_TEST_STEP:
			DisconnectLoad(pInstHandleSet->hCOMCtrl);
			DisconnectCharge(pInstHandleSet->hCOMCtrl);
			pInstHandleSet->pRecordItem->bChargerConnected = FALSE;
			pInstHandleSet->pRecordItem->bLoadConnected = FALSE;
			ShutdownLoad(pInstHandleSet->viLoad);
			ShutdownCharger(pInstHandleSet->viVoltSrc);
			PostMessage(pInstHandleSet->hWnd, WM_TEST_SCHEDULE_DONE, 0, 0);
			break;
		default:
			break;
		}
	}

	return ERROR_SUCCESS;
}

BOOL TurnOnChargePath(HANDLE hComCtrl)
{
	DCB CommDCB;
	BOOL bResult = TRUE;
	
	bResult = GetCommState(hComCtrl, &CommDCB);

	CommDCB.fDtrControl = 1;

	SetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	return SetCommBreak(hComCtrl);
}

BOOL TurnOnDischargePath(HANDLE hComCtrl)
{
	DCB CommDCB;
	BOOL bResult = TRUE;

	bResult = GetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	CommDCB.fRtsControl = 0;

	SetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	return bResult;
}

BOOL DisconnectCharge(HANDLE hComCtrl)
{
	DCB CommDCB;
	BOOL bResult = TRUE;

	bResult = GetCommState(hComCtrl, &CommDCB);

	CommDCB.fDtrControl = 0;

	SetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	return ClearCommBreak(hComCtrl);
}

BOOL GetRelayStatus(HANDLE hComCtrl, REL_STATUS *Chg_Rel_Status, REL_STATUS *Dsg_Rel_Status)
{
	DCB CommDCB;
	BOOL bResult;

	bResult = GetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	*Chg_Rel_Status = (REL_STATUS)CommDCB.fRtsControl;
	*Dsg_Rel_Status = (REL_STATUS)CommDCB.fDtrControl;

	return TRUE;
}

BOOL DisconnectLoad(HANDLE hComCtrl)
{
	DCB CommDCB;
	BOOL bResult = TRUE;

	bResult = GetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	CommDCB.fRtsControl = 1;

	SetCommState(hComCtrl, &CommDCB);
	if (!bResult)
		return bResult;

	return TRUE;
}

BOOL SetCharger(ViSession viCharger, TEST_STEP *pTestStep)
{
	ViStatus viStatus = VI_SUCCESS;

	//viStatus = viPrintf(viCharger, "OUTP OFF\n");
	viStatus = viPrintf(viCharger, "VOLT:PROT:CLE\n");
	viStatus = viPrintf(viCharger, "CURR:PROT:CLE\n");
	viStatus = viPrintf(viCharger, "VOLT %f\n", pTestStep->iVoltage/1000.0);
	viStatus = viPrintf(viCharger, "CURR %lf\n", pTestStep->dblCurrent/1000.0);
	viStatus = viPrintf(viCharger, "OUTP ON\n");

	return TRUE;
}

BOOL ShutdownCharger(ViSession viCharger)
{
	ViStatus viStatus = VI_SUCCESS;
	UINT uiCount;
	viStatus = viWrite(viCharger, "*RST\n", strlen("*RST\n"), &uiCount);

	/*
	viStatus = viPrintf(viCharger, "OUTP OFF\n");
	viStatus = viPrintf(viCharger, "VOLT %f\n", 0);
	viStatus = viPrintf(viCharger, "CURR %f\n", 0);
	*/
	return TRUE;
}

BOOL ShutdownLoad(ViSession viLoad)
{
	ViStatus viStatus = VI_SUCCESS;
	UINT uiCount;

	viStatus = viWrite(viLoad, "*RST\n", strlen("*RST\n"), &uiCount);
//	viStatus = viPrintf(viLoad, "*RST\n");
	/*
	viStatus = viPrintf(viLoad, "OUTP OFF\n");
	viStatus = viPrintf(viLoad, "VOLT %f\n", 6);
	viStatus = viPrintf(viLoad, "CURR %f\n", 0);
	viStatus = viPrintf(viLoad, "RES %f\n", 1000);
	*/
	return TRUE;
}

BOOL SetLoad(ViSession viLoad, TEST_STEP *pTestStep)
{
	ViStatus viStatus = VI_SUCCESS;

	//viStatus = viPrintf(viLoad, "INPUT OFF\n");
	viStatus = viPrintf(viLoad, "CURR %lf\n", pTestStep->dblCurrent / 1000.0);
	viStatus = viPrintf(viLoad, "INPUT ON\n");

	return TRUE;
}

BOOL CheckConditionTrueOrFalse(TERM_COND *pCondition, REC_ITEM *pRecordItem)
{
	BOOL bResult = FALSE;

	bResult = RecursiveCheckConditionTrueOrFalse(pCondition, pRecordItem, FALSE);

	return bResult;
}

BOOL RecursiveCheckConditionTrueOrFalse(TERM_COND *pCondition, REC_ITEM *pRecordItem, BOOL bAdjacentLeftResult)
{
	BOOL bResultLeft = TRUE, bResultRight = TRUE, bResultNode = TRUE, bResultBifurcate = TRUE;

	if (NULL != pCondition->pLeftCondition)
		bResultLeft = RecursiveCheckConditionTrueOrFalse(pCondition->pLeftCondition, pRecordItem, FALSE);

	if (NULL != pCondition->pRightCondition)
		bResultRight = RecursiveCheckConditionTrueOrFalse(pCondition->pRightCondition, pRecordItem, bResultLeft);
	
	if (NULL == pCondition->pLeftCondition && NULL == pCondition->pRightCondition)
	{
		bResultNode = CheckNodeCondition(pCondition, pRecordItem, bAdjacentLeftResult);
		return bResultNode;
	}
	else
	{
		switch (pCondition->RelationShip)
		{
		case AND:
			bResultBifurcate = bResultLeft && bResultRight;
			break;
		case OR:
			bResultBifurcate = bResultLeft || bResultRight;
			break;
		default:
			break;
		}
		return bResultBifurcate;
	}

}

BOOL CheckNodeCondition(TERM_COND *pCondition, REC_ITEM *pRecordItem, BOOL bAdjacentLeftResult)
{
	LONGLONG llCompValue;
	BOOL bResult = TRUE;

	switch (pCondition->ConditionType)
	{
	case Voltage:
		llCompValue = pRecordItem->iVoltage;
		break;
	case Current:
		llCompValue = (INT)(pRecordItem->dblCurrent);
		break;
	case Temperature:
		llCompValue = pRecordItem->iTemperature / 10;
		break;
	case AvgVoltage:
		llCompValue = pRecordItem->iAvgVoltage;
		break;
	case AvgCurrent:
		llCompValue = pRecordItem->iAvgCurrent;
		break;
	case VoltageRate:
		llCompValue = pRecordItem->iVoltRate;
		break;
	case CurrentRate:
		llCompValue = pRecordItem->iCurrRate;
		break;
	case AllTime:
		if (pRecordItem->bTimerCountOnConditionCheck)
			pRecordItem->uiStepTimer = GetTickCount() / 1000 - pRecordItem->uiStepTimerStart;

		llCompValue = pRecordItem->uiStepTimer;
		break;
	case ConditionTime:
		if (0 == pRecordItem->uiConditionTimerStart)
			pRecordItem->uiConditionTimerStart = GetTickCount() / 1000;
		if (bAdjacentLeftResult)
		{
			if (pRecordItem->bTimerCountOnConditionCheck)
				pCondition->uiConditionTimer = GetTickCount() / 1000 - pRecordItem->uiConditionTimerStart;
		}
		else
		{
			pRecordItem->uiConditionTimerStart = GetTickCount()/1000;
			pCondition->uiConditionTimer = 0;
		}

		llCompValue = pCondition->uiConditionTimer;
		break;
	case Bifurcation:
		break;
	case PassedCharge:
		llCompValue = pRecordItem->llPresentPassedCharge;
		break;
	case Counter:
		llCompValue = pRecordItem->iCounter;
		break;
	default:
		break;
	}

	switch (pCondition->ComparisonType)
	{
	case GreaterThan:
		if (llCompValue > pCondition->iThreshold)
		{
			bResult = TRUE;
		}
		else
			bResult = FALSE;
		break;
	case EqualTo:
		if (llCompValue = pCondition->iThreshold)
		{
			bResult = TRUE;
		}
		else
			bResult = FALSE;
		break;
	case LessThan:
		if (llCompValue < pCondition->iThreshold)
		{
			bResult = TRUE;
		}
		else
			bResult = FALSE;
		break;
	case NoGreaterThan:
		if (llCompValue <= pCondition->iThreshold)
		{
			bResult = TRUE;
		}
		else
			bResult = FALSE;
		break;
	case NoLessThan:
		if (llCompValue >= pCondition->iThreshold)
		{
			bResult = TRUE;
		}
		else
			bResult = FALSE;
		break;
	default:
		break;
	}

	if (VoltageRate == pCondition->ConditionType)
	{
		INT iRate = (0 == pRecordItem->iVoltRate) ? 1 : pRecordItem->iVoltRate;
		bResult = bResult && (abs(pRecordItem->iVoltWindow * iRate) >= MIN_TIME_RATE_PRODUCT) && (pRecordItem->iVoltRMSE < TRUST_THRESHOLD);
	}

	if (CurrentRate == pCondition->ConditionType)
	{
		INT iRate = (0 == pRecordItem->iCurrRate) ? 1 : pRecordItem->iCurrRate;
		bResult = bResult && (abs(pRecordItem->iCurrWindow * iRate) >= MIN_TIME_RATE_PRODUCT) && (pRecordItem->iCurrRMSE < TRUST_THRESHOLD);
	}

	if (bResult && ConditionTime == pCondition->ConditionType)
		pCondition->uiConditionTimer = pRecordItem->uiConditionTimerStart = 0;

	return bResult;
}
