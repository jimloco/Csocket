#include <Csocket.h>
#include <sys/wait.h>
#include <stdio.h>

#define CMD_KEY_DSA "openssl dsaparam -genkey -out %1$s 1024"
#define CMD_KEY_RSA "openssl genrsa -out %1$s 2048"
#define CMD_KEY_EC  "openssl ecparam -genkey -out %1$s -name prime256v1"
#define CMD_CERT    "openssl req -new -x509 -days 3650 -nodes -batch -sha256 -key %1$s -keyout %1$s -out %1$s"
#define CMD_DHPARAM "[ -f %1$s ] || openssl dhparam 1024 >%1$s; cat %1$s >>%2$s"
#define CMD_MODULUS "openssl x509 -in %1$s -noout -modulus | sed 's/^Modulus=//;s/^0\\+//'"
#define CMD_FINGER  "openssl x509 -in %1$s -outform DER | openssl dgst -%2$s -hex | sed 's/^(stdin)= //'"
#define CMD_SERVER  "openssl s_server -msg -cert %1$s -cipher '%2$s' -serverpref -accept %3$u >%1$s-clients.out"
#define CMD_CLIENT  "openssl s_client -msg -cert %1$s -cipher '%2$s' -connect %3$s:%4$u >%1$s-%5$s_server.out 2>&1 </dev/null"

#ifdef OPENSSL_VERSION_NUMBER
# if OPENSSL_VERSION_NUMBER < 0x10001000
#  define OPENSSL_NO_TLS1_1
#  define OPENSSL_NO_TLS1_2
# endif
# if OPENSSL_VERSION_NUMBER < 0x10100000 || defined( LIBRESSL_VERSION_NUMBER )
#  define SSL_SESSION_get_protocol_version( session )   ( ( session )->ssl_version )
#  define SSL_SESSION_get0_cipher( session )            ( ( session )->cipher )
# endif
#endif /* OPENSSL_VERSION_NUMBER */

#ifdef _WIN32
#define ANSI_RED     ""
#define ANSI_GREEN   ""
#define ANSI_CYAN    ""
#define ANSI_RESET   ""
#else
#define ANSI_RED     "\x1b[31m"
#define ANSI_GREEN   "\x1b[32m"
#define ANSI_CYAN    "\x1b[36m"
#define ANSI_RESET   "\x1b[0m"
#endif /* _WIN32 */

const char *pCiphers = "ALL:!aNULL:!SSLv2:@STRENGTH"; // DSA doesn't work with openssl 1.1's DEFAULT ciphers
typedef std::set<const char *>::const_iterator finger_iter;
const std::set<const char *> g_sFingerAlgo { "SHA1", }; // Fingerprint algorithms to test
typedef std::map<const char *, const char *>::const_iterator keyalgo_iter;
const std::map<const char *, const char *> g_mKeyGenCmd { // Certificate types to test
	{ "DSA", CMD_KEY_DSA },
	{ "RSA", CMD_KEY_RSA },
	{ "EC",  CMD_KEY_EC },
};
typedef std::map<Csock::ESSLMethod, const char *>::const_iterator method_iter;
const std::map<Csock::ESSLMethod, const char *> g_mMethodName { // Protocol versions to test
	{ Csock::SSL23, "flexible" },
	{ Csock::SSL3,  "SSLv3" },
	{ Csock::TLS1,  "TLSv1" },
	{ Csock::TLS11, "TLSv1.1" },
	{ Csock::TLS12, "TLSv1.2" },
};
std::map<Csock::ESSLMethod, int> g_mExpectVersion;
int g_iTestsTotal = 0, g_iTestsFailed = 0;

static CS_STRING Sprintf( const char *pFormat, ... )
{
	char *pResult;
	va_list pArgs;
	va_start( pArgs, pFormat );
	int iResult = vasprintf( &pResult, pFormat, pArgs );
	va_end( pArgs );
	CS_STRING sResult( iResult < 0 ? NULL : pResult );
	free( pResult );
	return sResult;
}

class Test
{
	template <typename T>
	bool Equal( const T &expect, const T &actual ) const { return( expect == actual ); }
	bool Equal( const CS_STRING &expect, const CS_STRING &actual ) const { return( expect.compare( actual ) == 0 ); }
	const CS_STRING m_sName;
	bool m_bPassed, m_bFailed;

public:
	Test( const CS_STRING &name ) : m_sName( name ), m_bPassed( false ), m_bFailed( false )
	{
		cout << endl << ANSI_CYAN << " === TEST #" << ++g_iTestsTotal << ANSI_RESET << ": " << m_sName << endl;
	}

	~Test()
	{
		if( m_bPassed && !m_bFailed ) // needs to have had at least 1 good and no bad results
			cout << ANSI_GREEN << " --> PASS #" << g_iTestsTotal << ANSI_RESET << ": " << m_sName << endl;
		else
		{
			++g_iTestsFailed;
			cerr << ANSI_RED << " XXX FAIL #" << g_iTestsTotal << ANSI_RESET << ": " << m_sName << endl;
		}
	}

	std::ostream& Good()
	{
		m_bPassed = true;
		return( cout << ANSI_GREEN << " *** " << ANSI_RESET );
	}

	std::ostream& Bad()
	{
		m_bFailed = true;
		return( cerr << ANSI_RED << " !!! " << ANSI_RESET );
	}

	template <typename T>
	void Equal( const char *desc, const T &expect, const T &actual )
	{
		if( Equal( expect, actual ) )
			Good() << "matched " << desc << endl;
		else
			Bad() << "unexpected " << desc << "! Expected: " << expect << ", Actual: " << actual << endl;
	}
};

class ForkProcess
{
	FILE *m_pOut;
	pid_t m_pid;

public:
	ForkProcess( const CS_STRING &cmd, bool bRead = false ) : m_pOut( NULL )
	{ // bRead=true will read all output until EOF before sending SIGTERM, risk of hanging if child holds stdout open
		int pPipeOut[2];
		if( bRead )
			assert( pipe( pPipeOut ) == 0 );
		m_pid = fork();
		assert( m_pid >= 0 );
		if( m_pid == 0 )
		{ // child
			cout << " ::: " << cmd << endl;
			if( bRead )
			{
				assert( close( pPipeOut[0] ) == 0 );
				assert( dup2( pPipeOut[1], STDOUT_FILENO ) >= 0 );
				assert( close( pPipeOut[1] ) == 0 );
			}
			assert( setpgid( 0, 0 ) == 0 ); // make sure shell's children are in the shell's process group
			execlp( "sh", "sh", "-c", cmd.c_str(), NULL ); // use a shell to keep so many things simple
			perror( "exec() failed" );
			abort();
		}
		if( bRead && close( pPipeOut[1] ) != 0 )
			perror( "close() write end of output pipe failed" );
		if( bRead )
			m_pOut = fdopen( pPipeOut[0], "r" );
	}

	~ForkProcess()
	{
		if( m_pOut != NULL )
			Read();
		if( m_pOut != NULL && fclose( m_pOut ) != 0 )
			perror( "close() read end of output pipe failed" );
		kill( -1*m_pid, SIGTERM ); // stop all processes in the child's process group
		while( waitpid( -1*m_pid, NULL, 0 ) > 0 ); // intentionally empty loop
	}

	CS_STRING Read() const
	{ // reads all output until EOF, risk of hanging if child holds stdout open
		assert( m_pOut != NULL );
		CS_STRING sResult;
		size_t uBytes = 0;
		char szBuff[1024];
		while( ( uBytes = fread( szBuff, sizeof( char ), 1024, m_pOut ) ) > 0 )
			sResult.append( szBuff, uBytes );
		int iLen = sResult.size();
		if( iLen > 0 && sResult[iLen - 1] == '\n' )
			sResult.erase( iLen - 1, 1 );
		if( sResult.size() > 0 )
			cout << " <<< " << sResult << endl;
		return sResult;
	}
};

class Expectations
{
	CS_STRING m_sPubKey;
	std::map<const char *, CS_STRING> m_mFingerprints;

public:
	Expectations( const char *pPemName )
	{
		cout << "Calculating expected public key..." << endl;
		m_sPubKey = ForkProcess( Sprintf( CMD_MODULUS, pPemName ), true ).Read();
		for( finger_iter iter = g_sFingerAlgo.begin(); iter != g_sFingerAlgo.end(); ++iter )
		{
			cout << "Calculating expected " << *iter << " fingerprint..." << endl;
			m_mFingerprints[*iter] = ForkProcess( Sprintf( CMD_FINGER, pPemName, *iter ), true ).Read();
		}
	}

	CS_STRING GetPubKey() const { return( m_sPubKey ); }
	CS_STRING GetFinger( const char *algo ) const { return( m_mFingerprints.find( algo )->second ); }
};

class TestSock : public Csock
{
	const Expectations *m_pExpect;
	Test *m_pTest;
	ForkProcess *m_pChild;
	TestSock *m_pParent;
	bool m_bConnected;

	TestSock( const Expectations *e, Test *t, TestSock *p, const std::string & sHostname, uint16_t uPort ) :
		Csock( sHostname, uPort, 5 ), m_pExpect( e ), m_pTest( t ), m_pChild( NULL ), m_pParent( p ) {}

public:
	TestSock( int iTimeout = 5 ) : Csock( iTimeout ) {}
	TestSock( const std::string & sHostname, uint16_t uPort, int iTimeout = 5 ) :
		Csock( sHostname, uPort, iTimeout ) {}
	TestSock( const Expectations *e, Test *t ) :
		Csock( 5 ), m_pExpect( e ), m_pTest( t ), m_pChild( NULL ), m_pParent( NULL ) { m_bConnected = false; }

	virtual ~TestSock()
	{
		if( GetType() == INBOUND )
			m_pParent->Close( CLT_AFTERWRITE );
		else if( !m_bConnected )
			m_pTest->Bad() << "didn't connect" << endl;
		CS_Delete( m_pChild );
	}

	virtual void Listening( const CS_STRING & sBindIP, uint16_t uPort )
	{
		const char *pPemName = GetPemLocation().c_str();
		const char *pMethod = g_mMethodName.find( ( ESSLMethod )GetSSLMethod() )->second;
		cout << "Starting external client..." << endl;
		m_pChild = new ForkProcess( Sprintf( CMD_CLIENT, pPemName, pCiphers, sBindIP.c_str(), uPort, pMethod ) );
	}

	virtual Csock *GetSockObj( const CS_STRING & sHostname, uint16_t uPort )
	{
		if( GetType() != LISTENER )
			return( NULL );
		TestSock *pSock = new TestSock( m_pExpect, m_pTest, this, sHostname, uPort );
		pSock->SetSSLMethod( GetSSLMethod() );
		pSock->FollowSSLCipherServerPreference();
		return( pSock );
	}

	virtual void SSLHandShakeFinished()
	{
		Close( CLT_AFTERWRITE );
		const SSL_SESSION *pSession = GetSSLSession();
		if( !pSession )
			return;

		if( GetType() == INBOUND )
			m_pParent->m_bConnected = true;
		else
			m_bConnected = true;

		const char *pCipherName = SSL_CIPHER_get_name( SSL_SESSION_get0_cipher( pSession ) );
		m_pTest->Good() << "connected:\t" << SSL_get_version( GetSSLObject() ) << '\t' << pCipherName << endl;

		int iVer = SSL_SESSION_get_protocol_version( pSession );
		ESSLMethod iMethod = ( ESSLMethod )GetSSLMethod();
		if( g_mExpectVersion.find( iMethod ) == g_mExpectVersion.end() )
			m_pTest->Equal( "fallback protocol version", g_mExpectVersion[SSL23], iVer );
		else
			m_pTest->Equal( "protocol version", g_mExpectVersion[iMethod], iVer );

		const CS_STRING sExpectPubKey( m_pExpect->GetPubKey() );
		if( sExpectPubKey.compare( "Wrong Algorithm type" ) != 0 ) // openssl doesn't know how to do this for EC
		{                                                          // (and neither does Csocket)
			CS_STRING sPubKey( GetPeerPubKey() );
			sPubKey.erase( 0, sPubKey.find_first_not_of( "0" ) ); // openssl's output trims leading 0's
			m_pTest->Equal( "public key", sExpectPubKey, sPubKey );
		}

		CS_STRING sFinger;
		for( finger_iter iter = g_sFingerAlgo.begin(); iter != g_sFingerAlgo.end(); ++iter )
		{
			GetPeerFingerprint( sFinger ); // TODO #65: Csocket support for other fingerprint algorithms
			m_pTest->Equal( Sprintf( "%s fingerprint", *iter ).c_str(), m_pExpect->GetFinger( *iter ), sFinger );
		}
	}

	virtual void SockError( int iError, const std::string & sDescription )
	{
		cerr << "[Error " << iError << "] " << sDescription << endl;
	}

	virtual void ConnectionRefused() { cerr << "[Error] Connection refused" << endl; }
};

static void RunClientTests( const Expectations &expect, const char *pKeyAlgo, const char *pPemName, uint16_t uPort )
{
	TSocketManager<TestSock> cManager;
	CSSSLConnection csConn( "127.0.0.1", uPort, 5 );
	csConn.SetCipher( pCiphers );
	cout << "Starting external server..." << endl;
	ForkProcess pProcess( Sprintf( CMD_SERVER, pPemName, pCiphers, uPort ) );
	sleep( 1 ); // Note: Can't rely on s_server to say it's listening ("ACCEPT" is printed before listening starts)
	// or even flush output (openssl 0.9.8 doesn't), therefore just sleep instead of trying to parse output.
	// Also can't rely on "Q" command to stop it (if received before client connects then it quits before handshake
	// completes, if received after client disconnects then it doesn't quit until another client tries to connect)
	// and only recently supports "-naccept" (older versions just print usage text), therefore just send SIGTERM.

	for( method_iter iter = g_mMethodName.begin(); iter != g_mMethodName.end(); ++iter )
	{
		Test t( Sprintf( "%s cert, %s Csocket client", pKeyAlgo, iter->second ) );
		TestSock *pSock = new TestSock( &expect, &t );
		pSock->SetSSLMethod( iter->first );
		cManager.Connect( csConn, pSock );
		while( cManager.HasFDs() )
			cManager.Loop();
	}
}

static void RunServerTests( const Expectations &expect, const char *pKeyAlgo, const char *pPemName )
{
	TSocketManager<TestSock> cManager;
	CSSSListener cListen( 0, "127.0.0.1" ); // fewer L's than expected in that name? :p
	cListen.SetTimeout( 5 );
	cListen.SetCipher( pCiphers );
	cListen.SetPemLocation( pPemName );
	cListen.SetRequireClientCertFlags( SSL_VERIFY_PEER|SSL_VERIFY_FAIL_IF_NO_PEER_CERT );

	for( method_iter iter = g_mMethodName.begin(); iter != g_mMethodName.end(); ++iter )
	{
		Test t( Sprintf( "%s cert, %s Csocket server", pKeyAlgo, iter->second ) );
		TestSock *pSock = new TestSock( &expect, &t );
		pSock->SetSSLMethod( iter->first );
		cManager.Listen( cListen, pSock );
		while( cManager.HasFDs() )
			cManager.Loop();
	}
}

int main( int argc, char **argv )
{
	InitCsocket();
	uint16_t uPort = 4433;
	if( argc > 1 )
		uPort = ( uint16_t )atoi( argv[1] );
	assert( uPort > 1023 );

	int iBestVersion = 0;
#ifndef OPENSSL_NO_SSL3
	g_mExpectVersion[Csock::SSL3] = iBestVersion = SSL3_VERSION;
#endif /* OPENSSL_NO_SSL3 */
#ifndef OPENSSL_NO_TLS1
	g_mExpectVersion[Csock::TLS1] = iBestVersion = TLS1_VERSION;
#endif /* OPENSSL_NO_TLS1 */
#ifndef OPENSSL_NO_TLS1_1
	g_mExpectVersion[Csock::TLS11] = iBestVersion = TLS1_1_VERSION;
#endif /* OPENSSL_NO_TLS1_1 */
#ifndef OPENSSL_NO_TLS1_2
	g_mExpectVersion[Csock::TLS12] = iBestVersion = TLS1_2_VERSION;
#endif /* OPENSSL_NO_TLS1_2 */
	g_mExpectVersion[Csock::SSL23] = iBestVersion;

	for( keyalgo_iter iter = g_mKeyGenCmd.begin(); iter != g_mKeyGenCmd.end(); ++iter )
	{
		const char *pPemName = Sprintf( "TLSTest-%s.pem", iter->first ).c_str();
		if( access( pPemName, R_OK ) != 0 )
		{
			cout << "Generating " << iter->first << " private key..." << endl;
			ForkProcess( Sprintf( iter->second, pPemName ), true );
			cout << "Generating self-signed " << iter->first << " certificate..." << endl;
			ForkProcess( Sprintf( CMD_CERT, pPemName ), true );
			cout << "Appending Diffie-Hellman parameters (generating if necessary)..." << endl;
			ForkProcess( Sprintf( CMD_DHPARAM, "TLSTest-dhparam.pem", pPemName ), true );
		}
		assert( access( pPemName, R_OK ) == 0 );
		Expectations expect( pPemName );
		RunClientTests( expect, iter->first, pPemName, uPort );
		RunServerTests( expect, iter->first, pPemName );
		cout << endl;
	}

	if( g_iTestsFailed == 0 )
		cout << "Passed all " << g_iTestsTotal << " tests." << endl;
	else
		cout << "Failed " << g_iTestsFailed << " of " << g_iTestsTotal << " tests!" << endl;
	ShutdownCsocket();
	return( g_iTestsFailed );
}
