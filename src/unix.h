/*******************************************************************************
 * shroudBNC - an object-oriented framework for IRC                            *
 * Copyright (C) 2005-2007 Gunnar Beutner                                           *
 *                                                                             *
 * This program is free software; you can redistribute it and/or               *
 * modify it under the terms of the GNU General Public License                 *
 * as published by the Free Software Foundation; either version 2              *
 * of the License, or (at your option) any later version.                      *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the               *
 * GNU General Public License for more details.                                *
 *                                                                             *
 * You should have received a copy of the GNU General Public License           *
 * along with this program; if not, write to the Free Software                 *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. *
 *******************************************************************************/

// *nix specific things

#include <dlfcn.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <errno.h>
#include <sys/resource.h>
#include <limits.h>
#include <termios.h>
#include <strings.h>

typedef int SOCKET;

#define SD_BOTH SHUT_RDWR
#define closesocket close
#define INVALID_SOCKET (-1)
#define ioctlsocket ioctl

typedef int BOOL;

#define LoadLibrary(lpLibFileName) lt_dlopen(lpLibFileName)
#define FreeLibrary(hLibModule) hLibModule ? !lt_dlclose(hLibModule) : 0
#define GetProcAddress(hModule, lpProcName) lt_dlsym(hModule, lpProcName)

#ifdef __CYGWIN__
	#define EXPORT __declspec(dllexport)
#else
	#define EXPORT
#endif

#ifdef __FreeBSD__
#define sighandler_t sig_t
#endif

#define MAXPATHLEN PATH_MAX

#define mkdir(X) mkdir(X, 0700)

#define DebugBreak()

#define SBNCAPI
