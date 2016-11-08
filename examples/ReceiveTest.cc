#include <Csocket.h>
#include <sys/wait.h>
#include <stdio.h>

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

static void CreatePem()
{
	cout << "Generating a private/public key pair" << endl;
	FILE *pOut = fopen( "ReceiveTest.pem", "w" );
	assert( pOut );
	EVP_PKEY *pKey = NULL;
	X509 *pCert = NULL;
	X509_NAME *pName = NULL;
	int days = 3650;
	u_int iSeed = ( u_int )time( NULL );
	int serial = ( rand_r( &iSeed ) % 9999 );
	BIGNUM * pBNE = BN_new();
	assert( BN_set_word( pBNE, RSA_F4 ) );
	RSA * pRSA = RSA_new();

	RSA_generate_key_ex( pRSA, 2048, pBNE, NULL ); // pRSA and pBNE are all cleaned up by pKey free
	assert( pKey = EVP_PKEY_new() );
	assert( EVP_PKEY_assign_RSA( pKey, pRSA ) ) ;
	PEM_write_RSAPrivateKey( pOut, pRSA, NULL, NULL, 0, NULL, NULL );
	assert(( pCert = X509_new() ) );

	X509_set_version( pCert, 2 );
	ASN1_INTEGER_set( X509_get_serialNumber( pCert ), serial );
	X509_gmtime_adj( X509_get_notBefore( pCert ), 0 );
	X509_gmtime_adj( X509_get_notAfter( pCert ), ( long )60*60*24*days );
	X509_set_pubkey( pCert, pKey );

	pName = X509_get_subject_name( pCert );
	X509_NAME_add_entry_by_txt( pName, "C", MBSTRING_ASC, ( unsigned char * )"US", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "ST", MBSTRING_ASC, ( unsigned char * )"California", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "L", MBSTRING_ASC, ( unsigned char * )"San Jose", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "O", MBSTRING_ASC, ( unsigned char * )"Foo, Inc", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "OU", MBSTRING_ASC, ( unsigned char * )"Barny", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "CN", MBSTRING_ASC, ( unsigned char * )"foo.com", -1, -1, 0 );
	X509_NAME_add_entry_by_txt( pName, "emailAddress", MBSTRING_ASC, ( unsigned char * )"barny@foo.com", -1, -1, 0 );

	X509_set_subject_name( pCert, pName );

	assert( X509_sign( pCert, pKey, EVP_sha256() ) );
	PEM_write_X509( pOut, pCert );
	X509_free( pCert );
	EVP_PKEY_free( pKey );
	fclose( pOut );
}

int main( int argc, char **argv )
{
	InitCsocket();

	if( access( "ReceiveTest.pem", R_OK ) != 0 )
		CreatePem();
	assert( access( "ReceiveTest.pem", R_OK ) == 0 );
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
		CSListener cListen( 0, bIsIPv6 ? "::1" : "127.0.0.1" );
		if( bIsSSL )
		{
			cListen.SetIsSSL( true );
			cListen.SetPemLocation( "ReceiveTest.pem" );
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
			ossArgs << "ncat -v ";
			if( bIsSSL )
				ossArgs << "--ssl ";
			if( bIsIPv6 )
				ossArgs << "-6 ::1 ";
			else
				ossArgs << "-4 127.0.0.1 ";
			ossArgs << uPort;
			ossArgs << " --send-only < ReceiveTest.cc > ReceiveTest.txt.out 2>&1";
			cerr << "ncat command: " << ossArgs.str() << endl;
			if( 0xffff & system( ossArgs.str().c_str() ) )
				return( 1 );
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
			assert( iStatus == 0 );
			cerr << "Exited with status " << iStatus << endl;
		}
	}

	ShutdownCsocket();
	return( 0 );
}
