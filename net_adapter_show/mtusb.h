
#define con_reg                     0x80
#define fpga_reg_clk_sel_set      30        //喷头喷谢频率(133*1000*1000)/(桢大小*(X+1)*2)

int CypressUsb_GetChannelIndex(int Index);
HANDLE CypressUsb_OpenDevice(int Channel_Index);
void CypressUsb_CloseDevice(HANDLE hDevice);
BOOL CypressUsb_BulkWritePipe(HANDLE hDevice, void *pBuffer, DWORD BufferSize, int PipeNum);
BOOL CypressUsb_GetPagePosition(HANDLE hDevice, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty);

BOOL UsbCtrl_WriteRegister(int Channel_Index, int Address, int Data);
BOOL UsbCtrl_ReadRegister(int Channel_Index, BYTE Address, BYTE *pValue);

BOOL UsbCtrl_Reset(int Channel_Index);
BOOL UsbCtrl_LimitFifoSize(int Channel_Index, BOOL bLimit);
BOOL UsbCtrl_AbortPageEncoder(int Channel_Index);
DWORD UsbCtrl_GetVersion(int Channel_Index, BOOL bShow);
BOOL UsbCtrl_SetOnePipe(int Channel_Index, BOOL bFlag);
BOOL UsbCtrl_GetPagePosition(int Channel_Index, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty);
BOOL UsbCtrl_GetPagePosition(int Channel_Index, BOOL *pbPageEncoderReady, DWORD *pPage, DWORD *pPosition, DWORD *pbFifoEmpty);
BOOL UsbCtrl_BulkWriteOnePipe(int Channel_Index, void *pBuffer, DWORD BufferSize);
BOOL UsbCtrl_SetPrintCtrlPulse(int Channel_Index, const DWORD *pBuffer, DWORD Size);
BOOL UsbCtrl_SetPrintCtrlPulse(int Channel_Index);

DWORD UsbCtrl_GetWriteAddr(int Channel_Index);
DWORD UsbCtrl_GetReadAddr(int Channel_Index);

BOOL UsbCtrl_WriteSerialPort(int Channel_Index, const char *pText, int Port = 0);
BOOL UsbCtrl_ReadSerialPort(int Channel_Index, char *pBuffer, DWORD BufferSize, int Port = 0);

BOOL RequestSystemCommandEx(HANDLE hDevice, DWORD Command, DWORD Address, DWORD Value, WORD *pData);

class UsbMultiThreadPIPE
{
public:
  BYTE          m_MultiThreadBuffer[2][1024*64];
  DWORD         m_MultiThreadBufferSize;
  BOOL          m_bMultiThreadIsStart[2];
  BOOL          m_bMultiThreadIsExist[2];
  HANDLE        m_hMultiThread_MasterEvent[2];
  HANDLE        m_hMultiThread_SlaveEvent[2];
  BOOL          m_bMultiThread_BulkWrite[2];
  BOOL          m_bUsbMultiThreadInit;
  int           m_Channel_Index;
  

  UsbMultiThreadPIPE();
  void          Reset();
  void          Init();
  void          InitThread(int Index);
  DWORD         SendBulkFunc(int Index);
  BOOL          ExportBufferSize(HANDLE hDevice, int Index);

  void          Uninit();
  void          DestroyTransmitEvent();

  void          SplitBulkBuffer(BYTE *pBuffer, DWORD BufferSize);
  BOOL          BulkWritePipe(void *pBuffer, DWORD BufferSize);
};
