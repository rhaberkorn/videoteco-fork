char *tecterm_c_version = "tecterm.c: $Revision: 1.1 $";

/*
 * $Date: 2007/12/10 21:59:21 $
 * $Source: /cvsroot/videoteco/videoteco/tecterm.c,v $
 * $Revision: 1.1 $
 * $Locker:  $
 */

/* tecterm.c
 * Terminal Escape Sequence Subroutines (termcap/terminfo et all)
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

#include "teco.h"
#include "tecparse.h"

#ifdef TERMINFO
#include <curses.h>
#include <term.h>
#endif

/*
 * Global Storage is defined here for lack of a better place.
 */

/*
 * Global Variables
 */
    extern int curx;
    extern int cury;
    extern int term_columns;
    extern int term_lines;
    char scr_outbuf[SCREEN_OUTPUT_BUFFER_SIZE];
    char *scr_outbuf_ptr;
    int scr_outbuf_left;
    char insert_delete_line_capability;

    void term_insert_line();
    void term_delete_line();
    void term_puts();
    int term_putc(int);
    void term_flush();

#ifdef TERMCAP
    char *tgetstr();
#endif

#ifdef TERMCAP
    int		termcap_co;
    int		termcap_li;
    int		termcap_sg;
    char	*termcap_cm;
    char	*termcap_ti;
    char	*termcap_te;
    char	*termcap_pc;
    char	*termcap_so;
    char	*termcap_se;
    char	*termcap_cd;
    char	*termcap_ce;
    char	*termcap_cs;
    char	*termcap_sf;
    char	*termcap_sr;
    char	*termcap_al;
    char	*termcap_dl;
    char	*termcap_AL_arg;
    char	*termcap_DL_arg;
#endif

#ifdef TERMINFO
    int		terminfo_co;
    int		terminfo_li;
    int		terminfo_magic_cookie_glitch;
#endif

/*
 * External Routines, etc.
 */

    extern int term_speed;
    extern char teco_startup;
    extern int tty_input_chan;
    extern int tty_output_chan;

/*
 * The following array gives the number of tens of milliseconds per
 * character for each speed as returned by gtty.  Thus since 300
 * baud returns a 7, there are 33.3 milliseconds per char at 300 baud.
 */
#ifdef TERMCAP
static
short	tmspc10[] = {
	0, 2000, 1333, 909, 743, 666, 500, 333, 166, 83, 55, 41, 20, 10
};
#endif

/* TERM_INIT - Initialize the terminal for cursor operations
 *
 * Function:
 *
 *	This entry point gives the terminal package a chance to initialize
 *	the terminal for cursor type operations.
 */
void
term_init()
{
/*
 * Initialize our output buffer.
 */
    scr_outbuf_ptr = scr_outbuf;
    scr_outbuf_left = SCREEN_OUTPUT_BUFFER_SIZE;

#ifdef TERMCAP
/*
 * "TI - String to begin programs that use cm"
 */
    term_puts(termcap_ti,1);

/*
 * Set a fast flag which says whether or not terminal is capable of insert and
 * delete line functions.
 */
    insert_delete_line_capability = 1;

    if(termcap_dl == NULL && termcap_DL_arg == NULL){
	insert_delete_line_capability = 0;
    }/* End IF */

    if(termcap_al == NULL && termcap_AL_arg == NULL){
	insert_delete_line_capability = 0;
    }/* End IF */

    if(termcap_cs && termcap_sf && termcap_sr){
	insert_delete_line_capability = 1;
    }/* End IF */
#endif

#ifdef TERMINFO
/*
 * Initialize terminfo package
 */
    setupterm((char *)0, 1, (int*)0);

    insert_delete_line_capability = 1;

    if(parm_insert_line == NULL && insert_line == NULL){
	insert_delete_line_capability = 0;
    }/* End IF */

    if(parm_delete_line == NULL && delete_line == NULL){
	insert_delete_line_capability = 0;
    }/* End IF */

    if(change_scroll_region && scroll_forward && scroll_reverse){
	insert_delete_line_capability = 1;
    }/* End IF */

    terminfo_magic_cookie_glitch = magic_cookie_glitch > 0 ? 
	magic_cookie_glitch : 0;

#endif

}/* End Routine */



/* TERM_FINISH - Here when we are exiting the editor
 *
 * Function:
 *
 *	This routine is called when TECO is being exited to give us a chance
 *	to cleanly leave the screen package. We put the cursor at the bottom
 *	of the screen, send any termination escape sequences that are required
 *	and flush it all out.
 */
void
term_finish()
{

/*
 * We want to leave the cursor at the bottom of the screen
 */
    term_goto(0,term_lines-1);

#ifdef TERMCAP

/*
 * Output any termination escape sequences
 */
    term_puts(termcap_te,1);

#endif

/*
 * Now flush that all out
 */
    term_flush();

}/* End Routine */



/* TERM_STANDOUT - Set standout mode on the terminal
 *
 * Function:
 *
 *	This routine is called to set the highlighting standout
 *	mode. All characters output from now on will typically
 *	be reverse video, or whatever attribute the terminal uses.
 */
void
term_standout()
{

#ifdef TERMCAP

    if(!termcap_sg) term_puts(termcap_so,1);

#endif

#ifdef TERMINFO

    if(terminfo_magic_cookie_glitch <= 0){
	tputs(enter_standout_mode,1,term_putc);
    }

#endif

}/* End Routine */

/* TERM_STANDEND - Clear standout mode on the terminal
 *
 * Function:
 *
 *	This routine is called to reset the highlighting standout
 *	mode. All characters output from now on will be in the
 *	normal rendition mode.
 */
void
term_standend()
{

#ifdef TERMCAP

    if(!termcap_sg) term_puts(termcap_se,1);

#endif

#ifdef TERMINFO

    if(terminfo_magic_cookie_glitch <= 0){
	tputs(exit_standout_mode,1,term_putc);
    }

#endif

}/* End Routine */

/* TERM_CLRTOEOL - Clear to End Of Line
 *
 * Function:
 *
 *	This routine sends the ESCape sequence to clear all characters
 *	from the present position until the end of the line.
 */
void
term_clrtoeol()
{
#ifdef TERMCAP

    term_puts(termcap_ce,1);

#endif

#ifdef TERMINFO

    tputs(clr_eol,1,term_putc);

#endif

}/* End Routine */

/* TERM_CLRTOBOT - Clear to End Of Screen
 *
 * Function:
 *
 *	This routine sends the ESCape sequence to clear all characters
 *	from the present position until the end of the terminal screen.
 */
void
term_clrtobot()
{

#ifdef TERMCAP

    term_puts(termcap_cd,1);


#endif

#ifdef TERMINFO

    tputs(clr_eos,1,term_putc);

#endif

}/* End Routine */



/* TERM_INSERT_LINE - Perform an insert-line terminal function
 *
 * Function:
 *
 *	This routine is called to output the insert-line escape sequence.
 *	It checks to see what insert-line capabilities the terminal has,
 *	and tries to use the optimum one.
 */
void
term_insert_line(position,count)
int position;
register int count;
{
/*
 * Test whether the terminal supports an insert line escape sequence which
 * takes an argument
 */
#ifdef TERMCAP
    if(count > 1 && termcap_AL_arg){
	term_goto(0,position);
	term_putnum(termcap_AL_arg,count,term_lines-position);
	return;
    }/* End IF */
#endif

#ifdef TERMINFO
    if(count > 1 && parm_insert_line){
	term_goto(0,position);
	tputs(
	    tparm(parm_insert_line,count),
	    term_lines-position,
	    term_putc
	);
	return;
    }/* End IF */
#endif

/*
 * Ok, here if either the terminal does not support insert line with an
 * argument, or if count happens to be 1
 */
#ifdef TERMCAP
    if(termcap_al){
	term_goto(0,position);
	while(count--){
	    term_puts(termcap_al,term_lines-position);
	}/* End While */
	return;
    }/* End IF */
#endif

#ifdef TERMINFO
    if(insert_line){
	term_goto(0,position);
	while(count--){
	    tputs(insert_line,term_lines-position,term_putc);
	}/* End While */
	return;
    }/* End IF */
#endif


/*
 * Hmm, termcap_al must be undefined. That must mean that cs is defined and
 * that we have to do this by using scroll regions.
 */
    if(position) term_scroll_region(position,term_lines-1);
    if(curx != 0 || cury != position) term_goto(0,position);
    while(count--){

#ifdef TERMCAP
	term_puts(termcap_sr,term_lines-position);
#endif

#ifdef TERMINFO
	tputs(scroll_reverse,term_lines-position,term_putc);
#endif

    }/* End While */
    if(position) term_scroll_region(0,term_lines-1);

#ifdef TERMCAP
    if(curx != 0 || cury != position) term_goto(0,term_lines-1);
#endif
#ifdef TERMINFO
	curx = cury = -1;	/* Unsure of present position... */
#endif

}/* End Routine */



/* TERM_DELETE_LINE - Perform an delete-line terminal function
 *
 * Function:
 *
 *	This routine is called to output the delete-line escape sequence.
 *	It checks to see what delete-line capabilities the terminal has,
 *	and tries to use the optimum one.
 */
void
term_delete_line(position,count)
int position;
register int count;
{
    term_goto(0,position);

/*
 * Test whether the terminal supports an delete line escape sequence which
 * takes an argument
 */
#ifdef TERMCAP
    if(count > 1 && termcap_DL_arg){
	term_putnum(termcap_DL_arg,count,term_lines-position);
	return;
    }/* End IF */
#endif

#ifdef TERMINFO
    if(count > 1 && parm_delete_line){
	term_goto(0,position);
	tputs(
	    tparm(parm_delete_line,count),
	    term_lines-position,
	    term_putc
	);
	return;
    }/* End IF */
#endif

/*
 * Ok, here if either the terminal does not support delete line with an
 * argument, or if count happens to be 1
 */
#ifdef TERMCAP
    if(termcap_dl){
	while(count--){
	    term_puts(termcap_dl,term_lines-position);
	}/* End While */
	return;
    }/* End IF */
#endif

#ifdef TERMINFO
    if(delete_line){
	term_goto(0,position);
	while(count--){
	    tputs(delete_line,term_lines-position,term_putc);
	}/* End While */
	return;
    }/* End IF */
#endif

/*
 * Hmm, termcap_dl must be undefined. That must mean that cs is defined and
 * that we have to do this by using scroll regions.
 */
    if(position) term_scroll_region(position,term_lines-1);
    if(curx != 0 || cury != (term_lines-1)) term_goto(0,term_lines-1);
    while(count--){

#ifdef TERMCAP
	term_puts(termcap_sf,term_lines-position);
#endif

#ifdef TERMINFO
	tputs(scroll_forward,term_lines-position,term_putc);
#endif

    }/* End While */
    if(position) term_scroll_region(0,term_lines-1);

#ifdef TERMCAP
    if(curx != 0 || cury != (term_lines-1)) term_goto(0,term_lines-1);
#endif
#ifdef TERMINFO
	curx = cury = -1;	/* Unsure of present position... */
#endif


}/* End Routine */



#ifdef TERMCAP

/* TERM_PUTNUM - Output a termcap string which takes a single argument
 *
 * Function:
 *
 *	This routine is called to output a termcap string which has an
 *	imbedded %d to be replaced with the single argument.
 */
int
term_putnum(termcap_string,argument,affected)
char *termcap_string;
int argument;
int affected;
{
register char *cp;
register char *dp;
register int c;
static char result[40];

    cp = termcap_string;
    if(cp == NULL) return(FAIL);
    dp = result;

/*
 * Loop until we hit the end of the string
 */
    while((c = *cp++)){
	if(c != '%'){
	    *dp++ = c;
	    continue;
	}/* End IF */

	switch(c = *cp++){
	    case 'd':
		if(argument < 10) goto one;
		if(argument < 100) goto two;
		/* fall through */
	    case '3':
		*dp++ = (argument / 100) + '0';
		argument %= 100;
		/* fall through */
	    case '2':
two:
		*dp++ = argument / 10 | '0';
one:
		*dp++ = argument % 10 | '0';
		continue;
	    case '%':
		*dp++ = c;
		continue;

	    case 'B':
		argument = (argument/10 << 4) + argument%10;
		continue;

	    case 'D':
		argument = argument - 2 * (argument%16);
		continue;
	    case 'p':
		cp++;	/* eat p1, p2 type stuff */
		continue;

	    default:
		return(FAIL);
	}/* End Switch */
    }/* End While */

    *dp++ = '\0';
    term_puts(result,affected);
    return(SUCCESS);

}/* End Routine */

#endif



#ifdef TERMCAP

/* TERM_PUTS - Output a termcap string with padding
 *
 * Function:
 *
 *	This routine is called to output a termcap string with padding added.
 */
void
term_puts(termcap_string,lines_affected)
register char *termcap_string;
int lines_affected;
{
register int delay;
register int mspc10;

/*
 * If this termcap string is undefined, we can't do much for him.
 */
    if(termcap_string == NULL) return;

/*
 * Convert the number representing the delay
 */
    delay = 0;
    while(isdigit((int)*termcap_string)){
	delay = delay * 10 + *termcap_string++ - '0';
    }/* End While */

/*
 * If there is a decimal place, read up to one decimal and then
 * eat all remaining decimal places.
 */
    delay *= 10;
    if(*termcap_string == '.'){
	termcap_string++;
	if(isdigit((int)*termcap_string)) delay += *termcap_string - '0';
	while(isdigit((int)*termcap_string)) termcap_string++;
    }/* End IF */

/*
 * If the delay is followed by a `*', then multiply by the affected
 * lines count.
 */
    if(*termcap_string == '*'){
	termcap_string++;
	delay *= lines_affected;
    }/* End IF */

/*
 * Output all the rest of the string
 */
    while(*termcap_string){
	*scr_outbuf_ptr++ = *termcap_string++;
	scr_outbuf_left -= 1;
	if(scr_outbuf_left <= 0){
	    term_flush();
	}/* End IF */
    }/* End While */

/*
 * If no delay is needed, or we can't figure out what the output
 * speed is, then we just give up on the delay.
 */
    if(delay == 0) return;
    if(term_speed <= 0) return;
    if(term_speed >= (sizeof(tmspc10)/sizeof(tmspc10[0]))) return;

/*
 * Round up by half a character frame, and then do the delay.
 */
    mspc10 = tmspc10[term_speed];
    delay += mspc10 / 2;
    delay /= mspc10;
    while(delay > 0){
	delay -= 1;
	*scr_outbuf_ptr++ = *termcap_pc;
	scr_outbuf_left -= 1;
	if(scr_outbuf_left <= 0){
	    term_flush();
	}/* End IF */
    }/* End While */

}/* End Routine */

#endif



/* TERM_GOTO - Send sequence to position the terminal cursor
 *
 * Function:
 *
 *	This routine provides direct cursoring capability by using the
 *	CM termcap/terminfo string. This string looks like a printf which
 *	must be interpreted by this code. This allows pretty much
 *	arbitrary terminal escape sequences to be supported. The meaning
 *	of the special characters in the string are:
 *
 *	%d	- Same as in printf
 *	%2	- like %2d
 *	%3	- like %3d
 *	%.	- gives %c hacking special case characters
 *	%+x	- like %c but adding x first
 *
 *	The following codes affect the state but don't use up a value
 *
 *	%>xy	if value > x add y
 *	%r	reverses row column
 *	%i	increments row column (for one origin indexing)
 *	%%	gives %
 *	%B	BCD (2 decimal digits encoded in one byte)
 *	%D	Delta Data (backwards bcd)
 *
 *	all other characters simply get output
 */
int
term_goto(dest_x,dest_y)
int dest_x;
int dest_y;
{
#ifdef TERMCAP
register char *cp;
register char *dp;
register int c;
register int which;
static char result[40];
static char added[10];
char	*UP = NULL;
char	*BC = NULL;
#endif /* TERMCAP */

#ifdef TERMCAP
int oncol = 0;
#endif /* TERMCAP */

#ifdef TERMINFO
    if(cursor_address == NULL) return(FAIL);
    tputs(tparm(cursor_address,dest_y,dest_x),1,term_putc);
    curx = dest_x;
    cury = dest_y;
    return(SUCCESS);
#endif

#ifdef TERMCAP
    cp = termcap_cm;

    if(cp == NULL) return(FAIL);
    dp = result;
    which = dest_y;

    added[0] = '\0';

    curx = dest_x;
    cury = dest_y;

/*
 * Loop until we hit the end of the string
 */
    while((c = *cp++)){
	if(c != '%'){
	    *dp++ = c;
	    continue;
	}/* End IF */

	switch(c = *cp++){
	    case 'p':
	    	cp++; /* for p1, p2, or p<n>, eat the digit, because I don't understand what it means yet */
		continue;
	    case 'n':
		dest_x ^= 0140;
		dest_y ^= 0140;
		goto setwhich;
	    case 'd':
		if(which < 10) goto one;
		if(which < 100) goto two;
		/* fall through */
	    case '3':
		*dp++ = (which / 100) + '0';
		which %= 100;
		/* fall through */
	    case '2':
	two:
		*dp++ = which / 10 | '0';
	one:
		*dp++ = which % 10 | '0';
	swap:
		oncol = 1 - oncol;
	setwhich:
		which = oncol ? dest_x : dest_y;
			continue;

	    case '>':
		if (which > *cp++) which += *cp++;
		else cp++;
		continue;

	    case '+':
		which += *cp++;
		/* fall into... */

	    case '.':
		if(
		    which == 0 ||
		    which == CNTRL_D ||
		    which == '\t' ||
		    which == '\n'
		){
		    if (oncol || UP){
			do {
			    (void) strcat(
					added,
					oncol ? (BC ? BC : "\b") : UP
			    );
			    which++;
			} while (which == '\n');
		    }/* End IF */
		}/* End IF */

		*dp++ = which;
		goto swap;

	    case 'r':
		oncol = 1;
		goto setwhich;

	    case 'i':
		dest_x++;
		dest_y++;
		which++;
		continue;

	    case '%':
		*dp++ = c;
		continue;

	    case 'B':
		which = (which/10 << 4) + which%10;
		continue;

	    case 'D':
		which = which - 2 * (which%16);
		continue;

	    default:
fprintf(stderr,"\n\r\n\rBad char in TGOTO string: %c\n",c);
sleep(5);
		return(FAIL);
	}/* End Switch */
    }/* End While */

    (void) strcpy(dp,added);
    term_puts(result,1);
    return(SUCCESS);

#else
    return(SUCCESS);
#endif


}/* End Routine */



#ifdef TERMCAP

/* TERM_SCROLL_REGION - Send sequence to set up a scroll region ala VT100
 *
 * Function:
 *
 *	This routine is identical to term_goto in most respects, except that
 *	it is sending the 'set scroll region' escape sequence instead of the
 *	cursor position sequence. The following escapes are defined for
 *	substituting row and column:
 *
 *	%d	- Same as in printf
 *	%2	- like %2d
 *	%3	- like %3d
 *	%.	- gives %c hacking special case characters
 *	%+x	- like %c but adding x first
 *
 *	The following codes affect the state but don't use up a value
 *
 *	%>xy	if value > x add y
 *	%r	reverses row column
 *	%i	increments row column (for one origin indexing)
 *	%%	gives %
 *	%B	BCD (2 decimal digits encoded in one byte)
 *	%D	Delta Data (backwards bcd)
 *
 *	all other characters simply get output
 */
int
term_scroll_region(top,bottom)
int top;
int bottom;
{
register char *cp;
register char *dp;
register int c;
register int which;
int onbottom = 0;
static char result[40];
static char added[10];

char	*UP = NULL;
char	*BC = NULL;

    cp = termcap_cs;
    if(cp == NULL) return(FAIL);
    dp = result;
    which = top;

    added[0] = '\0';

    curx = -1;
    cury = -1;

/*
 * Loop until we hit the end of the string
 */
    while((c = *cp++)){
	if(c != '%'){
	    *dp++ = c;
	    continue;
	}/* End IF */

	switch(c = *cp++){
	    case 'n':
		top ^= 0140;
		bottom ^= 0140;
		goto setwhich;
	    case 'd':
		if(which < 10) goto one;
		if(which < 100) goto two;
		/* fall through */
	    case '3':
		*dp++ = (which / 100) + '0';
		which %= 100;
		/* fall through */
	    case '2':
two:
		*dp++ = which / 10 | '0';
one:
		*dp++ = which % 10 | '0';
swap:
		onbottom = 1 - onbottom;
setwhich:
		which = onbottom ? bottom : top;
			continue;

	    case '>':
		if (which > *cp++) which += *cp++;
		else cp++;
		continue;

	    case '+':
		which += *cp++;
		/* fall into... */

	    case '.':
		if(
		    which == 0 ||
		    which == CNTRL_D ||
		    which == '\t' ||
		    which == '\n'
		){
		    if (onbottom || UP){
			do {
			    (void) strcat(
					added, 
					onbottom ? (BC ? BC : "\b") : UP
			    );
			    which++;
			} while (which == '\n');
		    }/* End IF */
		}/* End IF */

		*dp++ = which;
		goto swap;

	    case 'r':
		onbottom = 1;
		goto setwhich;

	    case 'i':
		top++;
		bottom++;
		which++;
		continue;

	    case '%':
		*dp++ = c;
		continue;

	    case 'B':
		which = (which/10 << 4) + which%10;
		continue;

	    case 'D':
		which = which - 2 * (which%16);
		continue;

	    default:
		return(FAIL);
	}/* End Switch */
    }/* End While */

    (void) strcpy(dp,added);
    term_puts(result,1);
    return(SUCCESS);

}/* End Routine */

#endif

#ifdef TERMINFO

/* TERM_SCROLL_REGION - Set up a scroll region
 *
 * Function:
 *
 *	This routine is called to set up a scrolling region on the
 *	terminal. This is typically being used to emulate insert/delete
 *	line.
 */
int
term_scroll_region(top,bottom)
int top;
int bottom;
{
    if(change_scroll_region == NULL) return(FAIL);

    tputs(tparm(change_scroll_region,top,bottom),1,term_putc);
    return(SUCCESS);

}/* End Routine */

#endif



/* TERM_PUTC - Output a single character
 *
 * Function:
 *
 *	This routine is called to output a single character to the screen
 *	output buffer. If this fills the output buffer, flush the buffer.
 */
int
term_putc(data)
//char data;
int data;
{
    *scr_outbuf_ptr++ = data;
    scr_outbuf_left -= 1;

    if(scr_outbuf_left <= 0){
	term_flush();
    }/* End IF */

    return( data );

}/* End Routine */

/* TERM_FLUSH - Flush any data in the output buffer
 *
 * Function:
 *
 *	This routine flushes any bytes sitting in the screen output
 *	buffer to the terminal.
 */
void
term_flush()
{
    register int i;

    i = SCREEN_OUTPUT_BUFFER_SIZE - scr_outbuf_left;

#ifdef UNIX
    if(i) write(tty_output_chan,scr_outbuf,(unsigned)i);
#endif

#ifdef VMS
    if(i){
	i = sys$qiow(0,tty_output_chan,IO$_WRITEVBLK|IO$M_NOFORMAT,0,0,0,
	    scr_outbuf,i,0,0,0,0);
	if(!(i & STS$M_SUCCESS)) exit(i);
    }/* End IF*/
#endif

    scr_outbuf_left = SCREEN_OUTPUT_BUFFER_SIZE;
    scr_outbuf_ptr = scr_outbuf;

}/* End Routine */



/* INIT_TERMCAP - Read in the termcap description of the tty
 *
 * Function:
 *
 *	This routine looks up the terminal description in the termcap file, and
 *	sets up the stuff for the screen package. It also checks that the tty
 *	has certain basic capabilities that we require.
 */
int
init_term_description()
{
    char termcap_description_buffer[TERMCAP_BUFFER_SIZE];
    char *terminal_name;
#ifdef TERMCAP
    char *cp;
#endif
    char temp_string[256];
    register int status;

    if(teco_startup == NO) return( SUCCESS );

    if((terminal_name = getenv("TECO_TERM")) == NULL){
	if((terminal_name = getenv("TERM")) == NULL){
	    tec_error(ENOENT,"Environment variable TERM not found");
	}/* End IF */
    }/* End IF */

    status = tgetent(termcap_description_buffer,terminal_name);
    if(status == -1) tec_error(EIO,"Cannot read termcap file");
    if(status == 0){
	sprintf(temp_string,"No termcap entry for terminal type %s",
	  terminal_name);
	tec_error(ESRCH,temp_string);
    }/* End IF */

/*
 * Determine how many rows and columns this terminal has. If either number
 * is already non-zero, then it has been set up by some previous code that
 * really knows the size of the terminal window, and in this case, we will
 * leave it alone.
 */
#ifdef TERMCAP
    if(!term_columns) term_columns = tgetnum("co");
    if(!term_lines) term_lines = tgetnum("li");
#endif

#ifdef TERMINFO
    if(!term_columns) term_columns = tgetnum("columns");
    if(!term_lines) term_lines = tgetnum("lines");
#endif

#ifdef TERMCAP
/*
 * Now we read the escape sequence description strings for termcap
 */

/*
 * CM is the basic cursor movement string. This allows you to position the
 * cursor to any line,column on the terminal.
 */
    cp = &temp_string[0];
    termcap_cm = NULL;
    if(tgetstr("cm",&cp)){
	termcap_cm = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_cm == NULL) return(FAIL);
	(void) strcpy(termcap_cm,temp_string);
    }/* End IF */
    else {
	tec_error(ENOTTY,"?TECO: Terminal does not support cursor addressing");
    }/* End Else */

/*
 * TI is a string which needs to be output at init time by programs
 * which are going to use CM
 */
    cp = &temp_string[0];
    termcap_ti = NULL;
    if(tgetstr("ti",&cp)){
	termcap_ti = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_ti == NULL) return(FAIL);
	(void) strcpy(termcap_ti,temp_string);
    }/* End IF */

/*
 * TE is a string which needs to be output before exit time by programs
 * which have been using CM
 */
    cp = &temp_string[0];
    termcap_te = NULL;
    if(tgetstr("te",&cp)){
	termcap_te = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_te == NULL) return(FAIL);
	(void) strcpy(termcap_te,temp_string);
    }/* End IF */

/*
 * PC is the padd character which is used to fill for terminals which
 * require some time after certain sequences.
 */
    cp = &temp_string[0];
    termcap_pc = NULL;
    if(tgetstr("pc",&cp)){
	termcap_pc = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_pc == NULL) return(FAIL);
	(void) strcpy(termcap_pc,temp_string);
    }/* End IF */
    else termcap_pc = "";

/*
 * SG is the number of screen locations it takes to set or clear
 * standout mode. It alerts us to brain damaged magic cookie ttys
 */
    termcap_sg = tgetnum("sg");
    if(termcap_sg < 0) termcap_sg = 0;

/*
 * SO is the string which will cause the terminal to enter reverse
 * video (stand-out) mode.
 */
    cp = &temp_string[0];
    termcap_so = NULL;
    if(tgetstr("so",&cp)){
	termcap_so = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_so == NULL) return(FAIL);
	(void) strcpy(termcap_so,temp_string);
    }/* End IF */

/*
 * SE is the string which will cause the terminal to leave reverse
 * video (stand-out) mode and re-enter normal drawing mode.
 */
    cp = &temp_string[0];
    termcap_se = NULL;
    if(tgetstr("se",&cp)){
	termcap_se = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_se == NULL) return(FAIL);
	(void) strcpy(termcap_se,temp_string);
    }/* End IF */

/*
 * CD is the string which will cause the terminal to clear from
 * the present cursor location to the end of the screen
 */
    cp = &temp_string[0];
    termcap_cd = NULL;
    if(tgetstr("cd",&cp)){
	termcap_cd = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_cd == NULL) return(FAIL);
	(void) strcpy(termcap_cd,temp_string);
    }/* End IF */

/*
 * CE is the string which will cause the terminal to clear from
 * the present cursor location to the end of the current line
 */
    cp = &temp_string[0];
    termcap_ce = NULL;
    if(tgetstr("ce",&cp)){
	termcap_ce = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_ce == NULL) return(FAIL);
	(void) strcpy(termcap_ce,temp_string);
    }/* End IF */

/*
 * cs is the string which will cause the terminal to change the scroll region
 * This is mostly useful in emulating insert / delete line on vt100 terminals
 */
    cp = &temp_string[0];
    termcap_cs = NULL;
    if(tgetstr("cs",&cp)){
	termcap_cs = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_cs == NULL) return(FAIL);
	(void) strcpy(termcap_cs,temp_string);
    }/* End IF */

/*
 * sf is the string which will cause the terminal to scroll forward. This is
 * used in conjunction with scroll regions for delete line.
 */
    cp = &temp_string[0];
    termcap_sf = NULL;
    if(tgetstr("sf",&cp)){
	termcap_sf = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_sf == NULL) return(FAIL);
	(void) strcpy(termcap_sf,temp_string);
    }/* End IF */

/*
 * sr is the string which will cause the terminal to scroll in reverse. This
 * is used in conjunction with scroll regions for insert line.
 */
    cp = &temp_string[0];
    termcap_sr = NULL;
    if(tgetstr("sr",&cp)){
	termcap_sr = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_sr == NULL) return(FAIL);
	(void) strcpy(termcap_sr,temp_string);
    }/* End IF */

/*
 * al is the string which will cause the terminal to insert a blank line at the
 * present cursor location.
 */
    cp = &temp_string[0];
    termcap_al = NULL;
    if(tgetstr("al",&cp)){
	termcap_al = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_al == NULL) return(FAIL);
	(void) strcpy(termcap_al,temp_string);
    }/* End IF */

/*
 * dl is the string which will cause the terminal to delete the current line.
 * The following lines will all scroll up.
 */
    cp = &temp_string[0];
    termcap_dl = NULL;
    if(tgetstr("dl",&cp)){
	termcap_dl = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_dl == NULL) return(FAIL);
	(void) strcpy(termcap_dl,temp_string);
    }/* End IF */

/*
 * AL is the string which will cause the terminal to insert a blank line at the
 * present cursor location.
 */
    cp = &temp_string[0];
    termcap_AL_arg = NULL;
    if(tgetstr("AL",&cp)){
	termcap_AL_arg = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_AL_arg == NULL) return(FAIL);
	(void) strcpy(termcap_AL_arg,temp_string);
    }/* End IF */

/*
 * DL is the string which will cause the terminal to delete the current line.
 * The following lines will all scroll up.
 */
    cp = &temp_string[0];
    termcap_DL_arg = NULL;
    if(tgetstr("DL",&cp)){
	termcap_DL_arg = tec_alloc(TYPE_C_CPERM,strlen(temp_string)+1);
	if(termcap_DL_arg == NULL) return(FAIL);
	(void) strcpy(termcap_DL_arg,temp_string);
    }/* End IF */

#endif

    return(SUCCESS);

}/* End Routine */



#ifdef VMS
char *current_termcap_description;
char *termcap =
"d1|vt100-80|vt-100|dec vt100:\
:co#80:li#24:cl=50\\E[;H\\E[2J:bs:am:cm=5\\E[%i%2;%2H:nd=2\\E[C:up=2\\E[A:\
:ce=3\\E[K:cd=50\\E[J:so=2\\E[7m:se=2\\E[m:us=2\\E[4m:ue=2\\E[m:\
:is=\\E>\\E[?3l\\E[?4l\\E[?5l\\E[?7h\\E[?8h:ks=\\E[?1h\\E=:ke=\\E[?1l\\E>:\
:ku=\\E[A:kd=\\E[B:kr=\\E[C:kl=\\E[D:\
:cs=\\E[%i%d;%dr:sf=5\\ED:\
:kh=\\E[H:k1=\\EOP:k2=\\EOQ:k3=\\EOR:k4=\\EOS:pt:sr=5\\EM:xn:xv:";


/* TGETENT - Version of TGETENT for Non-UNIX systems
 *
 * Function:
 *
 *	This routine is used on non-UNIX systems to return a termcap
 *	description into the specified buffer.
 */
int
tgetent(buffer,term_name)
char *buffer;
char *term_name;
{
register char *cp = NULL;
register char *dp;
char *temp_buffer = NULL;
char *termcap_filename;
FILE *fd;

/*
 * If current_termcap_description isn't set up yet, check for a logical
 * name which points to a termcap file, and see if we can find the entry
 * in there.
 */
    if(current_termcap_description == NULL){
	termcap_filename = getenv("TERMCAP");
	if(termcap_filename != NULL){
	    fd = fopen(termcap_filename,"r");
	    if(fd != NULL){
		temp_buffer = malloc(TERMCAP_BUFFER_SIZE);
		if(temp_buffer == NULL) return(FAIL);
		if(scan_termcap(fd,temp_buffer,term_name)){
		    cp = temp_buffer;
		}/* End IF */
		fclose(fd);
	    }/* End IF */
	}/* End IF */
    }/* End IF */

    if(cp == NULL) cp = termcap;
    dp = current_termcap_description = buffer;

    while(*cp != NULL){
	if(cp[0] == '\\'){
	    if(cp[1] == 'E' || cp[1] == 'e'){
		*dp++ = ESCAPE;
		cp += 2;
		continue;
	    }/* End IF */
	}/* End IF */

	*dp++ = *cp++;

    }/* End While */

    if(temp_buffer){
	free(temp_buffer);
	temp_buffer = NULL;
    }/* End IF */

    return(1);

}/* End Routine */

/* SCAN_TERMCAP - Attempt to find a terminal description in the termcap file
 *
 * Function:
 *
 *	This routine scans the termcap file for an entry which matches our
 *	terminal, and returns non-zero if it finds it.
 */
scan_termcap(fd,temp_buffer,term_name)
FILE *fd;
char *temp_buffer;
char *term_name;
{
register char *cp;
char *temp_name;
char status;

/*
 * Loop reading terminal descriptions into the buffer
 */
    while(1){
	cp = temp_buffer;
	bzero(cp,TERMCAP_BUFFER_SIZE);
	if(fgets(cp,TERMCAP_BUFFER_SIZE,fd) == NULL) return(0);
	if(*cp == '#') continue;
/*
 * Ok, we have the start of an entry. Check for continuation.
 */
	while(cp[0]){
	    if(cp[0] == '\n'){
		cp[1] = '\0';
		break;
	    }/* End IF */
	    if(cp[0] == '\\' && cp[1] == '\n'){
		if(fgets(cp,TERMCAP_BUFFER_SIZE-(cp-temp_buffer),fd) == NULL){
		    cp[0] = '\n'; cp[1] = '\0';
		}/* End IF */
	    }/* End IF */
	    cp++;
	}/* End While */

	cp = temp_name = temp_buffer;

	while(*cp){
	    if(*cp == '|'){
		*cp = '\0';
		status = strcmp(temp_name,term_name);
		*cp = '|';
		if(status == 0) return(1);
		cp++;
		temp_name = cp;
		continue;
	    }/* End IF */
	    if(*cp == ':'){
		*cp = '\0';
		status = strcmp(temp_name,term_name);
		*cp = ':';
		if(status == 0) return(1);
		break;
	    }/* End IF */
	    cp++;
	}/* End While */

    }/* End While */

    return(0);

}/* End Routine */



/* TGETNUM - Version of TGETNUM for Non-UNIX systems
 *
 * Function:
 *
 *	This routine is used on non-UNIX systems to return a specific
 *	termcap number from the termcap description of our terminal.
 */
int
tgetnum(num_name)
char *num_name;
{
register char *cp;
int temp;
char minus_seen = 0;

    cp = current_termcap_description;
    if(strlen(num_name) != 2){
	fprintf(stderr,"TGETNUM: capability name %s should be 2 bytes long\n",
	    num_name);
	exit(1);
    }/* End IF */

    for(cp = current_termcap_description; *cp != NULL; cp++){
	if(cp[0] != ':') continue;
	if(cp[1] != num_name[0]) continue;
	if(cp[2] != num_name[1]) continue;

	cp += 3;
	if(*cp == '#') cp++;
	temp = 0;
	if(*cp == '-'){
	    minus_seen = 1;
	    cp++;
	}/* End IF */

	while(isdigit(*cp)){
	    temp = temp * 10 + *cp++ - '0';
	}/* End While */

	if(minus_seen) temp = 0 - temp;
	return(temp);

    }/* End FOR */

   return(0);

}/* End Routine */

/* TGETSTR - Version of TGETSTR for Non-UNIX systems
 *
 * Function:
 *
 *	This routine is used on non-UNIX systems to return a specific
 *	termcap string from the termcap description of our terminal.
 */
char *
tgetstr(str_name,buffer_ptr)
char *str_name;
char **buffer_ptr;
{
register char *cp;
register char *dp;
char *buffer;

    buffer = *buffer_ptr;
    cp = current_termcap_description;
    dp = buffer;
    if(strlen(str_name) != 2){
	fprintf(stderr,"TGETSTR: capability name %s should be 2 bytes long\n",
	    str_name);
	exit(1);
    }/* End IF */

    for(cp = current_termcap_description; *cp != NULL; cp++){
	if(cp[0] != ':') continue;
	if(cp[1] != str_name[0]) continue;
	if(cp[2] != str_name[1]) continue;

	cp += 3;
	if(*cp == '=') cp += 1;
	while(*cp != ':' && *cp != NULL){
	    *dp++ = *cp++;
	}/* End While */
	*dp++ = '\0';

	return(buffer);

    }/* End FOR */

    return(NULL);
}

/* END OF VMS CONDITIONAL CODE */

#endif
