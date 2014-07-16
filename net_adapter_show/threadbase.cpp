#include "stdafx.h"

//#include <winsock2.h>

#include "threadbase.h"

static DWORD _stdcall ThreadBaseCommProc(LPVOID lpParam);

CriticalSECTION::CriticalSECTION()
{
  InitializeCriticalSection(&m_Cs);
}

CriticalSECTION::~CriticalSECTION()
{
  DeleteCriticalSection(&m_Cs);
}

void CriticalSECTION::Enter()
{
  EnterCriticalSection(&m_Cs);
}

void CriticalSECTION::Leave()
{
  LeaveCriticalSection(&m_Cs);
}

SignalEVENT::SignalEVENT()
{
  hEvent = NULL;
  Init();
}

void SignalEVENT::Init(int bManualReset, 
                       int InitState, 
                       int WaitForTime)
{
  InitSignalState = InitState;
  bManual = bManualReset;
  WaitTime = WaitForTime;
}

SignalEVENT::~SignalEVENT()
{
  if(hEvent)
  {
    CloseHandle(hEvent);
    hEvent = NULL;
  }
}

void SignalEVENT::Create(int bManualReset, 
                       int InitState, 
                       int WaitForTime)
{
  Init(bManualReset, InitState, WaitForTime);
  hEvent = CreateEvent(NULL, bManual, InitSignalState, NULL);
}

void SignalEVENT::Set()
{
  SetEvent(hEvent);
}

void SignalEVENT::Reset()
{
  ResetEvent(hEvent);
}

DWORD SignalEVENT::WaitFor()
{
  return WaitForSingleObject(hEvent, WaitTime);
}

BOOL SignalEVENT::WaitForTimeOut()
{
  return (WaitForSingleObject(hEvent, WaitTime) == WAIT_TIMEOUT);
}

static DWORD _stdcall ThreadBaseCommProc(LPVOID lpParam)
{
  ThreadBASE *pThread = (ThreadBASE *)lpParam;
  return pThread->ThreadProc();
}

ThreadBASE::ThreadBASE()
{
  hThread = NULL;
  pRunState = NULL;

  memset(ThreadName, 0, 0x100);
  JobRet = 0xffff0000;

  JobFinish.Create(); // wait infinity
}

ThreadBASE::~ThreadBASE()
{
  if(hThread)
  {
    CloseHandle(hThread);   // don't terminate it
  }
}

void ThreadBASE::BeginThread()
{
  JobRet = 0xffff0000;
  JobFinish.Reset();
  hThread = CreateThread(NULL, NULL, 
    ThreadBaseCommProc, (LPVOID)this, 
    NULL, NULL);
}

void ThreadBASE::Terminate()
{
  if(hThread)
  {
    TerminateThread(hThread, 0);
    CloseHandle(hThread);
    hThread = NULL;
  }
  pRunState = NULL;
  JobFinish.Set();
}

void ThreadBASE::TerminateCommonRootThread()
{
  if(pRunState)
  {
    *pRunState = TRS_STOP;
  }
}

BOOL ThreadBASE::ThreadProc()
{
  int Ret = 0;
  if(InitProc())
  {
    int UnNeedCheckRunState = 0;
    if(pRunState == 0)
      UnNeedCheckRunState = 1;

    while(UnNeedCheckRunState || (*pRunState != TRS_STOP))
    {
      if(pRunState && (*pRunState == TRS_PAUSE))
      {
        Sleep(100);
        continue;
      }

      Ret = MainProc();
      if(Ret == 0 || Ret == 1)
        break;
    }
  }
  CloseHandle(hThread);
  hThread = NULL;

  JobFinish.Set();
  return Ret;
}