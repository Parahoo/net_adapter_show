#ifndef _NEW_INTERFACE_
#define _NEW_INTERFACE_

// Õº–Œµƒ Ù–‘
typedef struct _PictureINFO
{
  int   XDpi;
  int   YDpi;

  int   Width;
  int   Height;

  int   Colors;
} PictureINFO , *pPictureINFO;

typedef int(_stdcall  *CheckSystemExistFUNC)();
typedef int(_stdcall  *CheckSystemRunningFUNC)();

typedef int(_stdcall  *SystemInitPrinterFUNC)(PictureINFO *pPicInfo);
typedef int(_stdcall  *SystemPrintAColorLineFUNC)(unsigned char *pALineData, int DataSize, int iColor);
typedef int(_stdcall  *SystemPrintBlankLinesFUNC)(int BlankLines);
typedef int(_stdcall  *SystemPrintEndFUNC)();
typedef int(_stdcall  *SystemCancelFUNC)();

enum ResultDataTYPE
{
  RDT_STRUCT            = 0,   // sturct must has mark of version, and must check struct mark;
  RDT_INTDWORD          = 1,   // direct output 
  RDT_DOUBLE            = 2,   // direct output
  RDT_CHARSTRING        = 3,   // must check bufsize
};
typedef int(_stdcall  *SystemGetRuningStateFUNC)(int  StateItemId, int ResultDataType, void *pOutBuf, int OutBufSize);
typedef int(_stdcall  *SystemGetErrorCodeFUNC)();

typedef int(_stdcall  *SystemGetSystemConfigFUNC)(void *pConfig, int DataSize);
typedef int(_stdcall  *SystemSetSystemConfigFUNC)(void *pConfig, int DataSize);
typedef int(_stdcall  *SystemSaveSystemConfigFUNC)();

struct  PrintMETHORD
{
  CheckSystemExistFUNC          CheckSystemExist;
  CheckSystemRunningFUNC        CheckSystemRunning;

  SystemInitPrinterFUNC         SystemInitPrinter;
  SystemPrintAColorLineFUNC     SystemPrintAColorLine;
  SystemPrintBlankLinesFUNC     SystemPrintBlankLines;
  SystemPrintEndFUNC            SystemPrintEnd;
  SystemCancelFUNC              SystemCancel;

  SystemGetRuningStateFUNC      SystemGetRuningState;
  SystemGetErrorCodeFUNC        SystemGetErrorCode;    //if Print return error, use it

  SystemGetSystemConfigFUNC     SystemGetSystemConfig;
  SystemSetSystemConfigFUNC     SystemSetSystemConfig;
  SystemSaveSystemConfigFUNC    SystemSaveSystemConfig;
};

struct  CADPrinterINTERFACE
{
  unsigned int  VerId;
  PrintMETHORD  PrintMethord;
};

_stdcall int InitCADPrinterInterface(void *pInterface, int DataSize);

#endif