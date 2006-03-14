
/*
 *  Time-stamp:      "2005-02-23 07:51:34 bkorb"
 *
 *  autoopts.h  $Id: autoopts.h,v 4.9 2005/03/13 19:51:58 bkorb Exp $
 *  Time-stamp:      "2005-02-14 05:59:50 bkorb"
 *
 *  This file defines all the global structures and special values
 *  used in the automated option processing library.
 */

/*
 *  Automated Options copyright 1992-2005 Bruce Korb
 *
 *  Automated Options is free software.
 *  You may redistribute it and/or modify it under the terms of the
 *  GNU General Public License, as published by the Free Software
 *  Foundation; either version 2, or (at your option) any later version.
 *
 *  Automated Options is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Automated Options.  See the file "COPYING".  If not,
 *  write to:  The Free Software Foundation, Inc.,
 *             59 Temple Place - Suite 330,
 *             Boston,  MA  02111-1307, USA.
 *
 * As a special exception, Bruce Korb gives permission for additional
 * uses of the text contained in his release of AutoOpts.
 *
 * The exception is that, if you link the AutoOpts library with other
 * files to produce an executable, this does not by itself cause the
 * resulting executable to be covered by the GNU General Public License.
 * Your use of that executable is in no way restricted on account of
 * linking the AutoOpts library code into it.
 *
 * This exception does not however invalidate any other reasons why
 * the executable file might be covered by the GNU General Public License.
 *
 * This exception applies only to the code released by Bruce Korb under
 * the name AutoOpts.  If you copy code from other sources under the
 * General Public License into a copy of AutoOpts, as the General Public
 * License permits, the exception does not apply to the code that you add
 * in this way.  To avoid misleading anyone as to the status of such
 * modified files, you must delete this exception notice from them.
 *
 * If you write modifications of your own for AutoOpts, it is your choice
 * whether to permit this exception to apply to your modifications.
 * If you do not wish that, delete this exception notice.
 */

#ifndef AUTOGEN_AUTOOPTS_H
#define AUTOGEN_AUTOOPTS_H

#define __EXTENSIONS__

#include "config.h"
#include "compat/compat.h"
#include <sys/param.h>
#include <sys/wait.h>
#include <time.h>
#include <utime.h>

#define AO_NAME_LIMIT    127
#define AO_NAME_SIZE     (AO_NAME_LIMIT + 1)
#ifndef MAXPATHLEN
#  define MAXPATHLEN     4096
#endif
#undef  EXPORT
#define EXPORT

/*
 *  Convert the number to a list usable in a printf call
 */
#define NUM_TO_VER(n)       ((n) >> 12), ((n) >> 7) & 0x001F, (n) & 0x007F

#define NAMED_OPTS(po) \
        (((po)->fOptSet & (OPTPROC_SHORTOPT | OPTPROC_LONGOPT)) == 0)

#define SKIP_OPT(p)  (((p)->fOptState & (OPTST_DOCUMENT|OPTST_OMITTED)) != 0)

typedef int tDirection;
#define DIRECTION_PRESET  -1
#define DIRECTION_PROCESS  1
#define DIRECTION_CALLED   0

#define PROCESSING(d)     ((d)>0)
#define PRESETTING(d)     ((d)<0)

#define ISNAMECHAR( c )    (isalnum(c) || ((c) == '_') || ((c) == '-'))

/*
 *  Procedure success codes
 *
 *  USAGE:  define procedures to return "tSuccess".  Test their results
 *          with the SUCCEEDED, FAILED and HADGLITCH macros.
 */
#define SUCCESS  ((tSuccess) 0)
#define FAILURE  ((tSuccess)-1)
#define PROBLEM  ((tSuccess) 1)

typedef int tSuccess;

#define SUCCEEDED( p )     ((p) == SUCCESS)
#define SUCCESSFUL( p )    SUCCEEDED( p )
#define FAILED( p )        ((p) <  SUCCESS)
#define HADGLITCH( p )     ((p) >  SUCCESS)

/*
 *  The pager state is used by optionPagedUsage() procedure.
 *  When it runs, it sets itself up to be called again on exit.
 *  If, however, a routine needs a child process to do some work
 *  before it is done, then 'pagerState' must be set to
 *  'PAGER_STATE_CHILD' so that optionPagedUsage() will not try
 *  to run the pager program before its time.
 */
typedef enum {
    PAGER_STATE_INITIAL,
    PAGER_STATE_READY,
    PAGER_STATE_CHILD
} tePagerState;

extern tePagerState pagerState;

typedef enum {
    ENV_ALL,
    ENV_IMM,
    ENV_NON_IMM
} teEnvPresetType;

typedef enum {
    TOPT_UNDEFINED = 0,
    TOPT_SHORT,
    TOPT_LONG,
    TOPT_DEFAULT
} teOptType;

typedef struct {
    tOptDesc*  pOD;
    tCC*       pzOptArg;
    tUL        flags;
    teOptType  optType;
} tOptState;
#define OPTSTATE_INITIALIZER(st) \
    { NULL, NULL, OPTST_ ## st, TOPT_UNDEFINED }

#define TEXTTO_TABLE \
        _TT_( LONGUSAGE ) \
        _TT_( USAGE ) \
        _TT_( VERSION )
#define _TT_(n) \
        TT_ ## n ,

typedef enum { TEXTTO_TABLE COUNT_TT } teTextTo;

#undef _TT_

typedef struct {
    tCC*    pzStr;
    tCC*    pzReq;
    tCC*    pzNum;
    tCC*    pzKey;
    tCC*    pzKeyL;
    tCC*    pzBool;
    tCC*    pzNest;
    tCC*    pzOpt;
    tCC*    pzNo;
    tCC*    pzBrk;
    tCC*    pzNoF;
    tCC*    pzSpc;
    tCC*    pzOptFmt;
} arg_types_t;

#  define AGALOC( c, w )        malloc( c )
#  define AGREALOC( p, c, w )   realloc( p, c )
#  define AGFREE( p )           free( p )
#  define AGDUPSTR( p, s, w )   p = strdup( s )
#  define TAGMEM( m, t )

#ifdef AUTOGEN_BUILD
#  include <snprintfv/printf.h>
#endif /* AUTOGEN_BUILD */

/*
 *  DO option handling?
 *
 *  Options are examined at two times:  at immediate handling time and at
 *  normal handling time.  If an option is disabled, the timing may be
 *  different from the handling of the undisabled option.  The OPTST_DIABLED
 *  bit indicates the state of the currently discovered option.
 *  So, here's how it works:
 *
 *  A) handling at "immediate" time, either 1 or 2:
 *
 *  1.  OPTST_DISABLED is not set:
 *      IMM           must be set
 *      DISABLE_IMM   don't care
 *      TWICE         don't care
 *      DISABLE_TWICE don't care
 *      0 -and-  1 x x x
 *
 *  2.  OPTST_DISABLED is set:
 *      IMM           don't care
 *      DISABLE_IMM   must be set
 *      TWICE         don't care
 *      DISABLE_TWICE don't care
 *      1 -and-  x 1 x x
 */
#define DO_IMMEDIATELY(_flg) \
    (  (((_flg) & (OPTST_DISABLED|OPTST_IMM)) == OPTST_IMM) \
    || (   ((_flg) & (OPTST_DISABLED|OPTST_DISABLE_IMM))    \
        == (OPTST_DISABLED|OPTST_DISABLE_IMM)  ))

/*  B) handling at "regular" time because it was not immediate
 *
 *  1.  OPTST_DISABLED is not set:
 *      IMM           must *NOT* be set
 *      DISABLE_IMM   don't care
 *      TWICE         don't care
 *      DISABLE_TWICE don't care
 *      0 -and-  0 x x x
 *
 *  2.  OPTST_DISABLED is set:
 *      IMM           don't care
 *      DISABLE_IMM   don't care
 *      TWICE         must be set
 *      DISABLE_TWICE don't care
 *      1 -and-  x x 1 x
 */
#define DO_NORMALLY(_flg) ( \
       (((_flg) & (OPTST_DISABLED|OPTST_IMM))            == 0)  \
    || (((_flg) & (OPTST_DISABLED|OPTST_DISABLE_IMM))    ==     \
                  OPTST_DISABLED)  )

/*  C)  handling at "regular" time because it is to be handled twice.
 *      The immediate bit was already tested and found to be set:
 *
 *  3.  OPTST_DISABLED is not set:
 *      IMM           is set (but don't care)
 *      DISABLE_IMM   don't care
 *      TWICE         must be set
 *      DISABLE_TWICE don't care
 *      0 -and-  ? x 1 x
 *
 *  4.  OPTST_DISABLED is set:
 *      IMM           don't care
 *      DISABLE_IMM   is set (but don't care)
 *      TWICE         don't care
 *      DISABLE_TWICE must be set
 *      1 -and-  x ? x 1
 */
#define DO_SECOND_TIME(_flg) ( \
       (((_flg) & (OPTST_DISABLED|OPTST_TWICE))          ==     \
                  OPTST_TWICE)                                  \
    || (((_flg) & (OPTST_DISABLED|OPTST_DISABLE_TWICE))  ==     \
                  (OPTST_DISABLED|OPTST_DISABLE_TWICE)  ))

/*
 *  text_mmap structure.  Only active on platforms with mmap(2).
 */
#ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#else
#  ifndef  PROT_READ
#   define PROT_READ    0x01
#  endif
#  ifndef  PROT_WRITE
#   define PROT_WRITE   0x02
#  endif
#  ifndef  MAP_SHARED
#   define MAP_SHARED   0x01
#  endif
#  ifndef  MAP_PRIVATE
#   define MAP_PRIVATE  0x02
#  endif
#endif

#ifndef MAP_FAILED
#  define  MAP_FAILED   ((void*)-1)
#endif

#ifndef  _SC_PAGESIZE
# ifdef  _SC_PAGE_SIZE
#  define _SC_PAGESIZE _SC_PAGE_SIZE
# endif
#endif

/*
 *  This is an output only structure used by text_mmap and text_munmap.
 *  Clients must not alter the contents and must provide it to both
 *  the text_mmap and text_munmap procedures.  BE ADVISED: if you are
 *  mapping the file with PROT_WRITE the NUL byte at the end MIGHT NOT
 *  BE WRITABLE.  In any event, that byte is not be written back
 *  to the source file.  ALSO: if "txt_data" is valid and "txt_errno"
 *  is not zero, then there *may* not be a terminating NUL.
 */
typedef struct {
    void*       txt_data;      /* text file data   */
    size_t      txt_size;      /* actual file size */
    int         txt_fd;        /* file descriptor  */
    size_t      txt_full_size; /* mmaped mem size  */
    int         txt_zero_fd;   /* fd for /dev/zero */
    int         txt_errno;     /* warning code     */
    int         txt_prot;      /* "prot" flags     */
    int         txt_flags;     /* mapping type     */
} tmap_info_t;

/*
 *  Define and initialize all the user visible strings.
 *  We do not do translations.  If translations are to be done, then
 *  the client will provide a callback for that purpose.
 */
#undef DO_TRANSLATIONS
#include "autoopts/usage-txt.h"

/*
 *  File pointer for usage output
 */
extern FILE* option_usage_fp;

extern tOptProc optionPrintVersion, optionPagedUsage, optionLoadOpt;

#define LOCAL static
#include "proto.h"

#endif /* AUTOGEN_AUTOOPTS_H */
/*
 * Local Variables:
 * mode: C
 * c-file-style: "stroustrup"
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 * end of autoopts/autoopts.h */