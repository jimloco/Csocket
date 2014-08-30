/**
 * @file CurlSock.h
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
#ifndef HAVE_CURLSOCK_H
#define HAVE_CURLSOCK_H

#include "Csocket.h"
#include <curl/curl.h>

#ifndef _NO_CSOCKET_NS
namespace Csocket
{
#endif /* _NO_CSOCKET_NS */

/**
 * @class CCurlSock
 * @brief Csocket style wrapper around libcurl-multi
 *
 * http://curl.haxx.se/libcurl/c/libcurl-multi.html
 * This isn't finished, but this is my basic approach. I added a means to which Csocket can
 * monitor file descriptors it doesn't directly control. This class hooks into that by virtue
 * of a callback "GatherFDsForSelect" and ties that libcurl-multi. When Csocket looks for fd's to
 * monitor, this class calls "curl_multi_socket_action" which does a couple things ...
 * - 1. Calls our CURL callback CCurlSock::SetupSock to let us know the FD's and what they should be monitored for
 * - 2. Calls our CURL callback CCurlSock::SetupTimer which lets us know the maxium wait time for activity
 * - 3. Process any handles that are in a ready state
 *
 * The end point here is a non-blocking method to fetch documents via CURL within Csocket
 */
class CS_EXPORT CCurlSock : public CSMonitorFD
{
public:
	CCurlSock();
	virtual ~CCurlSock();

	//! the hook we tie into to get events from csocket
	virtual bool GatherFDsForSelect( std::map< int, int16_t > & miiReadyFds, long & iTimeoutMS );
	//! reimplement this to do nothing as its not needed
	virtual bool CheckFDs( const std::map< int, int16_t > & miiReadyFds ) { return( m_bEnabled ); }

	/**
	 * @brief initiates a GET style transfer, but the process doesn't get started until the next GatherFDsForSelect() is called
	 * @param sURL the target document
	 * @param sReferrer the referring URL
	 *
	 * Its important to check the man page on curl_easy_setopt for the various variables. Certain data is tracked and some is not.
	 * - CURLOPT_POSTFIELDS used to posting data. It is NOT copied by libcurl, so you have to track it until OnCURLComplete is called and the tranfer is complete
	 * - CURLOPT_HTTPPOST used for multipart post. The linked list passed to this needs to be tracked, and following OnCURLComplete you should set CURLOPT_HTTPPOST with null and then free your data
	 */
	CURL * Retr( const CS_STRING & sURL, const CS_STRING & sReferrer = "" );

protected:
	//! called when the transfer associate with this CURL object is completed
	virtual void OnCURLComplete( CURL * pCURL ) = 0;

	//! called as header information for the document is returned
	virtual size_t OnHeader( CURL * pCURL, const char * pData, size_t uBytes ) { return( uBytes ); }
	//! called as the document is returned
	virtual size_t OnBody( CURL * pCURL, const char * pData, size_t uBytes ) { return( uBytes ); }

private:
	static size_t WriteData( void * pData, size_t uSize, size_t uNemb, void * pStream );
	static size_t WriteHeader( void * pData, size_t uSize, size_t uNemb, void * pStream );
	static int SetupSock( CURL * pCurlHandle, curl_socket_t iFD, int iWhat, void * pcbPtr, void * pSockPtr );
	static int SetupTimer( CURLM * pMulti, long iTimeout_ms, void * pcdPtr );

	void SetTimeoutMS( long iTimeoutMS )
	{
		m_iTimeout_ms = iTimeoutMS;
	}

	long 		m_iTimeout_ms; 	//!< current timeout to use
	CURLM * 	m_pMultiHandle; //!< the main multi handle
	std::map< CURL *, bool > m_pcbCurlHandles; //!< map of CURL objects to bools. If the bool is true then that object is in use
};

#ifndef _NO_CSOCKET_NS
};
#endif /* _NO_CSOCKET_NS */

#endif /* HAVE_CURLSOCK_H */

