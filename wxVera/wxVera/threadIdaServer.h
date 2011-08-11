#ifndef _THREAD_IDA_SERVER_H_
#define _THREAD_IDA_SERVER_H_

#include "wxvera.h"
#include <vector>


using namespace std;

#define IDA_MSG_RECEIVED "THANK YOU SIR MAY I HAVE ANOTHER\n"
#define IDA_DEFAULT_PORT 2005
#define MAX_IDA_SERVER_MSG_SIZE 128
#define MAX_IDA_BACKLOG 5

#ifndef _WIN32
#define DWORD uint32_t
#define SOCKET int
#define ULONG unsigned long
#endif

typedef struct _SOCKET_INFORMATION {

   char Buffer[MAX_IDA_SERVER_MSG_SIZE];
   SOCKET Socket;
   DWORD BytesSEND;
   DWORD BytesRECV;

#ifdef _WIN32
   WSABUF DataBuf;
   OVERLAPPED Overlapped;
#endif
} SOCKET_INFORMATION, * LPSOCKET_INFORMATION;


class threadIdaServer :
	public wxThread
{
public:
	threadIdaServer(wxFrame *parentFrame, unsigned short defaultport=IDA_DEFAULT_PORT);
	~threadIdaServer(void);

	virtual void *		Entry();
	virtual void		OnExit();
	bool sendData(char *data, size_t len);

private:
	bool CreateSocketInformation(SOCKET s);
	void FreeSocketInformation(DWORD Index);
	void sendDoneEvent(wxString msg, int retCode);
	void sendRecvEvent (wxString *data);
	unsigned short port;
	DWORD i;
	DWORD Total;
	ULONG NonBlock;
	DWORD Flags;
	DWORD SendBytes;
	DWORD RecvBytes;
	DWORD TotalSockets;
	SOCKET ListenSocket;
	SOCKET AcceptSocket;
#ifdef _WIN32
	// Windows Specific stuff
	LPSOCKET_INFORMATION SocketArray[FD_SETSIZE];
	SOCKADDR_IN InternetAddr;
	FD_SET WriteSet;
	FD_SET ReadSet;
#else defined _GNUC
	struct sockaddr_in InternetAddr;
	fd_set WriteSet;
	fd_set ReadSet;
#endif
	timeval tv;
	wxFrame *m_parentFrame;
	wxMutex m_mutex;
	vector<wxString *> commandList;
};

#endif


