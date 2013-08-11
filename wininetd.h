/*
 *  WinInetd by Davide Libenzi ( Inetd-like daemon for Windows )
 *  Copyright (C) 2003  Davide Libenzi
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  Davide Libenzi <davidel@xmailserver.org>
 *
 */

#if !defined(_WININETD_H)
#define _WININETD_H

#include <stdarg.h>

#define WINET_APPNAME "wininetd"
#define WINET_VERSION "0.7"


#define COUNTOF(a) (sizeof(a) / sizeof(a[0]))

#define WINET_LOG_MESSAGE 1
#define WINET_LOG_WARNING 2
#define WINET_LOG_ERROR 3

int _winet_log(int level, char const *fmt, va_list args);
int winet_log(int level, char const *fmt, ...);

int winet_stop_service(void);
int winet_main(int argc, char const **argv);



#endif /* #if !defined(_WININETD_H) */
