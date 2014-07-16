


class UsbCommandIO : public FeederControlIO
{
public:
  virtual   BOOL CheckWrReAddrTheSame()    = 0;
  virtual   BOOL SetPageLen(int Len)       = 0;
  virtual   BOOL CheckFifoError()          = 0;
};

int UsbDevice_SetPageLen(int Channel_Index, int PageLen);
int UsbDevice_CheckFifoError(int Channel_Index);

class UsbCommandACT
{
  int   ChannelIndex;
public:
  void SetChannelIndex(int ChannelIndex_I){ ChannelIndex = ChannelIndex_I; }

  BOOL CheckWrReAddrTheSame()
  {
    DWORD         ReadAddr, WriteAddr;
    WriteAddr = UsbCtrl_GetWriteAddr(ChannelIndex);
    ReadAddr = UsbCtrl_GetReadAddr(ChannelIndex);
    return (ReadAddr == WriteAddr);
  }
  BOOL SetPageLen(int Len)
  {
    return UsbDevice_SetPageLen(ChannelIndex, Len);
  }
  BOOL CheckFifoError()
  {
    return UsbDevice_CheckFifoError(ChannelIndex);
  }
};

class UsbCommandCtriticalSectionActIO : public UsbCommandIO
{
  UsbCommandACT       UsbCommandAct;
  FeederControlUsbIO  UsbFeederIo;
  CriticalSECTION     ActCriticalSection;
public:
  void   SetChannelIndex(int ChannelIndex_I)
  {
    UsbCommandAct.SetChannelIndex(ChannelIndex_I);
    UsbFeederIo.SetChannelIndex(ChannelIndex_I);
  }
  
  virtual int   Write(BYTE *pData, int DataLen)
  {
    int Ret;
    ActCriticalSection.Enter();
    Ret = UsbFeederIo.Write(pData, DataLen);
    ActCriticalSection.Leave();
    return Ret;
  }
  virtual int   Read(BYTE *pBuf, int BufLen, int *pDataLen)
  {
    int Ret;
    ActCriticalSection.Enter();
    Ret = UsbFeederIo.Read(pBuf, BufLen, pDataLen);
    ActCriticalSection.Leave();
    return Ret;
  }
  virtual   BOOL CheckWrReAddrTheSame()
  {
    int Ret;
    ActCriticalSection.Enter();
    Ret = UsbCommandAct.CheckWrReAddrTheSame();
    ActCriticalSection.Leave();
    return Ret;
  }
  virtual   BOOL SetPageLen(int Len)
  {
    int Ret;
    ActCriticalSection.Enter();
    Ret = UsbCommandAct.SetPageLen(Len);
    ActCriticalSection.Leave();
    return Ret;
  }
  virtual   BOOL CheckFifoError()
  {
    int Ret;
    ActCriticalSection.Enter();
    Ret = UsbCommandAct.CheckFifoError();
    ActCriticalSection.Leave();
    return Ret;
  }
};

class UsbBulkTHREAD : public ThreadBASE
{
  int                  ChannelIndex;
  UsbMultiThreadPIPE   Pipe;
  UsbCommandIO        *pUsbCommandIo;

  BYTE                *pData;
  int                  DataSize;
  BOOL InitProc();
  BOOL MainProc();
public:
  void SetChannelIndex(int ChannelIndex);
  void SetUsbCommandIo(UsbCommandIO *pUsbCommandIo_I)
  {
    pUsbCommandIo = pUsbCommandIo_I;
  }
};
