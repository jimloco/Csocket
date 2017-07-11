#include <string.h>
#include <Csocket.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <getopt.h>


// forward declarations
class LConn;
class MySock;
///////////

class CTimeoutLogin : public CCron
{
public:
	CTimeoutLogin() : CCron()
	{
		m_pcSock = NULL;
	}
	virtual ~CTimeoutLogin() {}

	void SetSock( MySock *pcSock ) { m_pcSock = pcSock; }

protected:
	void RunJob();
	MySock		*m_pcSock;

};

class MySock : public Csock
{
public:
	MySock() : Csock() { EnableReadLine(); m_pcParent = NULL; }
	MySock( const std::string & sHostname, uint16_t iport, int itimeout = 60 ) :
		Csock( sHostname, iport, itimeout ) { EnableReadLine(); m_pcParent = NULL; }

	virtual ~MySock() {}


	virtual void SetParent( LConn *pcParent ) { m_pcParent = pcParent; }
	virtual void WriteAll( const std::string & sLine );
	virtual void ReadLine( const std::string & sLine );
	virtual void Disconnected()
	{
		if( GetType() == INBOUND )
		{
			if( !m_sName.empty() )
			{
				WriteAll( "[" + m_sName + "] Has Left the party line.\n" );
			}
			WriteAll( "Lost connection from " + GetHostName() + "\n" );

		}
	}

	std::string & GetNick() { return( m_sName ); }

	virtual void Connected()
	{
		if( GetType() == INBOUND )
		{
			// set the job
			CTimeoutLogin *pcCron = new CTimeoutLogin();
			pcCron->SetSock( this );
			pcCron->Start( 10 );	// check to see if we're logged-in, in 10 seconds

			AddCron( pcCron );
			Write( "Hello, Whats Your Nick ?\n" );
		}
	}

	virtual void SockError( int iError, const std::string & sDescription )
	{
		cerr << "ACK [" << iError << "] " << sDescription << endl;
	}

	virtual bool ConnectionFrom( const std::string & sHost, uint16_t iPort )
	{
		std::ostringstream ossTmp;
		ossTmp << "Connection from " << sHost << ":" << iPort << endl;
		WriteAll( ossTmp.str() );
		return( true );
	}
	virtual void Listening( const std::string & sBindIP, uint16_t uPort )
	{
		cerr << "Listening: " << sBindIP << ":" << uPort << endl;
	}

private:
	LConn		*m_pcParent;
	std::string		m_sName;

};

class LConn : public TSocketManager<MySock>
{
public:
	LConn() : TSocketManager<MySock>() {}
	virtual ~LConn() {}


	virtual void AddSock( Csock *pcSock, const std::string & sSockName )
	{
		pcSock->SetSockName( sSockName );
		(( MySock * )pcSock )->SetParent( this );
		push_back( pcSock );
	}
};


void MySock::WriteAll( const std::string & sLine )
{
	for( size_t i = 0; i < m_pcParent->size(); i++ )
	{
		if((( *m_pcParent )[i] != this ) && (( *m_pcParent )[i]->GetType() == MySock::INBOUND ) )
			if( !(( MySock * )( *m_pcParent )[i] )->GetNick().empty() )
				( *m_pcParent )[i]->Write( sLine );
	}
}

void MySock::ReadLine( const std::string & sLine )
{
	if( GetType() == INBOUND )
	{
		if( m_sName.empty() )
		{
			m_sName = sLine.substr( 0, 9 );
			while( m_sName.size() && isspace( *m_sName.rbegin() ) )
				m_sName.erase( m_sName.size() - 1, 1 );
			SetTimeout( 0 );	// they logged, lets keep them that way
			WriteAll( "[" + m_sName + "] Has joined the partyline.\n" );
//			cerr << "Public Key: " << GetPeerPubKey() << endl;

		}
		else
		{
			if( sLine.compare( 0, 5, ".quit" ) == 0 )
			{
				Write( "Bye.\n" );
				Close();
				return;

			}
			else
				WriteAll( m_sName + ": " + sLine );
		}

	}
}

void CTimeoutLogin::RunJob()
{
	if( m_pcSock )
		if( m_pcSock->GetNick().empty() )
			m_pcSock->Close();
}

static struct option s_apOpts[] =
{
	{ "port", 					required_argument,	0, 0 },
	{ "bind-host", 				required_argument,	0, 0 },
	{ "enable-ssl",				no_argument,		0, 0 },
	{ "pem-file",				required_argument,	0, 0 },
	{ "require-client-cert",	required_argument,	0, 0 },
	{ NULL, 					0, 0, 0 }
};

int main( int argc, char **argv )
{
	InitCsocket();
	LConn cConn;

	int iRet = 0;
	int iOptIndex = 0;
	uint16_t uPort = 0;
	bool bEnableSSL = false;
	bool bReqClientCert = false;
	bool bDumpHelp = true;
	std::string sPemFile;
	std::string sBindHost;
	while(( iRet = getopt_long( argc, argv, "", s_apOpts, &iOptIndex ) ) >= 0 )
	{
		if( iRet == '?' )
			break;
		bDumpHelp = false;
		if( strcmp( s_apOpts[iOptIndex].name, "port" ) == 0 )
			uPort = ( uint16_t )atoi( optarg );
		else if( strcmp( s_apOpts[iOptIndex].name, "enable-ssl" ) == 0 )
			bEnableSSL = true;
		else if( strcmp( s_apOpts[iOptIndex].name, "pem-file" ) == 0 )
			sPemFile = optarg;
		else if( strcmp( s_apOpts[iOptIndex].name, "require-client-cert" ) == 0 )
			bReqClientCert = true;
		else if( strcmp( s_apOpts[iOptIndex].name, "bind-host" ) == 0 )
			sBindHost = optarg;
		else
		{
			cerr << "Beats me what you sent" << endl;
			bDumpHelp = true;
			break;
		}
	}
	if( uPort == 0 || bDumpHelp )
	{
		cerr << "Usage: " << argv[0] << " <options>" << endl;
		cerr << "--port <port to use>" << endl;
		cerr << "--bind-host <bind host to use>" << endl;
		cerr << "--pem-file <pemfile to use>" << endl;
		cerr << "--enable-ssl" << endl;
		cerr << "--require-client-cert" << endl;
		return( 1 );
	}
	else
	{
		cerr << "Creating Listening port: " << uPort << endl;
	}

	// sample ssl server compile with -D_HAS_SSL
	CSListener cListen(( uint16_t )uPort, sBindHost, true );
	cListen.SetSockName( "talk" );
	cListen.SetIsSSL( bEnableSSL );

#ifdef HAVE_LIBSSL
	if( cListen.GetIsSSL() )
	{
		cListen.SetCipher( "HIGH" );
		cListen.SetPemLocation( sPemFile );	// set this to your pem file location
		if( bReqClientCert )
		{
			cerr << "Requiring Client Certificate" << endl;
			cListen.SetRequiresClientCert( true );
		}
	}
#endif /* HAVE_LIBSSL */

	if( !cConn.Listen( cListen ) )
	{
		cerr << "Could not bind to port" << endl;
		exit( 1 );
	}

	while( cConn.size() )
		cConn.DynamicSelectLoop( 50000, 5000000 );

	ShutdownCsocket();

	return( 0 );
}






