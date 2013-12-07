#include <Csocket.h>
#include <sys/wait.h>
#include <stdio.h>

int main( int argc, char **argv )
{
	InitCsocket();
	uint16_t uPort = 45545;
	if( argc > 1 )
		uPort = (uint16_t)atoi( argv[1] );
	for( int iType = 0; iType < 4; ++iType )
	{
		cerr << "------------ New Test ------------" << endl;
		bool bTestIPv6 = ( iType == 1 || iType == 3 );
		if( bTestIPv6 )
			cerr << "IPv6 Test now!" << endl;
		bool bTestSSL = ( iType == 2 || iType == 3 );
		if( bTestSSL )
			cerr << "Testing SSL!" << endl;
		unlink( "SendTest-Port.txt.out" );
		unlink( "SentTest.txt.out" );
		if( fork() == 0 )
		{
			std::ostringstream ossArgs;
			ossArgs << "ncat -l -o SentTest.txt.out -w 5 -v -p " << uPort << " ";
			if( bTestIPv6 )
				ossArgs << "-6 ";
			if( bTestSSL )
				ossArgs << "--ssl ";
			ossArgs << ">SendTest-Port.txt.out 2>&1";
			cerr << "Launching ncat with " << ossArgs.str() << endl;
			if( 0xffff & system( ossArgs.str().c_str() ) )
				return( 1 );
			return( 0 );
		}
		else
		{
			cerr << "Waiting for ncat to launch on fork" << endl;
			for( int iTry = 0; iTry < 5; ++iTry )
			{
				// fetch the port number its listening on
				FILE * pFile = fopen( "SendTest-Port.txt.out", "r" );
				char szBuff[1024];
				bool bReady = false;
				if( pFile )
				{
					while( !bReady && fgets( szBuff, 1024, pFile ) )
					{
						const char *pszNCatReady = "Ncat: Listening on";
						if( strncmp( pszNCatReady, szBuff, strlen( pszNCatReady ) ) == 0 )
							bReady = true;
					}
					fclose( pFile );
				}
				if( bReady )
					break;
				sleep( 1 );
			}
			cerr << "Found ncat listening on Port: " << uPort << endl;

			cerr << "Reading in SendTest.cc" << endl;
			CSocketManager cSockManager;
			FILE * pFile = fopen( "SendTest.cc", "r" );
			assert( pFile );
			std::string sBuffer;
			size_t uBytes = 0;
			char szBuff[1024];
			while( (uBytes = fread( szBuff, sizeof( char ), 1024, pFile )) > 0 )
				sBuffer.append( szBuff, uBytes );
			fclose( pFile );
			assert( sBuffer.size() );
			cerr << "Connecting to localhost @ " << uPort << endl;
			Csock * pSock = new Csock;
			CSConnection csConn( bTestIPv6 ? "::1" : "127.0.0.1", uPort );
			csConn.SetIsSSL( bTestSSL );
			csConn.SetBindHost( bTestIPv6 ? "::1" : "127.0.0.1" );
			cSockManager.Connect( csConn, pSock );
			pSock->Write( sBuffer );
			pSock->Close( Csock::CLT_AFTERWRITE );
			while( cSockManager.HasFDs() )
				cSockManager.Loop();
			cerr << "Sent file, waiting for fork to exit" << endl;
			int iStatus = 0;
			wait( &iStatus );
			assert( WIFEXITED( iStatus ) );

			cerr << "diffing source and dest" << endl;
			// verify the data
			assert( !( 0xffff & system( "diff -u SendTest.cc SentTest.txt.out" ) ) );
			cerr << "Success!" << endl;
		}
	}
	ShutdownCsocket();
	return( 0 );
}

