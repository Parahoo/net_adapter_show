#include "stdafx.h"

#include "threadbase.h"
#include "feeder_control.h"

#ifdef _ABC
static const int ComIndex = 6;

struct MyComCFG
{
  int  BaudRate;
  int  ByteSize;
  int  Parity;
  int  StopBits;
  int  BitStreamControl;
};

static void CommPort_Init(HANDLE hComFile, MyComCFG MyComCfg)
{
  if(!hComFile)
    return;
  COMMCONFIG CommConfig;
  DWORD ConfigSize = sizeof(CommConfig);
  memset(&CommConfig, 0, ConfigSize);
  GetCommConfig(hComFile, &CommConfig, &ConfigSize); 
  if(1)
  {
    CommConfig.dcb.BaudRate = MyComCfg.BaudRate;
    CommConfig.dcb.ByteSize = MyComCfg.ByteSize;
    CommConfig.dcb.Parity   = MyComCfg.Parity;
    CommConfig.dcb.StopBits = MyComCfg.StopBits;
    
    if(MyComCfg.BitStreamControl == 0)
    {
      CommConfig.dcb.fTXContinueOnXoff = 0;
      CommConfig.dcb.fInX = 0;
      CommConfig.dcb.fOutX = 0;
      CommConfig.dcb.fOutxCtsFlow = 0;
      CommConfig.dcb.fOutxDsrFlow = 0;
      CommConfig.dcb.fDsrSensitivity = 0;
      CommConfig.dcb.fDtrControl = 0;
      CommConfig.dcb.fRtsControl = 0;
    }
  }
  SetCommConfig(hComFile, &CommConfig, ConfigSize); 
  PurgeComm(hComFile, PURGE_TXCLEAR|PURGE_RXCLEAR);
  
  COMMTIMEOUTS Timeout;
  GetCommTimeouts(hComFile, &Timeout);
  Timeout.ReadIntervalTimeout = 1;
  Timeout.ReadTotalTimeoutMultiplier = 1;
  Timeout.ReadTotalTimeoutConstant = 1;
  SetCommTimeouts(hComFile, &Timeout);
}

static HANDLE OpenCommPort(int ComPort)
{
  char CommFileName[0x100];
  sprintf(CommFileName, "COM%d", ComPort);
  HANDLE hComFile = CreateFile(CommFileName, 
                  GENERIC_READ | GENERIC_WRITE, 
                  0,      //   串口设备必须是独占使用   
                  NULL,   //   no   security   attrs   
                  OPEN_EXISTING,   //串口设备必须使用OPEN_EXISTING设置 
                  0,      //   输入输出不能重叠 
                  NULL    //   hTemplate对于串口设备必须是NULL 
                  ); 
  if(hComFile == INVALID_HANDLE_VALUE)
    return NULL;
  return hComFile;
}

static void CommPort_Close(HANDLE hComFile)
{
  if(hComFile == INVALID_HANDLE_VALUE)
    return;
  if(hComFile)
    CloseHandle(hComFile);
}

static BOOL CommPort_Write(HANDLE hComFile, const char *pBuffer)
{
  int ErrorCode = 0;
  if(hComFile == NULL)
    return -1;
  
  PurgeComm(hComFile, PURGE_TXCLEAR|PURGE_RXCLEAR);
  DWORD WriteLen = 0;
  DWORD BufferLen = strlen(pBuffer);
  if(WriteFile(hComFile, pBuffer, BufferLen, &WriteLen, NULL) == TRUE)
  {
    if(WriteLen == BufferLen)
    {
      char  ReadBuf[0x200] = {0};
      int Pos = 0;
      int Count = 10;
      while(1)
      {
        if(ReadFile(hComFile, ReadBuf + Pos, (BufferLen - Pos), &WriteLen, NULL) == TRUE)
        {
          if(WriteLen)
          {
            Count = 10;
            Pos += WriteLen;
            if(Pos == BufferLen)
              break;
          }
          else
          {
            Count--;
            if(Count == 0)
              return 0;
          }
        }
        else
          return 0;
      }
      return strcmp(ReadBuf, pBuffer) == 0;
    }
    else
      return 0;
  }
  else
    return -1;
  /*
  for(int i = 0; i < BufferLen; i++)
  {
    if(WriteFile(hComFile, pBuffer+i, 1, &WriteLen, NULL) == TRUE)
    {
      if(1 == WriteLen)
      {
        char Feedback;
        int Count = 10;
        while(1)
        {
          int RRet = ReadFile(hComFile, &Feedback, 1, &WriteLen, NULL);
          if(RRet != 1)
            return -1;
          else
          {
            if(WriteLen == 1)
              break;
            Count--;
            if(Count == 0)
            {
              return 0;
            }
          }
        }
        if(Feedback != pBuffer[i])
          return 0;
      }
      else
        return 0;
    }
    else
      return -1;
  }*/
  return 1;
}

static BOOL CommPort_Read(HANDLE hComFile, char *pBuffer, int BufferLen)
{
  int ErrorCode = 0;
  if(hComFile == NULL)
    return -1;

  memset(pBuffer, 0, BufferLen);

  DWORD ReadLen = 0;
  int Ret = FALSE;
  Ret = ReadFile(hComFile, pBuffer, 1, &ReadLen, NULL);

  if(Ret == 0)
    return -1;

  return ReadLen;
}

class ComIO : public ThreadBASE
{
  int           ComIndex;
  HANDLE        hCom;
  MyComCFG      MyComCfg;

  int           TaskMark;
  SignalEVENT   Te;
  SignalEVENT   We;
  SignalEVENT   Re;

  char         *pWd; //write data
  char         *pRb; // Read Buf;
  int           Rbl; // Read Buf Len;

  int           Wret;
  int           Rret;

  int           Rp;  // Read Pos;

  int           SetComCfg;

  BOOL  CheckCom(int i);
  void  OpenAndInitCom();
  void  CloseCom();
  void  ShowComCfg();

  BOOL          MainProc();
public:
  ComIO();
  ~ComIO();

  void   SetBaudRate(int BaudRate);

  // -1, can't open com port
  // 1, success
  // 0, act error
  BOOL  Write(char *pData);
  BOOL  Read(char *pBuf, int BufLen, int *pDataLen);
};

ComIO::ComIO()
{
  ComIndex = 6;
  hCom = NULL;

  MyComCfg.BaudRate = 9600;
  MyComCfg.ByteSize = 8;
  MyComCfg.Parity   = NOPARITY;
  MyComCfg.StopBits = ONESTOPBIT;
  MyComCfg.BitStreamControl = 0;

  TaskMark = 0;
  Te.Create(0, 0, 200);
  We.Create(0);
  Re.Create(0);

  Rp = 0;
  SetComCfg = 0;
  BeginThread();
}

ComIO::~ComIO()
{
  CloseCom();
  Terminate();
}

void ComIO::SetBaudRate(int BaudRate)
{
  SetComCfg = 1;
  CloseCom();
  MyComCfg.BaudRate = BaudRate;
  SetComCfg = 0;
}

BOOL ComIO::MainProc()
{
  if(SetComCfg == 1)
  {
    Sleep(100);
    return -1;
  }
  if(hCom == NULL)
  {
    OpenAndInitCom();
    if(hCom == NULL)
      Sleep(10);
  }
  else
  {
  if(!Te.WaitForTimeOut())
  {
    if(TaskMark & 0x01)
    {
      Wret = CommPort_Write(hCom, pWd);

      TaskMark &= ~0x01;
      We.Set();
      if(Wret == -1)
      {
        CloseCom();
      }
    }
    if(TaskMark & 0x02)
    {
      Rret = CommPort_Read(hCom, pRb, Rbl);
      TaskMark &= ~0x02;
      Re.Set();
      if(Rret == -1)
      {
        CloseCom();
      }
    }
  }
  }

  return -1;
}


void ComIO::OpenAndInitCom()
{
  /*
  hCom = OpenCommPort(ComIndex);
  if(hCom)
    CommPort_Init(hCom);*/
  if(CheckCom(ComIndex))
  {
    printf("Find Com%d\r\n", ComIndex);
    ShowComCfg();
    return ;
  }
  for(int i = 0; i < 20; i++)
  {
    if((i != ComIndex) && CheckCom(i))
    {
      printf("Find Com%d\r\n", i);
      ShowComCfg();
      break;
    }
  }
}

BOOL ComIO::CheckCom(int i)
{
  HANDLE hCurCom = OpenCommPort(i);
  if(hCurCom == NULL)
    return 0;
  CommPort_Init(hCurCom, MyComCfg);
  hCom = hCurCom;
  return 1;
  /*
  int  Ret;
  Ret = CommPort_Write(hCurCom, "sta\r\n");
  if(Ret == 1)
  {
    char  Buf[0x10];
    Sleep(50);
    Ret = CommPort_Read(hCurCom, Buf, 0x10);
    if(Ret == 1)
    {
      hCom = hCurCom;
      return 1;
    }
  }*/
  CommPort_Close(hCurCom);
  return 0;
}

void ComIO::CloseCom()
{
  if(hCom)
  {
    CommPort_Close(hCom);
    hCom = NULL;
  }
}

void ComIO::ShowComCfg()
{
  char *pSNull = "";
  char *pCur   = pSNull;
  printf("CurComCFG:\r\n");
  printf("  BaudRate:%d\r\n", MyComCfg.BaudRate);
  printf("  ByteSize:%d\r\n", MyComCfg.ByteSize);

  pCur = pSNull;
  char *pNoParity = "无";
  char *pOddParity = "奇";
  char *pEvenParity = "偶";
  switch(MyComCfg.Parity)
  {
  case NOPARITY:
    pCur = pNoParity;
    break;
  case ODDPARITY:
    pCur = pOddParity;
    break;
  case EVENPARITY:
    pCur = pEvenParity;
    break;
  }
  printf("  奇偶校验:%s\r\n",   pCur);

  
  pCur = pSNull;
  char *pOneStopBit = "1";
  char *pOne5StopBits = "1.5";
  char *pTwoStopBits = "2";
  switch(MyComCfg.StopBits)
  {
  case ONESTOPBIT:
    pCur = pOneStopBit;
    break;
  case ONE5STOPBITS:
    pCur = pOne5StopBits;
    break;
  case TWOSTOPBITS:
    pCur = pTwoStopBits;
    break;
  }
  printf("  停止位:%s\r\n", pCur);

  printf("  流控制:%d\r\n", MyComCfg.BitStreamControl);
}

BOOL ComIO::Write(char *pData)
{
  if(hCom == NULL)
    return -1;
  
  TaskMark |= 0x01;
  pWd =pData;
  Te.Set();

  We.WaitForTimeOut();
  return Wret;
 /* return CommPort_Write(hCom, pData);*/
}

BOOL ComIO::Read(char *pData, int BufLen, int *pDataLen)
{
  if(hCom == NULL)
    return -1;
 
  TaskMark |= 0x02;
  pRb = pData;
  Rbl = BufLen;
  Te.Set();

  Re.WaitForTimeOut();
  if(pDataLen)
    *pDataLen = 1;
  return Rret;
  /* if(pDataLen)
    *pDataLen = 1;
  return CommPort_Read(hCom, pData, BufLen);*/
}

///////////////////////////////////////////////////////
class ComListenTHREAD : public ThreadBASE
{
  ComIO     *pCom;
  BOOL      InitProc();
  BOOL      MainProc();
public:
  ComListenTHREAD(){ pCom = NULL; }
  void      SetCom(ComIO *pCom_I){ pCom = pCom_I; }
};

BOOL ComListenTHREAD::InitProc()
{
  printf("InitCom Ok, Listening...\r\n");
  return 1;
}

BOOL ComListenTHREAD::MainProc()
{
  char  Read[0x200];
  memset(Read, 0, 0x200);
  int Ret = pCom->Read(Read, 0x200,NULL);
  if(Ret > 0)
  {
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
      FOREGROUND_INTENSITY | FOREGROUND_GREEN);
    //printf("~~~~~~~~~~~~~~~~~~~~~~\r\n%s\r\n^^^^^^^^^^^^^^^^^^^^^\r\n",Read);
    printf(Read);
    SetConsoleTextAttribute(GetStdHandle(STD_OUTPUT_HANDLE),
      FOREGROUND_INTENSITY | FOREGROUND_BLUE);
  }
  return -1;
}

class ConsoleListenTHREAD : public ThreadBASE
{
  ComIO    *pCom;
  char      DeviceStreamEnd[0x10];

  void      ProcInput(char *pBuf);
  BOOL      MainProc();
public:
  ConsoleListenTHREAD(){ 
    strcpy(DeviceStreamEnd, "A");
    pCom = NULL; }
  void      SetCom(ComIO *pCom_I){ pCom = pCom_I; }
};

void ConsoleListenTHREAD::ProcInput(char *pBuf)
{
  char *pTemp = pBuf;
  while(*pTemp == ' ')
    pTemp++;

  if(*pTemp == ':')
  {
    pTemp++;
    while(*pTemp == ' ')
      pTemp++;

    printf("Enter Command Mode:\r\n");
    
    char *pType[3] = {0};
    pType[0] = strstr(pTemp, "set");
    pType[1] = strstr(pTemp, "cmd");
    pType[2] = strstr(pTemp, "open");

    int ProcType = -1;
    char *pLast = pBuf + 0x200;
    for(int i = 0; i < 3; i++)
    {
      if(pType[i])
      {
        if(pType[i] == pTemp)
        {
          ProcType = i;
          pLast = pType[i];
        }
      }
    }

    switch(ProcType)
    {
      case 0:
      {
        char *pSet = pType[ProcType] + 4;
        int BaudRate = atoi(pSet);
        if(BaudRate != 0)
        {
          pCom->SetBaudRate(BaudRate);
        }
        else
          printf("Set BaudRate Error\r\n");
        break;
      }

      case 1:
      {
        char *pCmdLine = pType[ProcType] + 4;
        strcat(pCmdLine, "\r\n");
        int Ret = pCom->Write(pCmdLine);
        break;
      }

      case 2:
      {
        char *pFileName = pType[ProcType] + 5;
        FILE *pFile = fopen(pFileName, "rb");
        if(pFile)
        {
          char Buf[0x1000];
          int ReadSize;
          do
          {
            memset(Buf, 0, 0x1000);
            ReadSize = fread(Buf, 1, 0x1000, pFile);
            if(ReadSize)
              pCom->Write(Buf);
          } while(ReadSize == 0x1000);
          fclose(pFile);
          char End[2];
          End[0] = 128;
          End[1] = 0;
          pCom->Write(End);
        }
        else
          printf("Cann't Open File\"%s\"\r\n",pFileName);
        break;
      }
      default:
        printf("{Input Unknow}\r\n");
        break;
    }
  }
  else
  {
    strcat(pBuf, "\r");
    pCom->Write(pBuf);
  }
}

BOOL ConsoleListenTHREAD::MainProc()
{
  char Buf[0x200];
  gets(Buf);
  ProcInput(Buf);
  return -1;
}

static BOOL  bExit = 0;
static BOOL  bExitSu = 0;
BOOL WINAPI ConsoleCtrlRoutine(DWORD CtrlId)
{
  switch(CtrlId)
  {
  case CTRL_CLOSE_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    bExit = 1;
    while(!bExitSu)
      Sleep(100);
    return 0;
  default:
    return 0;
  }
}

int main()
{
    bExit = 0;
    bExitSu = 0;
    SetConsoleCtrlHandler(ConsoleCtrlRoutine, TRUE);

    printf("输入':'支持命令\"set\" \"open\"(区分大小写)\r\n");
    printf("直接输入并回车后,使用串口输出输入字符串\r\n");
    printf("  set   -> :set <波特率>\r\n");
    printf("  open  -> :open <打开的文件全路径>\r\n\r\n");

    ComListenTHREAD     ComListen;
    ConsoleListenTHREAD ConsoleListen;
    ComIO               ComIo;
    ComListen.SetCom(&ComIo);
    ConsoleListen.SetCom(&ComIo);

    ComListen.BeginThread();
    ConsoleListen.BeginThread();
    /*
    while(!bExit)
    {
      for(int i = 0; i < 50; i++)
      {
        Sleep(100);
        if(bExit)
          break;
      }
      if(bExit)
        break;

      printf("Auto Feed Command\r\n");
      int Ret = ComIo.Write("feed 1 1000\r");
      if(Ret != 1)
        printf("Auto Feed Command be Error[%d]!\r\n", Ret);
    }*/
    
    while(!bExit)
      Sleep(10);

    ComListen.Terminate();
    ConsoleListen.Terminate();
    ComIo.Terminate();

    ComListen.WaitJobFinish();
    ConsoleListen.WaitJobFinish();
    ComIo.WaitJobFinish();
    bExitSu = 1;
    return 1;
}
#endif

static BOOL  bExit = 0;
static BOOL  bExitSu = 0;
static BOOL  bMexit = 0;
BOOL WINAPI ConsoleCtrlRoutine(DWORD CtrlId)
{
  switch(CtrlId)
  {
  case CTRL_CLOSE_EVENT:
  case CTRL_SHUTDOWN_EVENT:
    bExit = 1;
    bMexit = 1;
    while(!bExitSu)
      Sleep(100);
    return 0;
  default:
    return 0;
  }
}

#define _USB_COM_LINK_

#include "mtusb.h"
#include "UsbBulkThread.h"
int main()
{
    bExit = 0;
    bExitSu = 0;
    SetConsoleCtrlHandler(ConsoleCtrlRoutine, TRUE);

    printf("Begin\r\n");
#ifndef _USB_COM_LINK_
    FeederControlComIO       FeederCom;
#else
    FeederControlUsbIO       FeederCom;
    if(!FeederCom.AutoSetChannelIndex())
    {
      printf("Cann't find USB\r\n");
      bExit = 1;
    }
    else
      printf("Find Usb Com\r\n");
    UsbBulkTHREAD   UsbBulk;
    UsbCommandCtriticalSectionActIO  UsbCsActIo;
    UsbCsActIo.SetChannelIndex(FeederCom.GetChannelIndex());
    UsbBulk.SetChannelIndex(FeederCom.GetChannelIndex());
    UsbBulk.SetUsbCommandIo(&UsbCsActIo);
    UsbBulk.BeginThread();
#endif
    FeederCONTROL            FeederControl;
#ifndef _USB_COM_LINK_
    FeederControl.SetFeederControlIo(&FeederCom);
#else
    FeederControl.SetFeederControlIo(&UsbCsActIo);
#endif
    FeederControl.BeginThread();
    
    FeederControl.SetFeedPaperInfo(0, 1189);
    Sleep(2000);
    int iFeed = 0;
    while(!bExit)
    {/*
      for(int i = 0; i < 30; i++)
      {
        Sleep(100);
        if(bExit)
          break;
      }*/
      if(bExit)
        break;

      printf("*************** Auto Feed Command[%d] *****************\r\n", iFeed++);
      int Ret = FeederControl.Feed();
      if(Ret != 1)
      {
        printf("Auto Feed Command be Error[%d]!\r\nExiting", Ret);
        bExit = 1;
        break;
      }
      while(FeederControl.CheckFeedFinish() != 1)
      {
        printf(".");
        //UsbCsActIo.CheckWrReAddrTheSame();
        UsbCsActIo.SetPageLen(100);
        //printf(".");
        Sleep(1000);
      }
      int ErrorCode = FeederControl.GetFeederErrorCode();
      if(ErrorCode)
      {
        printf("FeederError[%d] Occur!\r\n, Exit...\r\n", ErrorCode);
        bExit = 1;
        break;
      }
    }

    while(!bExit)
      Sleep(10);

    FeederControl.Terminate();
    bExitSu = 1;
    if(!bMexit)
      Beep(1000, 5000);
    printf("wait manul exit...");
    while(!bMexit)
      Sleep(100);
    return 1;
}