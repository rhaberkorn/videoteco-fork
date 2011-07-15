/*
 * $Date: 2007/12/10 22:13:08 $
 * $Source: /cvsroot/videoteco/videoteco/tecvms.h,v $
 * $Revision: 1.2 $
 * $Locker:  $
 */

/**
 * \file tecvms.h
 * \brief Include file for VMS build
 */

/*
 *                     Copyright (C) 1985-2007 BY Paul Cantrell
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define VMS

#include file
#include errno
#include stdio
#include ctype
#include perror
#include signal
#include jpidef
#include ssdef
#include iodef
#include stsdef
#include descrip
#include ttdef

#ifndef LIB$K_CLI_LOCAL_SYM
#define LIB$K_CLI_LOCAL_SYM 1
#endif
#ifndef CLI$M_NOWAIT
#define CLI$M_NOWAIT (1<<0)
#endif
#ifndef CLI$M_NOTIFY
#define CLI$M_NOTIFY (1<<4)
#endif
