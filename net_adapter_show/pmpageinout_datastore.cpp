#include "stdafx.h"

#include "pmpageinout_datastore.h"

PmPageIoDataSTORE::PmPageIoDataSTORE()
{
  StoreMark = new int[PageIoDataStoreCount];
  pDataStore = new PBYTE[PageIoDataStoreCount];
  StoreBufLen = new int[PageIoDataStoreCount];
  DataLen     = new int[PageIoDataStoreCount];

  CurPrint = 0;
  CurFreePos = 0;
  for(int i = 0; i < PageIoDataStoreCount; i++)
  {
    StoreMark[i]   = 0;
    pDataStore[i]  = NULL;
    StoreBufLen[i] = 0;
    DataLen[i]     = 0;
  }
}

PmPageIoDataSTORE::~PmPageIoDataSTORE()
{
  for(int i = 0; i < PageIoDataStoreCount; i++)
  {
    if(pDataStore[i])
      delete pDataStore[i];
  }

  delete StoreMark;
  delete pDataStore;
  delete StoreBufLen;
  delete DataLen;
}

void PmPageIoDataSTORE::Reset()
{
  CurPrint = 0;
  CurFreePos = 0;

  for(int i = 0; i < PageIoDataStoreCount; i++)
    StoreMark[i] = 0;
}

BOOL PmPageIoDataSTORE::CheckFreeSpace()
{
  return StoreMark[CurFreePos & PageIoDataStoreMask] == 0;
}

BOOL PmPageIoDataSTORE::AddANewPageIoData(BYTE *pData_I, int DataLen_I)
{
  DWORD CurPos = CurFreePos & PageIoDataStoreMask;
  if(StoreMark[CurPos] != 0)
    return 0;

  if(StoreBufLen[CurPos] < DataLen_I)
  {
    if(pDataStore[CurPos])
      delete pDataStore[CurPos];
    StoreBufLen[CurPos] = DataLen_I;
    pDataStore[CurPos] = new BYTE[DataLen_I];
  }
  memcpy(pDataStore[CurPos], pData_I, DataLen_I);
  DataLen[CurPos] = DataLen_I;

  StoreMark[CurPos] = 1;
  CurFreePos++;
  return 1;
}

int PmPageIoDataSTORE::PrintToHere(int PrintHere)
{
  PrintHere += 5;
  PrintHere = min(PrintHere, CurFreePos);
  if(PrintHere <= 0)
    return 0;
  int Printed = PrintHere;
  if(Printed > CurPrint)
    return Printed - CurPrint;
  else
    return 0;
}

void PmPageIoDataSTORE::GetData(int iPrint, BYTE **ppData_O, int *pDataLen_O)
{
  int Cur = (CurPrint + iPrint) & PageIoDataStoreMask;
  if(ppData_O)
    *ppData_O = pDataStore[Cur];
  if(pDataLen_O)
    *pDataLen_O = DataLen[Cur];
}

void PmPageIoDataSTORE::PrintedHere(int Count)
{
  for(int i = CurPrint; i < CurPrint+Count; i++)
  {
    StoreMark[i & PageIoDataStoreMask] = 0;
  }
  CurPrint += Count;
}