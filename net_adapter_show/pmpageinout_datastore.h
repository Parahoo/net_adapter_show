#ifndef _PMPAGEINOUT_DATASTORE_
#define _PMPAGEINOUT_DATASTORE_

static const int PageIoDataStoreCount = 0x100;
static const int PageIoDataStoreMask  = 0xff;

class  PmPageIoDataSTORE
{
  int          *StoreMark;
  DWORD         CurPrint;
  DWORD         CurFreePos;

  PBYTE         *pDataStore;
  int           *StoreBufLen;
  int           *DataLen;
public:
  PmPageIoDataSTORE();
  ~PmPageIoDataSTORE();

  void          Reset();

  BOOL          CheckFreeSpace();
  BOOL          AddANewPageIoData(BYTE *pData_I, int DataLen);

  int           PrintToHere(int PrintHere);
  void          GetData(int iPrint, BYTE **ppData_O, int *pDataLen_O);
  void          PrintedHere(int PrintHere);
};

#endif