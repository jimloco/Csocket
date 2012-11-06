#include <string.h>
#include <CurlSock.h>
#include <curl/curl.h>

// HI, this is only a proof of concept :)

class CGetWebPage : public CCurlSock
{
public:
	CGetWebPage( std::string & sHeader, std::string & sBody ) : CCurlSock(), m_sHeader( sHeader ), m_sBody( sBody )
	{
	}

protected:
	virtual void OnCURLComplete( CURL * pCURL ) { m_bEnabled = false; }
	size_t OnHeader( CURL * pCURL, const char * pData, size_t uBytes )
	{
		long iData = 0;
		// only interested in the 200 response, in this test google does a 301
		if( curl_easy_getinfo( pCURL, CURLINFO_RESPONSE_CODE, &iData ) == CURLE_OK && iData == 200 )
			m_sHeader.append( pData, uBytes );
		return( uBytes );
	}
	size_t OnBody( CURL * pCURL, const char * pData, size_t uBytes )
	{
		if( m_sHeader.size() )
			m_sBody.append( pData, uBytes );
		return( uBytes );
	}
private:
	std::string	&	m_sHeader;
	std::string	&	m_sBody;
};


int main( int argc, char **argv )
{
	std::string sHeader, sBody;
	CSocketManager cFoo;
	CGetWebPage * pCurl = new CGetWebPage( sHeader, sBody );
	pCurl->Retr( "http://google.com" );
	cFoo.MonitorFD( pCurl );

	while( cFoo.HasFDs() )
		cFoo.Loop();

	cerr << "-------- Header --------" << endl;
	cerr << sHeader << endl;
	assert( sHeader.compare( 0, 15, "HTTP/1.1 200 OK" ) == 0 );
	assert( sBody.size() );
	cerr << "Body Size: " << sBody.size() << endl;

	return( 0 );
}

