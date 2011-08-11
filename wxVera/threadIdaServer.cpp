#include "wxvera.h"
#include "threadIdaServer.h"

/* This isn't finished yet. 
 Adapting from this website: 
 http://www.winsocketdotnetworkprogramming.com/winsock2programming/winsock2advancediomethod5a.html 
 
 */
bool threadIdaServer::sendData(char *data, size_t len)
{
	m_mutex.Lock();

	commandList.push_back(new wxString(data));

	m_mutex.Unlock();
	return true;
}

threadIdaServer::threadIdaServer(wxFrame *parentFrame, unsigned short defaultport)
{
	port = defaultport;
	TotalSockets = 0;

	m_parentFrame = parentFrame;

#ifdef _WIN32
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2,2), &wsaData);
#endif
}

void threadIdaServer::sendDoneEvent(wxString msg, int retCode)
{
	// Tell the parent frame we're done so it can load the GUI
	wxCommandEvent DoneEvent( wxEVT_COMMAND_BUTTON_CLICKED );

	// Put the save file name in the event for later display 
	DoneEvent.SetString(msg);
	DoneEvent.SetInt(retCode);
	
	if (m_parentFrame)
	{
		wxMutexGuiEnter();
		wxPostEvent(m_parentFrame, DoneEvent);
		wxMutexGuiLeave();
	}
}

void threadIdaServer::sendRecvEvent(wxString *data)
{
	wxCommandEvent DoneEvent( wxEVT_COMMAND_BUTTON_CLICKED );

	DoneEvent.SetString(*data);
	DoneEvent.SetInt(THREAD_IDA_MSG_RECEIVED);

	if (m_parentFrame)
	{
		wxMutexGuiEnter();
		wxPostEvent(m_parentFrame, DoneEvent);
		wxMutexGuiLeave();
	}

	delete data;
}


void *threadIdaServer::Entry()
{

#ifdef _WIN32
	if ((ListenSocket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED)) == INVALID_SOCKET)
	{
		wxString retString = wxString::Format(wxT("Error creating socket: %d"), WSAGetLastError());
		this->sendDoneEvent(retString, THREAD_IDA_FINISHED_ERROR);

		return NULL;
	}
#endif

	InternetAddr.sin_family = AF_INET;
	InternetAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	InternetAddr.sin_port = htons(port);

	if(bind(ListenSocket, (sockaddr *) &InternetAddr, sizeof(InternetAddr)) == SOCKET_ERROR)
	{
		wxMutexGuiEnter();
		wxLogDebug(wxT("bind error: %d"), WSAGetLastError());
		wxMutexGuiLeave();

		return NULL;
	}

	if(listen(ListenSocket, MAX_IDA_BACKLOG))
	{
		wxMutexGuiEnter();
		wxLogDebug(wxT("listen error: %d"), WSAGetLastError());
		wxMutexGuiLeave();

		return NULL;
	}

	NonBlock = 1;
	if (ioctlsocket(ListenSocket, FIONBIO, &NonBlock) == SOCKET_ERROR)
	{
		wxMutexGuiEnter();
		wxLogDebug(wxT("Could not set non-blocking mode: %d"), WSAGetLastError());
		wxMutexGuiLeave();

		return NULL;
	}

	// Set the timeout value
	tv.tv_sec = 1;
	tv.tv_usec = 0;

	wxLogDebug(wxT("IDA Server started"));

	while(1)
	{
		if (this->TestDestroy() == true)
			return NULL;

		FD_ZERO(&ReadSet);
		FD_ZERO(&WriteSet);

		FD_SET(ListenSocket, &ReadSet);

		// Get the items from the list
		m_mutex.Lock();
		DWORD numItems = commandList.size();
		vector<wxString *> commandsToSend;
		for (i = 0 ; i < numItems ; i++)
		{
			commandsToSend.push_back(commandList.back());
			commandList.pop_back();
		}
		m_mutex.Unlock();

		for(i = 0 ; i < TotalSockets ; i++)
		{
			// If we have items to transmit, send them, otherwise check for data to read
			if (commandsToSend.size() > 0)
				FD_SET(SocketArray[i]->Socket, &WriteSet);
			else
				FD_SET(SocketArray[i]->Socket, &ReadSet);
		}

		if ((Total = select(0, &ReadSet, &WriteSet, NULL, &tv)) == SOCKET_ERROR)
		{
			wxMutexGuiEnter();
			wxLogDebug(wxT("Select error: %d"), WSAGetLastError());
			wxMutexGuiLeave();

			return NULL;
		}

		if(FD_ISSET(ListenSocket, &ReadSet))
		{
			Total--;

			if((AcceptSocket = accept(ListenSocket, NULL, NULL)) != INVALID_SOCKET)
			{
				wxMutexGuiEnter();
				wxLogDebug(wxT("Accepted a connection"));
				wxMutexGuiLeave();

				NonBlock = 1;

				if(ioctlsocket(AcceptSocket, FIONBIO, &NonBlock) == SOCKET_ERROR)
				{
					wxMutexGuiEnter();
					wxLogDebug(wxT("Error setting socket to non-blocked: %d"), WSAGetLastError());
					wxMutexGuiLeave();

					return NULL;
				}

				if(CreateSocketInformation(AcceptSocket) == FALSE)
				{
					wxMutexGuiEnter();
					wxLogDebug(wxT("CreateSocketInformation error: %d"), WSAGetLastError());
					wxMutexGuiLeave();
				}

				// All is ok if we get here.
			}
			else
			{
				if (WSAGetLastError() != WSAEWOULDBLOCK)
				{
					wxMutexGuiEnter();
					wxLogDebug(wxT("Error with accept: %d"), WSAGetLastError());
					wxMutexGuiLeave();
				}
			}
		}

		// Check the sockets for information
		for (i = 0; Total > 0 && i < TotalSockets;i++)
		{
			LPSOCKET_INFORMATION SocketInfo = SocketArray[i];

			if(FD_ISSET(SocketInfo->Socket, &ReadSet))
			{
				Total--;

				SocketInfo->DataBuf.buf = SocketInfo->Buffer;
				SocketInfo->DataBuf.len = MAX_IDA_SERVER_MSG_SIZE;

				Flags = 0;
				
				// The SocketInfo->DataBuf WSABUF structure should limit the size of received data here
				// according to MSDN http://msdn.microsoft.com/en-us/library/ms741688%28VS.85%29.aspx

#ifdef _WIN32
				if(WSARecv(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &RecvBytes, &Flags, NULL, NULL) == SOCKET_ERROR)
				{
					if(WSAGetLastError() != WSAEWOULDBLOCK)
					{
						wxMutexGuiEnter();
						wxLogDebug(wxT("Error with recv: %d"), WSAGetLastError());
						wxMutexGuiLeave();

						FreeSocketInformation(i);
						continue;
					}
				}
#else defined _GNUC
				if (false)
				{
					// This is a stub for compilation
				}
#endif
				
				else
				{
					SocketInfo->BytesRECV = RecvBytes;

					// If zero bytes are received this indicates the peer closed the connection.
					if (RecvBytes == 0)
					{
						wxMutexGuiEnter();
						wxLogDebug(wxT("Socket disconnect"));
						wxMutexGuiLeave();

						FreeSocketInformation(i);
						continue;
					}

					size_t len = strlen(SocketInfo->Buffer);
					
					// Trim off the carriage return
					if (len < MAX_IDA_SERVER_MSG_SIZE && len > 0 && SocketInfo->Buffer[len-1] == '\n')
						SocketInfo->Buffer[len-1] = 0;

					// Process the buffer here
					this->sendRecvEvent(new wxString(SocketInfo->Buffer, RecvBytes));

					// Clear the buffer as we're done with it
					memset(SocketInfo->Buffer, 0, sizeof(SocketInfo->Buffer));

				}
			} // End read check

			// If the write set is marked then buffers are ready for more data
			if(FD_ISSET(SocketInfo->Socket, &WriteSet))
			{
				Total--;

				if (commandsToSend.size() > 0)
				{
					wxString *cmd = commandsToSend.back();
					commandsToSend.pop_back();

					// Convert to a C string
					char *cmd_c = (char *) cmd->mb_str(wxConvUTF8);
					size_t len  = strlen(cmd_c);

					// Make sure we only send the proper buffer size
					len = (MAX_IDA_SERVER_MSG_SIZE < len) ? MAX_IDA_SERVER_MSG_SIZE : len;
					
					memcpy(SocketInfo->Buffer, cmd_c, len);

					// We're done with the cmd string now, so free it
					delete cmd;
					cmd = NULL;
					
					SocketInfo->DataBuf.buf = SocketInfo->Buffer;
					SocketInfo->DataBuf.len = len;
				}
				else
				{
					wxMutexGuiEnter();
					wxLogDebug(wxT("Send set with no data available! ERROR"));
					wxMutexGuiLeave();
				}

				if(WSASend(SocketInfo->Socket, &(SocketInfo->DataBuf), 1, &SendBytes, 0, NULL, NULL) == SOCKET_ERROR)
				{
					if(WSAGetLastError() != WSAEWOULDBLOCK)
					{
						wxMutexGuiEnter();
						wxLogDebug(wxT("WSA Send error"));
						wxMutexGuiLeave();

						FreeSocketInformation(i);
					}
					else
					{
						wxMutexGuiEnter();
						wxLogDebug(wxT("WSASend() ok"));
						wxMutexGuiLeave();
					}
					
					continue;
				}
				else
				{
					SocketInfo->BytesSEND += SendBytes;

					if(SocketInfo->BytesSEND == SocketInfo->BytesRECV)
					{
						SocketInfo->BytesSEND = 0;
						SocketInfo->BytesRECV = 0;
					}
				} // End Send
			} // End Write check
		} // End socket for loop
	} // End while loop

	return NULL;
}

void threadIdaServer::OnExit(void)
{
	// Called when the thread exits by termination or with Delete()
	// but not Kill()ed
	wxMutexGuiEnter();
	wxLogDebug(wxT("Exiting"));
	wxMutexGuiLeave();

	closesocket(ListenSocket);

	for(i = 0 ; i < TotalSockets ; i++)
		FreeSocketInformation(i);

	this->sendDoneEvent(wxT("Done"), THREAD_IDA_FINISHED_NOERROR);


}

threadIdaServer::~threadIdaServer(void)
{
	
	WSACleanup();
}

bool threadIdaServer::CreateSocketInformation(SOCKET s)

{

   LPSOCKET_INFORMATION SI;

   printf("Accepted socket number %d\n", s);

   if ((SI = (LPSOCKET_INFORMATION) GlobalAlloc(GPTR, sizeof(SOCKET_INFORMATION))) == NULL)
   {
      printf("GlobalAlloc() failed with error %d\n", GetLastError());
      return FALSE;
   }
   else
      printf("GlobalAlloc() for SOCKET_INFORMATION is OK!\n");

 

   // Prepare SocketInfo structure for use

   SI->Socket = s;
   SI->BytesSEND = 0;
   SI->BytesRECV = 0;

   SocketArray[TotalSockets] = SI;
   TotalSockets++;
   return(TRUE);

}

 

void threadIdaServer::FreeSocketInformation(DWORD Index)
{

   LPSOCKET_INFORMATION SI = SocketArray[Index];
   DWORD i;

   closesocket(SI->Socket);
   printf("Closing socket number %d\n", SI->Socket);
   GlobalFree(SI);

   // Squash the socket array
   for (i = Index; i < TotalSockets; i++)
   {
      SocketArray[i] = SocketArray[i + 1];
   }

   TotalSockets--;
}
