/**
 * @file CurlSock.cc
 * @author Jim Hull <csocket@jimloco.com>
 *
 *    Copyright (c) 1999-2012 Jim Hull <csocket@jimloco.com>
 *    All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list
 * of conditions and the following disclaimer in the documentation and/or other materials
 * provided with the distribution.
 * Redistributions in any form must be accompanied by information on how to obtain
 * complete source code for this software and any accompanying software that uses this software.
 * The source code must either be included in the distribution or be available for no more than
 * the cost of distribution plus a nominal fee, and must be freely redistributable
 * under reasonable conditions. For an executable file, complete source code means the source
 * code for all modules it contains. It does not include source code for modules or files
 * that typically accompany the major components of the operating system on which the executable file runs.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
 * OR NON-INFRINGEMENT, ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OF THIS SOFTWARE BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "CurlSock.h"

#ifndef _NO_CSOCKET_NS
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */

CCurlSock::CCurlSock()
{
	m_iTimeout_ms = -1;
	m_pMultiHandle = NULL;
}

CCurlSock::~CCurlSock()
{
	for( std::map< CURL *, bool >::iterator it = m_pcbCurlHandles.begin(); it != m_pcbCurlHandles.end(); ++it )
	{
		CURL * pCurl = it->first;
		if( m_pMultiHandle )
			curl_multi_remove_handle( m_pMultiHandle, pCurl );
		curl_easy_cleanup( pCurl );
	}
	m_pcbCurlHandles.clear();
	if( m_pMultiHandle )
	{
		curl_multi_cleanup( m_pMultiHandle );
		m_pMultiHandle = NULL;
	}
}

bool CCurlSock::GatherFDsForSelect( std::map< int, int16_t > & miiReadyFds, long & iTimeoutMS )
{
	if( m_pMultiHandle )
	{
		// look for any timeout changes, and any fd changes
		int iRunningHandles = 0;
		curl_multi_socket_action( m_pMultiHandle, CURL_SOCKET_TIMEOUT, 0, &iRunningHandles );

		if( iRunningHandles >= 1 )
		{
			// this means there is a request working
			size_t uNumFDs = m_miiMonitorFDs.size();
			if( uNumFDs > 0 )
			{
				// the call back came through and there are fd's to work on

				int * aiFDs = ( int * )malloc( sizeof( int ) * uNumFDs );
				int * pTmp = aiFDs;
				// cycle through the available fd's and get the specific actions needed to proceed, need to copy as m_miiMonitorFDs might change during the callback
				for( std::map< int, int16_t >::iterator it = m_miiMonitorFDs.begin(); it != m_miiMonitorFDs.end(); ++it, ++pTmp )
					*pTmp = it->first;
				pTmp = aiFDs;
				for( size_t uCount = 0; uCount < uNumFDs; ++uCount, ++pTmp )
					curl_multi_socket_action( m_pMultiHandle, aiFDs[uCount], 0, &iRunningHandles );
				free( aiFDs );

				// cycle through any additional fd's and set them into Csocket to monitor
				for( std::map< int, int16_t >::iterator it = m_miiMonitorFDs.begin(); it != m_miiMonitorFDs.end(); ++it )
				{
					if( it->second > 0 )
						miiReadyFds[it->first] = it->second;
				}
			}

			// change the timeout if reqested to do so
			if( m_iTimeout_ms >= 0 )
				iTimeoutMS = m_iTimeout_ms;
		}
		// check for anything complete
		CURLMsg * pMSG = NULL;
		int iNumMsgQueue = 0;
		do
		{
			pMSG = curl_multi_info_read( m_pMultiHandle, &iNumMsgQueue );
			if( pMSG && pMSG->msg == CURLMSG_DONE )
			{
				OnCURLComplete( pMSG->easy_handle );
				m_pcbCurlHandles.at( pMSG->easy_handle ) = false;
			}
		}
		while( pMSG && iNumMsgQueue > 0 );

	}

	return( m_bEnabled );
}

CURL * CCurlSock::Retr( const CS_STRING & sURL, const CS_STRING & sReferrer )
{
	CURL * pCURL = NULL;
	if( !m_pMultiHandle )
	{
		m_pMultiHandle = curl_multi_init();
		// assign the next functions to get information about the internal fd's and the suggested timeout
		curl_multi_setopt( m_pMultiHandle, CURLMOPT_SOCKETFUNCTION, CCurlSock::SetupSock );
		curl_multi_setopt( m_pMultiHandle, CURLMOPT_SOCKETDATA, this );
		curl_multi_setopt( m_pMultiHandle, CURLMOPT_TIMERFUNCTION, CCurlSock::SetupTimer );
		curl_multi_setopt( m_pMultiHandle, CURLMOPT_TIMERDATA, this );
	}
	for( std::map< CURL *, bool >::iterator it = m_pcbCurlHandles.begin(); it != m_pcbCurlHandles.end(); ++it )
	{
		if( !it->second )
		{
			pCURL = it->first;
			curl_easy_reset( pCURL );
			break;
		}
	}

	if( !pCURL )
	{
		pCURL = curl_easy_init();
	}
	m_pcbCurlHandles[pCURL] = false;
	// prepare the handle, this is just a proof of concept, so doing it right here
	// you can create a CURL handle for each query, re-use them (to persist connections), drop them off, etc
	// the easiest method if you are serially retrieving documents is to do them through one CURL handle
	// otherwise you can make a pool of handles, etc using curl_multi_info_read to see which is ready and so forth
// curl_easy_setopt( pCURL, CURLOPT_VERBOSE, 1 );
	curl_easy_setopt( pCURL, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( pCURL, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1 );
	curl_easy_setopt( pCURL, CURLOPT_ENCODING, "" );
	curl_easy_setopt( pCURL, CURLOPT_SSL_VERIFYPEER, 0 );
	curl_easy_setopt( pCURL, CURLOPT_WRITEFUNCTION, CCurlSock::WriteData );
	curl_easy_setopt( pCURL, CURLOPT_HEADERFUNCTION, CCurlSock::WriteHeader );
	curl_easy_setopt( pCURL, CURLOPT_COOKIEFILE, "" ); // empty string means enable cookie handling

	// send curl back as the argument to the functions,
	// and tie this class as a reference to that object for function calls
	curl_easy_setopt( pCURL, CURLOPT_WRITEDATA, pCURL );
	curl_easy_setopt( pCURL, CURLOPT_WRITEHEADER, pCURL );
	curl_easy_setopt( pCURL, CURLOPT_PRIVATE, this );
	curl_multi_add_handle( m_pMultiHandle, pCURL );

	if( curl_easy_setopt( pCURL, CURLOPT_URL, sURL.c_str() ) != CURLE_OK )
		return( NULL );
	if( !sReferrer.empty() && curl_easy_setopt( pCURL, CURLOPT_REFERER, sReferrer.c_str() ) != CURLE_OK )
		return( NULL );
	m_pcbCurlHandles[pCURL] = true;
	return( pCURL );
}

size_t CCurlSock::WriteData( void * pData, size_t uSize, size_t uNemb, void * pCBPtr )
{
	CURL * pCURL = static_cast< CURL * >( pCBPtr );
	CCurlSock * pManager = NULL;
	if( curl_easy_getinfo( pCURL, CURLINFO_PRIVATE, &pManager ) != CURLE_OK )
		return( 0 );
	assert( pManager );
	size_t uBytes = uSize * uNemb;
//cout.write( (const char *)pData, uBytes );
	return( pManager->OnBody( pCURL, ( const char * )pData, uBytes ) );
}

size_t CCurlSock::WriteHeader( void * pData, size_t uSize, size_t uNemb, void * pCBPtr )
{
	CURL * pCURL = static_cast< CURL * >( pCBPtr );
	CCurlSock * pManager = NULL;
	if( curl_easy_getinfo( pCURL, CURLINFO_PRIVATE, &pManager ) != CURLE_OK )
		return( 0 );
	assert( pManager );
	size_t uBytes = uSize * uNemb;
	return( pManager->OnHeader( pCURL, ( const char * )pData, uBytes ) );
}

int CCurlSock::SetupSock( CURL * pCurl, curl_socket_t iFD, int iWhat, void * pCBPtr, void * pSockPtr )
{
	CCurlSock * pManager = static_cast< CCurlSock * >( pCBPtr );
	if( iWhat == CURL_POLL_IN )
	{
		pManager->Add( iFD, CSocketManager::ECT_Read );
	}
	else if( iWhat == CURL_POLL_OUT )
	{
		pManager->Add( iFD, CSocketManager::ECT_Write );
	}
	else if( iWhat == CURL_POLL_INOUT )
	{
		pManager->Add( iFD, CSocketManager::ECT_Write|CSocketManager::ECT_Read );
	}
	else if( iWhat == CURL_POLL_REMOVE )
	{
		pManager->Remove( iFD );
	}
	else
	{
		pManager->Add( iFD, 0 );
	}
	return( 0 );
}

int CCurlSock::SetupTimer( CURLM * pMulti, long iTimeoutMS, void * pCBPtr )
{
	CCurlSock * pManager = static_cast< CCurlSock * >( pCBPtr );
	pManager->SetTimeoutMS( iTimeoutMS );
	return( 0 );
}

#ifndef _NO_CSOCKET_NS
};
#endif /* _NO_CSOCKET_NS */

