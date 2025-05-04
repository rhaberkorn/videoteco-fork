char *tecdisp_c_version = "tecdisp.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/tecdisp.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/**
 * \file tecdisp.c
 * \brief Terminal Screen Display Subroutines
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
 */
/*
 * External Variables
 */
    extern char teco_startup;
    extern char screen_startup;
    extern int term_columns;
    extern int term_lines;
    extern int forced_height,forced_width;
    extern int tty_input_chan;
    extern int tty_output_chan;
    extern char insert_delete_line_capability;
    extern char eight_bit_output_flag;
    extern char hide_cr_flag;
    extern char tab_expand[];
    extern int tab_width;

#ifdef TERMCAP
    extern int termcap_sg;
#endif

#ifdef TERMINFO
    extern int terminfo_magic_cookie_glitch;
#endif

void screen_message( char *string );

/*
 * Global Variables
 */
    int curx;
    int cury;
    short cur_mode;
    int cursor_x;
    int cursor_y;
    int screen_sequence;
    int echo_cnt;
    char ring_audible_bell;
    int buffer_li;
    int buffer_half_li;
    char format_lines_invalid;

    struct format_line *
		allocate_format_buffer(struct window *,struct buff_line *);
    struct format_line *
	screen_find_window_format_line(struct window *,struct buff_line *);
    struct window *create_window(int,int);
    void screen_optimize_lines(void);
    void screen_optimize_by_pattern_matching(void);
    void screen_gc_format_lines(void);
    void screen_free_window_format_lines(struct window *,struct buff_line *);
    void screen_account_for_delete_line(int y, int count);
    void screen_account_for_insert_line(int y, int count);
    int screen_rebuild_from_scratch(struct window *wptr);
    int screen_format_buff_line(struct window *wptr,struct buff_line *lbp);
    int screen_find_dot(struct window *wptr);

/*
 * Global structures
 */
    struct screen_line *saved_screen;
    char saved_screen_valid;

    int next_window_number = 1;
    struct window *curwin;
    struct window *window_list;
    struct format_line message_line;
    char message_flag = NO;
    struct format_line echo_line;

    struct format_line *format_line_free_list = NULL;
    struct format_line *format_line_alloc_list = NULL;

/*
 * External Routines, etc.
 */
    extern struct buff_header *curbuf;
    extern struct buff_header *buffer_headers;

    extern int term_speed;
    extern char input_pending_flag;
    extern char resize_flag;

/**
 * \brief Initialize the TECO screen package
 *
 * This entry point gives the screen package a chance to initialize
 * itself.
 */
int
screen_init()
{
register int i,j;
register struct window *wptr;
register struct screen_line *lp;
register short *sp;

    PREAMBLE();

/*
 * Call the terminal handling routines, and give them a chance to set
 * the terminal up.
 */
    term_init();

/*
 * Set the cursor to the upper left of the display and perform a clear to end
 * of display command. This has the effect of clearing the entire display.
 */
    term_goto(0,0);
    term_clrtobot();

/*
 * If this is a restart (from ^Z most likely), then that is all we have to do.
 * The rest of this is only to initialize data structures the first time
 * through
 */
    if(screen_startup == NO) return(SUCCESS);
    screen_startup = NO;

/*
 * Calculate how many lines are available for display of the edit buffer.
 */
    buffer_li = term_lines - SCREEN_RESERVED_LINES;
    buffer_half_li = ((buffer_li + 1) & ~1) / 2;

/*
 * Allocate memory for the saved screen structure. This is used to remember
 * what is currently showing on the terminal. Since we just cleared the screen,
 * we set the current contents of this to be all spaces (i.e., blank).
 */
    i = sizeof(struct screen_line) * term_lines;
    saved_screen = (struct screen_line *)tec_alloc(TYPE_C_SCR,i);
    if(saved_screen == NULL) return(FAIL);

    bzero(saved_screen,i);
    screen_sequence = 1;

    for(lp = saved_screen, i = 0; i < term_lines; i++,lp++){
	MAGIC_UPDATE(lp, MAGIC_SCREEN);
	lp->companion = NULL;
	lp->sequence = 0;
	lp->buffer = (short *)
	    tec_alloc(TYPE_C_SCRBUF,(int)(sizeof(short) * term_columns));
	sp = lp->buffer;
	for(j = 0; j < term_columns; j++,sp++) *sp = ' ';
    }/* End FOR */

    saved_screen_valid = 1;

/*
 * Allocate the initial window structure
 */
    window_list = wptr = 
	create_window(term_columns,term_lines - SCREEN_RESERVED_LINES);
    curwin = wptr;
    wptr->win_buffer = curbuf;

    lp = saved_screen;
    lp += term_lines - SCREEN_RESERVED_LINES - 1;
    wptr->win_label_line.fmt_saved_line = lp;
    lp->companion = &wptr->win_label_line;
/*
 * Now we set up the structures for our special lines which form the bottom
 * of the display screen.
 */
    echo_line.fmt_buffer_line = message_line.fmt_buffer_line = NULL;
    echo_line.fmt_buffer_size = message_line.fmt_buffer_size = term_columns;

    echo_line.fmt_buffer = (short *)
	tec_alloc(
	    TYPE_C_SCRBUF,
	    (int)(sizeof(short) * echo_line.fmt_buffer_size)
	);
    message_line.fmt_buffer = (short *)
	tec_alloc(
	    TYPE_C_SCRBUF,
	    (int)(sizeof(short)*message_line.fmt_buffer_size)
	);

    for(i = 0; i < term_columns; i++){
	message_line.fmt_buffer[i] = ' ';
	echo_line.fmt_buffer[i] = ' ';
    }/* End FOR */

    echo_line.fmt_buffer[0] = '*';
    echo_line.fmt_sequence = message_line.fmt_sequence = 1;
    echo_line.fmt_next_line = message_line.fmt_next_line = NULL;

    message_line.fmt_visible_line_position =
	echo_line.fmt_visible_line_position = -1;

    lp = saved_screen;
    lp += term_lines - SCREEN_RESERVED_LINES;

    message_line.fmt_permanent = 1;
    message_line.fmt_saved_line = lp;
    MAGIC_UPDATE(&magic_line, MAGIC_FORMAT_LOOKASIDE);
    lp->companion = &message_line;

    lp += 1;
    echo_line.fmt_permanent = 1;
    echo_line.fmt_saved_line = lp;
    MAGIC_UPDATE(&echo_line, MAGIC_FORMAT_LOOKASIDE);
    lp->companion = &echo_line;

    screen_label_line(curbuf," TECO",LABEL_C_TECONAME);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Cause the screen to be refreshed at a new size
 *
 * This function is called when the size of the terminal screen has
 * changed. This is common on a windowing terminal. This routine has
 * to clean up the existing screen database and re-initialize it.
 */
void
screen_resize()
{
register int i;
register struct window *wptr;
register struct screen_line *lp;

#ifdef SUN_STYLE_WINDOW_SIZING
    struct winsize os_window;
#endif

    PREAMBLE();

/*
 * The screen_gc_format_line is going to scavenge all the lines because
 * we're making them look not-used here. However, we still go ahead and
 * clobber the line_buffer's pointer, because we don't like the idea of
 * having hanging pointers pointed at our screen lines.
 */
    if(saved_screen_valid){
	for(lp = saved_screen, i = 0; i < term_lines; i++,lp++){
    	    if(lp->companion){
		if(lp->companion->fmt_saved_line != lp){
		    fprintf(stderr,
		    	    "lp->companion %p != ->fmt_saved_line %p\n",
		    	    lp->companion, lp->companion->fmt_saved_line);
		    CAUSE_BUS_ERROR();
		}/* End IF */
    		lp->companion->fmt_saved_line = NULL;
		lp->companion = NULL;
    	    }/* End IF */
	}/* End FOR */
    }/* End IF */

    screen_gc_format_lines();

/*
 * Return the memory used for the saved screen structure. This structure
 * is the one used to remember what is currently showing on the terminal.
 */
    if(saved_screen_valid){
	saved_screen_valid = 0;
	for(lp = saved_screen, i = 0; i < term_lines; i++,lp++){
	    if(lp->buffer) tec_release(TYPE_C_SCRBUF,(char *)lp->buffer);
	    lp->buffer = NULL;
	}/* End FOR */

	if(saved_screen) tec_release(TYPE_C_SCR,(char *)saved_screen);
	saved_screen = NULL;

    }/* End IF */

/*
 * Reclaim all the window structures
 */
    while(1){
	wptr = window_list;
	if(wptr == NULL) break;
	window_list = wptr->win_next_window;
/*
 * Release any memory used to hold the strings in the label line
 */
	for(i = 0; i < SCREEN_MAX_LABEL_FIELDS; i++){
	    if(wptr->win_label_field_contents[i]){
		tec_release(
		    TYPE_C_LABELFIELD,
		    wptr->win_label_field_contents[i]
		);
		wptr->win_label_field_contents[i] = NULL;
	    }/* End IF */
	}/* End FOR */
/*
 * Release the buffer used to hold formatted data
 */
	tec_release(TYPE_C_SCRBUF,(char *)wptr->win_label_line.fmt_buffer);
	tec_release(TYPE_C_WINDOW,(char *)wptr);

    }/* End While */

    window_list = NULL;

    if(echo_line.fmt_buffer){
	tec_release(TYPE_C_SCRBUF,(char *)echo_line.fmt_buffer);
    }/* End IF */

    if(message_line.fmt_buffer){
	tec_release(TYPE_C_SCRBUF,(char *)message_line.fmt_buffer);
    }/* End IF */
    echo_cnt = 0;

    echo_line.fmt_buffer_line = message_line.fmt_buffer_line = NULL;
    echo_line.fmt_buffer_size = message_line.fmt_buffer_size = 0;

/*
 * If the OS supports window size ioctls, we try for that since it
 * will tend to be more correct for a window environment.
 */
#ifdef SUN_STYLE_WINDOW_SIZING
    if(ioctl(tty_output_chan,TIOCGWINSZ,&os_window) >= 0){
	term_lines = os_window.ws_row;
	term_columns = os_window.ws_col;
    }/* End IF */
#endif
    if(forced_height > 0) term_lines = forced_height;
    if(forced_width > 0) term_columns = forced_width;

    if(term_lines == 0) term_lines = 24;
    if(term_columns == 0) term_columns = 80;

    resize_flag = NO;
    screen_startup = YES;
    screen_init();
    buff_switch(curbuf,0);
    parser_reset_echo();
    screen_echo('\0');
    screen_format_windows();

}/* End Routine */



/**
 * \brief Here when we are exiting the editor
 *
 * This routine is called when TECO is being exited to give us a chance
 * to cleanly leave the screen package. We put the cursor at the bottom
 * of the screen, send any termination escape sequences that are required
 * and flush it all out.
 */
void
screen_finish()
{

    term_finish();

}/* End Routine */



/**
 * \brief Redraw the screen based on new database
 *
 * This routine gets called after the new companion entries have been set
 * up for the saved_screen database. Therefore, we can run through the
 * lines and update only those which have changed.
 */
void
screen_refresh()
{
register int x,y;
register struct screen_line *lp;
register struct format_line *his_lp;
register short *sp,*osp;
char pos_flag;

    PREAMBLE();

    input_pending_flag = tty_input_pending();
    if(input_pending_flag == YES) return;

/*
 * First try to optimize output by performing insert/delete line operations
 */
    if(insert_delete_line_capability){
	screen_optimize_lines();
    }/* End IF */
/*
 * Go through each line and insure that the screen appearence is changed to
 * match those described by the line buffer structures.
 */
    for(y = 0; y < term_lines; y++){
	if(term_speed <= 2400){
	    term_flush();
	}/* End IF */

	input_pending_flag = tty_input_pending();
	if(input_pending_flag == YES) break;

/*
 * Get pointers to the saved screen entry which tells us what the screen
 * currently looks like, and pointers to the line buffer structure which
 * tells us what the screen ought to look like when we are done.
 */
	lp = saved_screen + y;
	osp = lp->buffer;
	his_lp = lp->companion;
/*
 * If there is no line buffer structure, the line ought to appear blank on
 * the screen. If it already is blank, no sweat, otherwise we want to do
 * a clear to end of line function. If the saved_screen sequence number
 * is zero, this is a flag that the line is already clear.
 */
	if(his_lp == NULL){
	    if(lp->sequence == 0) continue;
	    for(x = 0; x < term_columns; x++,osp++){
		if((*osp & SCREEN_M_DATA) != ' '){
		    if(cury != y || curx > x) term_goto(x,y);
		    if(cur_mode & SCREEN_M_REVERSE){
			term_standend();
			cur_mode &= ~SCREEN_M_REVERSE;
		    }/* End IF */
		    term_clrtoeol();
		    while(x < term_columns){
			*osp++ = ' ';
			x += 1;
		    }/* End While */
		}/* End IF */
	    }/* End FOR */
	    lp->sequence = 0;
	    continue;
	}/* End IF */
/*
 * If the line buffer sequence number is less than that of the saved screen
 * sequence number, we know that this line has not been changed, and thus we
 * don't even have to check to see whether they match.
 */
	if(his_lp->fmt_sequence <= lp->sequence) continue;
	lp->sequence = screen_sequence;
	sp = his_lp->fmt_buffer;
	his_lp->fmt_visible_line_position = y;
/*
 * Well, changes have been made, so we have to go through the line on a byte
 * by byte basis comparing character positions.
 */
	for(x = 0; x < term_columns; x++,sp++,osp++){
/*
 * If the characters are the same we don't have to take any special action
 * unless it is a newline in which case sometimes we have special things to do
 */
	    if((*sp & SCREEN_M_DATA) != '\n' && *osp == *sp){
		continue;
	    }/* End If */
/*
 * If it IS a newline, we want to make sure that the screen shows blanks
 * for the rest of this line. If it does not, we have to do a clear to
 * end of line operation and update the saved screen database to reflect
 * this.
 */
	    if((*sp & SCREEN_M_DATA) == '\n'){
		for(; x < term_columns; x++,osp++){
		    if(*osp == ' ') continue;
		    if(curx != x || cury != y) term_goto(x,y);
		    if(cur_mode & SCREEN_M_REVERSE){
			term_standend();
			cur_mode &= ~SCREEN_M_REVERSE;
		    }/* End IF */
		    term_clrtoeol();
		    for(; x < term_columns; x++){
			*osp++ = ' ';
		    }/* End FOR */
		}/* End FOR */
		break;
	    }/* End IF */
/*
 * Copy the byte into our database to reflect the new appearance of the screen
 */
	    *osp = *sp;
	    pos_flag = 1;
/*
 * If the current position of the cursor is not the position of the character
 * we want to update, we have to position to that place. If we are on the
 * wrong line, or if we are already to the right of the desired position,
 * we have to do a full position escape sequence.
 */
	    if(cury != y || curx > x){
		term_goto(x,y);
		pos_flag = 0;
	    }/* End IF */
/*
 * If we are not positioned yet, check to see if we are only a few positions
 * to the left of where we want to be. If this is the case, it is faster to
 * output the actual characters than to generate a full cursor motion escape
 * sequence.
 */
	    if(pos_flag && x - curx < 5){
		while(curx < x){
		    if((his_lp->fmt_buffer[curx] & SCREEN_M_REVERSE) !=
		       (cur_mode & SCREEN_M_REVERSE)) break;
		    term_putc(his_lp->fmt_buffer[curx] & SCREEN_M_DATA);
		    curx++;
		}/* End While */
		pos_flag = 0;
	    }/* End IF */
/*
 * If we get to here and the position is not correct, no shortcuts helped so
 * we have to output the full escape sequence to position the cursor
 */
	    if(curx != x || cury != y) term_goto(x,y);
/*
 * Check to see that we are in the correct mode (standout or not). If not,
 * we have to change it before we output the data byte
 */
	    if((*sp & SCREEN_M_REVERSE) != (cur_mode & SCREEN_M_REVERSE)){
		if(*sp & SCREEN_M_REVERSE){
		    term_standout();
		    cur_mode |= SCREEN_M_REVERSE;
		}/* End IF */
		else {
		    term_standend();
		    cur_mode &= ~SCREEN_M_REVERSE;
		}/* End Else */
	    }/* End IF */
/*
 * Output the data byte and record that that changes the current x position
 */
	    term_putc(*sp & SCREEN_M_DATA);
	    curx++;

	}/* End FOR */
    }/* End FOR */

/*
 * We don't want to leave the terminal in reverse video between screen updates,
 * so if we are in reverse video mode, put it back to non-reverse.
 */
    if(cur_mode & SCREEN_M_REVERSE){
	term_standend();
	cur_mode &= ~SCREEN_M_REVERSE;
    }/* End IF */

/*
 * Make sure that the cursor gets left in the position that indicates the
 * "dot" position in the edit buffer.
 */
    if(curx != cursor_x || cury != cursor_y){
	term_goto(cursor_x,cursor_y);
    }/* End IF */

/*
 * Force this all to be written to the terminal, and increment the sequence
 * number
 */
    term_flush();
    screen_gc_format_lines();
    screen_sequence += 1;

}/* End Routine */



/**
 * \brief Optimize output with insert/delete line operations
 *
 * This routine looks for left over mappings between our saved screen
 * array and the formatted format_line structures. Old mappings can
 * tell us how lines that are still going to be visible have moved.
 */
void
screen_optimize_lines()
{
register int y;
int vlp;
register struct screen_line *lp;
int blk_delete_count;
int blk_insert_count;
int companion_count;
int offset;

    PREAMBLE();

    companion_count = 0;

/*
 * Search for a line which is known to be out of place
 */
    for(y = 0; y < buffer_li; y++){
	lp = &saved_screen[y];
	if(lp->companion == NULL) continue;
	if((vlp = lp->companion->fmt_visible_line_position) == -1) continue;
	if(vlp != y) break;
	companion_count += 1;
	break;
    }/* End FOR */

    if(y == buffer_li){
	if(companion_count == 0) screen_optimize_by_pattern_matching();
	return;
    }/* End IF */

/*
 * Search for delete line opportunities
 */
    offset = 0;
    for(y = 0; y < term_lines; y++){
	lp = &saved_screen[y];
	if(lp->companion == NULL) continue;
	if((vlp = lp->companion->fmt_visible_line_position) == -1) continue;
	vlp += offset;
	if(vlp == y) continue;

	if(vlp > y){
	    blk_delete_count = vlp - y;
	    term_delete_line(y-offset,blk_delete_count);
	    screen_account_for_delete_line(y-offset,blk_delete_count);
	    continue;
	}/* End IF */

	if(vlp < y){
	    offset += y - vlp;
	}/* End IF */

    }/* End FOR */
/*
 * Search for insert line opportunities
 */
    for(y = 0; y < term_lines; y++){
	lp = &saved_screen[y];
	if(lp->companion == NULL) continue;
	if((vlp = lp->companion->fmt_visible_line_position) == -1) continue;
	if(vlp == y) continue;

	if(vlp > y){
	    screen_message("insert_line confused");
	}/* End IF */

	if(vlp < y){
	    blk_insert_count = y - vlp;
	    term_insert_line(vlp,blk_insert_count);
	    screen_account_for_insert_line(vlp,blk_insert_count);
	}/* End IF */

    }/* End FOR */

}/* End Routine */



/**
 * \brief Update screen database
 *
 * This routine is called when a delete line sequence is sent to
 * the terminal. It adjusts the screen database to reflect what
 * has happened to the terminal.
 */
void
screen_account_for_delete_line( int y, int count )
{
register int i,x;
register struct screen_line *top_lp,*bottom_lp;
register struct format_line *slp;
register short *sp;

    PREAMBLE();

/*
 * The result of a delete line operation will be that starting at the
 * location it was issued, lines will be replaced by the contents of
 * lines lower down on the screen.
 */
    top_lp = &saved_screen[y];
    bottom_lp = top_lp + count;

    for(i = y + count; i < term_lines; i++,top_lp++,bottom_lp++){
	sp = top_lp->buffer;
	top_lp->buffer = bottom_lp->buffer;
	bottom_lp->buffer = sp;

	top_lp->sequence = bottom_lp->sequence;
    }/* End FOR */

    top_lp = saved_screen;
    for(i = 0; i < term_lines; i++,top_lp++){
	if((slp = top_lp->companion) == NULL) continue;
	if(slp->fmt_visible_line_position >= y){
	    slp->fmt_visible_line_position -= count;
	}/* End IF */
	if(slp->fmt_visible_line_position < 0){
	    slp->fmt_visible_line_position = -1;
	}/* End IF */
    }/* End FOR */

    top_lp = &saved_screen[term_lines - count];
    for(i = term_lines - count; i < term_lines; i++,top_lp++){

	top_lp->sequence = 0;

	sp = top_lp->buffer;
	for(x = 0; x < term_columns; x++){
	    *sp++ = ' ';
	}/* End FOR */

    }/* End FOR */

}/* End Routine */

/**
 * \brief Update screen database
 *
 * This routine is called when an insert line sequence is sent to
 * the terminal. It adjusts the screen database to reflect what
 * has happened to the terminal appearence.
 */
void
screen_account_for_insert_line( int y, int count )
{
register int i,x;
register struct screen_line *top_lp,*bottom_lp;
register struct format_line *slp;
register short *sp;

    PREAMBLE();

    top_lp = &saved_screen[term_lines - count - 1];
    bottom_lp = &saved_screen[term_lines - 1];

    for(i = term_lines - count - 1; i >= y; i--,top_lp--,bottom_lp--){
	sp = bottom_lp->buffer;
	bottom_lp->buffer = top_lp->buffer;
	top_lp->buffer = sp;

	bottom_lp->sequence = top_lp->sequence;

    }/* End FOR */

    top_lp = saved_screen;
    for(i = 0; i < term_lines; i++,top_lp++){
	if((slp = top_lp->companion) == NULL) continue;
	if(slp->fmt_visible_line_position >= y){
	    slp->fmt_visible_line_position += count;
	}/* End IF */
	if(slp->fmt_visible_line_position >= term_lines){
	    slp->fmt_visible_line_position = -1;
	}/* End IF */
    }/* End FOR */

    top_lp = &saved_screen[y];
    for(i = 0; i < count; i++,top_lp++){

	top_lp->sequence = 0;

	sp = top_lp->buffer;
	for(x = 0; x < term_columns; x++){
	    *sp++ = ' ';
	}/* End FOR */
    }/* End FOR */

}/* End Routine */



/**
 * \brief More insert/delete line optimizations
 *
 * This routine is called previous to the brute force screen repaint,
 * but after the first crack at insert/delete line optimization. This
 * routine looks for patterns of lines which indicate insert/delete
 * line can save us some output.
 */
void
screen_optimize_by_pattern_matching()
{
register int x,y,i,c;
register struct screen_line *lp;
register struct format_line *his_lp;
register short *sp,*osp;
char stuff_flag;
int old_check[SCREEN_MAX_LINES],new_check[SCREEN_MAX_LINES];
int first_change,last_change;

    PREAMBLE();

    stuff_flag = 0;
    for(y = 0; y < buffer_li; y++){

/*
 * Get pointers to the saved screen entry which tells us what the screen
 * currently looks like, and pointers to the line buffer structure which
 * tells us what the screen ought to look like when we are done.
 */
	lp = saved_screen + y;
	osp = lp->buffer;
	his_lp = lp->companion;
/*
 * If there is no line buffer structure, the line ought to appear blank on
 * the screen. If it already is blank, no sweat, otherwise we want to do
 * a clear to end of line function.
 */
	new_check[y] = old_check[y] = 0;
	if(his_lp && his_lp->fmt_sequence <= lp->sequence) continue;
	if(his_lp == NULL && lp->sequence == 0) continue;
/*
 * Calculate the checksum for the data currently showing on the screen
 */
	for(x = 0; x < term_columns; x++,osp++){
	    if((c = *osp & SCREEN_M_DATA) != ' '){
		old_check[y] = (old_check[y] << 1) + c;
		stuff_flag = 1;
	    }/* End IF */
	}/* End FOR */

	if(his_lp == NULL) continue;
	sp = his_lp->fmt_buffer;
/*
 * Calculate the checksum for the data which we want to be showing on the
 * screen.
 */
	for(x = 0; x < term_columns; x++,sp++){
	    c = *sp & SCREEN_M_DATA;
	    if(c == ' ') continue;
	    if(c == '\n') break;
	    new_check[y] = (new_check[y] << 1) + c;
	    stuff_flag = 1;
	}/* End FOR */

    }/* End FOR */

/*
 * If no changes have been made, just return
 */
    if(!stuff_flag) return;

/*
 * Determine which region of the screen contains lines which have changed
 */
    first_change = last_change = -1;
    for(y = 0; y < buffer_li; y++){
	if(old_check[y] == new_check[y]) continue;
	first_change = y;
	break;
    }/* End FOR */

/*
 * If checksum arrays seem to be identical or only one line changed, then
 * just return and don't attempt further optimization.
 */
    if(first_change == -1) return;

    for(y = buffer_li - 1; y >= first_change; y--){
	if(old_check[y] == new_check[y]) continue;
	last_change = y;
	break;
    }/* End FOR */

    if(first_change == last_change) return;

/*
 * Here is the code which tries for a delete-line optimization
 */
    for(i = 1; i < last_change - first_change - 3; i++){
	if(old_check[first_change + i] == new_check[first_change]){
	    for(y = first_change; y < last_change - i; y++){
		if(old_check[y + i] != new_check[y]) break;
	    }/* End FOR */
	    if(y == (last_change - i)){

		term_delete_line(first_change,i);
		screen_account_for_delete_line(first_change,i);
		term_insert_line(last_change+1-i,i);
		screen_account_for_insert_line(last_change+1-i,i);

		return;

	    }/* End IF */
	}/* End IF */
    }/* End FOR */

/*
 * Here is the code which tries for a insert-line optimization
 */
    for(i = 1; i < last_change - first_change - 3; i++){
	if(old_check[first_change] == new_check[first_change + i]){
	    for(y = first_change; y < last_change - i; y++){
		if(old_check[y] != new_check[y + i]) break;
	    }/* End FOR */

	    if(y == (last_change - i)){
		term_delete_line(last_change+1-i,i);
		screen_account_for_delete_line(last_change+1-i,i);
		term_insert_line(first_change,i);
		screen_account_for_insert_line(first_change,i);
		break;
	    }/* End IF */

	}/* End IF */
    }/* End FOR */

}/* End Routine */



/**
 * \brief Echo the character
 *
 * This routine allows the parser to hand us input characters which need
 * to be echoed. The reason this is done here instead of by the normal
 * screen formatting routines is that some characters are echoed in a
 * different way than they are normally displayed. The most notable
 * example is newline which just indicates end of line in the normal
 * buffer output, but which gets echoed as '\<CR\>' when it is an echo
 * operation.
 */
void
screen_echo( char data )
{
register int i;

    PREAMBLE();

/*
 * If we are getting close to the right edge of the screen, we remove the
 * first 3/4 of the echo line and just redisplay the last 1/4 to make room
 * for future echo characters.
 */
    if(echo_cnt > (term_columns - 4)){
	for(i = 0; i < (term_columns / 4); i++){
	    echo_line.fmt_buffer[ 1 + i ] =
		echo_line.fmt_buffer[ 1 + echo_cnt - (term_columns / 4) + i ];
	}/* End FOR */
	echo_cnt = i;
	for(; i < term_columns-2; i++){
	    echo_line.fmt_buffer[ 1 + i ] = ' ';
	}/* End FOR */
    }/* End IF */

/*
 * Dispatch the byte to determine how it should be formatted. NULL is a
 * special case which is used to flag the fake cursor position on the
 * echo line. This is shown by outputing a reverse-video space to the
 * position.
 */
    switch(data){
	case '\0':

/*
 * If we are on a high-speed line, do the reverse-video space trick to
 * show a non-blinking cursor.
 */
	    if(term_speed > 1200){
		echo_line.fmt_buffer[ 1 + echo_cnt ] = ' ' | SCREEN_M_REVERSE;
		echo_line.fmt_buffer[ 2 + echo_cnt ] = ' ';
		echo_line.fmt_buffer[ 3 + echo_cnt ] = '\n';
	    }/* End IF */
/*
 * Otherwise, if we are on a low speed line, just insert a carriage return
 * so that the rest of the line is shown as blanks.
 */
	    else {
		echo_line.fmt_buffer[ 1 + echo_cnt ] = '\n';
	    }/* End IF */

	    echo_line.fmt_sequence = screen_sequence;
	    return;

	case ESCAPE:
	    data = '$';
	    break;
	case '\n':
	    for(i = 0; (unsigned)i < strlen("<CR>"); i++){
		screen_echo("<CR>"[i]);
	    }/* End FOR */
	    return;
	case RUBOUT:
	    return;
/*
 * Tabs are always echoed as a fixed length string of spaces regardless of
 * the screen position since normal tab algorithms look weird on the echo
 * line.
 */
	case '\t':
	    for(i = 0; (unsigned)i < strlen("    "); i++){
		screen_echo("    "[i]);
	    }/* End FOR */
	    return;
	default:
	    break;
    }/* End Switch */

/*
 * If this is a control character, echo as ^byte
 */
    if((unsigned char)data < ' '){
	screen_echo('^');
	screen_echo(data + 'A' - 1);
	return;
    }/* End IF */

/*
 * Here if it is a normal printable character. We can just echo the character
 * as is with no special stuff going on.
 */
    echo_line.fmt_buffer[ 1 + echo_cnt++ ] = data;
    echo_line.fmt_sequence = screen_sequence;

}/* End Routine */

/**
 * \brief Reset the echo line
 *
 * This routine is called to reset the echo line according to the current
 * list of command tokens. This is used after difficult things like rubout
 * which would be very difficult to back out of.
 */
void
screen_reset_echo( struct cmd_token *ct )
{

    PREAMBLE();

    echo_line.fmt_buffer[ 1 + echo_cnt ] = ' ';

    while(echo_cnt){
	echo_cnt -= 1;
	echo_line.fmt_buffer[ 1 + echo_cnt ] = ' ';
    }/* End While */

    while(ct){
	if(ct->opcode == TOK_C_INPUTCHAR){
	    screen_echo(ct->input_byte);
	}/* End IF */
	ct = ct->next_token;
    }/* End While */

    echo_line.fmt_buffer[ 1 + echo_cnt ] = '\n';

    echo_line.fmt_sequence = screen_sequence;

}/* End Routine */



/**
 * \brief Place a message in the message line
 *
 * This routine allows the parser to hand us messages which need to be
 * displayed. These can be error messages generated by the parse, or
 * they can be output messages from a user generated with the ^A command.
 */
void
screen_message( char *string )
{
register short *sp;
register int i;
register int c = 0;
register char *mep;
char multi_echo_buffer[32];

    PREAMBLE();

    message_flag = YES;
    sp = message_line.fmt_buffer;
    i = term_columns;
    mep = "";

    while(*mep || *string){
	while(*mep){
	    *sp++ = c = *mep++;
	    if(i-- <= 0) goto too_wide;
	    continue;
	}/* End While */

	c = *string++;

	switch(c){
//	    case NULL:
	    case 0:
		goto too_wide;
	    case ESCAPE:
		c = '$';
		break;
	    case '\n':
		mep = "<CR>";
		continue;
	    case RUBOUT:
		mep = "<RUBOUT>";
		continue;
	    case '\t':
		mep = "    ";
		continue;
	}/* End Switch */

	if((unsigned char)c < ' '){
	    multi_echo_buffer[0] = '^';
	    multi_echo_buffer[1] = c + 'A' - 1;
	    multi_echo_buffer[2] = '\0';
	    mep = multi_echo_buffer;
	    continue;
	}/* End IF */

	*sp++ = c;
	if(i-- <= 0) goto too_wide;
    }/* End While */

too_wide:
    if(c != '\n' && i > 0) *sp++ = '\n';
    message_line.fmt_sequence = screen_sequence;

}/* End Routine */

/**
 * \brief Place an error message in the message line
 *
 * This routine allows the parser to hand us error messages
 * which need to be displayed. In addition, the BELL character
 * is output to get the humans attention.
 */
void
error_message( char *string )
{
    PREAMBLE();

    screen_message(string);
    ring_audible_bell = 1;

}/* End Routine */

/**
 * \brief Reset the message line
 *
 * This routine is called to reset the message line to null
 */
void
screen_reset_message()
{
register int i;
register short *sp;

    PREAMBLE();

    if(message_flag == NO) return;
    message_flag = NO;
    sp = message_line.fmt_buffer;

    for(i = 0; i < term_columns; i++){
	*sp++ = ' ';
    }/* End FOR */

    message_line.fmt_sequence = screen_sequence;

}/* End Routine */

/**
 * \brief Copy out the current message
 *
 * This routine copies out the current message up to a certain length.
 */
void
screen_save_current_message(
								char *message_save_buff,
								int message_save_max_length )
{
int i;
short *sp;

    PREAMBLE();

    sp = message_line.fmt_buffer;
    i = term_columns;
    if(i >= message_save_max_length) i = message_save_max_length - 1;
    while(i-- > 0){
	if((*sp & SCREEN_M_DATA) == '\n') break;
	*message_save_buff++ = *sp++ & SCREEN_M_DATA;
    }
    *message_save_buff = '\0';

}/* End Routine */



/**
 * \brief Routine to change the contents of the label line
 *
 * This routine is called to change the label line
 */
int
screen_label_line(	struct buff_header *buffer, char *string, int field )
{
register short *sp;
register char *cp;
register int i;
register int column;
register struct window *wptr;

    PREAMBLE();

/*
 * If the field specified is out of range, punt
 */
    if(field < 0 || field >= SCREEN_MAX_LABEL_FIELDS){
	screen_message("Illegal field specified to label line\n");
	return(FAIL);
    }/* End IF */

/*
 * We want to modify the label in any window which is displaying this buffer.
 * There could be several windows with the same buffer displayed...
 */
    for(wptr = window_list ; wptr != NULL; wptr = wptr->win_next_window){
	if(wptr->win_buffer != buffer) continue;
/*
 * If no space has been allocated for this field yet, we need to call the
 * allocator. Notice the overkill in that each field gets more than enough
 * memory.
 */
	if(wptr->win_label_field_contents[field] == NULL){
	    wptr->win_label_field_contents[field] = 
		tec_alloc(TYPE_C_LABELFIELD,SCREEN_NOMINAL_LINE_WIDTH);
	    if(wptr->win_label_field_contents[field] == NULL){
		return(FAIL);
	    }/* End IF */
	    *(wptr->win_label_field_contents[field]) = '\0';
	}/* End IF */
/*
 * If the current contents of the field are already what we want, we don't
 * have to change a thing.
 */
	if(strcmp(string,wptr->win_label_field_contents[field]) == 0) continue;
	(void) strcpy(wptr->win_label_field_contents[field],string);
/*
 * Figure out which column the field begins on. This changes as fields change,
 * so it must be computed each time a change happens.
 */
	for(i = 0 , column = 0; i < field; i++){
	    if(wptr->win_label_field_contents[i] == NULL) continue;
	    column += strlen(wptr->win_label_field_contents[i]) + 1;
	}/* End FOR */
/*
 * Now we have to update the label line from this column onward until we
 * reach the end of the final field.
 */
	sp = &wptr->win_label_line.fmt_buffer[column];
	for(i = field; i < SCREEN_MAX_LABEL_FIELDS; i++){
	    cp = wptr->win_label_field_contents[i];
	    if(cp == NULL) continue;
	    while(*cp){
		if(column >= term_columns) break;
		*sp++ = *cp++ | SCREEN_M_REVERSE;
		column += 1;
	    }/* End While */
	    if(column >= term_columns) break;
	    *sp++ = ' ' | SCREEN_M_REVERSE;
	    column += 1;
	}/* End FOR */
/*
 * Blank fill to the end of the screen incase we shrank the label line.
 */
	while(column < term_columns){
#ifdef TERMCAP
	    *sp++ = termcap_sg ? '-' : ' ' | SCREEN_M_REVERSE;
#endif
#ifdef TERMINFO
	    *sp++ = (terminfo_magic_cookie_glitch > 0) ? 
			'-' : ' ' | SCREEN_M_REVERSE;
#endif
	    column += 1;
	}/* End While */
/*
 * Insure that the next screen refresh knows this line has changed
 */
	wptr->win_label_line.fmt_sequence = screen_sequence;

    }/* End FOR */

    return( SUCCESS );

}/* End Routine */



/**
 * \brief Routine to set the window number field
 *
 * This routine is called to modify the TECONAME field of the
 * label lines to reflect the window number.
 */
int
screen_label_window()
{
register short *sp;
register char *cp;
register int i;
register int column;
register struct window *wptr;
char tmp_buffer[SCREEN_NOMINAL_LINE_WIDTH];
int field = LABEL_C_TECONAME;

    PREAMBLE();

/*
 * We want to check the label in every window
 */
    for(wptr = window_list ; wptr != NULL; wptr = wptr->win_next_window){
/*
 * If no space has been allocated for this field yet, we need to call the
 * allocator. Notice the overkill in that each field gets more than enough
 * memory.
 */
	if(wptr->win_label_field_contents[field] == NULL){
	    wptr->win_label_field_contents[field] = 
		tec_alloc(TYPE_C_LABELFIELD,SCREEN_NOMINAL_LINE_WIDTH);
	    if(wptr->win_label_field_contents[field] == NULL){
		return(FAIL);
	    }/* End IF */
	    *(wptr->win_label_field_contents[field]) = '\0';
	}/* End IF */
/*
 * If the current contents of the field are already what we want, we don't
 * have to change a thing.
 */
	sprintf(tmp_buffer," TECO-%d",wptr->win_window_number);
	if(strcmp(tmp_buffer,wptr->win_label_field_contents[field]) == 0){
	    continue;
	}/* End IF */

	(void) strcpy(wptr->win_label_field_contents[field],tmp_buffer);

/*
 * Now we have to update the label line from this column onward until we
 * reach the end of the final field.
 */
	column = 0;
	sp = &wptr->win_label_line.fmt_buffer[column];
	for(i = 0; i < SCREEN_MAX_LABEL_FIELDS; i++){
	    cp = wptr->win_label_field_contents[i];
	    if(cp == NULL) continue;
	    while(*cp){
		if(column >= term_columns) break;
		*sp++ = *cp++ | SCREEN_M_REVERSE;
		column += 1;
	    }/* End While */
	    if(column >= term_columns) break;
	    *sp++ = ' ' | SCREEN_M_REVERSE;
	    column += 1;
	}/* End FOR */
/*
 * Blank fill to the end of the screen incase we shrank the label line.
 */
	while(column < term_columns){
	    *sp++ = ' ' | SCREEN_M_REVERSE;
	    column += 1;
	}/* End While */
/*
 * Insure that the next screen refresh knows this line has changed
 */
	wptr->win_label_line.fmt_sequence = screen_sequence;

    }/* End FOR */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Generate display contents for all windows
 *
 * This is the main entry point to the screen formatting function.
 * It causes screen format_lines to be generated for all the buffers
 * that are currently visible on the screen.
 */
void
screen_format_windows()
{
register struct window *wptr;

    PREAMBLE();

    for(wptr = window_list; wptr != NULL; wptr = wptr->win_next_window){
	screen_display_window(wptr);
    }/* End FOR */

}/* End Routine */

/**
 * \brief Reformat windows (because tab size changed)
 *
 * This entry point is called when the format buffers of all the
 * displayed windows need to be regenerated. The current case in
 * mind is when the user changes the tab stops - this causes all
 * the format lines to be obsolete.
 */
void
screen_reformat_windows()
{
register int i;
register struct screen_line *lp;

    PREAMBLE();

/*
 * The screen_gc_format_line is going to scavenge all the lines because
 * we're making them look not-used here. However, we still go ahead and
 * clobber the line_buffer's pointer, because we don't like the idea of
 * having hanging pointers pointed at our screen lines.
 */
    if(saved_screen_valid){
	for(lp = saved_screen, i = 0; i < term_lines; i++,lp++){
    	    if(lp->companion){
		if(lp->companion->fmt_saved_line != lp){
		    fprintf(stderr,
		    	    "lp->companion %p != ->fmt_saved_line %p\n",
		    	    lp->companion, lp->companion->fmt_saved_line);
		    CAUSE_BUS_ERROR();
		}/* End IF */
		if( lp->companion->fmt_permanent ) continue;
    		lp->companion->fmt_saved_line = NULL;
		lp->companion = NULL;
    	    }/* End IF */
	}/* End FOR */
    }/* End IF */

    screen_gc_format_lines();

}/* End Routine */

/**
 * \brief Generate display for a window
 *
 * This routine is called to build up the format line descriptor
 * structures which represent the edit buffer displayed in the
 * specified window.
 */
int
screen_display_window( struct window *wptr )
{
struct buff_header *hbp;
struct buff_line *tlbp,*blbp;
struct format_line *tsbp,*bsbp;
struct screen_line *sp;
register int i;
int lines;
int anchor_line;
char saw_dot;

    PREAMBLE();

/*
 * Find a line in our saved screen array that still maps to a buffer location
 */
    sp = saved_screen + wptr->win_y_base;
    anchor_line = -1;
    for(i = wptr->win_y_base; i < wptr->win_y_end; i++,sp++){
	if(sp->companion == NULL) continue;
	if(sp->companion->fmt_owning_buffer != wptr->win_buffer) continue;
	anchor_line = i;
	break;
    }/* End FOR */

    if(anchor_line < 0){
	return(screen_rebuild_from_scratch(wptr));
    }/* End IF */

/*
 * Now walk the top back
 */
    tsbp = sp->companion;
    tlbp = tsbp->fmt_buffer_line;
    while(i > wptr->win_y_base){

/*
 * If there is a format line in front of the one we are on, just back
 * up to it.
 */
	if(tsbp->fmt_prev_line){
	    tsbp = tsbp->fmt_prev_line;
	    i -= 1;
	    continue;
	}/* End IF */
/*
 * Else if there is a buffer line in front of us, get to the final
 * format line associated with it.
 */
	if(tlbp->prev_line != NULL){
	    tlbp = tlbp->prev_line;
	    tsbp = screen_find_window_format_line(wptr,tlbp);
	    if(tsbp == NULL){
		if(screen_format_buff_line(wptr,tlbp) == FAIL) return(FAIL);
		tsbp = screen_find_window_format_line(wptr,tlbp);
	    }/* End IF */
	    while(tsbp->fmt_next_line) tsbp = tsbp->fmt_next_line;
	    i -= 1;
	    continue;
	}/* End IF */
/*
 * If we got here, that means that before we could work back up to the old
 * top line, we hit the begining of the buffer. Give up.
 */
	return(screen_rebuild_from_scratch(wptr));

    }/* End While */

/*
 * If we make it to here, we have worked back to the top of the screen, now
 * we want to work down to the bottom of the screen.
 */
    lines = 1;
    blbp = tlbp;
    bsbp = tsbp;

/*
 * Determine the format_line that dot lies on so that as we work forward we can
 * keep an eye out for it.
 */
    hbp = wptr->win_buffer;
    buff_find_line(hbp,hbp->dot);
    screen_find_dot(wptr);
    saw_dot = 0;

    while(lines <= wptr->win_y_size){

/*
 * If we see the line that dot is on, remember it. The one case when we don't
 * do this is if it is the top line of the screen. The reason for this is to
 * avoid over-optimizing such that the user makes changes but never sees them
 * because the changes are above the top line of the screen.
 */
	if(bsbp == wptr->win_dot_format_line && lines != 1) saw_dot = 1;

/*
 * If there is a format line below our current bottom one, simply go
 * forward to it.
 */
	if(bsbp->fmt_next_line){
	    bsbp = bsbp->fmt_next_line;
	    lines += 1;
	    continue;
	}/* End IF */
/*
 * If we have to go on to the next buffer line, but there is not one there,
 * that means that we hit the end of the buffer.
 */
	if(blbp->next_line == NULL){
	    return(screen_rebuild_from_scratch(wptr));
	}/* End IF */
/*
 * Else, if there is another buffer line after this, go to the first
 * format line associated with it.
 */
	if(blbp->next_line != NULL){
	    blbp = blbp->next_line;
	    bsbp = screen_find_window_format_line(wptr,blbp);
	    if(bsbp == NULL){
		if(screen_format_buff_line(wptr,blbp) == FAIL) return(FAIL);
		bsbp = screen_find_window_format_line(wptr,blbp);
	    }/* End IF */
	    lines += 1;
	}/* End IF */

    }/* End While */

/*
 * Test to see whether we ever saw dot. If not, we need to do the scratch
 * screen rebuild.
 */
    if(curwin == wptr && saw_dot == 0){
	return(screen_rebuild_from_scratch(wptr));
    }/* End IF */

/*
 * Now we can start setting up the screen appearance
 */
    sp = saved_screen + wptr->win_y_base;

    for(i = wptr->win_y_base; i < wptr->win_y_end; i++,sp++){
	if(tlbp == NULL){
	    if(sp->companion) sp->companion->fmt_saved_line = NULL;
	    sp->companion = NULL;
	    continue;
	}/* End IF */

	if(sp->companion != tsbp){
	    if(sp->companion) sp->companion->fmt_saved_line = NULL;
	    sp->companion = NULL;
	    if(tsbp->fmt_saved_line) tsbp->fmt_saved_line->companion = NULL;
	    tsbp->fmt_saved_line = NULL;
	    sp->companion = tsbp;
	    tsbp->fmt_saved_line = sp;
	    tsbp->fmt_sequence = screen_sequence;
	}/* End IF */

	if(curwin == wptr && tsbp == wptr->win_dot_format_line){
	    cursor_y = i;
	    cursor_x = wptr->win_dot_screen_offset;
	}/* End IF */

	tsbp = tsbp->fmt_next_line;
	if(tsbp == NULL){
	    tlbp = tlbp->next_line;
	    if(tlbp){
		tsbp = screen_find_window_format_line(wptr,tlbp);
	    }/* End IF */
	}/* End IF */
    }/* End FOR */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Things have changed a lot, just rebuild it
 *
 * This routine is called to build up the format line descriptor
 * structures which represent the current edit buffer.
 */
int
screen_rebuild_from_scratch( struct window *wptr )
{
struct buff_header *hbp;
struct buff_line *tlbp,*blbp;
struct format_line *tsbp,*bsbp;
struct screen_line *scrbp;
register int i;
int lines,olines;

    PREAMBLE();

/*
 * Find the buffer line that dot is on, and then the format_line that
 * it is on.
 */
    hbp = wptr->win_buffer;
    tlbp = blbp = buff_find_line(hbp,hbp->dot);
    if(screen_find_dot(wptr) == FAIL) return(FAIL);
    tsbp = bsbp = wptr->win_dot_format_line;

    lines = 1;

/*
 * Now walk the top back and the bottom forward
 */
    while(lines < wptr->win_y_size){
	olines = lines;

/*
 * If there is a format_line in front of the one we are on, just back
 * up to it.
 */
	if(tsbp->fmt_prev_line){
	    tsbp = tsbp->fmt_prev_line;
	    lines += 1;
	}/* End IF */

/*
 * Else if there is a buffer line in front of us, get to the final
 * format_line associated with it.
 */
	else if(tlbp->prev_line != NULL){
	    tlbp = tlbp->prev_line;
	    tsbp = screen_find_window_format_line(wptr,tlbp);
	    if(tsbp == NULL){
		if(screen_format_buff_line(wptr,tlbp) == FAIL) return(FAIL);
		tsbp = screen_find_window_format_line(wptr,tlbp);
	    }/* End IF */
	    while(tsbp && tsbp->fmt_next_line) tsbp = tsbp->fmt_next_line;
	    lines += 1;
	}/* End IF */

/*
 * If there is a format_line below our current bottom one, simply go
 * forward to it.
 */
	if(bsbp->fmt_next_line){
	    bsbp = bsbp->fmt_next_line;
	    lines += 1;
	}/* End IF */

/*
 * Else, if there is another buffer line after this, go to the first
 * format_line associated with it.
 */
	else if(blbp->next_line != NULL){
	    blbp = blbp->next_line;
	    bsbp = screen_find_window_format_line(wptr,blbp);
	    if(bsbp == NULL){
		if(screen_format_buff_line(wptr,blbp) == FAIL) return(FAIL);
		bsbp = screen_find_window_format_line(wptr,blbp);
	    }/* End IF */
	    lines += 1;
	}/* End IF */

/*
 * If we havn't made any progress, then the buffer is smaller than the
 * screen, so just display it.
 */
	if(lines == olines) break;

    }/* End While */

/*
 * Now we can start setting up the screen appearance
 */
    scrbp = saved_screen + wptr->win_y_base;

    for(i = wptr->win_y_base; i < wptr->win_y_end; i++,scrbp++){
	if(tlbp == NULL){
	    if(scrbp->companion) scrbp->companion->fmt_saved_line = NULL;
	    scrbp->companion = NULL;
	    continue;
	}/* End IF */

	if(scrbp->companion != tsbp){
	    if(scrbp->companion) scrbp->companion->fmt_saved_line = NULL;
	    scrbp->companion = NULL;
	    if(tsbp->fmt_saved_line){
		tsbp->fmt_saved_line->companion = NULL;
		tsbp->fmt_saved_line = NULL;
	    }/* End IF */
	    scrbp->companion = tsbp;
	    tsbp->fmt_saved_line = scrbp;
	    tsbp->fmt_sequence = screen_sequence;
	}/* End IF */

	if(curwin == wptr && tsbp == wptr->win_dot_format_line){
	    cursor_y = i;
	    cursor_x = wptr->win_dot_screen_offset;
	}/* End IF */

	tsbp = tsbp->fmt_next_line;
	if(tsbp == NULL){
	    tlbp = tlbp->next_line;
	    if(tlbp){
		tsbp = screen_find_window_format_line(wptr,tlbp);
		if(tsbp == NULL){
		    if(screen_format_buff_line(wptr,tlbp) == FAIL){
			return(FAIL);
		    }/* End IF */
		    tsbp = screen_find_window_format_line(wptr,tlbp);
		}/* End IF */
	    }/* End IF */
	}/* End IF */
    }/* End FOR */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Move the current screen up or down specified # lines
 *
 * Here in response to the ES command. The user can scroll the screen
 * up and down using this command. The current edit position does not
 * move, so it is pointless to move it such that 'dot' moves off of the
 * screen.
 */
void
screen_scroll( int count )
{
struct window *wptr = curwin;
struct screen_line *sp;
register int i;

    PREAMBLE();

    i = count;
    if(i < 0) i = -i;
    if(i > wptr->win_y_end) return;

/*
 * If the scroll is forward, move the saved lines up in the array by count
 */
    if(count > 0){
	sp = &saved_screen[wptr->win_y_base];
	for(i = wptr->win_y_base; i < wptr->win_y_end - count; i++,sp++){
	    if(sp->companion) sp->companion->fmt_saved_line = NULL;
	    sp->companion = (sp+count)->companion;
	    (sp+count)->companion = NULL;
	    if(sp->companion == NULL) continue;
	    sp->companion->fmt_saved_line = sp;
	    sp->companion->fmt_sequence = screen_sequence;
	}/* End FOR */
	sp = &saved_screen[wptr->win_y_end - count];
	for(i = wptr->win_y_end - count; i < wptr->win_y_end; i++,sp++){
	    if(sp->companion == NULL) continue;
	    sp->companion->fmt_saved_line = NULL;
	    sp->companion = NULL;
	}/* End FOR */
    }/* End IF */
/*
 * Else, if the scroll is backward, move the saved lines down in the array
 */
    else if(count < 0){
	sp = &saved_screen[wptr->win_y_end - 1];
	for(i = wptr->win_y_end - 1; i >= wptr->win_y_base - count; i--,sp--){
	    if(sp->companion) sp->companion->fmt_saved_line = NULL;
	    sp->companion = (sp+count)->companion;
	    (sp+count)->companion = NULL;
	    if(sp->companion == NULL) continue;
	    sp->companion->fmt_saved_line = sp;
	    sp->companion->fmt_sequence = screen_sequence;
	}/* End FOR */
	sp = &saved_screen[wptr->win_y_base];
	for(i = wptr->win_y_base; i < wptr->win_y_base - count; i++,sp++){
	    if(sp->companion == NULL) continue;
	    sp->companion->fmt_saved_line = NULL;
	    sp->companion = NULL;
	}/* End FOR */
    }/* End IF */

}/* End Routine */



/**
 * \brief Garbage collect unused format line structures
 *
 * This routine determines which format structures are in use and
 * deallocates the rest of them so that we don't use an excessive
 * number of them.
 */
void
screen_gc_format_lines()
{
register struct format_line *sbp,*tsbp;
register struct screen_line *sp;
register struct buff_line *lbp;
register int i;
struct window *wptr;

    PREAMBLE();

/*
 * First mark all the format_line structures as unused
 */
    sbp = format_line_alloc_list;
    while(sbp){
	sbp->fmt_in_use = 0;
	sbp = sbp->fmt_next_alloc;
    }/* End While */

/*
 * Now for each displayed line, find the format structure associated with it.
 * Then find the line_buffer structure which owns that format structure and
 * mark all of the format structures under it as in use.
 */
    for(wptr = window_list ; wptr != NULL; wptr = wptr->win_next_window){
	sp = saved_screen + wptr->win_y_base;
	for(i = wptr->win_y_base; i < wptr->win_y_end; i++,sp++){
	    sbp = sp->companion;
	    if(sbp == NULL) continue;
	    if(sbp->fmt_permanent) continue;
	    if(sbp->fmt_buffer_line == NULL) continue;
	    sbp =
		screen_find_window_format_line(
		    wptr,
		    sbp->fmt_buffer_line
		);
	    if(sbp == NULL) continue;
	    while(sbp){
		sbp->fmt_in_use = 1;
		sbp = sbp->fmt_next_line;
	    }/* End While */
	}/* End FOR */
    }/* End FOR */
/*
 * Now go back and deallocate any format_line structures which are not in use
 */
    sbp = format_line_alloc_list;
    tsbp = NULL;
    while(sbp){
	if(sbp->fmt_in_use){
	    tsbp = sbp;
	    sbp = sbp->fmt_next_alloc;
	    continue;
	}/* End IF */

	lbp = sbp->fmt_buffer_line;

	screen_free_window_format_lines(sbp->fmt_window_ptr,lbp);

	sbp = tsbp ? tsbp : format_line_alloc_list;

    }/* End While */

}/* End Routine */



/**
 * \brief Deallocate a window's format lines
 *
 * This routine is called to deallocate the list of format lines
 * linked off of a line buffer, if they are associated with the
 * specified window.
 */
void
screen_free_window_format_lines( struct window *wptr, struct buff_line *lbp )
{
register struct format_line *sbp,*tsbp;

    PREAMBLE();

/*
 * There are a couple cases to check for: The first situation is when
 * the very first format_line (chained off the buff_line structure) is
 * the one we want to clobber.
 */
    sbp = lbp->format_line;
    if(sbp == NULL) return;

    if(sbp->fmt_window_ptr == wptr){
	format_lines_invalid = 1;
	tsbp = sbp->fmt_next_window;
	lbp->format_line = tsbp;
	if(tsbp) tsbp->fmt_prev_window = NULL;
	sbp->fmt_next_window = NULL;
	screen_free_format_lines(sbp);
	format_lines_invalid = 0;
	return;
    }/* End IF */

/*
 * Here if it's not the head of the list. We want to keep the forward
 * and backward pointers consistent, so we need to treat this case a
 * little differently.
 */
    while((tsbp = sbp->fmt_next_window)){
	if(tsbp->fmt_window_ptr == wptr){
	    format_lines_invalid = 1;
	    sbp->fmt_next_window = tsbp->fmt_next_window;
	    if(tsbp->fmt_next_window){
		tsbp->fmt_next_window->fmt_prev_window = sbp;
	    }/* End IF */

	    tsbp->fmt_next_window = tsbp->fmt_prev_window = NULL;
	    screen_free_format_lines(tsbp);
	    format_lines_invalid = 0;
	    return;
	}/* End IF */

	sbp = tsbp;

    }/* End While */

}/* End Routine */



/**
 * \brief Deallocate a list of format lines
 *
 * This routine will clean up all the storage associated with a list of
 * format lines. It is called with the address of the head of the list,
 * and it will continue on down until the entire list is cleaned up.
 * It will also take care of cleaning up any companion pointers from the
 * screen array.
 */
void
screen_free_format_lines( struct format_line *sbp )
{
register struct format_line *osbp;
register struct format_line *next_win;
    PREAMBLE();

/*
 * Loop on down the list till we hit the tail
 */
    while(sbp){

/*
 * If this structure is tied to one of the current screen lines, break the
 * connection.
 */
	if(sbp->fmt_saved_line){
	    if(sbp->fmt_saved_line->companion != sbp){
		fprintf(stderr,
			"sbp->fmt_saved_line %p != sbp->...companion %p\n",
			sbp->fmt_saved_line, sbp->fmt_saved_line->companion);
		CAUSE_BUS_ERROR();
	    }/* End IF */
	    sbp->fmt_saved_line->companion = NULL;
	    sbp->fmt_saved_line = NULL;
	}/* End IF */

	next_win = sbp->fmt_next_window;
	if(next_win){
	    sbp->fmt_next_window = NULL;
	    sbp->fmt_prev_window = NULL;
	    if(sbp->fmt_next_line){
		sbp->fmt_next_line->fmt_next_window = next_win;
	    }/* End IF */
	}/* End IF */
/*
 * Take it off the head of the list and place it on the free list.
 */
	MAGIC_UPDATE(sbp, MAGIC_FORMAT_LOOKASIDE);
	osbp = sbp;
	sbp = sbp->fmt_next_line;
	if(sbp == NULL) sbp = next_win;
	osbp->fmt_next_line = format_line_free_list;
	format_line_free_list = osbp;

/*
 * Remove it from the allocated list. This list is used to find all
 * the format_line structures which are outstanding at any given time.
 */
	if(osbp->fmt_prev_alloc){
	    osbp->fmt_prev_alloc->fmt_next_alloc = osbp->fmt_next_alloc;
	}/* End IF */

	else if(format_line_alloc_list == osbp){
	    format_line_alloc_list = osbp->fmt_next_alloc;
	}/* End Else */

	if(osbp->fmt_next_alloc){
	    osbp->fmt_next_alloc->fmt_prev_alloc = osbp->fmt_prev_alloc;
	}/* End IF */

	osbp->fmt_next_alloc = osbp->fmt_prev_alloc = NULL;

    }/* End While */

}/* End Routine */

void
screen_check_format_lines( struct format_line *sbp, int who )
{

    PREAMBLE();

/*
 * Loop on down the list till we hit the tail
 */
    while(sbp){

/*
 * If this structure is tied to one of the current screen lines, break the
 * connection.
 */
	if(sbp->fmt_saved_line){
	    if(sbp->fmt_saved_line->companion != sbp){
		fprintf(stderr,
			"w%d sbp->fmt_saved_line %p != sbp->...companion %p\n",
			who, sbp->fmt_saved_line, sbp->fmt_saved_line->companion);
		CAUSE_BUS_ERROR();
	    }/* End IF */
	}/* End IF */

/*
 * Take it off the head of the list and place it on the free list.
 */
	sbp = sbp->fmt_next_line;

    }/* End While */

}


/**
 * This routine frees up any format_lines we've cached on our local
 * lookaside list.
 */
void
screen_deallocate_format_lookaside_list()
{
register struct format_line *sbp;

    PREAMBLE();

    while((sbp = format_line_free_list) != NULL){
	format_line_free_list = sbp->fmt_next_line;
	MAGIC_UPDATE(sbp, 0);
	tec_release(TYPE_C_SCREENBUF,(char *)sbp->fmt_buffer);
	tec_release(TYPE_C_SCREEN,(char *)sbp);
    }/* End While */

}/* End Routine */



/**
 * \brief Finds format lines for a particular window
 *
 * Each buffer line has chained off of it the format lines which hold
 * the data as it appears on the screen. In order to handle multiple
 * windows, we need the ability to have a format line per visible
 * window. Thus, from the buff_line structure, we chain off a list
 * of format lines. Since a buffer line may need many format lines to
 * represent it (because it may wrap) we need a list. In addition,
 * we want to have one of these lists on a per-window basis, so we
 * have a pointer to the next list of format lines for another window.
 */
struct format_line *
screen_find_window_format_line( struct window *wptr, struct buff_line *lbp )
{
struct format_line *sbp;

    PREAMBLE();

    sbp = lbp->format_line;
    while(sbp){
	if(sbp->fmt_window_ptr == wptr) return(sbp);
	sbp = sbp->fmt_next_window;
    }/* End While */

    return(NULL);

}/* End Routine */



/**
 * \brief Create the format_line structures for a buff line
 *
 * This routine is called with the address of a buff_line structure. It
 * creates the format_line structures which represent the printed version
 * of the data, i.e., ready to be written to the terminal.
 */
int
screen_format_buff_line( struct window *wptr, struct buff_line *lbp )
{
register struct format_line *sbp;
register char *cp;
register short *sp;
register unsigned int c,c_data;
register int i;
int byte_count;
int current_column;
char *multi_byte_echo;
int multi_byte_echo_reverse;
char expand_buffer[MAXOF(32,MAX_TAB_WIDTH+1)];

    PREAMBLE();

/*
 * If we're called with a null lbp, we've run out of memory or something,
 * and we just punt
 */
    if(lbp == NULL) return(FAIL);

/*
 * If there is an old format structure hanging around (doubtful), make it
 * go away. Note that this has been modified to try to deal with the new
 * scheme where we have a different set of format structures for each
 * window.
 */
    if(lbp->format_line){
	screen_free_window_format_lines(wptr,lbp);
    }/* End IF */

    sbp = allocate_format_buffer(wptr,lbp);
    if(sbp == NULL) return(FAIL);

    sbp->fmt_next_window = lbp->format_line;
    if(sbp->fmt_next_window){
	sbp->fmt_next_window->fmt_prev_window = sbp;
    }/* End IF */
    lbp->format_line = sbp;

    sp = sbp->fmt_buffer;
    cp = lbp->buffer;
    byte_count = lbp->byte_count;
    c = ' ';
    current_column = 0;
    multi_byte_echo = NULL;
    multi_byte_echo_reverse = 0;

    while(byte_count > 0 || multi_byte_echo){

	if(current_column >= term_columns){
	    sbp->fmt_next_line = allocate_format_buffer(wptr,lbp);
	    if(sbp->fmt_next_line == NULL) return(FAIL);
	    sbp->fmt_next_line->fmt_prev_line = sbp;
	    sbp = sbp->fmt_next_line;
	    sp = sbp->fmt_buffer;
	    current_column = 0;
	}/* End IF */

	if(multi_byte_echo){
	    /*
	     * All multi-byte echos are for Escape ($) or ^x control chars.
	     * Therefore they can be printed in reverse like in SciTECO.
	     */
	    c = *multi_byte_echo++;
	    if(multi_byte_echo_reverse) c |= SCREEN_M_REVERSE;
	    if(*multi_byte_echo == '\0'){
		multi_byte_echo = NULL;
		multi_byte_echo_reverse = 0;
	    }
	}/* End IF */

	else {
	    c = *cp++;
	    byte_count -= 1;
	}/* End Else */

	/* without possible SCREEN_M_REVERSE flags */
	c_data = c & SCREEN_M_DATA;

	switch(c_data){
	    case '\t':
		i = tab_width - ( current_column % tab_width );
		multi_byte_echo = tab_expand + MAX_TAB_WIDTH - i;
		break;
	    case ESCAPE:
		multi_byte_echo = "$";
		multi_byte_echo_reverse = 1;
		break;

	    case '\n':
		*sp++ = c;
		current_column = 0;
		break;
	    default:
		if(c_data < ' '){
		    if( hide_cr_flag ){
			expand_buffer[0] = ' ';
			expand_buffer[1] = '\0';
		    } else {
			expand_buffer[0] = '^';
			expand_buffer[1] = c_data + 'A' - 1;
			expand_buffer[2] = '\0';
			multi_byte_echo_reverse = 1;
		    }
		    multi_byte_echo = expand_buffer;
		    continue;
		}/* End IF */
/*
 * If the high bit is on, display it as the code with that bit stripped off,
 * or as '.' if it is a non-printable character. We display it reverse video
 * if the terminal is not a brain damaged 'magic cookie' terminal.
 */
		if(eight_bit_output_flag == NO && c_data >= 128){
		    c = c_data - 128;
		    if(isprint(c)) c |= SCREEN_M_REVERSE;
		    else c = '.' | SCREEN_M_REVERSE;
#ifdef TERMCAP
		    if(termcap_sg){
			c &= ~SCREEN_M_REVERSE;
		    }/* End IF */
#endif
		}/* End IF */
		*sp++ = c;
		current_column += 1;
		break;
	}/* End Switch */

    }/* End While */

    if(c != '\n'){
	if(current_column < (term_columns - 1)){
	    *sp++ = '\n';
	}/* End IF */
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Find position of dot in a format_line
 *
 * There are several times that we need to find the format buffer that is
 * associated with dot. This routine determines which format_line dot is
 * on, and the offset onto that line (in bytes).
 */
int
screen_find_dot( struct window *wptr )
{
register struct buff_line *lbp;
register struct format_line *sbp;
register char *cp;
register unsigned char c;
int i;
int byte_count;
int current_column;
char *multi_byte_echo;
char expand_buffer[32];
struct buff_header *hbp = wptr->win_buffer;

    PREAMBLE();

/*
 * Find the line buffer and format buffer structures
 */
    lbp = buff_find_line(hbp,hbp->dot);

    sbp = screen_find_window_format_line(wptr,lbp);
    if(sbp == NULL){
	if(screen_format_buff_line(wptr,lbp) == FAIL) return(FAIL);
	sbp = screen_find_window_format_line(wptr,lbp);
    }/* End IF */

    cp = lbp->buffer;
    byte_count = buff_find_offset(hbp,lbp,hbp->dot);
    current_column = 0;

    multi_byte_echo = NULL;

    while(byte_count > 0 || multi_byte_echo){

	if(current_column >= term_columns){
	    sbp = sbp->fmt_next_line;
	    current_column = 0;
	}/* End IF */

	if(multi_byte_echo){
	    c = *multi_byte_echo++;
	    if(*multi_byte_echo == '\0') multi_byte_echo = NULL;
	}/* End IF */

	else {
	    c = *cp++;
	    byte_count -= 1;
	}/* End Else */

	switch(c){
	    case '\t':
		i = tab_width - ( current_column % tab_width );
		multi_byte_echo = tab_expand + MAX_TAB_WIDTH - i;
		break;
	    case ESCAPE:
		multi_byte_echo = "$";
		break;
	    case '\n':
		current_column = 0;
		break;
	    default:
		if(c < ' '){
		    expand_buffer[0] = '^';
		    expand_buffer[1] = c + 'A' - 1;
		    expand_buffer[2] = '\0';
		    multi_byte_echo = expand_buffer;
		    continue;
		}/* End IF */
		current_column += 1;
		break;
	}/* End Switch */

    }/* End While */

    wptr->win_dot_format_line = sbp;
    wptr->win_dot_screen_offset = current_column;

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Routine to return a format_line structure
 *
 * This routine will return a format_line structure to the caller. It
 * first looks on the free list, and if there is not one there, it will
 * create one the hard way.
 */
struct format_line *
allocate_format_buffer( struct window *wptr, struct buff_line *lbp )
{
register struct format_line *sbp;
struct buff_header *hbp = wptr->win_buffer;

    PREAMBLE();

    sbp = format_line_free_list;
    if(sbp != NULL){
	if(sbp->fmt_buffer_size == (size_t)term_columns){
	    format_line_free_list = sbp->fmt_next_line;
	}
	else {
	    screen_deallocate_format_lookaside_list();
	    sbp = NULL;
	}
    }/* End IF */

    if(sbp == NULL){
	sbp = (struct format_line *)
	    tec_alloc(TYPE_C_SCREEN,sizeof(struct format_line));
	if(sbp == NULL) return(NULL);

	memset(sbp,0,sizeof(*sbp));
	sbp->fmt_buffer_size = term_columns;
	sbp->fmt_buffer = (short *)
	    tec_alloc(
		TYPE_C_SCREENBUF,
		(int)(sizeof(short) * sbp->fmt_buffer_size)
	    );
	if(sbp->fmt_buffer == NULL){
	    tec_release(TYPE_C_SCREEN,(char *)sbp);
	    return(NULL);
	}/* End IF */
    }/* End Else */

    MAGIC_UPDATE(sbp, MAGIC_FORMAT);
    sbp->fmt_owning_buffer = hbp;
    sbp->fmt_buffer_line = lbp;
    sbp->fmt_sequence = screen_sequence;
    sbp->fmt_next_line = sbp->fmt_prev_line = NULL;
    sbp->fmt_next_window = sbp->fmt_prev_window = NULL;
    sbp->fmt_window_ptr = wptr;
    sbp->fmt_saved_line = NULL;
    sbp->fmt_visible_line_position = -1;
    sbp->fmt_permanent = 0;

/*
 * Place this at the head of the allocated queue
 */
    sbp->fmt_next_alloc = format_line_alloc_list;
    sbp->fmt_prev_alloc = NULL;
    if(sbp->fmt_next_alloc) sbp->fmt_next_alloc->fmt_prev_alloc = sbp;
    format_line_alloc_list = sbp;

    return(sbp);

}/* End Routine */



/**
 * \brief Cause a redraw of the screen
 *
 * This routine is called when the screen may have become corrupted
 * and needs to be refreshed.
 */
void
screen_redraw()
{
register struct screen_line *lp;
register short *sp;
register int i,j;

    PREAMBLE();

    term_goto(0,0);
    term_clrtobot();

    for(lp = saved_screen, i = 0; i < term_lines; i++,lp++){
	sp = lp->buffer;
	for(j = 0; j < term_columns; j++,sp++) *sp = ' ';
	lp->sequence = 0;
    }/* End FOR */

    screen_format_windows();
    screen_refresh();

}/* End Routine */



/**
 * \brief Create a window data structure
 *
 * This function is called to create a window data structure which
 * then occupies some portion of the screen. The initial structure
 * starts out owning all but the reserved lines, but then lines are
 * stolen from it as it is split into other windows.
 */
struct window *
create_window( int x_size, int y_size )
{
register struct window *wptr;
register int i;

    PREAMBLE();

    wptr = (struct window *)tec_alloc(TYPE_C_WINDOW,sizeof(struct window));
    if(wptr == NULL) return(NULL);

    memset(wptr,0,sizeof(*wptr));
    wptr->win_label_line.fmt_buffer = (short *)
	tec_alloc(TYPE_C_SCRBUF,(int)(sizeof(short) * x_size));
    if(wptr->win_label_line.fmt_buffer == NULL){
	tec_release(TYPE_C_WINDOW,(char *)wptr);
	return(NULL);
    }/* End IF */

    wptr->win_window_number = next_window_number++;
/*
 * Initialize the xy size of the window. The y gets set to size-1 to leave
 * room for the label line.
 */
    wptr->win_x_size = x_size;
    wptr->win_y_size = y_size - 1;
    wptr->win_y_end = wptr->win_y_base + wptr->win_y_size;
/*
 * Initialize the label line structure. Each window has one of these in
 * reverse video showing which buffer is being displayed.
 */
    wptr->win_label_line.fmt_permanent = 1;
    wptr->win_label_line.fmt_visible_line_position = -1;

    wptr->win_label_line.fmt_window_ptr = wptr; /* FIX */
    wptr->win_label_line.fmt_buffer_size = x_size;

    for(i = 0; i < x_size; i++){
	wptr->win_label_line.fmt_buffer[i] = ' ' | SCREEN_M_REVERSE;
    }/* End FOR */

    wptr->win_label_line.fmt_sequence = screen_sequence;

    return(wptr);

}/* End Routine */



/**
 * \brief Make two where there is only one
 *
 * This routine is called when the user wants to split the current window
 * into two. This allows him to display several buffers simultaneously.
 */
struct window *
screen_split_window( struct window *old_wptr, int lines, int buffer_number )
{
register int i;
register struct window *new_wptr;
register struct screen_line *lp;

    PREAMBLE();

    if(old_wptr->win_y_size < SCREEN_MINIMUM_WINDOW_HEIGHT){
	error_message("Window is already as small as possible");
	return(NULL);
    }/* End IF */

    if(lines < SCREEN_MINIMUM_WINDOW_HEIGHT){
	lines = SCREEN_MINIMUM_WINDOW_HEIGHT;
    }/* End If */

    if((old_wptr->win_y_size - (lines + 1)) < SCREEN_MINIMUM_WINDOW_HEIGHT){
	error_message("Resulting windows would be too small");
	return(NULL);
    }/* End IF */

    new_wptr = create_window(old_wptr->win_x_size,old_wptr->win_y_size-lines);
    new_wptr->win_next_window = window_list;
    window_list = new_wptr;
    new_wptr->win_buffer = old_wptr->win_buffer;
/*
 * Take the lines of the new window away from the old window
 */
    old_wptr->win_y_size -= new_wptr->win_y_size + 1;
/*
 * Have the new window start above the old one
 */
    new_wptr->win_y_base = old_wptr->win_y_base;
    old_wptr->win_y_base += new_wptr->win_y_size + 1;
/*
 * Recalculate the ends of each window
 */
    old_wptr->win_y_end = old_wptr->win_y_base + old_wptr->win_y_size;
    new_wptr->win_y_end = new_wptr->win_y_base + new_wptr->win_y_size;
/*
 * Initialize the label line structure. Each window has one of these in
 * reverse video showing which buffer is being displayed.
 */
    for(i = 0; i < new_wptr->win_x_size; i++){
	new_wptr->win_label_line.fmt_buffer[i] =
	    old_wptr->win_label_line.fmt_buffer[i];
    }/* End FOR */

    for(i = 0; i < SCREEN_MAX_LABEL_FIELDS; i++){
	if(old_wptr->win_label_field_contents[i]){
	    screen_label_line(old_wptr->win_buffer,
				old_wptr->win_label_field_contents[i],i);
	}/* End IF */
    }/* End FOR */

    screen_label_window();

    lp = saved_screen + new_wptr->win_y_end;
    if(new_wptr->win_label_line.fmt_saved_line){
	new_wptr->win_label_line.fmt_saved_line->companion = NULL;
    }/* End IF */
    new_wptr->win_label_line.fmt_saved_line = lp;
    if(lp->companion) lp->companion->fmt_saved_line = NULL;
    lp->companion = &new_wptr->win_label_line;
    new_wptr->win_label_line.fmt_sequence = screen_sequence;

    if(buffer_number){
	buff_openbuffnum(buffer_number,0);
    }/* End IF */

    return(new_wptr);

}/* End Routine */



/**
 * \brief Here to delete a window
 *
 * This function is called when a user wants to delete a window.
 * The space gets given back to the window next to it.
 */
void
screen_delete_window( struct window *old_wptr )
{

register int i;
register struct window *new_wptr;
register struct screen_line *lp;

    PREAMBLE();

    if((old_wptr->win_y_size + 1) == (term_lines - SCREEN_RESERVED_LINES)){
	error_message("Illegal to delete the final window");
	return;
    }/* End IF */
/*
 * Find the old_wptr on the list of windows, and remove it
 */
    if(window_list == old_wptr) window_list = old_wptr->win_next_window;
    else {
	new_wptr = window_list;
	while(new_wptr->win_next_window != old_wptr){
	    new_wptr = new_wptr->win_next_window;
	}/* End While */
	new_wptr->win_next_window = old_wptr->win_next_window;
    }/* End Else */
/*
 * Remove the label line from the screen display
 */
    lp = old_wptr->win_label_line.fmt_saved_line;
    lp->companion = NULL;
    old_wptr->win_label_line.fmt_saved_line = NULL;
/*
 * If the window is at the bottom of the screen, we will combine it with
 * the window above it.
 */
    if((old_wptr->win_y_end + 1) == (term_lines - SCREEN_RESERVED_LINES)){
	new_wptr = window_list;
	while(new_wptr){
	    if((new_wptr->win_y_end + 1) == old_wptr->win_y_base) break;
	    new_wptr = new_wptr->win_next_window;
	}/* End While */
	if(!new_wptr){
	    error_message("Failure to find window above");
	    return;
	}/* End IF */
	lp = new_wptr->win_label_line.fmt_saved_line;
	lp->companion = NULL;
	new_wptr->win_y_size += old_wptr->win_y_size + 1;
	new_wptr->win_y_end = new_wptr->win_y_base + new_wptr->win_y_size;
	lp = saved_screen + new_wptr->win_y_end;
	lp->companion = &new_wptr->win_label_line;
	new_wptr->win_label_line.fmt_saved_line = lp;
    }/* End IF */
/*
 * Else, we combine it with the window below it
 */
    else {
	new_wptr = window_list;
	while(new_wptr){
	    if(new_wptr->win_y_base == (old_wptr->win_y_end + 1)) break;
	    new_wptr = new_wptr->win_next_window;
	}/* End While */
	if(!new_wptr){
	    error_message("Failure to find window below");
	    return;
	}/* End IF */
	lp = new_wptr->win_label_line.fmt_saved_line;
	lp->companion = NULL;
	new_wptr->win_y_base -= old_wptr->win_y_size + 1;
	new_wptr->win_y_size += old_wptr->win_y_size + 1;
	new_wptr->win_y_end = new_wptr->win_y_base + new_wptr->win_y_size;
	lp = saved_screen + new_wptr->win_y_end;
	lp->companion = &new_wptr->win_label_line;
	new_wptr->win_label_line.fmt_saved_line = lp;
    }/* End IF */
/*
 * Release any memory used to hold the strings in the label line
 */
    for(i = 0; i < SCREEN_MAX_LABEL_FIELDS; i++){
	if(old_wptr->win_label_field_contents[i]){
	    tec_release(
		TYPE_C_LABELFIELD,
		old_wptr->win_label_field_contents[i]
	    );
	    old_wptr->win_label_field_contents[i] = NULL;
	}/* End IF */
    }/* End FOR */
/*
 * Release the buffer used to hold formatted data
 */
    tec_release(TYPE_C_SCRBUF,(char *)old_wptr->win_label_line.fmt_buffer);
    tec_release(TYPE_C_WINDOW,(char *)old_wptr);

    curwin = new_wptr;
    curbuf = new_wptr->win_buffer;

}/* End Routine */



/**
 * \brief Switch to a new window
 *
 * This function is called to switch the current window to a different
 * window.
 */
int
window_switch( int window_number )
{
register struct window *wptr;

    PREAMBLE();

/*
 * If the window number is specified as zero, just select the next window
 * on the list.
 */
    if(window_number == 0){
	if((wptr = curwin->win_next_window) == NULL) wptr = window_list;
	window_number = wptr->win_window_number;
    }/* End IF */

    wptr = window_list;
/*
 * Search the list until we find the correct window.
 */
    while(wptr && wptr->win_window_number != window_number){
	wptr = wptr->win_next_window;
    }/* End While */
/*
 * If the search failed, give up.
 */
    if(wptr == NULL){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(tmp_message,"can't switch to window %d",window_number);
	error_message(tmp_message);
	return(FAIL);
    }/* End IF */

/*
 * Check to see that we actually found the right window.
 */
    if(wptr->win_window_number != window_number) return(FAIL);
/*
 * Switch the current window and edit buffer to the newly selected window.
 */
    curwin = wptr;
    return(SUCCESS);

}/* End Routine */
