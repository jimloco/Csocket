#include <Csocket.h>
#include <sys/wait.h>
#include <stdio.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

class CRecSock : public Csock
{
public:
	CRecSock( int itimeout = 60 ) : Csock( itimeout ) { m_bAllDataOK = false; }
	CRecSock( const std::string & sHostname, uint16_t uPort, int itimeout = 60 ) : Csock( sHostname, uPort, itimeout ) { m_bAllDataOK = false; }
	virtual ~CRecSock()
	{
		if( GetType() == Csock::INBOUND )
		{
			cerr << "\tEverything is ok" << endl;
			assert( m_bAllDataOK );
		}
	}
	virtual bool ConnectionFrom( const std::string & sHost, uint16_t iPort )
	{
		cerr << "CRecSock: Connection from: " << sHost << ":" << iPort << endl;
		Close();
		return( true );
	}
	virtual void ReadData( const char *data, size_t len )
	{
		m_sRecData.append( data, len );
		cerr << "CRecSock: Got " << len << " bytes." << endl;
	}
	virtual void Disconnected()
	{
		cerr << "CRecSock: Disconnected, wrapping up!" << endl;
		std::string sTestData;
		FILE * pFile = fopen( "ReceiveTest.cc", "r" );
		assert( pFile );
		char szBuff[1024];
		size_t uBytes = 0;
		while(( uBytes = fread( szBuff, sizeof( char ), 1024, pFile ) ) > 0 )
			sTestData.append( szBuff, uBytes );
		assert( m_sRecData.size() );
		assert( sTestData.size() );
		assert( sTestData == m_sRecData );
		m_bAllDataOK = true;
	}
private:
	std::string	m_sRecData;
	bool	m_bAllDataOK;
};

int main( int argc, char **argv )
{
	InitCsocket();
	signal( SIGPIPE, SIG_IGN );

	for( int iType = 0; iType < 4; ++iType )
	{
		cerr << "------------ New Test -------------" << endl;
		bool bIsIPv6 = ( iType == 1 || iType == 3 );
		bool bIsSSL = ( iType == 2 || iType == 3 );
		if( bIsIPv6 )
			cerr << "Testing IPv6!" << endl;
		if( bIsSSL )
			cerr << "Testing SSL!" << endl;

		TSocketManager<CRecSock> cManager;
		std::string sPemFile = bIsIPv6 ? "ReceiveTest6.pem" : "ReceiveTest.pem";
		CSListener cListen( 0, bIsIPv6 ? "::1" : "127.0.0.1" );
		if( bIsSSL )
		{
			cListen.SetIsSSL( true );
			cListen.SetPemLocation( sPemFile );
		}
		cListen.SetTimeout( 5 );
		uint16_t uPort = 0;
		assert( cManager.Listen( cListen, NULL, &uPort ) );
		cerr << "Listening on port: " << uPort << endl;
		assert( uPort > 0 );
		for( int iInit = 0; iInit < 5; ++iInit )
			cManager.Loop();
		assert( cManager.size() == 1 );
		if( fork() == 0 )
		{
			// shutting down the manager in the child thread
			cManager.at( 0 )->Dereference();
			cManager.clear();
			std::ostringstream ossArgs;
			ossArgs << "ncat -vvvv ";
			if( bIsSSL )
				ossArgs << "--ssl-trustfile " << sPemFile << " --ssl ";
			if( bIsIPv6 )
				ossArgs << "-6 ::1 ";
			else
				ossArgs << "-4 127.0.0.1 ";
			ossArgs << uPort;
			ossArgs << " --send-only < ReceiveTest.cc > ReceiveTest.txt.out 2>&1";
			cerr << "ncat command: " << ossArgs.str() << endl;
			if( 0xffff & system( ossArgs.str().c_str() ) )
			{
				cerr << "ncat failed!!!" << endl;
				return( 1 );
			}
			return( 0 );
		}
		else
		{
			cerr << "Start Looping" << endl;
			while( cManager.HasFDs() )
				cManager.Loop();
			cerr << "End Looping" << endl;
			int iStatus = 0;
			cerr << "Waiting for fork to exit" << endl;
			wait( &iStatus );
			cerr << "Exited with status " << iStatus << endl;
			assert( iStatus == 0 );
		}
	}

	ShutdownCsocket();
	return( 0 );
}
#pragma GCC diagnostic pop
