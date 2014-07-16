#ifndef _FEEDER_CONTROL_
#define _FEEDER_CONTROL_

class FeederControlINTERFACE
{
public:
  virtual int   GetFeederErrorCode()                           = 0;
  virtual void  SetFeedPaperInfo(int PaperIndex, int FeedLen_mm) = 0;
  virtual int   Feed()                                         = 0;
};

class FeederControlIO
{
public:
  virtual int   Write(BYTE *pData, int DataLen)                = 0;
  virtual int   Read(BYTE *pBuf, int BufLen, int *pDataLen)    = 0;
};

enum FeederListenWorkTYPE
{
  FLWT_IDLE             = 0,
  FLWT_COMMAND_RET      = 1,
  FLWT_ERROR_CODE       = 2,
  FLWT_FEED_FINISH      = 3,
  FLWT_COMMAND_FEEDBACK = 4,
};

class FeederCONTROL : public FeederControlINTERFACE , public ThreadBASE
{
  FeederControlIO      *pFeederIo;
  int                   PaperIndex;
  int                   FeedLen_mm;

  FeederListenWorkTYPE  FeederListenWorkType;
  BOOL          MainProc();

  char                  Word[0x100];
  char                  ListenBuf[0x100];
  char                  Command[0x100];
  int                   BufPos;
  int                   WordPos;
  inline void   ProcessListen(char R);
  inline void   ProcessListenWork();
  inline void   ProcessWord();

  int                   CommandFeedbackOk;
  int                   CommandRetOk;
  int                   CommandRet;
  int                   ErrorCode;
  int                   bFinishFeed;
  void          BeginFeederCommand(char *pCommand){
    CommandRetOk = 0; 
    CommandRet = 0;
    CommandFeedbackOk = 0;
    strcpy(Command, pCommand);}

  DWORD         WaitFeederCommandResult();
public:
  FeederCONTROL();
  ~FeederCONTROL();
  void  SetFeederControlIo(FeederControlIO  *pFeederIo);

  // FeederControlINTERFACE
  int   GetFeederErrorCode();
  void  SetFeedPaperInfo(int PaperIndex, int FeedLen_mm);
  int   Feed();
  int   CheckFeedFinish(){ return bFinishFeed;}
};

class ComIO : public ThreadBASE
{
  int           ComIndex;
  HANDLE        hCom;

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

  BOOL  CheckCom(int i);
  void  OpenAndInitCom();
  void  CloseCom();

  BOOL          MainProc();
public:
  ComIO();
  ~ComIO();

  // -1, can't open com port
  // 1, success
  // 0, act error
  BOOL  Write(char *pData);
  BOOL  Read(char *pBuf, int BufLen, int *pDataLen);
};

class FeederControlComIO : public FeederControlIO
{
  ComIO           ComIo;
public:
  virtual int   Write(BYTE *pData, int DataLen)
  {
    return ComIo.Write((char *)pData);
  }
  virtual int   Read(BYTE *pBuf, int BufLen, int *pDataLen)
  {
    return ComIo.Read((char *)pBuf, BufLen, pDataLen);
  }
};

class FeederControlUsbIO : public FeederControlIO
{
  int ChannelIndex;
public:
  FeederControlUsbIO();
  ~FeederControlUsbIO(){}
  void SetChannelIndex(int ChannelIndex_I){ ChannelIndex = ChannelIndex_I; }
  int  GetChannelIndex(){ return ChannelIndex;}
  BOOL AutoSetChannelIndex();

  virtual int   Write(BYTE *pData, int DataLen);
  virtual int   Read(BYTE *pBuf, int BufLen, int *pDataLen);
};
#endif