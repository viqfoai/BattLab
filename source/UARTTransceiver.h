#ifndef UARTTransceiver_INCLUDED
#define UARTTransceiver_INCLUDED

#define ID_OPEN_COM_BUTTON 101
#define ID_CLOSE_COM_BUTTON 102
#define ID_COMBOBOX 103
#define ID_REFRESH_BUTTON 104
#define ID_SCAN_BUTTON 105
#define ID_EDIT_BOX 106
#define ID_LIST_VIEW 107

#define DBT_DEVNODES_CHANGED 7

#define ADDRESS_OFFSET 0
#define FRAME_LENGTH_OFFSET 1
#define OPERATION_TYPE_OFFSET 2

#define WRITE_OPERATION 2
#define READ_OPERATION 1
#define EXECUTION_OPERATION 3

#define MAX_TEXT_LENGTH	64*1024

typedef enum { CommandIdel, CommandReady, CommandInProcess, ResponseReady, ResponseFail }ComStatus;
typedef enum { Idle, Singular, Repeatative }DataAcqMode;
typedef enum { ON, OFF }ButtonState;

typedef struct
{
	unsigned char SendCommand[40];
	int ExpectedLength;
	unsigned char Response[40];
	HANDLE hOutGoingDataReady;			//用于触发发送中断的标志
	ComStatus CommandStatus;			//其可能值为枚举变量ComStatus中的值
	INT CommandIndex;
	INT TempCommandIndex;
	DataAcqMode DataAcquisitionMode;
}TransBuffer;

typedef struct _Element *PNext;
typedef struct _Element
{
	char *Name;
	PNext Next;
}Element;

typedef Element *ElementString;

void ClearList(Element **StrList);
void AddStringToList(Element **StrList, char *NameString);
void DisposeResponseInformation(TransBuffer);

#endif // !UARTTransceiver_INCLUDED

