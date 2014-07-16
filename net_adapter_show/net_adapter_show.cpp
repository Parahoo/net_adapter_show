#include "stdafx.h"

#include "pmpageinout_datastore.h"
#include "threadbase.h"

static const int  TestScale = 50000000;

class WriteTHREAD : public ThreadBASE
{
  PmPageIoDataSTORE  *pStore;
  int                 Times;
  virtual  BOOL  InitProc();
  virtual  BOOL  MainProc();
public:
  WriteTHREAD()
  {
    pStore = NULL;
    Times = 0;
  }
  ~WriteTHREAD()
  {
    Terminate();
  }
  void SetDataStore(PmPageIoDataSTORE  *pStore_I)
  {
    pStore = pStore_I;
  }
};

BOOL WriteTHREAD::InitProc()
{
  if(pStore == NULL)
    return 0;
  Times = 0;
  return 1;
}

BOOL WriteTHREAD::MainProc()
{
  if(pStore->CheckFreeSpace() == 0)
    return -1;
  pStore->AddANewPageIoData((BYTE *)(&Times), sizeof(int));
  //printf("Add Page %d\r\n", Times);
  Times++;

  if(Times < TestScale)
    return -1;
  else
    return 1;
}

class ReadTHREAD : public ThreadBASE
{
  PmPageIoDataSTORE  *pStore;
  int                 Times;
  virtual  BOOL  InitProc();
  virtual  BOOL  MainProc();
public:
  ReadTHREAD()
  {
    pStore = NULL;
    Times = 0;
  }
  ~ReadTHREAD()
  {
    Terminate();
  }
  void SetDataStore(PmPageIoDataSTORE  *pStore_I)
  {
    pStore = pStore_I;
  }
};

BOOL ReadTHREAD::InitProc()
{
  if(pStore == NULL)
    return 0;
  Times = 0;
  return 1;
}

BOOL ReadTHREAD::MainProc()
{
  int Result;

  BYTE *pResult;
  int ResultLen;

  int Here = Times + 5;
  int Count = pStore->PrintToHere(Here);
  if(Count)
  {
   // printf("Read Count %d\r\n", Count);
    for(int i = 0; i < Count; i++)
    {
      pStore->GetData(i, &pResult, &ResultLen);
      if(ResultLen != sizeof(int))
      {
        printf("OutData[%d] Len = %d Error!\r\n", Times, ResultLen);
        return 0;
      }
      Result = *((int *)pResult);
      if(Result != Times)
      {
        printf("OutData[%d] Data = %d Error!\r\n", Times, Result);
        return 0;
      }
    //  printf("Read Page %d\r\n", Times);
      Times++;
    }
    pStore->PrintedHere(Count);
  }

  if(Times < TestScale)
    return -1;
  else
    return 1;
}

int main()
{
  printf("Begin Task(TestScale:%d)...\r\n", TestScale);
  int  StartTick = GetTickCount();

  /////////////////////////////////////////////////////
  PmPageIoDataSTORE     DataStore;
  WriteTHREAD           Write;
  ReadTHREAD            Read;

  Write.SetDataStore(&DataStore);
  Read.SetDataStore(&DataStore);

  Write.BeginThread();
  Read.BeginThread();

  //////////////////////////////////////////////////////
  /*
  PmPageIoDataSTORE     DataStore1;
  WriteTHREAD           Write1;
  ReadTHREAD            Read1;

  Write1.SetDataStore(&DataStore1);
  Read1.SetDataStore(&DataStore1);

  Write1.BeginThread();
  Read1.BeginThread();

  Write1.WaitJobFinish();
  Read1.WaitJobFinish();*/

  Write.WaitJobFinish();
  Read.WaitJobFinish();

  int  EndTick = GetTickCount();
  printf("Use %dms Finish The Task\r\n",(EndTick - StartTick));
  printf("Press Any Key to Continue...");
  getchar();
  return 1;
}