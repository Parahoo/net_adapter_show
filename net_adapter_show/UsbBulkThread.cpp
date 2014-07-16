#include "stdafx.h"

#include "mtusb.h"
#include "feeder_control.h"
#include "UsbBulkThread.h"

struct UsbCtrlWriteRegisterCOMMAND
{
  int           m_Count;
  BYTE          m_Command[0x100][2];

  UsbCtrlWriteRegisterCOMMAND();
  void          WriteRegister(int Command, int Value);
  void          DebugOutPrint();
  int           RunCommand(int Channel_Index);
};
// UsbCtrlWriteRegisterCOMMAND Begin
UsbCtrlWriteRegisterCOMMAND::UsbCtrlWriteRegisterCOMMAND()
{
  memset(this, 0, sizeof(UsbCtrlWriteRegisterCOMMAND));
}

void UsbCtrlWriteRegisterCOMMAND::WriteRegister(int Command, int Value)
{
  m_Command[m_Count][0] = Command;
  m_Command[m_Count][1] = Value;
  m_Count++;
}

void UsbCtrlWriteRegisterCOMMAND::DebugOutPrint()
{
  for(int iCmd = 0; iCmd < m_Count; iCmd++)
  {
    int Command = m_Command[iCmd][0];
    int Value = m_Command[iCmd][1];

    FILE *pFile = fopen("d:\\regx.log", "ab+");
    if(pFile)
    {
      fseek(pFile, 0, SEEK_END);
      fprintf(pFile, "command_reg: %02d; value: 0x%02X\r\n", Command, Value);
      fclose(pFile);
    }
  }
}

int UsbCtrlWriteRegisterCOMMAND::RunCommand(int Channel_Index)
{
  for(int iCmd = 0; iCmd < m_Count; iCmd++)
  {
    int Command = m_Command[iCmd][0];
    int Value = m_Command[iCmd][1];
    if(!UsbCtrl_WriteRegister(Channel_Index, Command, Value))
      return FALSE;
  }
  //DebugOutPrint();
  return TRUE;
}
// struct UsbCtrlWriteRegisterCOMMAND End
int UsbDevice_SetPageLen(int Channel_Index, int PageLen)
{
  //PageLen += 100;    // Test FIFO Error, Manual FIFO Error
  UsbCtrlWriteRegisterCOMMAND Command;
  Command.WriteRegister(33, LOBYTE(PageLen));                             //[33] Ò³³¤¶È
  Command.WriteRegister(34, HIBYTE(PageLen));                             //[34]
  Command.WriteRegister(35, ((PageLen >> 16) & 0x0f));                    //[35]

  if(!Command.RunCommand(Channel_Index))
    return FALSE;
  //MessageBox(NULL, "SetPageLen", "SetPageLen", MB_OK);
  return TRUE;
}

int UsbDevice_CheckFifoError(int Channel_Index)
{
  BYTE   CurState = 0;
  if(UsbCtrl_ReadRegister(Channel_Index, 19, &CurState))
  {
    if(CurState & 0x01)
      return 1;
    else
      return 0;
  }
  return 0;
}

void UsbBulkTHREAD::SetChannelIndex(int ChannelIndex_I)
{
  ChannelIndex = ChannelIndex_I;
  Pipe.m_Channel_Index = ChannelIndex;
}

BOOL UsbBulkTHREAD::InitProc()
{
  Pipe.Init();
  DataSize = 2048 * 10;
  pData = new BYTE[DataSize];
  memset(pData, 0, DataSize);
  Sleep(10000);
  return 1;
}

BOOL UsbBulkTHREAD::MainProc()
{
  
  //Pipe.BulkWritePipe(pData, DataSize);
  //DWORD         ReadAddr, WriteAddr;
  //  WriteAddr = UsbCtrl_GetWriteAddr(ChannelIndex);
  //  ReadAddr = UsbCtrl_GetReadAddr(ChannelIndex);
    Sleep(10);
  //pUsbCommandIo->CheckWrReAddrTheSame();
  return -1;
}