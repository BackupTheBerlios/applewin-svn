#pragma once

extern class CSuperSerialCard sg_SSC;

enum {COMMEVT_WAIT=0, COMMEVT_ACK, COMMEVT_TERM, COMMEVT_MAX};

class CSuperSerialCard
{
public:
	CSuperSerialCard();
	~CSuperSerialCard();

	void    CommReset();
	void    CommDestroy();
	void    CommSetSerialPort(HWND,DWORD);
	void    CommUpdate(DWORD);
	DWORD   CommGetSnapshot(SS_IO_Comms* pSS);
	DWORD   CommSetSnapshot(SS_IO_Comms* pSS);

	DWORD	GetSerialPort() { return m_dwSerialPort; }
	void	SetSerialPort(DWORD dwSerialPort) { m_dwSerialPort = dwSerialPort; }

	BYTE __stdcall CommCommand(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommControl(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommDipSw(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommReceive(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommStatus(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);
	BYTE __stdcall CommTransmit(WORD pc, BYTE addr, BYTE bWrite, BYTE d, ULONG nCyclesLeft);

private:
	void			UpdateCommState();
	BOOL			CheckComm();
	void			CloseComm();
	void			CheckCommEvent(DWORD dwEvtMask);
	static DWORD WINAPI	CommThread(LPVOID lpParameter);
	bool			CommThInit();
	void			CommThUninit();

	//

private:
	DWORD  m_dwSerialPort;

	DWORD  m_dwBaudRate;
	BYTE   m_uByteSize;
	BYTE   m_uCommandByte;
	HANDLE m_hCommHandle;
	DWORD  m_dwCommInactivity;
	BYTE   m_uControlByte;
	BYTE   m_uParity;
	BYTE   m_uStopBits;

	//

	CRITICAL_SECTION	m_CriticalSection;	// To guard /g_vRecvBytes/
	BYTE				m_RecvBuffer[uRecvBufferSize];	// NB: More work required if >1 is used
	volatile DWORD		m_vRecvBytes;

	//

	bool m_bTxIrqEnabled;
	bool m_bRxIrqEnabled;

	bool m_bWrittenTx;

	//

	volatile bool m_vbCommIRQ;
	HANDLE m_hCommThread;

	HANDLE m_hCommEvent[COMMEVT_MAX];
	OVERLAPPED m_o;
};
