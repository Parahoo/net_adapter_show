#include "stdafx.h"
#pragma hdrstop

#include <commctrl.h>
#include <io.h>
#include <math.h>
#include <time.h>

#include <winioctl.h>

#include "mtusb.h"
#include "ezusbsys.h"
//#include "fpga_reg.h"

class CriticalSECTION1
{
  CRITICAL_SECTION      m_Cs;
public:
  CriticalSECTION1()
  {
    InitializeCriticalSection(&m_Cs);
  }
  ~CriticalSECTION1()
  {
    DeleteCriticalSection(&m_Cs);
  }
  void Enter()
  {
    EnterCriticalSection(&m_Cs);
  }
  void Leave()
  {
    LeaveCriticalSection(&m_Cs);
  }
};

static CriticalSECTION1   g_UsbRequestSystemCommandCritical;

#define FAST_CLOCK_DIVIDE_SETTING  1//12
static int g_UsbHandleCount = 0;

static BOOL CypressUsb_OpenDriver(HANDLE *pDeviceHandle, char *pDevName)
{
  char          CompleteDeviceName[64];
  sprintf(CompleteDeviceName, "\\\\.\\%s", pDevName);
  *pDeviceHandle = CreateFile(CompleteDeviceName,
                              GENERIC_WRITE | GENERIC_READ,
                              FILE_SHARE_WRITE | FILE_SHARE_READ,
                              NULL, OPEN_EXISTING, 0, NULL);

  return (*pDeviceHandle != INVALID_HANDLE_VALUE);
}

HANDLE CypressUsb_OpenDeviceEx(int Channel_Index)
{
  static int LastIndex = -1;
	HANDLE hDevice = NULL;
  if(LastIndex != -1)
  {
    char DriverName[0x100];
		sprintf(DriverName, "Ezusb-%d", LastIndex);
		if(CypressUsb_OpenDriver(&hDevice, DriverName) == TRUE)
    {
      if(hDevice)
        return hDevice;
    }
    else
      hDevice = NULL;
    LastIndex = -1;
  }
	int Index;
	for(Index = 0; Index < 32; Index++)
	{
    char DriverName[0x100];
		sprintf(DriverName, "Ezusb-%d", Index);
		if(CypressUsb_OpenDriver(&hDevice, DriverName) == TRUE)
    {
      LastIndex = Index;
			break;
    }
    else
    {
      hDevice = NULL;
    }
	}
  if(Index == 32)
		return NULL;
  if(!hDevice)
  {
    printf("SepiaPrinter>>  open error!\r\n"); 
//    getchar();
  }
	return hDevice;
}

HANDLE CypressUsb_OpenDevice(int Channel_Index)
{
  return CypressUsb_OpenDeviceEx(Channel_Index);
  HANDLE hDevice = NULL;
  char DriverName[0x100];
  sprintf(DriverName, "Ezusb-%d", Channel_Index);
  if(CypressUsb_OpenDriver(&hDevice, DriverName) == TRUE)
  {
    g_UsbHandleCount++;
    return hDevice;
  }
  return NULL;
}

void CypressUsb_CloseDevice(HANDLE hDevice)
{
  g_UsbHandleCount--;
  CloseHandle(hDevice);
}

BOOL RequestSystemCommandEx(HANDLE hDevice, DWORD Command, DWORD Address, DWORD Value, WORD *pData)
{
  VENDOR_OR_CLASS_REQUEST_CONTROL	Request;
  Request.request     = (UCHAR) Command;
  Request.value       = (USHORT) (Address | (Value << 8));
  Request.index       = (USHORT) 0xBEEF;
//  Request.direction   = FALSE;
  Request.direction   = TRUE;
  Request.requestType = 2; // vendor specific request type (2)
  Request.recepient   = 0; // recepient is device (0)

  BYTE Buffer[0x10];
  memset(Buffer, 0, sizeof(Buffer));
  DWORD BufferSize = 2;
  DWORD ReturnSize = 0;
  
  g_UsbRequestSystemCommandCritical.Enter();
  BOOL bResult = DeviceIoControl(hDevice,
    IOCTL_EZUSB_VENDOR_OR_CLASS_REQUEST,
    &Request,
    sizeof(VENDOR_OR_CLASS_REQUEST_CONTROL),
		Buffer,
		BufferSize,
		&ReturnSize,
    NULL);
  g_UsbRequestSystemCommandCritical.Leave();

  if(bResult)
  {
    *pData = Buffer[0] | (Buffer[1] << 8);
  }
  else
  {
    if(Buffer[0] || Buffer[1])
    {
      *pData = Buffer[0] | (Buffer[1] << 8);
      bResult = TRUE;
    }
    else
      *pData = 0;
  }
  return bResult;
}

int CypressUsb_GetDeviceIndex(HANDLE hDevice)
{
  WORD ReadIndex;
  if(!RequestSystemCommandEx(hDevice, 0xDF, 0, 0, &ReadIndex))
    return 0;
  return ReadIndex;
}

int CypressUsb_GetChannelIndex(int DeviceIndex)
{
  DeviceIndex = DeviceIndex + 1;
  char DriverName[0x100];
  for(int Index = 0; Index < 32; Index++)
  {
    sprintf(DriverName, "Ezusb-%d", Index);
    HANDLE hDevice = NULL;
    if(CypressUsb_OpenDriver(&hDevice, DriverName) == TRUE)
    {
      int CurDeviceIndex = CypressUsb_GetDeviceIndex(hDevice);
      CloseHandle(hDevice);

      if(CurDeviceIndex == DeviceIndex)
        return Index;
    }
  }
  return -1;
}

BOOL CypressUsb_BulkWritePipe(HANDLE hDevice, void *pBuffer, DWORD BufferSize, int PipeNum)
{
  BULK_TRANSFER_CONTROL OutBulkControl;
  OutBulkControl.pipeNum = PipeNum + 1;

  BOOL Result = TRUE;

  DWORD ReturnSize = 0;
  BOOL Ret = DeviceIoControl(hDevice,
                    IOCTL_EZUSB_BULK_WRITE,
                    &OutBulkControl,
                    sizeof(BULK_TRANSFER_CONTROL),
			              pBuffer,
			              BufferSize,
			              &ReturnSize,
			              NULL);
  if(!Ret)
    Result = FALSE;
  if((DWORD)BufferSize != ReturnSize)
    Result = FALSE;
  if(!Result)
  {
  }
  return Result;
}

BOOL CypressUsb_BulkWrite(HANDLE hDevice, const void *pBuffer, int BufferSize)
{
  return CypressUsb_BulkWritePipe(hDevice, (void *)pBuffer, BufferSize, 0);
}

BOOL CypressUsb_Reset(HANDLE hDevice)
{
  while(1)
  {
    WORD Setting;
    if(!RequestSystemCommandEx(hDevice, 0xD7, 0xAB, 0x12, &Setting))
      return FALSE;

    WORD Getting = (0xAB) | (0x12 << 8);
    if(Setting == Getting)
      break;
  }

  return TRUE;
}

BOOL CypressUsb_WriteRegister(HANDLE hDevice, BYTE Address, BYTE Data)
{
  while(1)
  {
    WORD        Setting;
    if(!RequestSystemCommandEx(hDevice, 0xD0, con_reg+Address, Data, &Setting))
      return FALSE;

    WORD Getting = (con_reg | Address) | (Data << 8);
    if(Setting == Getting)
      break;
  }

  while(1)
  {
    WORD        Ret;
    if(!RequestSystemCommandEx(hDevice, 0xD1, 0, 0, &Ret))
      return FALSE;
    else if(Ret == 0x101)
      break;
  }

  return TRUE;
}

BOOL CypressUsb_ReadRegister(HANDLE hDevice, BYTE Address, BYTE *pValue)
{
  while(1)
  {
    WORD        Setting;
    if(!RequestSystemCommandEx(hDevice, 0xD0, Address, Address, &Setting))
      return FALSE;

    WORD Getting = (Address) | (Address << 8);
    if(Setting == Getting)
      break;
  }

  while(1)
  {
    WORD Ret;
    if(!RequestSystemCommandEx(hDevice, 0xD2, 0, 0, &Ret))
      return FALSE;

    if((Ret & 0xFF) == 0x1)
    {
      *pValue   = Ret >> 8;
      break;
    }
  }

  return TRUE;
}

BOOL CypressUsb_GetPagePosition(HANDLE hDevice, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty)
{
  *pbFifoEmpty  = FALSE;
  *pPage        = 0;
  *pPosition    = 0;

  char  pBuffer[0x10];
  DWORD BufferSize = 6;
  BULK_TRANSFER_CONTROL OutBulkControl;
  OutBulkControl.pipeNum = 0;
  
  DWORD ReturnSize = 0;
  BOOL  Ret = DeviceIoControl(hDevice,
                              IOCTL_EZUSB_BULK_READ,
                              &OutBulkControl,
                              sizeof(BULK_TRANSFER_CONTROL),
                              pBuffer,
                              BufferSize,
                              &ReturnSize,
                              NULL);

  if(Ret && ReturnSize == BufferSize)
  {
    DWORD       Position        = 0;
    for(int i = 1; i <= 3; i++)
    {
      int Flag = pBuffer[i] & 0xC0;
      int Data = pBuffer[i] & 0x3F;

      if(Flag == 0)
        Position |= Data;
      else if(Flag == 0x40)
      {
        Position |= Data;
        *pbFifoEmpty = 1;
      }
      else if(Flag == 0x80)
        Position |= (Data << 6);
      else if(Flag == 0xC0)
        Position |= (Data << 12);
    }

    *pPosition  = Position;
    *pPage      = *(WORD*)(pBuffer + 4);
  }
  else
    Ret = FALSE;

  if(0)
    printf("SepiaPrinter>>  page: %d; position: %d; fifoempty: %d", *pPage, *pPosition, *pbFifoEmpty);
  return Ret;
}

BOOL UsbCtrl_GetPagePosition(int Channel_Index, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = CypressUsb_GetPagePosition(hDevice, pPage, pPosition, pbFifoEmpty);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL UsbCtrl_GetPagePosition(int Channel_Index, BOOL *pbPageEncoderReady, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    *pbPageEncoderReady = TRUE;
    bRet = CypressUsb_GetPagePosition(hDevice, pPage, pPosition, pbFifoEmpty);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL CypressUsb_LimitFifoSize(HANDLE hDevice, BOOL bLimit)
{
  while(1)
  {
    WORD        Setting;
    if(!RequestSystemCommandEx(hDevice, 0xD9, bLimit, 0xD9, &Setting))
      return FALSE;

    WORD Getting = WORD((bLimit ? 1 : 0) | (0xD9 << 8));
    if(Setting == Getting)
      break;
  }

  return TRUE;
}

DWORD CypressUsb_GetVersion(HANDLE hDevice)
{
  WORD Version_B0 = 0;
  WORD Version_DA = 0;
  RequestSystemCommandEx(hDevice, 0xB0, 0, 0xB0, &Version_B0);
  RequestSystemCommandEx(hDevice, 0xDA, 0, 0xDA, &Version_DA);
  printf("SepiaPrinter>>  B0: %d.%d\r\n", (DWORD)LOBYTE(Version_B0), (DWORD)HIBYTE(Version_B0));
  printf("SepiaPrinter>>  DA: %d.%d\r\n", (DWORD)LOBYTE(Version_DA), (DWORD)HIBYTE(Version_DA));
  return Version_B0;
}


DWORD CypressUsb_SetOnePipe(HANDLE hDevice, BOOL bFlag)
{
  WORD Version;
  if(!RequestSystemCommandEx(hDevice, 0xDA, bFlag, 0xDA, &Version))
    return 0;
  return Version;
}

static void PrintWriteRegisterLog(int Address, int Data)
{
//  return;
  FILE *pFile = NULL;
  if(0)//Address == 0)
    pFile = fopen("d:\\reg.log", "wb");
  else
    pFile = fopen("d:\\reg_01.log", "ab+");
  if(pFile)
  {
    fseek(pFile, 0, SEEK_END);
//    fprintf(pFile, "reg: %02d; value: 0x%02X\r\n", Address, Data);
    fprintf(pFile, "reg: %d; value: 0x%02X\r\n", Address, Data);
    fclose(pFile);
  }
}

static BOOL _UsbCtrl_WriteRegister(int Channel_Index, int Address, int Data)
{
  if(Address == fpga_reg_clk_sel_set)
  {
    if(Data < FAST_CLOCK_DIVIDE_SETTING)
      Data = FAST_CLOCK_DIVIDE_SETTING;
//    Data = 7;
  }
//  PrintWriteRegisterLog(Address, Data);

  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {      
    bRet = CypressUsb_WriteRegister(hDevice, Address, Data);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL UsbCtrl_WriteRegister(int Channel_Index, int Address, int Data)
{
  return _UsbCtrl_WriteRegister(Channel_Index, Address, Data);
}

BOOL UsbCtrl_ReadRegister(int Channel_Index, BYTE Address, BYTE *pValue)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = CypressUsb_ReadRegister(hDevice, Address, pValue);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL UsbCtrl_Reset(int Channel_Index)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = CypressUsb_Reset(hDevice);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

static BOOL CypressUsb_AbortPageEncoder(HANDLE hDevice)
{
  WORD Value;
  if(!RequestSystemCommandEx(hDevice, 0xDE, 0, 0xDE, &Value))
    return FALSE;
  return TRUE;
}

BOOL UsbCtrl_AbortPageEncoder(int Channel_Index)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = CypressUsb_AbortPageEncoder(hDevice);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL UsbCtrl_LimitFifoSize(int Channel_Index, BOOL bLimit)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = CypressUsb_LimitFifoSize(hDevice, bLimit);
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

DWORD UsbCtrl_GetVersion(int Channel_Index, BOOL bShow)
{
  DWORD Version = 0;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    printf("SepiaPrinter>>  Link Device!\r\n");
    if(bShow)
    {
      BYTE V1 = 0;
      BYTE V2 = 0;
      CypressUsb_ReadRegister(hDevice, 0x10, &V1);
      CypressUsb_ReadRegister(hDevice, 0x11, &V2);

      printf("SepiaPrinter>>  Version: (%d.%d) [%d.%d]\r\n", (int)(LOBYTE(Version)), (int)(HIBYTE(Version)), (int)V1, (int)V2);
    }
    Version = CypressUsb_GetVersion(hDevice);    
    CypressUsb_CloseDevice(hDevice);
  }
  else
  {
    printf("SepiaPrinter>>  Unlink Device!\r\n");
  }
  return Version;
}

BOOL UsbCtrl_SetOnePipe(int Channel_Index, BOOL bFlag)
{
  DWORD Ret = 0;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    Ret = CypressUsb_SetOnePipe(hDevice, bFlag);
    CypressUsb_CloseDevice(hDevice);
  }
  return Ret;
}

DWORD CypressUsb_GetAddr(HANDLE hDevice, int A0, int A1, int A2)
{
  BYTE Addr0 = 0;
  BYTE Addr1 = 0;
  BYTE Addr2 = 0;
  if(!CypressUsb_ReadRegister(hDevice, A0, &Addr0))
    return 0;
  if(!CypressUsb_ReadRegister(hDevice, A1, &Addr1))
    return 0;
  if(!CypressUsb_ReadRegister(hDevice, A2, &Addr2))
    return 0;
  DWORD Addr = Addr0 + (Addr1 << 8) + (Addr2 << 16);
  return Addr;
}

DWORD CypressUsb_GetWriteAddr(HANDLE hDevice)
{
  return CypressUsb_GetAddr(hDevice, 1, 2, 3);
}

DWORD CypressUsb_GetReadAddr(HANDLE hDevice)
{
  return CypressUsb_GetAddr(hDevice, 4, 5, 6);
}

DWORD UsbCtrl_GetWriteAddr(int Channel_Index)
{
  DWORD Addr = 0;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    Addr = CypressUsb_GetWriteAddr(hDevice);
    CypressUsb_CloseDevice(hDevice);
  }
  return Addr;
}

DWORD UsbCtrl_GetReadAddr(int Channel_Index)
{
  DWORD Addr = 0;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    Addr = CypressUsb_GetReadAddr(hDevice);
    CypressUsb_CloseDevice(hDevice);
  }
  return Addr;
}

//Serial  Begin
DWORD CypressUsb_RequestCypressCpu(HANDLE hDevice, DWORD Command, WORD Value)
{
  WORD Setting;
  if(!RequestSystemCommandEx(hDevice, Command, LOBYTE(Value), HIBYTE(Value), &Setting))
    return 0;
  return Setting;
}

static int GetUsbSerialCommandOffset(int Port)
{
  return Port ? 0x10 : 0;
}

DWORD CypressUsb_SerialWrite(HANDLE hDevice, BYTE Data, int Port)
{
  return CypressUsb_RequestCypressCpu(hDevice, 0xBC + GetUsbSerialCommandOffset(Port), Data) == 0xBCBC;
}

DWORD CypressUsb_SerialCount(HANDLE hDevice, int Port)
{
  return CypressUsb_RequestCypressCpu(hDevice, 0xBD + GetUsbSerialCommandOffset(Port), 0);
}

DWORD CypressUsb_SerialClear(HANDLE hDevice, int Port)
{
  return CypressUsb_RequestCypressCpu(hDevice, 0xBF + GetUsbSerialCommandOffset(Port), 0) == 0xBFBF;
}

BOOL UsbCtrl_WriteSerialPort(int Channel_Index, const char *pText, int Port)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    bRet = TRUE;
    int TextLen = strlen(pText);
    for(int iText = 0; iText < TextLen; iText++)
    {
      if(!CypressUsb_SerialWrite(hDevice, pText[iText], Port))
      {
        bRet = FALSE;
        break;
      }
    }
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

BOOL UsbCtrl_ReadSerialPort(int Channel_Index, char *pData, DWORD DataSize, int Port)
{
  BOOL bRet = FALSE;
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    DWORD Count = 0;
    while(1)
    {
      Sleep(20);
      DWORD A = CypressUsb_SerialCount(hDevice, Port);
      if(Count == A)
      {
        break;
      }
      Count = A;
    }
//    printf("SepiaPrinter>>  Count:%d\r\n", Count);
    if(Count > DataSize)
      Count = DataSize;
    bRet = TRUE;
    if(Count > 0)
    {
      for(DWORD iRead = 0; iRead < Count; iRead++)
      {
        int Try = 0;
        while(1)
        {
          WORD A = (WORD)CypressUsb_RequestCypressCpu(hDevice, 0xBE + GetUsbSerialCommandOffset(Port), (WORD)iRead);
          if(HIBYTE(A) == 0xBE)
          {
            pData[iRead] = LOBYTE(A);
            printf("SepiaPrinter>>  %c", LOBYTE(A));
            break;
          }
          else //if(!A)// || LOBYTE(A) != HIBYTE(A))
          {
            bRet = FALSE;
            pData[iRead] = '*';
            printf("SepiaPrinter>>  [%d_%d]", iRead, Try++);
            Sleep(10);
            continue;
          }
        }
      }
      if(!CypressUsb_SerialClear(hDevice, Port))
        bRet = FALSE;
    }
    CypressUsb_CloseDevice(hDevice);
  }
  return bRet;
}

//Serial End
