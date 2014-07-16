#include "stdafx.h"

#include "feeder_control.h"

FeederCONTROL::FeederCONTROL()
{
  pFeederIo = NULL;
  PaperIndex = 0;
  FeedLen_mm = 1000;

  FeederListenWorkType = FLWT_IDLE;
  memset(ListenBuf, 0, 0x100);
  memset(Word, 0, 0x100);
  BufPos = 0;
  WordPos = 0;

  CommandFeedbackOk = 0;
  CommandRetOk = -1;
  CommandRet = 0;
  ErrorCode = 0;
  bFinishFeed = 1;
}

FeederCONTROL::~FeederCONTROL()
{
}

void FeederCONTROL::SetFeederControlIo(FeederControlIO *pFeederIo_I)
{
  pFeederIo = pFeederIo_I;
}

BOOL FeederCONTROL::MainProc()
{
  char Buf[0x100] = {0};
  int  ReadLen = 0;
  if(pFeederIo->Read((BYTE *)Buf, 0x100, &ReadLen))
  {
    if(ReadLen > 0)
    {
      for(int i = 0; i < ReadLen; i++)
      {
        if(Buf[i] == 0)
          break;
        printf("%c", Buf[i]);
        ProcessListen(Buf[i]);
      }
    }
  }
  return -1;
}

void FeederCONTROL::ProcessListen(char R)
{
  if(R == '\r'|| R == '\n')
  {
    ProcessListenWork();
    return ;
  }

  switch(FeederListenWorkType)
  {
    case FLWT_IDLE:
    {
      if(R == '$')
        FeederListenWorkType = FLWT_COMMAND_RET;
      else if(R == '[')
        FeederListenWorkType = FLWT_ERROR_CODE;
      else if(R == '@')
        FeederListenWorkType = FLWT_FEED_FINISH;
      else
      {
        if(R == ' ')
          ProcessWord();
        else
        {
          Word[WordPos] = R;
          WordPos++;
          if(WordPos >= 0xFF)
            ProcessWord();
        }
      }
      break;
    }
    default:
    {
      ListenBuf[BufPos] = R;
      BufPos++;
      if(BufPos >= 0xFF)
        ProcessListenWork();
      break;
    }
  }
}

void FeederCONTROL::ProcessWord()
{
  Word[WordPos] = 0;
  int CharLen = WordPos;
  int Cr;
  //////////////////////////////////
  if(CharLen == 4)
  {
    Cr = strncmp(Word, "cmd:", 0x100);
    if(Cr == 0)
      FeederListenWorkType = FLWT_COMMAND_FEEDBACK;
  }
  //////////////////////////////////
  WordPos = 0;
}

void FeederCONTROL::ProcessListenWork()
{
  ListenBuf[BufPos] = 0;
  int CharLen = BufPos;

  switch(FeederListenWorkType)
  {
    case FLWT_COMMAND_FEEDBACK:
    {
      char *pTemp = ListenBuf;
      while(*pTemp)
      {
        if(*pTemp == ' ')
          pTemp++;
        else
          break;
      }
      if(strcmp(pTemp, Command) == 0)
        CommandFeedbackOk = 1;
      else
        CommandFeedbackOk = 0;
      break;
    }
    case FLWT_COMMAND_RET:
    {
      char *pNum = strchr(ListenBuf, ':');
      if(pNum)
      {
        pNum++;
        CommandRet = atoi(pNum);
        CommandRetOk = 1;
      }
      else
      {
        CommandRet = -1;
        CommandRetOk = 1;
      }
      break;
    }
    case FLWT_ERROR_CODE:
    {
      ErrorCode = 1;
      break;
    }
    case FLWT_FEED_FINISH:
    {
      bFinishFeed = 1;
      break;
    }
    case FLWT_IDLE:
    {
      ProcessWord();
      break;
    }
  }
  
  BufPos = 0;
  FeederListenWorkType = FLWT_IDLE;
}

DWORD  FeederCONTROL::WaitFeederCommandResult()
{
  int Count = 0;
  while(1)
  {
    if(CommandRetOk)
      break;
    Sleep(100);
    Count++;
    if(Count >= 40)
    {
      printf("Wait Feeder Command Result Timeout\r\n");
      return -1;
    }
  }
  if(CommandFeedbackOk)
    return CommandRet;
  else
    return -2;
}

int  FeederCONTROL::GetFeederErrorCode()
{
  // TODO
  int Ret = ErrorCode;
  if(ErrorCode)
    ErrorCode = 0;
  return Ret;
}

void FeederCONTROL::SetFeedPaperInfo(int PaperIndex_I, int FeedLen_mm_I)
{
  PaperIndex= PaperIndex_I;
  FeedLen_mm = FeedLen_mm_I;
}

int  FeederCONTROL::Feed()
{
  char Buf[0x100];
  char FeedCommand[0x100];
  sprintf(FeedCommand, "feed %d %d",PaperIndex, FeedLen_mm);
  sprintf(Buf, "feed %d %d\r",PaperIndex, FeedLen_mm);
  printf("[need] %s, wait last feed finish...\r\n",Buf);

  int Cr;
  while(!bFinishFeed)
  {
    Sleep(50);
  }
  printf("last feed finished!\r\n");

  BeginFeederCommand(FeedCommand);
  int Ret = pFeederIo->Write((BYTE *)Buf, strlen(Buf));
  if(Ret != 1)
  {
    printf("Com Write Error:%d", Ret);
    return Ret;
  }
  Cr = WaitFeederCommandResult();

  if(Cr == 0)
    bFinishFeed = 0;
  else
    printf("CommandResult:%d\r\n",Cr);

  return Cr ? 0 : 1;
}


static void CommPort_Init(HANDLE hComFile)
{
  if(!hComFile)
    return;
  COMMCONFIG CommConfig;
  DWORD ConfigSize = sizeof(CommConfig);
  memset(&CommConfig, 0, ConfigSize);
  GetCommConfig(hComFile, &CommConfig, &ConfigSize); 
  if(1)
  {
    CommConfig.dcb.BaudRate = CBR_57600;
    CommConfig.dcb.ByteSize = 8;
    CommConfig.dcb.Parity   = NOPARITY;
    CommConfig.dcb.StopBits = ONESTOPBIT;
    
    /*
    CommConfig.dcb.fTXContinueOnXoff = 0;
    CommConfig.dcb.fInX = 0;
    CommConfig.dcb.fOutX = 0;
    CommConfig.dcb.fOutxCtsFlow = 0;
    CommConfig.dcb.fOutxDsrFlow = 0;
    CommConfig.dcb.fDsrSensitivity = 0;
    CommConfig.dcb.fDtrControl = 0;
    CommConfig.dcb.fRtsControl = 0;*/
    /*CommConfig.dcb.StopBits = 0;
    (&CommConfig.dcb.BaudRate)[1] = 0x5091;*/
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
  
  //PurgeComm(hComFile, PURGE_TXCLEAR|PURGE_RXCLEAR);
  DWORD WriteLen = 0;
  DWORD BufferLen = strlen(pBuffer);
  if(WriteFile(hComFile, pBuffer, BufferLen, &WriteLen, NULL) == TRUE)
  {
    if(WriteLen == BufferLen)
    {
      return 1;
      
      char  ReadBuf[0x200] = {0};
      int Pos = 0;
      static const int MaxCount = 5;
      int Count = MaxCount;
      while(1)
      {
        if(ReadFile(hComFile, ReadBuf + Pos, (BufferLen - Pos), &WriteLen, NULL) == TRUE)
        {
          if(WriteLen)
          {
            Count = MaxCount;
            Pos += WriteLen;
            if(Pos == BufferLen)
              break;
          }
          else
          {
            Count--;
            if(Count == 0)
            {
              printf("Com Command Feedback Read Timeout\r\n");
              return 0;
            }
            Sleep(100);
          }
        }
        else
        {
          printf("Com Command Feedback Read Failed\r\n");
          return -1;
        }
      }
      if(strcmp(ReadBuf, pBuffer) == 0)
        return 1;
      else
      {
        printf("ComWriteFeedback not the same[%s][%s]\r\n",ReadBuf, pBuffer);
        return 0;
      }
    }
    else
    {
      printf("WriteLen not the Same[Bl%d][Wl%d]\r\n", BufferLen, WriteLen);
      return 0;
    }
  }
  else
  {
    printf("Com Command Write Failed\r\n");
    return -1;
  }
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

ComIO::ComIO()
{
  ComIndex = 6;
  hCom = NULL;

  TaskMark = 0;
  Te.Create(0, 0, 200);
  We.Create(0);
  Re.Create(0);

  Rp = 0;
  BeginThread();
}

ComIO::~ComIO()
{
  CloseCom();
  Terminate();
}

BOOL ComIO::MainProc()
{
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
      printf("Com Write:%s\r\n",pWd);
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
    return ;
  }
  for(int i = 0; i < 20; i++)
  {
    if((i != ComIndex) && CheckCom(i))
    {
      printf("Find Com%d\r\n", i);
      break;
    }
  }
}

BOOL ComIO::CheckCom(int i)
{
  HANDLE hCurCom = OpenCommPort(i);
  if(hCurCom == NULL)
    return 0;
  CommPort_Init(hCurCom);
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
  }
  CommPort_Close(hCurCom);
  return 0;*/
}

void ComIO::CloseCom()
{
  if(hCom)
  {
    CommPort_Close(hCom);
    hCom = NULL;
  }
}

BOOL ComIO::Write(char *pData)
{
  if(hCom == NULL)
  {
    OpenAndInitCom();
    if(hCom == NULL)
      return -1;
  }
  
  TaskMark |= 0x01;
  pWd =pData;
  Te.Set();

  We.WaitForTimeOut();
  return Wret;
  return CommPort_Write(hCom, pData);
}

BOOL ComIO::Read(char *pData, int BufLen, int *pDataLen)
{
  if(hCom == NULL)
  {
    OpenAndInitCom();
    if(hCom == NULL)
      return -1;
  }
  
  TaskMark |= 0x02;
  pRb = pData;
  Rbl = BufLen;
  Te.Set();

  Re.WaitForTimeOut();
  if(pDataLen)
    *pDataLen = 1;
  return Rret;

  if(pDataLen)
    *pDataLen = 1;
  return CommPort_Read(hCom, pData, BufLen);
}

/////////
#include "mtusb.h"
static BOOL SepiaUsb_Control(HANDLE hDevice, DWORD Command, DWORD Address, DWORD Value, WORD *pData)
{
  return RequestSystemCommandEx(hDevice, Command, Address, Value, pData);
}

static DWORD SepiaCtrl_SerialWrite(HANDLE hDevice, BYTE Data)
{
  WORD Setting;
  if(!SepiaUsb_Control(hDevice, 0xCC, Data, 0, &Setting))
    return FALSE;
  if(Setting != 0xBCBC)
    return FALSE;
  return TRUE;
}

static DWORD SepiaCtrl_SerialCount(HANDLE hDevice)
{
  WORD Setting;
  if(!SepiaUsb_Control(hDevice, 0xCD, 0, 0, &Setting))
    return FALSE;
  return Setting;
}

DWORD SepiaCtrl_SerialClear(HANDLE hDevice)
{
  WORD Setting;
  if(!SepiaUsb_Control(hDevice, 0xCF, 0, 0, &Setting))
    return FALSE;
  if(Setting != 0xBFBF)
    return FALSE;
  return TRUE;
}

DWORD SepiaCtrl_SerialWrite(HANDLE hDevice, const char *pText)
{
  int Len = strlen(pText);
  for(int i = 0; i < Len; i++)
  {
    if(!SepiaCtrl_SerialWrite(hDevice, pText[i]))
      return FALSE;
  }
  return TRUE;
}

DWORD SepiaCtrl_SerialRead(HANDLE hDevice, DWORD Pos, BYTE *pBuf)
{
  WORD Setting;
  if(!SepiaUsb_Control(hDevice, 0xCE, Pos, 0, &Setting))
    return FALSE;
  if(HIBYTE(Setting) != 0xBE)
    return FALSE;
  if(pBuf)
    *pBuf = LOBYTE(Setting);
  return 1;
}

FeederControlUsbIO::FeederControlUsbIO()
{
  ChannelIndex = 0;
}

BOOL FeederControlUsbIO::AutoSetChannelIndex()
{
  for(int i = 0; i < 10; i++)
  {
    HANDLE hDevice = CypressUsb_OpenDevice(i);
    if(hDevice)
    {
      CypressUsb_CloseDevice(hDevice);

      ChannelIndex = i;
      return 1;
    }
  }
  return 0;
}

int FeederControlUsbIO::Write(BYTE *pData, int DataLen)
{
  HANDLE hDevice = CypressUsb_OpenDevice(ChannelIndex);
  if(hDevice)
  {
    int Ret = 0;
    char *pBuffer = (char *)pData;
    DWORD BufferLen = strlen(pBuffer);

    if(SepiaCtrl_SerialWrite(hDevice, pBuffer))
    {
      Ret = 1;
    }
    CypressUsb_CloseDevice(hDevice);
    return Ret;
  }
  else
    return -1;
}

int FeederControlUsbIO::Read(BYTE *pBuf, int BufLen, int *pDataLen)
{
  HANDLE hDevice = CypressUsb_OpenDevice(ChannelIndex);
  if(hDevice)
  {
    int Ret = 1;

    int R = SepiaCtrl_SerialRead(hDevice, 0, pBuf);
    if(R == 0)
    {
      Ret = 0;
    }
    else
    {
      if(*pBuf)
      {
        if(pDataLen)
          *pDataLen = 1;
      }
      else
      {
        Sleep(10); // 无数据的情况等待
        if(pDataLen)
          *pDataLen = 0;
      }
    }

    CypressUsb_CloseDevice(hDevice);
    return Ret;
  }
  else
    return -1;
}