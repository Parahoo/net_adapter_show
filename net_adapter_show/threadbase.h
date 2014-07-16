#ifndef _Thread_BASE
#define _Thread_BASE

class CriticalSECTION
{
  CRITICAL_SECTION      m_Cs;
public:
  CriticalSECTION();
  ~CriticalSECTION();
  void Enter();
  void Leave();
};

class SignalEVENT
{
  HANDLE        hEvent;
  int           InitSignalState;    // default 0 --- nosignaled
  int           bManual;            // default 1
  int           WaitTime;           // INFINITE

  void          Init(int bManualReset = 1, 
                     int InitState = 0, 
                     int WaitForTime = INFINITE);
public:
  SignalEVENT();
  ~SignalEVENT();

  void          Create(int bManualReset = 1, 
                       int InitState    = 0, 
                       int WaitForTime  = INFINITE);

  void          Set();          // signaled
  void          Reset();        // nosignaled

  DWORD         WaitFor();
  BOOL          WaitForTimeOut();
};

enum ThreadRunSTATE
{
  TRS_RUN               = 1,
  TRS_STOP              = 0,
  TRS_PAUSE             = -1,
};

#define TBRR_UNUSE      0xffff0000

class ThreadBASE
{
  HANDLE                hThread;
  DWORD                *pRunState;  // just use, don't create;

  SignalEVENT           JobFinish;
  // return 1, continue to mainproc; return 0, fatal error
  virtual  BOOL  InitProc() { return 1; }

  // return 0, fatal error; 1,success; -1, continue;
  virtual  BOOL  MainProc()         = 0;
protected:
  char          ThreadName[0x100];
  DWORD         JobRet;     // Can Use or not for inherice class
  void          TerminateCommonRootThread(); // set *pRunState = TRS_STOP
public:
  ThreadBASE();
  ~ThreadBASE();

  void SetThreadName(const char *pThreadName){ strcpy(ThreadName, pThreadName); }
  void SetpRunState(DWORD *pRunState_I){ pRunState = pRunState_I;};
  void BeginThread();
  void Terminate();
  BOOL ThreadProc();

  void WaitJobFinish(DWORD  *pJobResult = NULL)
  {
    if(pJobResult)
      *pJobResult = JobRet;
    JobFinish.WaitForTimeOut();
  }
};

#endif