#include <Csocket.h>
#include <sys/wait.h>
#include <stdio.h>

using namespace std;

class CImapClient : public Csock
{
public:
	CImapClient( int itimeout = 60 ) : Csock( itimeout ) {}
	CImapClient( const string & sHostname, uint16_t uPort, int itimeout = 60 ) : Csock( sHostname, uPort, itimeout ) {}
	virtual void ReadData( const char * data, size_t len )
	{
		cout << "<<<< ";
		cout.write( data, len );
		string sFoo;
		sFoo.assign( data, len );
		if( sFoo.find( "CAPABILITY" ) != string::npos )
		{
			Write( "a001 STARTTLS\n" );
		}
		else if( sFoo.find( "STARTTLS completed" ) != string::npos )
		{
			StartTLS(); // start the client tls connection
			Write( "HELP\n" );
		}
		else if( sFoo.find( "HELP BAD" ) != string::npos )
		{
			string sFinger;
			GetPeerFingerprint( sFinger );
			cerr << "FingerPrint: " << sFinger << endl;
			Write( "AUTH LOGOUT\n" );
		}
	}

	using Csock::Write;
	virtual bool Write( const string & sData )
	{
		if( sData.size() )
			cout << ">>>> " << sData;
		return( Csock::Write( sData ) );
	}
};

int main( int argc, char **argv )
{
	InitCsocket();
	TSocketManager< CImapClient > cManager;
	cManager.Connect( CSConnection( "127.0.0.1", 143 ) );
	while( cManager.HasFDs() )
		cManager.Loop();

	ShutdownCsocket();
	return( 0 );
}
