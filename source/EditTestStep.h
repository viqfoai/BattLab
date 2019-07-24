#pragma once

BOOL WINAPI EditTestStep(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL ReCreateTestStep(TEST_STEP **ppDestTestStep, TEST_STEP *pSourceStep);
BOOL ReCreateCondition(TERM_COND **ppDestTermCond, TERM_COND *pSourcTermCond);
void RecursiveReCreateCondition(TERM_COND **ppDestTermCond, TERM_COND *pSourcTermCond);
void InitEditStepDialog(HWND hWnd, CONTROL_HANDLE_SET *pHandleSet, TEST_SCHEDULE_PTR pTestSchedule);
BOOL LitterTestStep(TEST_STEP **pStep);
void SubstituteTestStepInScheduleTree(TEST_SCHEDULE_PTR pTestSchedule, TEST_STEP *pTestStep);
BOOL ReCreateTestStepWithStepNumber(TEST_STEP **ppDestTestStep, TEST_SCHEDULE_PTR pSourceSchedule, UINT uiStepNumber);
void RefreshEditStepDialog(HWND hWnd, CONTROL_HANDLE_SET *pHandleSet, TEST_STEP *pTestStep);