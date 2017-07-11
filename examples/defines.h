#ifndef _DEFINES_H
#define _DEFINES_H
#include <string>
#include <iostream>

using std::cerr;
using std::cout;
using std::endl;

#define CS_STRING std::string
#define _NO_CSOCKET_NS

#ifdef _WIN32
#define HAVE_IPV6
#define NOMINMAX
#endif /* _WIN32 */

#endif /* _DEFINES_H */
