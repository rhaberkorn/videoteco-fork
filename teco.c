char *teco_c_version = "teco.c: $Revision: 1.3 $";
char *copyright = "Copyright (c) 1985-2007 Paul Cantrell";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/teco.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/**
 * \file teco.c
 * \brief Main TECO entry point with most of the initialization code
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

#include "teco.h"
#include "tecparse.h"

/*
 * Global Storage is defined here for lack of a better place.
 * First off are all the flags.
 */
    char	readonly_flag = NO;
    char	piped_input_flag = NO;
    char	piped_output_flag = NO;
    char	tty_modes_saved = NO;
    char	exit_flag = NO;
    char	intr_flag = 0;
    char	susp_flag = 0;
    char	input_pending_flag = NO;
    int		tab_width = NORMAL_TAB_WIDTH;
    char	alternate_escape_character = ESCAPE;
    char	main_delete_character = RUBOUT;
    char	alternate_delete_character = RUBOUT;
    char	eight_bit_output_flag = NO;
    char	hide_cr_flag = NO;
    char	waiting_for_input_flag = NO;
    char	resize_flag = NO;
    char	teco_startup = YES;
    char	screen_startup = YES;
    char	checkpoint_flag = NO;
    char	checkpoint_enabled = YES;
    char	checkpoint_modified = NO;
    char	outof_memory = NO;
    char	suspend_is_okay_flag = YES;

/*
 * Global Variables
 */
    int		tty_input_chan;
    int		tty_output_chan;
    int		term_speed;
    FILE	*teco_debug_log;

    int		term_columns;
    int 	term_lines;
    int		forced_width,forced_height;

    int		checkpoint_interval = DEFAULT_CHECKPOINT;

    char	*output_tty_name;
/**
 * Table of Bit Positions
 */
    unsigned int IntBits[BITS_PER_INT];

/**
 * Table of Spaces for tab expansion
 */
    char	tab_expand[MAX_TAB_WIDTH+1];
/*
 * Global structures
 */
#if HAVE_TERMIOS_H
    static	struct termios tty_input_modes;
    static	struct termios tty_output_modes;
#else
#ifdef ATT_UNIX
    static	struct termio tty_input_modes;
    static	struct termio tty_output_modes;
#endif
#ifdef BSD_UNIX
    static	struct sgttyb tty_input_modes;
    static	struct sgttyb tty_output_modes;
#endif
#endif

/*
 * External Variables
 */

/*
 * Prototypes
 */
    void init_signals(void);
    void teco_ini(const char *);
    void process_directory(char *,char*,int);
    int match_name(char *,char *);
    int map_baud(int);

#ifdef __cplusplus
extern "C" {
#endif
	struct passwd *getpwnam( const char * );
#ifdef __cplusplus
}
#endif



/**
 * \brief Text Edit and COrrector
 *
 * This is the entry point for TECO. It initializes the terminal, reads
 * in the specified files, and generally gets things going.
 */
int
main( int argc, char **argv )
{
register int i;

    initialize_memory_stats();

    for(i = 0; i < BITS_PER_INT; i++){
	IntBits[i] = 1 << i;
    }/* End FOR */
    for(i = 0; i < MAX_TAB_WIDTH; i++){
	    tab_expand[i] = ' ';
    }/* End FOR */
    tab_expand[MAX_TAB_WIDTH] = '\0';

    if(buff_init() == FAIL){
	tec_error(ENOMEM,"Unable to initialize buffers");
    }/* End IF */

    handle_command_line(0,argc,argv);

    init_signals();
    initialize_tty();

    if(strcmp(argv[0],TECO_READONLY_NAME) == 0){
	readonly_flag = YES;
    }/* End IF */

    handle_command_line(1,argc,argv);

    teco_ini(argv[0]);

    teco_startup = NO;

    while(exit_flag == NO){
	tecparse();
	intr_flag = 0;
    }/* End While */

    if(piped_output_flag){
	extern struct buff_header *curbuf;
	buff_openbuffnum(-1,0);
	buff_write(curbuf,fileno(stdout),0,curbuf->zee);
    }/* End IF */

    restore_tty();
/*
 * Output a final newline to get the terminal onto a fresh
 * line.
 */
    term_putc('\n');
    term_flush();

#ifdef CHECKPOINT
    remove_checkpoint_file();
#endif /* CHECKPOINT */

#if defined(UNIX) || defined(MSDOS)
    return(0);
#endif

#ifdef VMS
    return(STS$M_SUCCESS);
#endif

}/* End Routine */



#ifdef UNIX

/**
 * \brief Set the tty up the way we want it
 *
 * This routine is called at startup time to set up the terminal the way
 * that we require, i.e., it sets the tty modes. It also gives the screen
 * package a chance to initialize itself.
 */
void
initialize_tty( void )
{
#if HAVE_TERMIOS_H
    struct termios tmp_tty_modes;
#else
#ifdef ATT_UNIX
#define GET_IOCTL TCGETA
#define SET_IOCTL TCSETA
    struct termio tmp_tty_modes;
#endif
#ifdef BSD_UNIX
#define GET_IOCTL TIOCGETP
#define SET_IOCTL TIOCSETN
#ifdef INTEGRATED_SOLUTIONS
    int mask;
#endif /* INTEGRATED_SOLUTIONS */

    struct sgttyb tmp_tty_modes;
#endif /* BSD_UNIX */
#endif /* HAVE_TERMIOS_H */

#ifdef SUN_STYLE_WINDOW_SIZING
    struct winsize os_window;
#endif

/*
 * We start by assuming that standard input is a tty device. If it turns out
 * that it is not, however, then we want to use /dev/tty as the keyboard.
 */
    if(!isatty(tty_input_chan = fileno(stdin))){
	tty_input_chan = open("/dev/tty",O_RDONLY);
	piped_input_flag = YES;
	if(tty_input_chan < 0){
	    perror("cannot open /dev/tty");
	    punt(errno);
	}/* End IF */
    }/* End IF */
/*
 * Standard output MUST by a tty at this time.
 */
    if(output_tty_name){
	tty_output_chan = open(output_tty_name,O_RDWR);
    }/* End IF */

    else {
	tty_output_chan = fileno(stdout);
    }/* End Else */

    if(!isatty(tty_output_chan)){
	tty_output_chan = open("/dev/tty",O_RDWR);
	piped_output_flag = YES;
	if(tty_output_chan < 0){
	    perror("cannot open /dev/tty");
	    punt(errno);
	}/* End IF */
    }/* End IF */
/*
 * Get the current modes of the tty so it can be restored later.
 */
#if HAVE_TERMIOS_H
    if(tcgetattr(tty_input_chan,&tty_input_modes) < 0){
	perror("could not get input tty modes");
	punt(errno);
    }/* End IF */

    if(tcgetattr(tty_output_chan,&tty_output_modes) < 0){
	perror("could not get output tty modes");
	punt(errno);
    }/* End IF */

#else
    if(ioctl(tty_input_chan,GET_IOCTL,&tty_input_modes) < 0){
	perror("could not get input tty modes");
	punt(errno);
    }/* End IF */

    if(ioctl(tty_output_chan,GET_IOCTL,&tty_output_modes) < 0){
	perror("could not get output tty modes");
	punt(errno);
    }/* End IF */

#endif

#if 0 /* Turned off because xterm is getting clobbered by this */
#if defined(ATT_UNIX) || defined(_POSIX_SOURCE)
    if((tty_output_modes.c_cflag & CSIZE) == CS8){
	eight_bit_output_flag = YES;
    }/* End IF */
#endif
#endif

#if defined(HAVE_DECL_CFGETOSPEED) && HAVE_DECL_CFGETOSPEED
    term_speed = map_baud( cfgetospeed(&tty_output_modes) );
#else
# if defined(HAVE_STRUCT_TERMIOS_C_CFLAG) && defined(CBAUD)
    term_speed = map_baud(tty_output_modes.c_cflag & CBAUD);
# else
#  if HAVE_STRUCT_TERMIOS_C_OSPEED
   term_speed = map_baud(tty_output_modes.c_ospeed);
#else 
#   if HAVE_STRUCT_TERMIOS_SG_OSPEED
   term_speed = map_baud(tty_output_modes.sg_ospeed);
#   endif
#  endif
# endif
#endif

#if HAVE_TERMIOS_H
    main_delete_character = tty_input_modes.c_cc[VERASE];
#else
#if defined(BSD_UNIX)
    main_delete_character = tty_input_modes.sg_erase;
#endif
#endif

    tty_modes_saved = YES;
/*
 * Check out the termcap description for this tty now. It makes
 * no sense to go changing the terminal modes all around until
 * we decide whether we can run on this terminal or not.
 */
    init_term_description();
/*
 * If the OS supports window size ioctls, we try for that since it
 * will tend to be more correct for a window environment.
 */
#ifdef SUN_STYLE_WINDOW_SIZING
    if(ioctl(tty_output_chan,TIOCGWINSZ,&os_window) >= 0){

	if((unsigned)os_window.ws_row >= SCREEN_MAX_LINES){
	    fprintf(stderr,"os_window.ws_row %d os_window.ws_col %d\n",
		(int)os_window.ws_row,(int)os_window.ws_col);
	    fprintf(stderr,"term_lines %d term_columns %d\n",
		term_lines,term_columns);
	    fprintf(stderr,"forced_height %d forced_width %d\n",
		forced_height,forced_width);
	    os_window.ws_row = term_lines;
	    os_window.ws_col = term_columns;
	}/* End IF */

	if(forced_height > 0) os_window.ws_row = forced_height;
	if(forced_width > 0) os_window.ws_col = forced_width;
	if(teco_startup == YES){
	    term_lines = os_window.ws_row;
	    term_columns = os_window.ws_col;
	}/* End IF */
	if(term_lines == 0) term_lines = 24;
	if(term_columns == 0) term_columns = 80;
	if(term_lines != os_window.ws_row || term_columns != os_window.ws_col){
	    screen_resize();
	}/* End IF */
    }/* End IF */
#else
    if(forced_height > 0) term_lines = forced_height;
    if(forced_width > 0) term_columns = forced_width;
    if(term_lines == 0) term_lines = 24;
    if(term_columns == 0) term_columns = 80;
#endif

    if(term_lines >= SCREEN_MAX_LINES){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(
	    tmp_message,
	    "terminal line count of %d exceeds maximum of %d",
	    term_lines,
	    SCREEN_MAX_LINES
	);
	tec_error(E2BIG,tmp_message);
    }/* End IF */

/*
 * Now set the modes that we want
 */
    tmp_tty_modes = tty_input_modes;

#if HAVE_TERMIOS_H
    tmp_tty_modes.c_lflag &= ~(ICANON | ECHO);
    tmp_tty_modes.c_cc[VMIN] = 1;
    tmp_tty_modes.c_cc[VTIME] = 1;
#else
#ifdef SYS_III_UNIX
    tmp_tty_modes.c_lflag &= ~(ICANON | ECHO | ISTATUS);
    tmp_tty_modes.c_cc[VMIN] = 1;
    tmp_tty_modes.c_cc[VTIME] = 1;
#endif

#ifdef BSD_UNIX
    tmp_tty_modes.sg_flags |= CBREAK;
    tmp_tty_modes.sg_flags &= ~(ECHO);
#endif
#endif /* SYSV or POSIX */

#if HAVE_TERMIOS_H
    if(tcsetattr(tty_input_chan,TCSANOW,&tmp_tty_modes) < 0){
	perror("could not set input tty modes");
	punt(errno);
    }/* End IF */
#else
    if(ioctl(tty_input_chan,SET_IOCTL,&tmp_tty_modes) < 0){
	perror("could not set input tty modes");
	punt(errno);
    }/* End IF */
#endif

#ifdef INTEGRATED_SOLUTIONS
    mask = LLITOUT;
    ioctl(tty_output_chan,TIOCLBIS,&mask);
#endif

    screen_init();

}/* End Routine */

#endif /* UNIX */

/* END OF UNIX CONDITIONAL CODE */

#ifdef MSDOS

void
initialize_tty( void )
{
    tty_input_chan = 0;
    tty_output_chan = 1;

    /*
     * The terminal speed is checked in various situations,
     * so it's important to be initialized even though we're
     * not on a real serial connection.
     */
    term_speed = 32000;
    main_delete_character = 8;

    // Block cursor
    //_settextcursor(0x0007);

/*
 * Check out the termcap description for this tty now. It makes
 * no sense to go changing the terminal modes all around until
 * we decide whether we can run on this terminal or not.
 */
    init_term_description();

    if(forced_height > 0) term_lines = forced_height;
    if(forced_width > 0) term_columns = forced_width;
    if(term_lines == 0) term_lines = 24;
    if(term_columns == 0) term_columns = 80;

    if(term_lines >= SCREEN_MAX_LINES){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(
	    tmp_message,
	    "terminal line count of %d exceeds maximum of %d",
	    term_lines,
	    SCREEN_MAX_LINES
	);
	tec_error(E2BIG,tmp_message);
    }/* End IF */

    screen_init();
}/* End Routine */

#endif /* MSDOS */



#ifndef MSDOS

#if !HAVE_TCGETATTR

/**
 * \brief Mimic the POSIX terminal get/set functions
 *
 * For a system being compiled with POSIX headers, but which doesn't
 * actually have all the POSIX functions in libc, we have to fake up
 * this call.
 */
int
tcgetattr(fd,termios_p)
int fd;
struct termios *termios_p;
{
struct termio tmp_modes;
register int i;
int status;

    bzero(&tmp_modes,sizeof(tmp_modes));
    if(status = ioctl(fd,TCGETA,&tmp_modes) < 0){
	return(-1);
    }/* End IF */

    termios_p->c_iflag = tmp_modes.c_iflag;
    termios_p->c_oflag = tmp_modes.c_oflag;
    termios_p->c_cflag = tmp_modes.c_cflag;
    termios_p->c_lflag = tmp_modes.c_lflag;
    termios_p->c_line = tmp_modes.c_line;
    for(i = 0; i < (NCCS < NCC ? NCCS : NCC); i++){
	termios_p->c_cc[i] = tmp_modes.c_cc[i];
    }/* End FOR */

    return(status);

}/* End Routine */

#endif /* !HAVE_TCGETATTR */

#if !HAVE_TCSETATTR

/**
 * \brief Mimic the POSIX terminal get/set functions
 *
 * For a system being compiled with POSIX headers, but which doesn't
 * actually have all the POSIX functions in libc, we have to fake up
 * this call.
 */
tcsetattr(fd,flags,termios_p)
int fd;
int flags;
const struct termios *termios_p;
{
struct termio tmp_modes;
register int i;
int status;

    bzero(&tmp_modes,sizeof(tmp_modes));
    tmp_modes.c_iflag = termios_p->c_iflag;
    tmp_modes.c_oflag = termios_p->c_oflag;
    tmp_modes.c_cflag = termios_p->c_cflag;
    tmp_modes.c_lflag = termios_p->c_lflag;
    tmp_modes.c_line = termios_p->c_line;
    for(i = 0; i < (NCCS < NCC ? NCCS : NCC); i++){
	tmp_modes.c_cc[i] = termios_p->c_cc[i];
    }/* End FOR */
    if(status = ioctl(fd,TCSETA,&tmp_modes) < 0){
	return(-1);
    }/* End IF */

    return(status);

}/* End Routine */

#endif /* !HAVE_TCSETATTR */

#endif /* !MSDOS */

#ifdef VMS


/**
 * \brief Set the tty up the way we want it
 *
 * This routine is called at startup time to set up the terminal the way
 * that we require, i.e., it sets the tty modes. It also gives the screen
 * package a chance to initialize itself.
 */
void
initialize_tty()
{
register int status;
$DESCRIPTOR(input_name,"SYS$INPUT");
$DESCRIPTOR(output_name,"SYS$OUTPUT");

struct sense_mode_buffer {
        unsigned char class;
        unsigned char type;
        unsigned short buffer_size;
        unsigned char characteristics[3];
        unsigned char page_length;
} sense_buffer;
 
struct sense_iosb {
        unsigned short status;
        unsigned char transmit_speed;
        unsigned char receive_speed;
        unsigned char cr_fill_count;
        unsigned char lf_fill_count;
        unsigned char parity_flags;
        unsigned char unused;
} sense_iosb;

/*
 * Assign channels to the terminal
 */
    status = sys$assign(&input_name,&tty_input_chan,0,0);
    if(!(status & STS$M_SUCCESS)){
	tec_error(status,"VTECO: ASSIGN of SYS$INPUT Failed");
    }/* End IF */

/*
 * Standard output MUST by a tty at this time.
 */
    status = sys$assign(&output_name,&tty_output_chan,0,0);
    if(!(status & STS$M_SUCCESS)){
	tec_error(status,"VTECO: ASSIGN of SYS$OUTPUT Failed");
    }/* End IF */

/*
 * Get the baud rate of the terminal
 */
    status = sys$qiow(0,tty_output_chan,IO$_SENSEMODE,
        &sense_iosb,0,0,&sense_buffer,sizeof(sense_buffer),0,0,0,0);
    if(!(status & STS$M_SUCCESS)){
        tec_error(status,"VTECO: Error on sense mode %d (%x)",status,status);
    }/* End IF */

    term_lines = sense_buffer.page_length;
    term_columns = sense_buffer.buffer_size;

    term_speed = map_baud(sense_iosb.transmit_speed);
    tty_modes_saved = YES;

/*
 * Check out the termcap description for this tty now. It makes
 * no sense to go changing the terminal modes all around until
 * we decide whether we can run on this terminal or not.
 */
    init_term_description();
/*
 * Now set the modes that we want
 */
    screen_init();

}/* End Routine */

/* END OF VMS CONDITIONAL CODE */

#endif



/**
 * \brief Set the tty back to the way that it was
 *
 * This routine is called at exit time to set the terminal modes back
 * how they were when we started.
 */
void
restore_tty()
{

/*
 * Give the screen package a chance to cleanup
 */
    screen_finish();

/*
 * Can't restore the terminal modes if they haven't been saved yet
 */
    if(tty_modes_saved != YES) return;

/*
 * Restore the modes that we saved away previously
 */
#if HAVE_TERMIOS_H
    if(tcsetattr(tty_input_chan,TCSADRAIN,&tty_input_modes) < 0){
	perror("could not set input tty modes");
	punt(errno);
    }/* End IF */
    if(tcsetattr(tty_output_chan,TCSADRAIN,&tty_output_modes) < 0){
	perror("could not set output tty modes");
	punt(errno);
    }/* End IF */
#else
#ifdef UNIX
    if(ioctl(tty_input_chan,SET_IOCTL,&tty_input_modes) < 0){
	perror("could not restore input tty modes");
    }/* End IF */

    if(ioctl(tty_output_chan,SET_IOCTL,&tty_output_modes) < 0){
	perror("could not restore output tty modes");
    }/* End IF */
#endif
#endif

}/* End Routine */



/**
 * \brief Check if there is input waiting on the cmd channel
 *
 * This routine returns a true false indication of whether or not there
 * is input waiting on the command channel. This generally gets used to
 * set the input_pending_flag variable. Now it is set to only work if
 * the baud rate is quite low, since some of the current side effects are
 * undesirable.
 */
int
tty_input_pending()
{
#ifdef HAS_FIONREAD
long temp;
#endif

#ifdef VMS
struct sense_typeahead_buffer {
        unsigned short count;
        unsigned char first_char;
        unsigned char reserved1;
        unsigned long reserved2;
} sense_typeahead_buffer;
#endif

#ifdef SEQUOIA
struct sequoia_weirdo_get_param_buffer {
    unsigned short fc_open_mode;
    unsigned long fc_num_bytes_remaining;
    char fc_no_delay;
    char fc_modify_no_delay;
    char fc_append;
    char fc_modify_append;
    char fc_close_across_exec;
    char fc_modify_close_across_exec;
    char fc_no_further_opens;
    char fc_modify_no_further_opens;
}term_params;

typedef enum
{
    FC_SET_PARAMETERS,
    FC_GET_PARAMETERS,
    FC_SET_BUFFER_REPLACEMENT_PARAMETERS,
    FC_GET_BUFFER_REPLACEMENT_PARAMETERS,
} FILE_CONTROL_REQUEST;
#endif

#ifdef SEQUOIA
int err;
    err = seqksr_file_control(&term_params,tty_input_chan,FC_GET_PARAMETERS);
    if(err == 0 && term_params.fc_num_bytes_remaining != 0) return(YES);
#endif

#ifdef HAS_POLL
	int err;
	struct pollfd pollStructs[1];
#endif

#ifdef HAS_SELECT
    int err;
    fd_set theFds;
    struct timeval zeroTime;
#endif
	
    PREAMBLE();

#ifdef HAS_SELECT
    FD_ZERO( &theFds );
    FD_SET( tty_input_chan, &theFds );
    zeroTime.tv_sec = 0;
    zeroTime.tv_usec = 0;
    err = select( tty_input_chan+1, &theFds, 0, 0, &zeroTime );
    if(err > 0)
    {
	return(YES);
    }
#endif

#ifdef HAS_POLL
    pollStructs[0].fd = tty_input_chan;
    pollStructs[0].events = POLLIN;
    pollStructs[0].revents = 0;
    err = poll( pollStructs, ELEMENTS(pollStructs), 0 );
    if( err >= 0 && pollStructs[0].revents & POLLIN) return(YES);
#endif

#ifdef HAS_FIONREAD
    temp = 0;
    if(ioctl(tty_input_chan,FIONREAD,&temp) < 0) return(NO);
    if(temp > 0) return(YES);
#endif

#ifdef VMS
    temp = sys$qiow(0,tty_input_chan,IO$_SENSEMODE|IO$M_TYPEAHDCNT,
        0,0,0,&sense_typeahead_buffer,sizeof(sense_typeahead_buffer),0,0,0,0);
    if(!(temp & STS$M_SUCCESS)){
	return(NO);
    }/* End IF */
 
    if(sense_typeahead_buffer.count > 0) return(YES);

#endif

#ifdef MSDOS
    if(kbhit()) return(YES);
#endif

    return(NO);

}/* End Routine */



/**
 * \brief Initialize the signal interface for our use
 *
 * This routine sets up the way that we want signals to behave
 */
void
init_signals()
{

#if 0
int cmd_suspend(void);
#endif
int cmd_interrupt(void);
int cmd_alarm(void);
int cmd_winch(void);

    PREAMBLE();

#ifdef CHECKPOINT
    signal(SIGALRM,(void (*)())cmd_alarm);
    alarm(checkpoint_interval);
#endif /* CHECKPOINT */

    signal(SIGINT,(void (*)(int))cmd_interrupt);

#ifdef JOB_CONTROL
    if(suspend_is_okay_flag == YES){
	signal(SIGTSTP,(void (*)(int))cmd_suspend);
    }
#endif

#ifdef SUN_STYLE_WINDOW_SIZING
    signal(SIGWINCH,(void (*)(int))cmd_winch);
#endif

}/* End Routine */



/**
 * \brief Routine to read the TECO_INI file
 *
 * This routine reads the user's TECO_INI file and sets up the default
 * Q-Register contents he has specified. It also executes Q-Register 0.
 */
void
teco_ini(const char *prg)
{
register char *cp;
register int c,dc;
register int i;
char filename[LINE_BUFFER_SIZE];
char tmp_message[LINE_BUFFER_SIZE];
FILE *fp;
struct buff_header *hbp;
char zero_flag = NO;
char comment_flag = NO;

    PREAMBLE();

#ifdef UNIX

    if((cp = getenv("HOME"))){
	(void) strcpy(filename,cp);
    }/* End IF */

    else return;

    (void) strcat(filename,"/.teco_ini");

#endif

#ifdef MSDOS

    /*
     * Locate TECO.INI in the same directory
     * as the executable.
     * This approach may fail in some command interpreters,
     * so you may have to point the VTECO environment
     * variable to the directory with TECO.INI instead.
     */
    if((cp = getenv("VTECO"))){
	strcpy(filename,cp);
    }/* End If */
    else if((cp = strrchr(prg,'\\'))){
	strncpy(filename,prg,cp-prg);
	filename[cp-prg] = '\0';
    }/* End If */
    else return;

    strcat(filename,"\\TECO.INI");

#endif /* MSDOS */

#ifdef VMS

    strcpy(filename,"SYS$LOGIN:TECO.INI");

#endif

    fp = fopen(filename,"r");

    if(fp == NULL) return;

/*
 * Outer loop reads the name of the Q-Register to be loaded and the delimiter,
 * inner loop reads contents of a Q-Register in.
 */
    while(1){
	while(1){
	    if((c = getc(fp)) == EOF){
		fclose(fp);
		if(zero_flag) exec_doq0();
		return;
	    }/* End IF */

	    if(comment_flag){
		if(c == '!') comment_flag = NO;
		continue;
	    }/* End IF */

	    if(c == '!'){
		comment_flag = YES;
		continue;
	    }/* End IF */

	    if(isspace(c)) continue;
	    break;

	}/* End While */

	if(c == '0') zero_flag = YES;

	hbp = buff_qfind(c,1);
	if(hbp == NULL){
	    fclose(fp);
	    return;
	}/* End IF */

	if((dc = getc(fp)) == EOF){
	    sprintf(tmp_message,"?EOF while loading Q-REGISTER <%c>",c);
	    error_message(tmp_message);
	    fclose(fp);
	    return;
	}/* End IF */

	while(1){
	    if((i = getc(fp)) == EOF){
		sprintf(tmp_message,"?EOF while loading Q-REGISTER <%c>",c);
		error_message(tmp_message);
		fclose(fp);
		return;
	    }/* End IF */

	    if(i == dc) break;

	    buff_insert_char(hbp,hbp->zee,i);

	}/* End While */

	hbp->ismodified = NO;

    }/* End While */

}/* End Routine */



void
check_for_forced_screen_size( int argc, char **argv )
{
register int i;
char *cp, c;

    PREAMBLE();

    for(i = 1; i < argc; i++){
	if(argv[i][0] == '-'){
	    cp = &argv[i][1];
	    while((c = *cp++)){
		switch(c){
		    case 'X':	case 'x':
		    case 'W':	case 'w':
			if(isdigit((int)*cp)){
			    forced_width = 0;
			    while(isdigit((int)*cp)){
				forced_width = forced_width * 10 + *cp++ - '0';
			    }/* End While */
			}/* End IF */
			else if( ((i+1) < argc) && isdigit((int)argv[i+1][0])){
			    forced_width = atoi(argv[i+1]);
			    i++;
			}
			else forced_width = 80;
			break;
		    case 'Y':	case 'y':
		    case 'H':	case 'h':
			if(isdigit((int)*cp)){
			    forced_height = 0;
			    while(isdigit((int)*cp)){
				forced_height = forced_height * 10 + 
				    *cp++ - '0';
			    }/* End While */
			}/* End IF */
			else if( ((i+1) < argc) && isdigit((int)argv[i+1][0])){
			    forced_height = atoi(argv[i+1]);
			    i++;
			}
			else forced_height = 24;
			break;
		    case 'O':    case 'o':
			output_tty_name = argv[i+1];
			i++;
			break;
		}/* End Switch */
	    }/* End While */
	    continue;
	}/* End IF */

    }/* End FOR */

}/* End Routine */



/**
 * \brief OS Dependent Code to handle command line arguments
 *
 * This code handles the command line arguments to VTECO. This requires
 * different processing for different operating systems.
 */
#if defined(UNIX) || defined(MSDOS)
/*
 * FIXME: Perhaps DOS should have its own version with `/` arguments.
 */
int
handle_command_line( int which_time, int argc, char **argv )
{
register int i;
extern struct buff_header *curbuf;
int first_file_argument;
char *cp, c;
int arg_skip;

    PREAMBLE();

    first_file_argument = 0;
    for(i = 1; i < argc; i += arg_skip){
	arg_skip = 1;
	if(argv[i][0] == '-'){
	    cp = &argv[i][1];
	    while((c = *cp++)){
		switch(c){
		    case 'C':   case 'c':
			checkpoint_enabled = NO;
			break;
		    case 'X':	case 'x':
		    case 'W':	case 'w':
			if(isdigit((int)*cp)){
			    forced_width = 0;
			    while(isdigit((int)*cp)){
				forced_width = forced_width * 10 + *cp++ - '0';
			    }/* End While */
			}/* End IF */
			else if( ((i+1) < argc) && isdigit((int)argv[i+1][0])){
			    forced_width = atoi(argv[i+1]);
			    arg_skip++;
			}
			else forced_width = 80;
			break;
		    case 'Y':	case 'y':
		    case 'H':	case 'h':
			if(isdigit((int)*cp)){
			    forced_height = 0;
			    while(isdigit((int)*cp)){
				forced_height = forced_height * 10 + 
				    *cp++ - '0';
			    }/* End While */
			}/* End IF */
			else if( ((i+1) < argc) && isdigit((int)argv[i+1][0])){
			    forced_height = atoi(argv[i+1]);
			    i++;
			}
			else forced_height = 24;
			break;
		    case 'O':    case 'o':
			output_tty_name = argv[i+1];
			arg_skip++;
			break;
		    default:
			fprintf(stderr,"teco: unknown switch '%c'\n",c);
			cp = "";
			break;
		}/* End Switch */
	    }/* End While */
	    continue;
	}/* End IF */

	if(which_time == 0) return( SUCCESS );

	if(!first_file_argument) first_file_argument = i;
	if(!outof_memory){
#ifdef MSDOS
	    struct wildcard_expansion *name_list,*np;
	    np = expand_filename(argv[i]);
	    while(np){
		buff_openbuffer(np->we_name,0,readonly_flag);
		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,(char *)name_list);
	    }/* End While */
#else
	    buff_openbuffer(argv[i],0,readonly_flag);
#endif
	}/* End IF */

    }/* End FOR */

    buff_openbuffnum(1,0);

    if(piped_input_flag){
	buff_openbuffnum(-1,0);
	buff_readfd(curbuf,"Standard Input",fileno(stdin));
	curbuf->dot = 0;
    }/* End IF */

    return( SUCCESS );

}/* End Routine */
#endif

#ifdef VMS

int
handle_command_line(which_time,argc,argv)
int which_time;
int argc;
char **argv;
{
register int i,j;
int count;
register char *cp,*dp;
struct wildcard_expansion *name_list,*np;
int number_of_buffers_opened = 0;
char switch_buffer[TECO_FILENAME_TOTAL_LENGTH];
char *command_line;
struct dsc$descriptor command_desc;
struct dsc$descriptor symbol_value;
$DESCRIPTOR (symbol_name,"teco_spawn_execute");
long table = LIB$K_CLI_LOCAL_SYM;
long flags = 0;
long status;
long completion_status;
long sub_pid;

    PREAMBLE();

    for(i = 1; i < argc; i++){
	cp = argv[i];
	if(*cp == '/'){
	    cp++;
	    dp = switch_buffer;
	    while(isalnum(*cp)) *dp++ = *cp++;
	    *dp = '\0';
	    if(strcmp(switch_buffer,"subjb") == 0){
		continue;
	    }/* End IF */

	    else if(strcmp(switch_buffer,"nosuspend") == 0){
		suspend_is_okay_flag = NO;
	    }/* End IF */

	    else if(strcmp(switch_buffer,"nocheckpoint") == 0){
		checkpoint_enabled = NO;
	    }/* End IF */

	    else if(strcmp(switch_buffer,"spawn") == 0){
		if(which_time == 1){
		    cp -= 5;
		    dp = "subjb";
		    while(*dp) *cp++ = *dp++;
		    strcpy(switch_buffer,"$ ");
		    strcat(switch_buffer,argv[0]);
		    symbol_value.dsc$w_length = strlen(switch_buffer);
		    symbol_value.dsc$b_dtype = 0;
		    symbol_value.dsc$b_class = DSC$K_CLASS_S;
		    symbol_value.dsc$a_pointer = switch_buffer;
		    status = lib$set_symbol(&symbol_name,&symbol_value,&table);
		    if(status != SS$_NORMAL){
			perror("could not create symbol 'teco_spawn_execute'");
			punt(status);
		    }/* End IF */
		    count = strlen(symbol_name.dsc$a_pointer);
		    for(j = 1; j < argc; j++){
			count += strlen(argv[j]) + 1;
		    }/* End FOR */
		    command_line = tec_alloc(TYPE_C_CBUFF,count+8);
		    if(command_line == NULL) return(FAIL);
		    dp = command_line;
		    cp = symbol_name.dsc$a_pointer;
		    while(*dp++ = *cp++); dp--;
		    for(j = 1; j < argc; j++){
			*dp++ = ' ';
			cp = argv[j];
			while(*dp++ = *cp++);
			dp--;
		    }/* End FOR */
		    command_desc.dsc$w_length = strlen(command_line);
		    command_desc.dsc$b_dtype = 0;
		    command_desc.dsc$b_class = DSC$K_CLASS_S;
		    command_desc.dsc$a_pointer = command_line;
    
		    printf("VTECO: Spawning subjob for edit\n");
    
		    status = lib$spawn(&command_desc,0,0,&flags,0,
			&sub_pid,&completion_status,0,0,0,0,0);
		    if(status != SS$_NORMAL){
			perror("VTECO: LIB$SPAWN failed");
			punt(status);
		    }/* End IF */

		    exit(SS$_NORMAL);

		}/* End IF */
	    }/* End IF */

	    else {
		char tmp_message[LINE_BUFFER_SIZE];
		sprintf(
		    tmp_message,
		    "?Unknown VTECO switch '%s'",
		    switch_buffer
		);
		tec_error(EINVAL,tmp_message);
	    }/* End Else */

	}/* End IF */

	if(which_time == 0) return(SUCCESS);

	if(argv[i][0] != '/'){
	    np = expand_filename(argv[i]);
	    while(np){
		number_of_buffers_opened++;
		buff_openbuffer(np->we_name,0,readonly_flag);
		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,name_list);
	    }/* End While */
	}/* End IF */

    }/* End FOR */

    if(number_of_buffers_opened > 0) buff_openbuffer(0,1,readonly_flag);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Expand VMS wildcards to a list of files
 *
 * This routine returns a linked list of expanded filenames.
 */
struct wildcard_expansion *
expand_filename(wildcard_string)
char *wildcard_string;
{
struct wildcard_expansion *name_list,*np;
struct dsc$descriptor argument;
struct dsc$descriptor result;
struct {
	unsigned short curlen;
	char body[TECO_FILENAME_TOTAL_LENGTH];
}result_buffer;
int context;
int fnd_flags;
int status;
char flags = 0;
char *cp,*sp;

#define NODE_SEEN	(1<<0)
#define DEV_SEEN	(1<<1)
#define DIR_SEEN	(1<<2)
#define VER_SEEN	(1<<3)

    PREAMBLE();

/*
 * Determine which fields are present in the supplied string so that we
 * may trim the resulting filespecs some.
 */
    for(cp = wildcard_string; *cp != '\0'; cp++){
	switch(*cp){
	    case ':':
		if(cp[1] == ':'){
		    flags |= NODE_SEEN;
		    cp++;
		}/* End IF */
		else flags |= DEV_SEEN | DIR_SEEN;
		break;
	    case '[':
		flags |= DIR_SEEN;
		break;
	    case ';':
		if(cp[1] != '\0') flags |= VER_SEEN;
		break;
	}/* End Switch */
    }/* End FOR */

    argument.dsc$w_length = strlen(wildcard_string);
    argument.dsc$b_dtype = 0;
    argument.dsc$b_class = DSC$K_CLASS_S;
    argument.dsc$a_pointer = wildcard_string;

    result.dsc$w_length = sizeof(result_buffer.body);
    result.dsc$b_dtype = DSC$K_DTYPE_VT;
    result.dsc$b_class = DSC$K_CLASS_VS;
    result.dsc$a_pointer = &result_buffer;

    context = 0;
    fnd_flags = (1<<1);

    name_list = NULL;
    
    while(1){
	status = lib$find_file(&argument,&result,&context,0,0,0,&fnd_flags);
	if((status & 0xFFFF) != SS$_NORMAL) break;
	np = tec_alloc(TYPE_C_WILD, sizeof(struct wildcard_expansion));
	if(np == NULL) return(NULL);

/*
 * Trim the resulting filespec as much as possible
 */
	sp = result_buffer.body;
	sp[result_buffer.curlen] = '\0';

	if((flags & DEV_SEEN) == 0){
	    cp = sp;
	    while(*cp && *cp != ':') cp++;
	    if(*cp == ':') strcpy(sp,cp + 1);
	}/* End IF */

	if((flags & DIR_SEEN) == 0){
	    cp = sp;
	    while(*cp && *cp != '[') cp++;
	    if(*cp == '['){
		sp = cp + 1;
		while(*sp && *sp != ']') sp++;
		if(*sp == ']'){
		    strcpy(cp,sp+1);
		}/* End IF */
		sp = result_buffer.body;
	    }/* End IF */
	}/* End IF */

	if((flags & VER_SEEN) == 0){
	    cp = sp;
	    while(*cp && *cp != ';') cp++;
	    *cp = '\0';
	}/* End IF */

	if(strlen(sp) > (sizeof(np->we_name)-1)){
	    error_message("Filename too long");
	    continue;
	}/* End IF */

	strcpy(np->we_name,sp);
	np->we_next = name_list;
	name_list = np;
    }/* End While */

    lib$find_file_end(&context);

/*
 * If no expansion names at all, perhaps he specified a non-wild file which
 * he wants to create. If so, we will just use the wild specification as is.
 */
    if(name_list == NULL){
	np = tec_alloc(TYPE_C_WILD, sizeof(struct wildcard_expansion));
	if(np == NULL) return(NULL);
	if(strlen(wildcard_string) > (sizeof(np->we_name)-1)){
	    error_message("Filename too long");
	    return(NULL);
	}/* End IF */

	strcpy(np->we_name,wildcard_string);
	np->we_next = name_list;
	name_list = np;

    }/* End IF */

    return(name_list);

}/* End Routine */

/* END OF VMS CONDITIONAL CODE */

#endif



/**
 * \brief Expand a wildcard specification to a list of files
 *
 * This routine returns a linked list of expanded filenames.
 */
#if defined(UNIX) || defined(MSDOS)
struct wildcard_expansion *name_list;
struct wildcard_expansion *name_list_end;

struct wildcard_expansion *
expand_filename( char *wildcard_string )
{
char temp_name[TECO_FILENAME_TOTAL_LENGTH];
register char *cp,*sp;
struct passwd *pw;

    PREAMBLE();

    name_list = NULL;
    name_list_end = NULL;

/*
 * Else, start branching down into the directory list he specified
 */
    if(wildcard_string[0] == TECO_DIRSEP){
	process_directory(&wildcard_string[0],TECO_DIRSEP_S,2);
    }
#ifdef UNIX
    else if (wildcard_string[0] == '~'){
	cp = temp_name;
	sp = &wildcard_string[1];
	while(*sp && *sp != TECO_DIRSEP) *cp++ = *sp++;
	*cp++ = '\0';
	if(temp_name[0] == '\0'){
	    if((cp = getenv("HOME"))) strcpy(temp_name,cp);
	}
	else {
	    pw = getpwnam(temp_name);
	    if(pw) strcpy(temp_name,pw->pw_dir);
	}
	strcat(temp_name,sp);
	process_directory(temp_name,TECO_DIRSEP_S,2);
    }
#endif
    else {
	strcpy(temp_name,"." TECO_DIRSEP_S);
	strcat(temp_name,wildcard_string);
	process_directory(temp_name,".",1);
    }

    return(name_list);

}/* End Routine */

void
process_directory( char *wildstr, char *path, int flags )
{
char directory_path[TECO_FILENAME_TOTAL_LENGTH];
struct wildcard_expansion *np;
DIR *dirp;

struct dirent *dp;

struct stat statbuf;
int c;
char *cp;

    PREAMBLE();

/*
 * Find the next slash, and if there isn't one, then we have reached
 * the end of the file specification, and this should be a file rather
 * than a directory.
 */
    wildstr = (char *)strchr(wildstr,TECO_DIRSEP);

    directory_path[0] = '\0';

    if(wildstr == NULL){
	if(flags < 0){
	    if(stat(path,&statbuf) < 0) return;
#ifndef USE_S_MACS
	    if((statbuf.st_mode & S_IFMT) != S_IFREG) return;
#else
	    if(!S_ISREG(statbuf.st_mode)) return;
#endif
	}/* End IF */
	if(strlen(path) > (sizeof(np->we_name)-1)){
	    error_message("Filename too long");
	    return;
	}/* End IF */
	np = (struct wildcard_expansion *)
	  tec_alloc(TYPE_C_WILD, sizeof(struct wildcard_expansion));
	if(np == NULL) return;
	np->we_next = NULL;
	strcpy(np->we_name,path);
	if(name_list == NULL) name_list = np;
	if(name_list_end)name_list_end->we_next = np;
	name_list_end = np;
	return;
    }/* End IF */

#ifndef USE_S_MACS
    if(stat(path,&statbuf) < 0 || ((statbuf.st_mode & S_IFMT) != S_IFDIR)){
	return;
    }/* End IF */
#else
   if (stat(path,&statbuf) < 0 || !S_ISDIR(statbuf.st_mode)) {
        return;
    }
#endif

/*
 * This loop tests whether there are any wildcard characters in the
 * next directory. If not, don't go through the overhead of reading
 * directories, just simply hop down to that subdir.
 */
    cp = wildstr+1;
    while((c = *cp++)){
	switch(c){
	    case '*':
	    case '?':
	    case '[':
	    case '{':
		while(*cp != '\0' && *cp != TECO_DIRSEP) cp++;
		goto read_directory;
	    case '\0':
		strcpy(directory_path,path);
		strcat(directory_path,TECO_DIRSEP_S);
		strcat(directory_path,wildstr+1);
		process_directory(NULL,directory_path,-1);
		return;
	    case TECO_DIRSEP:
		cp--;
		*cp = '\0';
		switch(flags){
		    case 2:
			strcpy(directory_path,TECO_DIRSEP_S);
			strcat(directory_path,wildstr+1);
			flags = 0;
			break;
		    case 1:
			strcpy(directory_path,wildstr+1);
			flags = 0;
			break;
		    case 0:
		    case -1:
			strcpy(directory_path,path);
			strcat(directory_path,TECO_DIRSEP_S);
			strcat(directory_path,wildstr+1);
			break;
		}/* End Switch */

		*cp = TECO_DIRSEP;
		process_directory(wildstr+1,directory_path,flags);
		return;
	}/* End Switch */
    }/* End While */

    switch(flags){
	case 2:
	    strcat(directory_path,wildstr);
	    flags = 0;
	    break;
	case 1:
	    strcpy(directory_path,wildstr+1);
	    flags = 0;
	    break;
	case 0:
	case -1:
	    strcpy(directory_path,path);
	    strcat(directory_path,wildstr);
	    break;
    }/* End Switch */
    process_directory("",directory_path,flags);
    return;

read_directory:

    dirp = opendir(path);

    while((dp = readdir(dirp)) != NULL) {
#if !defined(_POSIX_SOURCE) && !defined(MSDOS)
	if(dp->d_ino == 0) continue;
#endif /* !_POSIX_SOURCE && !MSDOS */
	if(match_name(dp->d_name,wildstr+1)){
	    switch(flags){
		case 2:
		    strcpy(directory_path,TECO_DIRSEP_S);
		    strcat(directory_path,dp->d_name);
		    break;
		case 1:
		    strcpy(directory_path,dp->d_name);
		    break;
		case -1:
		case 0:
		    strcpy(directory_path,path);
		    strcat(directory_path,TECO_DIRSEP_S);
		    strcat(directory_path,dp->d_name);
	    }/* End Switch */
	    process_directory(cp,directory_path,-1);
	}/* End IF */
    }/* End While */

    closedir(dirp);

}/* End Routine */

#ifdef MSDOS
/* case-insensitive */
#define MATCH_CHR(A,B) (TOUPPER(A) == TOUPPER(B))
#define MATCH_WITHIN(L,X,H) (TOUPPER(L) <= TOUPPER(X) && TOUPPER(X) <= TOUPPER(H))
#else
/* case-sensitive */
#define MATCH_CHR(A,B) ((A) == (B))
#define MATCH_WITHIN(L,X,H) ((L) <= (X) && (X) <= (H))
#endif

/**
 * \brief Check whether a name satisfies a wildcard specification
 *
 * This routine attempts to do csh style wildcard matching so that
 * internal teco routines may support this behavior.
 */
int
match_name( char *name, char *pattern )
{
char temp_buff[TECO_FILENAME_TOTAL_LENGTH];
register int c;
register char *cp,*sp;
int match;
int previous_char;
int pattern_char;

    PREAMBLE();

    while(1){
	switch(pattern_char = *pattern++){
	    case TECO_DIRSEP:
	    case '\0':
		return(*name == '\0' ? 1 : 0);
	    case '?':
		return(*name == '\0' || *name == TECO_DIRSEP ? 0 : 1);
/*
 * Open-Bracket allows any of a list of characters to match, such as [abc],
 * or a range such as [a-c], or a combination such as [abg-mz].
 */
	    case '[':
		match = 0;
		c = *name;
		previous_char = 'A';
		while((pattern_char = *pattern++)){
		    if(MATCH_CHR(pattern_char,c)) match = 1;
		    if(pattern_char == ']'){
			if(match) break;
			return(0);
		    }/* End IF */
		    if(pattern_char == '-'){
			pattern_char = *pattern++;
			if(pattern_char == ']'){
			    pattern--;
			    pattern_char = 'z';
			}/* End IF */
			if(MATCH_WITHIN(previous_char,c,pattern_char)){
			    match = 1;
			}/* End IF */
		    }/* End IF */
		    previous_char = pattern_char;
		}/* End While */
		break;
/*
 * Brace allows a list of strings, any one of which may match, and any
 * one of which may contain further wildcard specifications.
 */
	    case '{':
		cp = temp_buff;
		while((pattern_char = *pattern++)){
		    if(pattern_char == ',' || pattern_char == '}'){
			sp = pattern;
			if(pattern_char == ','){
			    while((c = *sp++)){
				if(c == '}') break;
			    }/* End While */
			}/* End IF */
			while((*cp++ = *sp++));
			if(match_name(name,temp_buff)) return(1);
			if(pattern_char == '}') return(0);
			cp = temp_buff;
		    }/* End IF */
		    else *cp++ = pattern_char;
		}/* End While */
		break;
/*
 * Asterisk matches any string
 */
	    case '*':
		if(*pattern == '\0') return(1);
		if(*pattern == TECO_DIRSEP) return(1);
		for(c = 0; name[c] != '\0'; c++){
		    if(match_name(&name[c],pattern)) return(1);
		}/* End FOR */
		return(0);
	    default:
		if(!MATCH_CHR(pattern_char,*name)) return(0);
		break;
	}/* End Switch */
	name++;
    }/* End While */
}/* End Routine */

#endif /* UNIX || MSDOS */



/**
 * \brief Exit with a specified error code
 *
 * PUNT is called with an errno code with which we want to exit
 */
void
punt( int exit_code )
{
    PREAMBLE();

    exit(exit_code);

}/* End Routine */


/**
 * \brief Print an error string
 *
 * This routine is called previous to punt to print an error string
 */
void
tec_panic( char *string )
{
    PREAMBLE();

    fputs(string,stdout);
    fputs("\n",stdout);

#ifndef UNIX
    exit(1);
#else
    kill(getpid(),SIGQUIT);
#endif

}/* End Routine */

/**
 * \brief Exit with specified error code, printing an error string
 *
 * This routine is called to exit with the specified error code
 * after printing the supplied error string.
 */
void
tec_error( int code, char *string )
{
    PREAMBLE();

    fprintf(stderr,"%s\n",string);

#ifdef VMS
    exit(1);
#endif

    exit(code);

}/* End Routine */

#if DEBUG_UNUSED
/**
 * \brief Open a file to write debug output to
 *
 * This routine is called so that a log file is open for writing of
 * debugging messages.
 */
void
open_debug_log_file()
{
    PREAMBLE();

    teco_debug_log = fopen("teco.dbg","w");
    if(teco_debug_log == NULL){
	teco_debug_log = stderr;
    }/* End IF */

}/* End Routine */
#endif



#ifndef MSDOS

/**
 * \brief Map system dependent baud fields to a standard integer
 *
 * This routine takes an operating system specific baud rate
 * representation, and maps it into a simple integer value.
 */
int
map_baud( int input_baud_rate )
{
register int i;
int return_baud;

#ifdef UNIX

#ifndef B19200
#define B19200 B9600
#endif
#ifndef B38400
#define B38400 B9600
#endif

static unsigned int encoded_bauds[] = {
	B0,B50,B75,B110,B134,B150,B200,B300,B600,B1200,
	B1800,B2400,B4800,B9600,B19200,B38400,
	EXTA,EXTB };
static int equivalent_baudrates[] = {
	0,50,75,110,134,150,200,300,600,1200,
	1800,2400,4800,9600,19200,38400,
	9600,9600 };

/* END OF UNIX CONDITIONAL CODE */

#endif

#ifdef VMS
static unsigned char encoded_bauds[] = {
	TT$C_BAUD_50,TT$C_BAUD_75,TT$C_BAUD_110,TT$C_BAUD_134,
	TT$C_BAUD_150,TT$C_BAUD_300,TT$C_BAUD_600,TT$C_BAUD_1200,
	TT$C_BAUD_1800,TT$C_BAUD_2000,TT$C_BAUD_2400,TT$C_BAUD_3600,
	TT$C_BAUD_4800,TT$C_BAUD_7200,TT$C_BAUD_9600,TT$C_BAUD_19200 };
static int equivalent_baudrates[] = {
	50,75,110,134,
	150,300,600,1200,
	1800,2000,2400,3600,
	4800,7200,9600,19200 };

/* END OF VMS CONDITIONAL CODE */

#endif

    PREAMBLE();

/*
 * Default is a common baud rate incase we miss somehow
 */
    return_baud = 9600;

/*
 * Determine how many entries there are in the baud table
 */
    i = sizeof(encoded_bauds) / sizeof(encoded_bauds[0]);

/*
 * Now scan the table for our input baud rate
 */
    while(--i >= 0){
	if((unsigned)input_baud_rate != encoded_bauds[i]) continue;
	return_baud = equivalent_baudrates[i];
	break;
    }/* End While */

    return(return_baud);

}/* End Routine */

#endif /* !MSDOS */



/**
 * \brief Return 'errno' style error string
 *
 * This routine is called to return the error string associated with an
 * errno error code.
 */
char *
error_text( int err_num )
{

#ifdef VMS

    int sys_nerr;

static char *tec_errlist[] = {
    "success",				/* ESUCCESS	 */
    "not owner",			/* EPERM	 */
    "no such file or directory",	/* ENOENT	 */
    "no such process",			/* ESRCH	 */
    "interrupted system call",		/* EINTR	 */
    "i/o error",			/* EIO	 */
    "no such device or address",	/* ENXIO	 */
    "arg list too long",		/* E2BIG	 */
    "exec format error",		/* ENOEXEC	 */
    "bad file number",			/* EBADF	 */
    "no children",			/* ECHILD	 */
    "no more processes",		/* EAGAIN	 */
    "not enough core",			/* ENOMEM	 */
    "permission denied",		/* EACCES	 */
    "bad address",			/* EFAULT	 */
    "block device required",		/* ENOTBLK	 */
    "mount device busy",		/* EBUSY	 */
    "file exists",			/* EEXIST	 */
    "cross-device link",		/* EXDEV	 */
    "no such device",			/* ENODEV	 */
    "not a directory",			/* ENOTDIR	 */
    "is a directory",			/* EISDIR	 */
    "invalid argument",			/* EINVAL	 */
    "file table overflow",		/* ENFILE	 */
    "too many open files",		/* EMFILE	 */
    "not a typewriter",			/* ENOTTY	 */
    "text file busy",			/* ETXTBSY	 */
    "file too large",			/* EFBIG	 */
    "no space left on device",		/* ENOSPC	 */
    "illegal seek",			/* ESPIPE	 */
    "read-only file system",		/* EROFS	 */
    "too many links",			/* EMLINK	 */
    "broken pipe",			/* EPIPE	 */
    "math argument",			/* EDOM	 */
    "result too large"			/* ERANGE	 */
};

    PREAMBLE();

    sys_nerr = sizeof(tec_errlist) / sizeof(tec_errlist[0]) - 1;

/* END OF VMS CONDITIONAL CODE */

#endif

#if 0
    if(err_num < 0 || err_num > sys_nerr){
	return("unknown error");
    }/* End IF */
#endif

#if defined(UNIX) || defined(MSDOS)
#if 0
    return((char *)sys_errlist[err_num]);
#endif
    return( strerror( err_num ));
#endif

#ifdef VMS
    return(tec_errlist[err_num]);
#endif

}/* End Routine */
