/*
 * $Date: 2007/12/10 22:13:07 $
 * $Source: /cvsroot/videoteco/videoteco/teco.h,v $
 * $Revision: 1.2 $
 * $Locker:  $
 */

/**
 * \file teco.h
 * \brief Global TECO Definitions
 */

/*
 *                     COPYRIGHT (c) 1985-2007 BY Paul Cantrell
 *
 *    Copyright (C) <year>  <name of author>
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

#define YES 1
#define NO  0
#define FAIL 0
#define SUCCESS 1
#define NULL 0

#if 0
#define CREATE_OLD_FILES	/**< Create \c .OLD backup files */
#endif

#define BLOCKED 2
#define INVALIDATE 3

#define VMAJOR 6
#define VMINOR 7
#define AUTO_DATE "$Date: 2007/12/10 22:13:07 $"

#define TECO_FILENAME_TOTAL_LENGTH 1024
#define TECO_FILENAME_COMPONENT_LENGTH 256
#define TECO_READONLY_NAME "visit"
#define TECO_INTERNAL_BUFFER_NAME "TECO-"
#define TERMCAP_BUFFER_SIZE 2048
#define SCREEN_OUTPUT_BUFFER_SIZE 4096
#define SCREEN_RESERVED_LINES 2
#define SCREEN_MAX_LABEL_FIELDS 4
#define SCREEN_NOMINAL_LINE_WIDTH 132
#define SCREEN_MAX_LINES 1024
#define SCREEN_MINIMUM_WINDOW_HEIGHT 4
#define IO_BUFFER_SIZE 512
#define LINE_BUFFER_SIZE 1024
#define DEFAULT_CHECKPOINT (5*60)
#define TAG_HASH_ENTRIES	1024
#define PARSER_STRING_MAX	1024
#define	SEARCH_STRING_MAX	PARSER_STRING_MAX
#define NORMAL_TAB_WIDTH 8
#define MAX_TAB_WIDTH 16
#define MAXOF( a,b) (a) > (b) ? (a) : (b)

#define INITIAL_LINE_BUFFER_SIZE 32
#define INCREMENTAL_LINE_BUFFER_SIZE 32
#define MINIMUM_ALLOCATION_BLOCK 32
#define LOOKASIDE_COUNT 32
#define LARGEST_LOOKASIDE_BLOCK (LOOKASIDE_COUNT * MINIMUM_ALLOCATION_BLOCK)
#define BIG_MALLOC_HUNK_SIZE (LARGEST_LOOKASIDE_BLOCK * 64)
#define NORMAL_TAB_WIDTH 8

/*
 * The following is where the different machines are described so that
 * conditional compilation based on different machines quirks can be done.
 */

#ifndef BITS_PER_CHAR
/**
 * Unless defined otherwise, we assume 8 bits / char
 */
#define BITS_PER_CHAR 8
#endif

#ifndef BITS_PER_INT
#define BITS_PER_INT (BITS_PER_CHAR * (int)sizeof(int))
#endif

#define ESCAPE 27
#define RUBOUT 127

#define CNTRL_A 1
#define CNTRL_D	4
#define CNTRL_E 5
#define CNTRL_G 7
#define CNTRL_N 14
#define CNTRL_R 18
#define CNTRL_U 21
#define CNTRL_V 22
#define CNTRL_W 23
#define CNTRL_X 24
#define CNTRL_Z 26
#define BELL 7

/*
 * Define memory allocation block types
 */
#define TYPE_C_MINTYPE		33
#define TYPE_C_CBUFF		33
#define TYPE_C_CPERM		34
#define TYPE_C_CMD		35
#define TYPE_C_UNDO		36
#define TYPE_C_SCR		37
#define TYPE_C_SCRBUF		38
#define TYPE_C_SCREEN		39
#define TYPE_C_SCREENBUF	40
#define TYPE_C_LINE		41
#define TYPE_C_LINEBUF		42
#define TYPE_C_BHDR		43
#define TYPE_C_WINDOW		44
#define TYPE_C_LABELFIELD	45
#define TYPE_C_WILD		46
#define TYPE_C_TAGS		47
#define TYPE_C_TAGENT		48
#define TYPE_C_TAGSTR		49
#define TYPE_C_MAXTYPE		49

#define MAGIC_SCREEN		0x01010101
#define MAGIC_BUFFER		0x02020202
#define MAGIC_LINE		0x03030303
#define MAGIC_LINE_LOOKASIDE	0x04040404
#define MAGIC_FORMAT		0x05050505
#define MAGIC_FORMAT_LOOKASIDE	0x06060606

#ifdef DEBUG1
#define PREAMBLE() 		\
    do_preamble_checks();

#define RETURN			\
    do_return_checks();		\
    return;

#define RETURN_VAL(val)		\
    do_return_checks();		\
    return(val);
#else
#define PREAMBLE()
#define RETURN
#define RETURN_VAL()
#endif /* DEBUG1 */


#define ELEMENTS(xyzzy) (sizeof(xyzzy)/sizeof(xyzzy[0]))

/*
 * Define structures used throughout TECO
 */
    struct position_cache {
	struct buff_line *lbp;
	int	base;
    };

#define BUFF_CACHE_CONTENTS(pos_cache) \
    (pos_cache)->lbp->buffer[(pos_cache)->base]
#define BUFF_INCREMENT_CACHE_POSITION(pos_cache) \
    if(++((pos_cache)->base) >= (pos_cache)->lbp->byte_count){ \
	(pos_cache)->lbp = (pos_cache)->lbp->next_line; (pos_cache)->base = 0; \
    }
#define BUFF_DECREMENT_CACHE_POSITION(pos_cache) \
    { \
	if((pos_cache)->base == 0){ \
	    (pos_cache)->lbp = (pos_cache)->lbp->prev_line; \
	    (pos_cache)->base = (pos_cache)->lbp->byte_count; \
	} \
	(pos_cache)->base -= 1; \
    }

    struct buff_header {
	int	buf_magic;
	unsigned int buf_hash;
	char	*name;
	int	buffer_number;
	struct	buff_header	*next_header;
	char	ismodified;
	char	isreadonly;
	char	isbackedup;
	int	dot;
	int	zee;
	int	ivalue;
	struct	position_cache	pos_cache;

	struct	buff_line	*first_line;
    };

    struct buff_line {
	int	lin_magic;
	int	buffer_size;
	int	byte_count;
	char	*buffer;
	struct	buff_line	*next_line;
	struct	buff_line	*prev_line;
	struct	format_line	*format_line;
    };

    struct format_line {
	int	fmt_magic;
	struct	buff_header	*fmt_owning_buffer;
	struct	buff_line	*fmt_buffer_line;
	int	fmt_buffer_size;
	short	*fmt_buffer;
	int	fmt_sequence;
	struct	format_line	*fmt_next_line;
	struct	format_line	*fmt_prev_line;
	struct	format_line	*fmt_next_alloc;
	struct	format_line	*fmt_prev_alloc;
	struct	format_line	*fmt_next_window;
	struct	format_line	*fmt_prev_window;
	char	fmt_in_use;
	char	fmt_permanent;
	struct	screen_line	*fmt_saved_line;
	short	fmt_visible_line_position;
	struct	window		*fmt_window_ptr;
    };

    struct screen_line {
	int	scr_magic;
	short	*buffer;
	struct	format_line	*companion;
	int	sequence;
    };

    struct window {
	struct	window		*win_next_window;
	int	win_window_number;
	int	win_x_size;
	int	win_y_size;
	int	win_y_base;
	int	win_y_end;
	struct	buff_header	*win_buffer;
	struct	format_line	win_label_line;
	struct	format_line	*win_dot_format_line;
	int	win_dot_screen_offset;
	char	*win_label_field_contents[SCREEN_MAX_LABEL_FIELDS];
    };

    struct search_element {
	unsigned char type;
	unsigned char value;
	union	{
		int	intarray[256/BITS_PER_INT];
		char	bytearray[256/BITS_PER_CHAR];
	}bitmask;
	union	{
		int	intarray[256/BITS_PER_INT];
		char	bytearray[256/BITS_PER_CHAR];
	}repmask;
    };

    struct search_buff {
	int	length;
	char	input[PARSER_STRING_MAX];
	struct	search_element	data[PARSER_STRING_MAX];
	char	error_message_given;
    };

    struct wildcard_expansion {
	struct	wildcard_expansion	*we_next;
	char	we_name[TECO_FILENAME_TOTAL_LENGTH-4];
    };

    struct tagent {
	char	*te_symbol;
	char	*te_filename;
	char	*te_lineno;
	char	*te_search_string;
	struct	tagent	*te_next;
    };

    struct tags {
	struct tagent	*current_entry;
	struct	tagent	*tagents[TAG_HASH_ENTRIES];
    };
	
/*
 * Define the various flags in a screen display short int
 */
#define SCREEN_M_DATA		0x00FF
#define SCREEN_M_FLAGS		0xFF00
#define SCREEN_M_REVERSE	0x0100

/*
 * Define constants for the label line fields
 */
#define LABEL_C_TECONAME	0
#define LABEL_C_FILENAME	1
#define LABEL_C_READONLY	2
#define LABEL_C_MODIFIED	3

/*
 * Define constants for EJ command
 */
#define SETOPTION_MIN_OPTION 1

#define SETOPTION_ALTERNATE_ESCAPE_CHAR 1
#define SETOPTION_SCREEN_HEIGHT 2
#define SETOPTION_SCREEN_WIDTH 3
#define SETOPTION_ALTERNATE_DELETE_CHAR 4
#define SETOPTION_FORCE_8_BIT_CHARS 5
#define SETOPTION_TAB_WIDTH 6
#define SETOPTION_HIDE_CR_CHARS 7

#define SETOPTION_MAX_OPTION 7

/*
 * Define constants for FT command
 */
#define TAGS_MIN_OPTION 0

#define TAGS_LOAD_TAGS_FILE		0
#define TAGS_SEARCH_TAGS_FILE		1
#define TAGS_TEST_FOR_LOADED_TAGS	2
#define TAGS_INSERT_TARGET_FILE		3
#define TAGS_INSERT_SEARCH_STRING	4
#define TAGS_INSERT_LINENO		5
#define TAGS_INSERT_ALL			6

#define TAGS_MAX_OPTION 6

/*
 * Define useful macros
 */
#define UPCASE(char) (islower(char) ? TOUPPER(char) : char)
#ifdef SYS_III_UNIX
#define TOUPPER(char) _toupper(char)
#define TOLOWER(char) _tolower(char)
#else
#define TOUPPER(char) toupper(char)
#define TOLOWER(char) tolower(char)
#endif

typedef unsigned long teco_ptrint_t;

/**
 * We define unix except for the really different operating systems, like
 * vms. It lets us write our own version of functions which simply do not
 * exist outside of unix.
 */
#ifndef VMS
#define UNIX
#define JOB_CONTROL
#endif

/*
 * Include Files From GNU Autoconf/Autoheader
 */
#include "config.h"

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

/*
 * sys/filio.h has the FIONREAD definition
 */
#ifdef HAVE_SYS_FILIO_H
#include <sys/filio.h>
#endif
#ifdef FIONREAD
#define HAS_FIONREAD
#endif

#if HAVE_SYS_TYPES_H
#include <sys/types.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
#endif

#if HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#else
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#endif

#if HAVE_STRING_H
# if !STDC_HEADERS && HAVE_MEMORY_H
#  include <memory.h>
# endif
# include <string.h>
# if !HAVE_STRCHR
#  if !defined(strchr)
#   define strchr index
#  endif
#  if !defined(strrchr)
#   define strrchr rindex
#  endif
# endif
#endif

/* on SGI this gets us BZERO */
#if HAVE_STRINGS_H
#include <strings.h>
#endif

#if HAVE_INTTYPES_H
#include <inttypes.h>
#else
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#endif

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#if HAVE_DIRENT_H
#include <dirent.h>
#define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#define dirent direct
#define NAMLEN(dirent) (dirent)->d_namlen
#if HAVE_SYS_NDIR_H
#include <sys/ndir.h>
#endif
#if HAVE_SYS_DIR_H
#include <sys/dir.h>
#endif
#if HAVE_NDIR_H
#include <ndir.h>
#endif
#endif

#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif

#if GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#define SUN_STYLE_WINDOW_SIZING 1
#else
#if HAVE_TERMIOS_H
# include <termios.h>
#define SUN_STYLE_WINDOW_SIZING 1
#endif
#endif

#if HAVE_TERMIO_H
# include <termio.h>
#endif

#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if !defined(HAVE_TERMIO_H) && !defined(HAVE_TERMIOS_H)
#if HAVE_SGTTY_H
#include <sgtty.h>
#endif
#endif

#if HAVE_SYS_FILE_H
#include <sys/file.h>
#endif

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if HAVE_CTYPE_H
#include <ctype.h>
#endif

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#if HAVE_ERRNO_H
#include <errno.h>
#endif

#if HAVE_SIGNAL_H
#include <signal.h>
#endif

#if HAVE_PWD_H
#include <pwd.h>
#endif

#if HAVE_TERMCAP_H
#include <termcap.h>
#endif

/* current end of autoconf stuff*/

#if HAVE_LIBTERMCAP
#define TERMCAP
#else
#if defined(HAVE_LIBTERMINFO)
#define TERMINFO
#endif
#endif

#if !defined(TERMCAP) && !defined(TERMINFO)
#if defined(HAVE_LIBNCURSES)
#define TERMINFO
#endif
#endif


/* we prefer select over poll */
#if HAVE_SYS_SELECT_H
#include <sys/select.h>
#define HAS_SELECT
#else
#if HAVE_POLL_H
#include <poll.h>
#define HAS_POLL
#endif
#endif

/*
 * Digital Equpment VMS Operating System
 */
#ifdef VMS
#include "tecvms.h"
#endif

#ifndef CAUSE_BUS_ERROR
#define CAUSE_BUS_ERROR() 					\
		{						\
		    int fake_destination;			\
		    fake_destination = *((int *)(0x0FFFFFFF));	\
		}
#endif

/* method declarations */
void error_message(char * string);
void tec_release(unsigned char type, char *addr);
void tec_panic( char *string);
int buff_switch(struct buff_header *hbp,int map_flag);
int buff_read(struct buff_header *hbp,char *name);
int buff_readfd(struct buff_header *hbp,char *name,int iochan);
int buff_insert(struct buff_header *hbp,int position,char *buffer,int length);
int screen_label_line(struct buff_header *buffer,char *string,int field);
int buff_write(struct buff_header *hbp,int chan,int start,int end);
void tecmem_stats();
void screen_free_format_lines(struct format_line *sbp);
int buff_delete_char(struct buff_header *hbp,int position);
int compile_search_string(struct search_buff *search_tbl);
int cmd_forward_search(int pos1,int pos2,struct search_buff *search_tbl);
int cmd_reverse_search(int pos1,int pos2,struct search_buff *search_tbl);
int buff_cached_contents(struct buff_header *,int,struct position_cache *);
int buff_contents(struct buff_header *hbp,int position);
void pause_while_in_input_wait(void);
void restore_tty(void);
void initialize_tty(void);
void screen_redraw(void);
void screen_resize(void);
void screen_refresh(void);
void screen_message(char *string);
int tty_input_pending(void);
void screen_format_windows(void);
void load_qname_register(void);
void buff_delete(struct buff_header *,int,int);
void screen_reformat_windows(void);
int tag_calc_hash(char *string);
void tag_free_struct(struct tags *tp);
void tecdebug_check_screen_magic(void);
void tecdebug_check_buffer_magic(void);
void tecdebug_check_line_magic(void);
void tecdebug_check_format_magic(void);
void tecdebug_check_companion_pointers(void);
void tecdebug_check_window_pointers(void);
void term_init(void);
int term_goto( int dest_x,int dest_y);
void term_clrtobot(void);
void parser_reset_echo(void);
void term_finish(void);
void term_standend(void);
void term_clrtoeol(void);
void term_standout(void);
void screen_save_current_message(char *,int);
int window_switch(int window_number);
void screen_delete_window( struct window *old_wptr);
void buff_reopenbuff(struct buff_header *bp);
void buff_free_line_buffer_list(struct buff_line *);
int screen_display_window(struct window *wptr);
int buff_find_offset(struct buff_header *,struct buff_line *,int);
int buff_openbuffnum(int,int);
int parse_illegal_buffer_position(int,int,char *);
int buff_insert_char(struct buff_header *,int,char);
int push_qregister(char);
void buff_bulk_insert(struct buff_header *,int,int,struct buff_line *);
void buff_destroy(struct buff_header *);
int cmd_wordmove(int count);
int cmd_search(int,int,struct search_buff *);
int buff_openbuffer(char *,int,int);
int cmd_write(struct buff_header *,char *);
void screen_scroll(int);
void parser_dump_command_line(struct buff_header *);
int parser_replace_command_line(struct buff_header *);
void tec_gc_lists(void);
void screen_deallocate_format_lookaside_list();
void buff_deallocate_line_buffer_lookaside_list();
int buff_init();
void tec_error(int, char *);
int handle_command_line(int,int,char **);
void tecparse();
//void term_putc(char);
int term_putc(int);
void term_flush();
int screen_init();
void screen_finish();
int exec_doq0();
void punt(int);
void screen_reset_message();
int tecparse_syntax(int);
void screen_echo(char);
void cmd_pause();
void cmd_suspend();
int term_putnum(char *,int,int);
int term_scroll_region(int,int);
int init_term_description();
//int tgetent(char *,char *);
//int tgetnum(char *);
char *tec_alloc(int,int);
void initialize_memory_stats( void );
struct buff_header *buff_qfind(char,char);
char *error_text(int);
struct buff_line *buff_find_line(struct buff_header *hbp,int);
void term_insert_line(int,int);
void term_delete_line(int,int);
struct buff_header *buff_find(char *);
struct buff_header *buff_duplicate(struct buff_header *);
struct window *screen_split_window(struct window *,int,int);
