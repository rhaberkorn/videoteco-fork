/*
 * $Date: 2007/12/10 21:59:21 $
 * $Source: /cvsroot/videoteco/videoteco/tecvms.h,v $
 * $Revision: 1.1 $
 * $Locker:  $
 */

/* tecvms.h
 * Include file for VMS build
 * %W% (PC) %G%
 *
 *                     COPYRIGHT (c) 1985-2003 BY
 *		     PAUL CANTRELL & J. M. NISHINAGA
 *                         SUDBURY, MA 01776
 *                        ALL RIGHTS RESERVED
 *
 * This software is furnished in it's current state free of  charge.
 * The   authors   reserve  all  rights  to  the  software.  Further
 * distribution of the software is not authorized. Modifications  to
 * the  software  may  be made locally, but shall not be distributed
 * without the consent of the authors. This software  or  any  other
 * copies  thereof,  may not be provided or otherwise made available
 * to anyone without express permission of the authors. Title to and
 * ownership of this software remains with the authors.
 * 
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
