#include "stdafx.h"
#pragma hdrstop

#include "mtusb.h"
//#include "fpga_reg.h"
#define _USB_DEBUG   0

UsbMultiThreadPIPE::UsbMultiThreadPIPE()
{
  memset(this, 0, sizeof(UsbMultiThreadPIPE));
}

static HANDLE MyCreateEvent(BOOL bInitialState)
{
  HANDLE hEvent = NULL;
  while(hEvent == NULL)
  {
    hEvent = CreateEvent(NULL, TRUE, bInitialState, NULL);
  }
  return hEvent;
}

static void MyDestroyEvent(HANDLE &hEvent)
{
  if(hEvent)
  {
    CloseHandle(hEvent);
    hEvent = NULL;
  }
}

BOOL UsbMultiThreadPIPE::ExportBufferSize(HANDLE hDevice, int Index)
{
  if(m_MultiThreadBufferSize > 0)
  {
    BOOL Ret = FALSE;
    Ret = CypressUsb_BulkWritePipe(hDevice, m_MultiThreadBuffer[Index], m_MultiThreadBufferSize, Index);
    if(Ret == 0)
      printf("MultiThreadPIPE:BulkWrite[%d]Error!\r\n",Index);
    return Ret;
  }
  return TRUE;
}

DWORD UsbMultiThreadPIPE::SendBulkFunc(int Index)
{
  m_bMultiThreadIsStart[Index] = TRUE;
  HANDLE hMasterEvent = m_hMultiThread_MasterEvent[Index];
  HANDLE hSlaveEvent = m_hMultiThread_SlaveEvent[Index];
  HANDLE hDevice = CypressUsb_OpenDevice(m_Channel_Index);
  if(hDevice)
  {
    while(m_bMultiThreadIsStart[Index])
    {
      if(WaitForSingleObject(hSlaveEvent, INFINITE) == WAIT_OBJECT_0)
      {
        ResetEvent(hSlaveEvent);
        m_bMultiThread_BulkWrite[Index] = ExportBufferSize(hDevice, Index);
        SetEvent(hMasterEvent);
      }
    }
    CypressUsb_CloseDevice(hDevice);
  }
  m_bMultiThreadIsExist[Index] = TRUE;
  return 0;
}

DWORD WINAPI UsbMultiThreadPipe_SendBulkFunc(LPVOID _Param)
{
  DWORD *pParam = (DWORD *)_Param;
  UsbMultiThreadPIPE *pPipe = (UsbMultiThreadPIPE *)pParam[0];
  int Index = pParam[1];
  return pPipe->SendBulkFunc(Index);
}

void UsbMultiThreadPIPE::InitThread(int Index)
{
  m_hMultiThread_MasterEvent[Index]  = MyCreateEvent(FALSE);
  m_hMultiThread_SlaveEvent[Index] = MyCreateEvent(FALSE);
  m_bMultiThreadIsStart[Index] = FALSE;
  m_bMultiThreadIsExist[Index] = FALSE;

  DWORD ThreadId;
  DWORD Param[2] = { (DWORD)this, Index };
  HANDLE hThread = CreateThread(NULL, 0, UsbMultiThreadPipe_SendBulkFunc, Param, 0, &ThreadId);
  if(hThread)
    CloseHandle(hThread);
  while(1)
  {
    if(m_bMultiThreadIsStart[Index])  // keep Param[2] alive for the threadproc
      break;
    else
      Sleep(10);
  }
}

void UsbMultiThreadPIPE::Reset()
{
  m_MultiThreadBufferSize = 0;

  for(int i = 0; i < 2; i++)
  {
    m_bMultiThreadIsStart[i]      = FALSE;
    m_bMultiThreadIsExist[i]      = FALSE;
    m_hMultiThread_MasterEvent[i] = NULL;
    m_hMultiThread_SlaveEvent[i]  = NULL;
    m_bMultiThread_BulkWrite[i]   = FALSE;
  }
}

void UsbMultiThreadPIPE::Init()
{
  if(_USB_DEBUG)
    printf("SepiaPrinter>>  UsbMultiThreadPIPE::Init\r\n");
  if(m_bUsbMultiThreadInit)
    return;
  m_bUsbMultiThreadInit = TRUE;
  
  Reset();
  
  HANDLE hDevice = CypressUsb_OpenDevice(m_Channel_Index);
  if(hDevice)
  {
    printf("MultiThreadPIPE, Open Device[Ez-%d] Ok!\r\n",m_Channel_Index);
    CypressUsb_CloseDevice(hDevice);
  }
  else
  {
    printf("MultiThreadPIPE, Open Device[Ez-%d] Failed!\r\n",m_Channel_Index);
    int ErrorCode = GetLastError();
    printf("GetLastErrorCode:%d\r\n", ErrorCode);
  }

  InitThread(0);
  InitThread(1);
}

void UsbMultiThreadPIPE::DestroyTransmitEvent()
{
  MyDestroyEvent(m_hMultiThread_MasterEvent[0]);
  MyDestroyEvent(m_hMultiThread_MasterEvent[1]);
  MyDestroyEvent(m_hMultiThread_SlaveEvent[0]);
  MyDestroyEvent(m_hMultiThread_SlaveEvent[1]);
}

void UsbMultiThreadPIPE::Uninit()
{
  if(_USB_DEBUG)
    printf("SepiaPrinter>>  UsbMultiThreadPIPE::Uninit\r\n");
  m_bUsbMultiThreadInit = FALSE;
  m_bMultiThreadIsStart[0] = FALSE;
  m_bMultiThreadIsStart[1] = FALSE;

  m_MultiThreadBufferSize = 0;

  if(m_hMultiThread_SlaveEvent[0])
    SetEvent(m_hMultiThread_SlaveEvent[0]);
  if(m_hMultiThread_SlaveEvent[1])
    SetEvent(m_hMultiThread_SlaveEvent[1]);
  while(1)
  {
    if(m_bMultiThreadIsExist[0] && m_bMultiThreadIsExist[1])
      break;
    else
      Sleep(10);
  }
  DestroyTransmitEvent();
}

void UsbMultiThreadPIPE::SplitBulkBuffer(BYTE *pBuffer, DWORD BufferSize)
{
  #define SPLIT_SIZE  512
  int Count = BufferSize/SPLIT_SIZE;
  if(1)
  {
    BYTE *pSplit0 = m_MultiThreadBuffer[0];
    BYTE *pSplit1 = m_MultiThreadBuffer[1];
    for(int iSector = 0; iSector < Count; iSector++)
    {
      if(iSector & 1)
      {
        memcpy(pSplit1, pBuffer, SPLIT_SIZE);
        pSplit1 += SPLIT_SIZE;
        pBuffer += SPLIT_SIZE;
      }
      else
      {
        memcpy(pSplit0, pBuffer, SPLIT_SIZE);
        pSplit0 += SPLIT_SIZE;
        pBuffer += SPLIT_SIZE;
      }
    }
  }
  m_MultiThreadBufferSize = BufferSize/2;
}

BOOL UsbCtrl_BulkWriteOnePipe(int Channel_Index, void *pBuffer, DWORD BufferSize)
{
  HANDLE hDevice = CypressUsb_OpenDevice(Channel_Index);
  if(hDevice)
  {
    BOOL Ret = CypressUsb_BulkWritePipe(hDevice, pBuffer, BufferSize, 0);
    CypressUsb_CloseDevice(hDevice);
    return Ret;
  }
  return FALSE;
}

BOOL UsbMultiThreadPIPE::BulkWritePipe(void *pBuffer, DWORD BufferSize)
{
  m_bMultiThread_BulkWrite[0] = FALSE;
  m_bMultiThread_BulkWrite[1] = FALSE;
  SplitBulkBuffer((BYTE *)pBuffer, BufferSize);

  SetEvent(m_hMultiThread_SlaveEvent[0]);
  SetEvent(m_hMultiThread_SlaveEvent[1]);

  if(WaitForSingleObject(m_hMultiThread_MasterEvent[0], INFINITE) == WAIT_OBJECT_0)
  {
    ResetEvent(m_hMultiThread_MasterEvent[0]);
  }
  if(WaitForSingleObject(m_hMultiThread_MasterEvent[1], INFINITE) == WAIT_OBJECT_0)
  {
    ResetEvent(m_hMultiThread_MasterEvent[1]);
  }
  return m_bMultiThread_BulkWrite[0] && m_bMultiThread_BulkWrite[1];
}
