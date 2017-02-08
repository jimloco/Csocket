#include <Csocket.h>

#ifdef HAVE_UNIX_SOCKET

static bool done = false;

class CEchoServer : public Csock
{
public:
	virtual void ReadData( const char * data, size_t len )
	{
		cout << "Echoing: ";
		cout.write( data, len );
		Write( data, len );
	}

	virtual void Disconnected( )
	{
		cout << "Client disconnected" << endl;
		done = true;
	}
};

class CEchoListener : public Csock
{
public:
	virtual void SockError( int iErrno, const CS_STRING & sDescription )
	{
		cerr << "Listener error: " << sDescription << endl;
	}

	virtual Csock *GetSockObj( const CS_STRING & sHostname, uint16_t iPort )
	{
		cout << "Incoming connection from " << sHostname << " on port " << iPort << endl;
		Close();
		return new CEchoServer();
	}
};

class CEchoClient : public Csock
{
public:
	virtual void Connected()
	{
		EnableReadLine();
		Write("Hello World!\n");
	}

	virtual void ReadLine( const CS_STRING & sLine )
	{
		if (sLine != "Hello World!\n")
			cerr << "Did not receive expected line: " << sLine << endl;
		Close();
	}
};

int main( int argc, char **argv )
{
	CS_STRING sPath = "echo";

	InitCsocket();
	TSocketManager< Csock > cManager;

	Csock *sock = new CEchoListener();
	if (!sock->ListenUnix(sPath))
	{
		cerr << "Failed to listen on '" << sPath << "'!" << endl;
		return 1;
	}
	cManager.AddSock(sock, "echo-listener");

	sock = new CEchoClient();
	if (!sock->ConnectUnix(sPath))
	{
		cerr << "Failed to connect to '" << sPath << "'!" << endl;
		return 1;
	}
	cManager.AddSock(sock, "echo-client");
	while( !done )
		cManager.Loop();

	ShutdownCsocket();
	return( 0 );
}

#else

int main( int argc, char **argv )
{
	cerr << "This program needs support for UNIX sockets" << endl;
	return 1;
}

#endif
