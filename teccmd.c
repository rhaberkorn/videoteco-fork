char *teccmd_c_version = "teccmd.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/teccmd.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/**
 * \file teccmd.c
 * \brief Subroutines which implement (usually large) TECO commands
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
 * Define constants for search table code
 */
#define SEARCH_M_MATCHREPEAT	0x80
#define SEARCH_M_NOT		0x40

#define SEARCH_M_TYPE		0x0F
#define SEARCH_C_BYMASK		1
#define SEARCH_C_ATLEASTONE	2
#define SEARCH_C_QREGNUM	3
#define SEARCH_C_QREGCHARS	4

/*
 * Globals
 */
    int last_search_pos1;
    int last_search_pos2;
    int last_search_status;
    struct tags *current_tags;

#ifdef CHECKPOINT
    static char *checkpoint_filename = NULL;
#endif /* CHECKPOINT */

/*
 * External Routines, etc.
 */
    extern int forced_height,forced_width;
    extern char eight_bit_output_flag,hide_cr_flag;

#ifdef CHECKPOINT
    void cmd_checkpoint(void);
#endif /* CHECKPOINT */

    struct tags *tag_load_file( char *string );

    void srch_setbits(int *bit_table,char *string);
    void srch_setbit(int *bit_table,unsigned char value);
    void srch_setallbits(int *bit_table);
    void srch_invert_sense(int *bit_table);
    int cmd_writebak(int,char *,char *,char *,int);

    extern struct buff_header *curbuf;
    extern struct buff_header *buffer_headers;
    extern char waiting_for_input_flag;
    extern char alternate_escape_character;
    extern char alternate_delete_character;
    extern char intr_flag;
    extern int term_lines;
    extern char checkpoint_flag;
    extern char checkpoint_enabled;
    extern int checkpoint_interval;
    extern char checkpoint_modified;
    extern char resize_flag;
    extern unsigned int IntBits[BITS_PER_INT];
    extern struct search_buff search_string;
    extern char suspend_is_okay_flag;

/**
 * \brief Execute a search command
 *
 * This is the runtime execution of the search command. \a arg1 and \a arg2 may
 * specify a buffer range <tt>a,bS\<mumble\></tt> in which case the search is
 * constrained within the range. In this case, the direction of the search
 * is determined by whether \a arg1 is greater than or less than \a arg2.
 * If \a arg2 == -1, then it is an ignored argument and \a arg1 specifies the
 * direction of search and the number of times to search.
 */
int
cmd_search(
			int arg1,
			int arg2,
			struct search_buff *search_tbl )
{
int count;
char forwards;
int status;
int pos1;
int pos2;
int original_dot;

    PREAMBLE();

/*
 * Compile the search Q-register into a search table
 */
    if(compile_search_string(search_tbl) == FAIL) return(FAIL);

/*
 * Before looking at the arguments, set up the defaults
 */
    original_dot = curbuf->dot;
    count = 1;
    forwards = 1;
    pos1 = 0;
    pos2 = curbuf->zee;

/*
 * If ARG2 is -1, then he did not specify a range, but rather a count
 */
    if(arg2 == -1){
	if(arg1 < 0){
	    forwards = 0;
	    count = -arg1;
	}/* End IF */

	else count = arg1;
    }/* End IF */
/*
 * Else, if ARG2 != -1, then he specified a range for the search
 */
    else {
	if(arg1 < arg2){
	    curbuf->dot = pos1 = arg1;
	    pos2 = arg2;
	}/* End IF */

	else {
	    forwards = 0;
	    pos1 = arg2;
	    curbuf->dot = pos2 = arg1;
	}/* End Else */
    }/* End Else */

/*
 * Now implement the search
 */
    while(count--){
	if(forwards){
	    status = cmd_forward_search(pos1,pos2,search_tbl);
	}/* End IF */

	else {
	    status = cmd_reverse_search(pos1,pos2,search_tbl);
	}/* End IF */

	if(status == FAIL){
	    curbuf->dot = original_dot;
	    last_search_pos1 = last_search_pos2 = -1;
	    last_search_status = 0;
	    return(FAIL);
	}/* End IF */
    }/* End While */

    last_search_status = -1;

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Search in a forward direction
 *
 * This routine is used when the search progresses forward
 */
int
cmd_forward_search(
					int pos1,
					int pos2,
					struct search_buff *search_tbl )
{
register struct search_element *ep;
register unsigned char buffer_char;
register int table_position;
int dotpos;
char token_match;
char tmp_message[LINE_BUFFER_SIZE];
struct position_cache base_position;
struct position_cache running_position;

    PREAMBLE();

    dotpos = curbuf->dot;
/*
 * Insure the search string is non-null
 */
    if(search_tbl->length <= 0){
	error_message("?Null Search String");
	search_tbl->error_message_given = YES;
	return(FAIL);
    }/* End IF */

    if(dotpos < pos1) return(FAIL);

    table_position = 0;
    ep = &search_tbl->data[table_position];
    buffer_char = buff_cached_contents(curbuf,dotpos,&base_position);
    running_position = base_position;

    while(dotpos < pos2){

	if(table_position == 0){
	    last_search_pos1 = dotpos;
	    base_position = running_position;
	}/* End IF */

	buffer_char = BUFF_CACHE_CONTENTS(&running_position);
	BUFF_INCREMENT_CACHE_POSITION(&running_position);
	dotpos++;
	token_match = 1;

	switch(ep->type & SEARCH_M_TYPE){
	    case SEARCH_C_BYMASK:
		if((ep->bitmask.intarray[buffer_char / BITS_PER_INT] & 
		    IntBits[buffer_char % BITS_PER_INT]) == 0){
			token_match = 0;
			break;
		}/* End IF */
		if(ep->type & SEARCH_M_MATCHREPEAT){
		    while(dotpos < pos2){
			buffer_char = BUFF_CACHE_CONTENTS(&running_position);
			if((ep->bitmask.intarray[buffer_char / BITS_PER_INT] &
			  IntBits[buffer_char % BITS_PER_INT]) == 0) break;
			BUFF_INCREMENT_CACHE_POSITION(&running_position);
			dotpos++;
		    }/* End While */
		}/* End IF */
		break;
	    case SEARCH_C_QREGCHARS:
		{ struct buff_header *qbp;
		register int i;
		qbp = buff_qfind(ep->value,0);
		token_match = 0;
		if(qbp == NULL) break;
		for(i = 0; i < qbp->zee; i++){
		    if(buffer_char == buff_contents(qbp,i)) token_match = 1;
		}/* End FOR */
		if(ep->type & SEARCH_M_NOT){
		    token_match = token_match ? 0 : 1;
		}
		}/* End Block */
		break;
	    case SEARCH_C_ATLEASTONE:
		error_message("?^EM not implemented...");
		search_tbl->error_message_given = YES;
		return(FAIL);
	    case SEARCH_C_QREGNUM:
		{ struct buff_header *qbp;
		qbp = buff_qfind(ep->value,0);
		token_match = 0;
		if(qbp == NULL) break;
		if(buffer_char == qbp->ivalue) token_match = 1;
		if(ep->type & SEARCH_M_NOT){
		    token_match = token_match ? 0 : 1;
		}/* End IF */
		}/* End Block */
		break;
	    default:
		sprintf(tmp_message,
		    "SEARCH: Illegal token type %d - internal error",
		    ep->type);
		tec_panic(tmp_message);
		return(FAIL);
	}/* End Switch */

	if(token_match){
	    table_position += 1;
	    ep++;
	    if(table_position == search_tbl->length){
		curbuf->dot = last_search_pos2 = dotpos;
		{/* Local Block */
		    register struct buff_header *hbp;
		    hbp = buff_qfind('_',1);
		    if(hbp == NULL) return(FAIL);
		    hbp->ivalue = last_search_pos1;
		}/* End Block */
		return(SUCCESS);
	    }/* End IF */
	}/* End IF */

	else {
	    if(intr_flag){
		error_message("?Search aborted");
		search_tbl->error_message_given = YES;
		return(FAIL);
	    }/* End IF */
	    table_position = 0;
	    ep = &search_tbl->data[table_position];
	    dotpos = last_search_pos1 + 1;
	    running_position = base_position;
	    BUFF_INCREMENT_CACHE_POSITION(&running_position);
	}/* End IF */

    }/* End While */

    return(FAIL);
	    
}/* End Routine */



/**
 * \brief Search in a reverse direction
 *
 * This routine is used when the search progresses backwards
 */
int
cmd_reverse_search(
					int pos1,
					int pos2,
					struct search_buff *search_tbl )
{
register struct search_element *ep;
register unsigned char buffer_char;
register int table_position;
int dotpos;
char token_match;
char tmp_message[LINE_BUFFER_SIZE];
struct position_cache base_position;
struct position_cache running_position;

    PREAMBLE();

    dotpos = curbuf->dot;
/*
 * Insure the search string is non-null
 */
    if(search_tbl->length <= 0){
	error_message("?Null Search String");
	search_tbl->error_message_given = YES;
	return(FAIL);
    }/* End IF */

    if(dotpos > pos2) return(FAIL);

    table_position = search_tbl->length - 1;
    ep = &search_tbl->data[table_position];
    buffer_char = buff_cached_contents(curbuf,dotpos,&base_position);
    running_position = base_position;

    while(dotpos > pos1){

	if(table_position == (search_tbl->length - 1)){
	    last_search_pos2 = dotpos;
	    base_position = running_position;
	}/* End IF */

	dotpos--;
	BUFF_DECREMENT_CACHE_POSITION(&running_position);
	buffer_char = BUFF_CACHE_CONTENTS(&running_position);
	token_match = 1;

	switch(ep->type & SEARCH_M_TYPE){
	    case SEARCH_C_BYMASK:
		if((ep->bitmask.intarray[buffer_char / BITS_PER_INT] & 
		    IntBits[buffer_char % BITS_PER_INT]) == 0){
			token_match = 0;
			break;
		}/* End IF */
		if(ep->type & SEARCH_M_MATCHREPEAT){
		    while(dotpos > pos1){
			BUFF_DECREMENT_CACHE_POSITION(&running_position);
			dotpos--;
			buffer_char = BUFF_CACHE_CONTENTS(&running_position);
			if(ep->bitmask.intarray[buffer_char / BITS_PER_INT] &
			  IntBits[buffer_char % BITS_PER_INT]) continue;
			dotpos++;
			BUFF_INCREMENT_CACHE_POSITION(&running_position);
			break;
		    }/* End While */
		}/* End IF */
		break;
	    case SEARCH_C_QREGCHARS:
		{ struct buff_header *qbp;
		register int i;
		qbp = buff_qfind(ep->value,0);
		token_match = 0;
		if(qbp == NULL) break;
		for(i = 0; i < qbp->zee; i++){
		    if(buffer_char == buff_contents(qbp,i)) token_match = 1;
		}/* End FOR */
		}/* End Block */
		break;
	    case SEARCH_C_ATLEASTONE:
		error_message("?^EM not implemented...");
		search_tbl->error_message_given = YES;
		return(FAIL);
	    case SEARCH_C_QREGNUM:
		{ struct buff_header *qbp;
		qbp = buff_qfind(ep->value,0);
		token_match = 0;
		if(qbp == NULL) break;
		if(buffer_char == qbp->ivalue) token_match = 1;
		}/* End Block */
		break;
	    default:
		sprintf(tmp_message,
		    "SEARCH: Illegal token type %d - internal error",
		    ep->type);
		tec_panic(tmp_message);
		return(FAIL);
	}/* End Switch */

	if(token_match){
	    table_position -= 1;
	    ep--;
	    if(table_position < 0){
		curbuf->dot = last_search_pos1 = dotpos;
		{/* Local Block */
		    register struct buff_header *hbp;
		    hbp = buff_qfind('_',1);
		    if(hbp == NULL) return(FAIL);
		    hbp->ivalue = last_search_pos2;
		}/* End Block */
		return(SUCCESS);
	    }/* End IF */
	}/* End IF */

	else {
	    if(intr_flag){
		error_message("?Search aborted\n");
		search_tbl->error_message_given = YES;
		return(FAIL);
	    }/* End IF */
	    table_position = search_tbl->length - 1;
	    ep = &search_tbl->data[table_position];
	    dotpos = last_search_pos2 - 1;
	    running_position = base_position;
	    BUFF_DECREMENT_CACHE_POSITION(&running_position);
	}/* End IF */

    }/* End While */

    return(FAIL);
	    
}/* End Routine */



/**
 * \brief Sets Q-register '_' to the new search string
 *
 * This routine is called from the exec functions when a search string
 * is to be set. It takes care of loading Q-register '_' with the new
 * search string, as well as setting up all the undo tokens to insure
 * that this is all reversable.
 */
int
set_search_string_with_undo(
								char *string,
								struct cmd_token *uct )
{
register struct buff_header *qbp;
register struct undo_token *ut;
register int i,c;
register char *cp;
int new_length;

    PREAMBLE();

/*
 * Get a pointer to the special Q-register which holds the search string. If
 * it does not exist, have it be created automatically.
 */
    qbp = buff_qfind('_',1);
    if(qbp == NULL){
	return(FAIL);
    }/* End IF */

/*
 * This code allows us to load the numeric half of the search string
 * Q-register with the length of the string we just found. In order
 * to be able to undo it, we have to protect the previous value, and
 * it just happens that by doing it here we probably catch all the
 * places where the search could have taken place. Then again, maybe
 * not (search really should not be calling this routine repeatedly
 * in an iteration or macro, for instance). If this gets cleaned up,
 * then this code might not be appropriate... XXX
 */
    ut = allocate_undo_token(uct);
    if(ut == NULL) return(FAIL);
    ut->opcode = UNDO_C_UQREGISTER;
    ut->iarg1 = '_';
    ut->iarg2 = qbp->ivalue;

    if(string == NULL) return(SUCCESS);
    if((new_length = strlen(string)) == 0) return(SUCCESS);

/*
 * Set up undo tokens for the search_state and search_position information
 * which typically gets used to pass search information between halves of
 * a search / replace type command.
 */
    ut = allocate_undo_token(uct);
    if(ut == NULL) return(FAIL);
    ut->opcode = UNDO_C_SET_SEARCH_GLOBALS;
    ut->iarg1 = last_search_pos1;
    ut->iarg2 = last_search_pos2;
    ut->carg1 = (char *)(uintptr_t)last_search_status;

/*
 * If the new length is the same as the old length, there is a chance we are
 * just setting the same search string over again. To find out, we actually
 * have to loop through comparing bytes in the Q-register to bytes in the
 * supplied string. If none of them are different, we can just return without
 * changing the search string. This happens often in iterations...
 */
    if(new_length == qbp->zee){
	cp = string;
	for(i = 0; i < new_length; i++){
	    c = buff_contents(qbp,i);
	    if(*cp++ != c) break;
	}/* End FOR */

	if(i == new_length) return(SUCCESS);

    }/* End IF */
/*
 * At this point we set an undo token so that if the following stuff gets
 * undone, we will re-compile the search string.
 */
    ut = allocate_undo_token(uct);
    if(ut == NULL) return(FAIL);
    ut->opcode = UNDO_C_SET_SEARCH_STRING;
/*
 * Delete the current contents of the search string Q-register to make room
 * for the new contents.
 */
    if(qbp->zee > 0) buff_delete_with_undo(uct,qbp,0,qbp->zee);
/*
 * Set up the undo token to delete the text we are going to insert into the
 * search string Q-register.
 */
    buff_insert_with_undo(uct,qbp,0,string,new_length);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Creates the search table for a given input string
 *
 * This routine parses the input search string and creates the search
 * table which will describe to the search routines how to match the
 * buffer characters. Note that the search table must be constructed
 * in such a way that it works equally well backward or forward.
 */
int
compile_search_string( struct search_buff *search_tbl )
{
register int position;
register char c;
register char *cp;
register struct buff_header *qbp;
register struct search_element *ep;
int radix;
int bracket_bits[256/BITS_PER_INT];
char tmp_message[LINE_BUFFER_SIZE];
char either_case_flag = YES;
char bracket_wildcard = NO;
char bracket_not_flag = NO;
char not_flag = NO;
char matchrepeat_flag = NO;

    PREAMBLE();

#define QBUF_CHAR() \
    (position < qbp->zee ? *cp++ = buff_contents(qbp,position++) : 0)

    search_tbl->error_message_given = NO;

/*
 * Get a pointer to the special Q-register which holds the search string. If
 * it does not exist, have it be created automatically.
 */
    qbp = buff_qfind('_',1);
    if(qbp == NULL){
	error_message("Internal error - qfind returned null");
	search_tbl->error_message_given = YES;
	return(FAIL);
    }/* End IF */

    if(qbp->zee >= SEARCH_STRING_MAX){
	error_message("Search Q-register too long");
	search_tbl->error_message_given = YES;
	return(FAIL);
    }/* End IF */

    search_string.length = 0;
    position = 0;
    cp = search_string.input;
    ep = search_string.data;
    bzero(&ep->bitmask,sizeof(ep->bitmask));

    while((c = QBUF_CHAR())){
/*
 * Check for CNTRL_E which is the standard lead-in for wildcard search
 * strings.
 */
	if(c == CNTRL_E){
	    c = QBUF_CHAR();
	    switch(c){
/*
 * [ specifies a list of possible matches, ANY of which satisfy the
 * position.
 */
		case '[':
		    bzero(bracket_bits,sizeof(bracket_bits));
		    bracket_wildcard = YES;
		    if(not_flag){
			not_flag = NO;
			bracket_not_flag = YES;
		    }/* End IF */
		    continue;
/*
 * A specifies any ALPHABETIC character as a match. Thus any upper or lower
 * case character in the range a-z will match.
 */
		case 'A': case 'a':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,
						"abcdefghijklmnopqrstuvwxyz");
		    srch_setbits((int *)&ep->bitmask,
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		    break;
/*
 * B specifies any separator character, when separator is defined as any
 * non-alphanumeric character.
 */
		case 'B': case 'b':
		{
		    register int i;
		    ep->type = SEARCH_C_BYMASK;
		    for(i = 0; i < 256; i++){
			if(!isalnum(i)) srch_setbit((int *)&ep->bitmask,i);
		    }/* End FOR */
		    break;
		}
/*
 * C specifies any symbol constituent. This is any alpha-numeric character,
 * or dot (.), dollar sign ($), or underscore (_).
 */
		case 'C': case 'c':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,
						"abcdefghijklmnopqrstuvwxyz");
		    srch_setbits((int *)&ep->bitmask,
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		    srch_setbits((int *)&ep->bitmask,"0123456789.$_");
		    break;
/*
 * D specifies that any digit (0 to 9) is acceptable.
 */
		case 'D': case 'd':
		    ep->type = SEARCH_C_BYMASK | SEARCH_M_MATCHREPEAT;
		    srch_setbits((int *)&ep->bitmask,"0123456789");
		    break;
/* 
 * E or ^E is a Video TECO only definition which means that whether searches
 * are case sensitive or not should be toggled.
 */
		case 'E': case 'e': case CNTRL_E:
		    either_case_flag ^= 1;
		    continue;
/*
 * G followed by a Q register name matches any character which is in the
 * text portion of the Q register.
 */
		case 'G': case 'g':
		    if(bracket_wildcard){
			error_message("^EG inside ^E[,,,] illegal");
			search_tbl->error_message_given = YES;
			return(FAIL);
		    }/* End IF */
		    ep->type = SEARCH_C_QREGCHARS;
		    ep->value = QBUF_CHAR();
		    break;
/*
 * L matches any line terminator character.
 */
		case 'L': case 'l':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,"\n\r\f");
		    break;
/* 
 * M is a flag that says any number of the following character (but at least
 * one) are acceptable. Thus ^EMA would match A or AA or AAA, etc.
 */
		case 'M': case 'm':
		    matchrepeat_flag = YES;
		    continue;
/*
 * R matches any alphanumeric
 */
		case 'R': case 'r':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,
						"abcdefghijklmnopqrstuvwxyz");
		    srch_setbits((int *)&ep->bitmask,
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		    srch_setbits((int *)&ep->bitmask,"0123456789");
		    break;
/*
 * S matches any number of tabs and spaces, in any mixture (but there must
 * be at least one).
 */
		case 'S': case 's':
		    ep->type = SEARCH_C_BYMASK | SEARCH_M_MATCHREPEAT;
		    srch_setbits((int *)&ep->bitmask," \t");
		    break;
/*
 * U matches against the ASCII code which is contained in the Q register's
 * numeric storage.
 */
		case 'U': case 'u':
		    if(bracket_wildcard){
			error_message("^EU inside ^E[,,,] illegal");
			search_tbl->error_message_given = YES;
			return(FAIL);
		    }/* End IF */
		    ep->type = SEARCH_C_QREGNUM;
		    ep->value = QBUF_CHAR();
		    break;
/*
 * V matches any lowercase letter.
 */
		case 'V': case 'v':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,
						"abcdefghijklmnopqrstuvwxyz");
		    break;
/*
 * W matches any uppercase letter
 */
		case 'W': case 'w':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbits((int *)&ep->bitmask,
						"ABCDEFGHIJKLMNOPQRSTUVWXYZ");
		    break;
/*
 * X is similar to ^X in that it matches any character
 */
		case 'X': case 'x':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setallbits((int *)&ep->bitmask);
		    break;
/*
 * The < character opens the sequence for ^E<nnn> where nnn is a numeric
 * (ASCII) code to search for. Classic TECO says that this code is octal,
 * but Video TECO defaults to decimal, octal if the first digit is zero,
 * or hex if the first two digits are 0x.
 */
		case '<':
		    ep->value = 0;
		    radix = 10;
		    c = QBUF_CHAR();
		    if(c == '0'){
			radix = 8;
			c = QBUF_CHAR();
			if(c == 'x' || c == 'X'){
			    radix = 16;
			    c = QBUF_CHAR();
			}/* End IF */
		    }/* End IF */
		    while(c && c != '>'){
			if(c >= '0' && c <= '7'){
			    ep->value *= radix;
			    ep->value += c - '0';
			    c = QBUF_CHAR();
			    continue;
			}/* End IF */
			if(c >= '8' && c <= '9' && radix > 8){
			    ep->value *= radix;
			    ep->value += c - '0';
			    c = QBUF_CHAR();
			    continue;
			}/* End IF */
			if(c >= 'a' && c <= 'f' && radix == 16){
			    c = TOUPPER(c);
			}/* End IF */
			if(c >= 'A' && c <= 'F' && radix == 16){
			    ep->value *= radix;
			    ep->value += c - 'A' + 10;
			    c = QBUF_CHAR();
			    continue;
			}/* End IF */
			sprintf(tmp_message,
			    "?Illegal ^E code '%c' in search string",c);
			search_tbl->error_message_given = YES;
			error_message(tmp_message);
			return(FAIL);
		    }/* End While */
		    if(c == '>') c = QBUF_CHAR();
		    else {
			error_message("?Missing '>' in ^E<nnn> search string");
			search_tbl->error_message_given = YES;
			return(FAIL);
		    }/* End Else */
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbit((int *)&ep->bitmask,ep->value);
		    break;
/*
 * This case is really just to catch the user typing ^E followed by the
 * end of the search string. We just make it search for ^E in this case.
 */
		case '\0':
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbit((int *)&ep->bitmask,CNTRL_E);
		    break;
/*
 * Any unrecognized ^E code just searches for the exact character which
 * follows the ^E. This is probably not very good, we should issue an
 * error message instead.
 */
		default:
		    ep->type = SEARCH_C_BYMASK;
		    srch_setbit((int *)&ep->bitmask,c);
		    break;
	    }/* End Switch */
	}/* End IF */
/*
 * The ^X code embedded in a search string means match any character at
 * this position.
 */
	else if(c == CNTRL_X){
	    ep->type = SEARCH_C_BYMASK;
	    srch_setallbits((int *)&ep->bitmask);
	}/* End Else */
/*
 * The ^N code embedded in a search string means match any character EXCEPT
 * the one that follows.
 */
	else if(c == CNTRL_N){
	    not_flag = YES;
	    continue;
	}/* End Else */
/*
 * Else, here if it is just a normal character which should be searched for
 */
	else {
	    ep->type = SEARCH_C_BYMASK;
	    srch_setbit((int *)&ep->bitmask,c);
	    if(either_case_flag){
		if(islower((int)c)) srch_setbit((int *)&ep->bitmask,TOUPPER((int)c));
		if(isupper((int)c)) srch_setbit((int *)&ep->bitmask,TOLOWER((int)c));
	    }/* End IF */
	}/* End Else */

/*
 * If sense has been inverted with ^N, invert the bits
 */
	if(not_flag){
	    if(ep->type & SEARCH_C_BYMASK){
		srch_invert_sense((int *)&ep->bitmask);
	    }/* End IF */
	    ep->type &= ~SEARCH_M_MATCHREPEAT;
	    ep->type |= SEARCH_M_NOT;
	    not_flag = NO;
	}/* End IF */
/*
 * If already inside a ^E[a,b,c] type wildcard, check for comma or close
 * bracket.
 */
	if(bracket_wildcard){
	    c = QBUF_CHAR();
	    switch(c){
		case ',':
		{
		    register int i;
		    for(i = 0; i < 256 / BITS_PER_INT; i++){
			bracket_bits[i] |= ep->bitmask.intarray[i];
		    }/* End FOR */
		    bzero(&ep->bitmask,sizeof(ep->bitmask));
		    continue;
		}
		case ']':
		{
		    register int i;
		    for(i = 0; i < 256 / BITS_PER_INT; i++){
			ep->bitmask.intarray[i] |= bracket_bits[i];
		    }/* End FOR */
		    if(bracket_not_flag){
			srch_invert_sense((int *)&ep->bitmask);
			ep->type &= ~SEARCH_M_MATCHREPEAT;
			bracket_not_flag = NO;
		    }/* End IF */
		    bracket_wildcard = NO;
		    break;
		}
		default:
		    error_message("Illegal syntax in [,,,] construct");
		    search_tbl->error_message_given = YES;
		    return(FAIL);
	    }/* End Switch */
	}/* End IF */
/*
 * Test for match repeating characters flag
 */
	if(matchrepeat_flag){
	    ep->type |= SEARCH_M_MATCHREPEAT;
	    matchrepeat_flag = NO;
	}/* End IF */
/*
 * Point to the next entry in the search string table
 */
	ep++;
	bzero(&ep->bitmask,sizeof(ep->bitmask));
	search_string.length += 1;

    }/* End While */

    *cp = '\0';

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Set ALL the bits on for a search table entry
 *
 * This routine makes ANY character match the search string table
 * entry.
 */
void
srch_setallbits( int *bit_table )
{
register int i;

    PREAMBLE();

    for(i = 0; i < 256 / BITS_PER_INT; i++){
	bit_table[i] = ~0;
    }/* End FOR */

}/* End Routine */

/**
 * \brief Invert the sense of a search table entry
 *
 * This routine is called when a 'NOT' modifier has been used,
 * meaning that just the opposite matching should occur. We
 * simply invert the setting of the bits in the search table entry.
 */
void
srch_invert_sense( int *bit_table )
{
register int i;

    PREAMBLE();

    for(i = 0; i < 256 / BITS_PER_INT; i++){
	bit_table[i] ^= ~0;
    }/* End FOR */

}/* End Routine */

/**
 * \brief Set the corresponding search bits to 'on'
 *
 * This routine is called with a zero terminated string of characters.
 * It sets each corresponding bit in the search table to a 1.
 */
void
srch_setbits( int *bit_table, char *string )
{
register int c;

    PREAMBLE();

    while((c = *string++)){
	bit_table[c/BITS_PER_INT] |= IntBits[c%BITS_PER_INT];
    }/* End While */

}/* End Routine */

/**
 * \brief Set the corresponding search bit to 'on'
 *
 * This routine is called with a char whose corresponding bit should
 * be set in the search table.
 */
void
srch_setbit( int *bit_table, unsigned char value )
{

    PREAMBLE();

    bit_table[value/BITS_PER_INT] |= IntBits[value%BITS_PER_INT];

}/* End Routine */



/**
 * \brief Implements the EW command by writing out a buffer
 *
 * This routine will write out the contents of the buffer after creating
 * the appropriate backup files. If there is not already a file with the
 * a .OLD extension, this is created. Otherwise, a .BAK file is created.
 * If the name is so long that we can't append a of suffix, we have to
 * hack it up a bit. This is not guaranteed to work, but...
 */
int
cmd_write( struct buff_header *hbp, char *filename )
{
char base_filename[TECO_FILENAME_COMPONENT_LENGTH + 1];
char path_name[TECO_FILENAME_TOTAL_LENGTH + 1];
char tmp_filename[TECO_FILENAME_TOTAL_LENGTH + 1];
char tmp_message[LINE_BUFFER_SIZE];
register char *cp1,*cp2;
register int i;
int path_len;
int file_len;
int combined_len;
int fi;
int status;

    PREAMBLE();

    /*
     * FIXME: This could be supported on MS-DOS if we
     * respected DOS directory separators and
     * used short filename extensions.
     */
#ifdef UNIX
/*
 * First step is to separate the path elements from the filename itself.
 * To do this, we search for the directory path separator.
 */
    cp1 = cp2 = filename;
    while(*cp1){
	if(*cp1++ == '/') cp2 = cp1;
    }/* End While */

    combined_len = strlen(filename);
    file_len = strlen(cp2);
    path_len = combined_len - file_len;

/*
 * The filename really should not be longer than this.
 */
    if(file_len > TECO_FILENAME_COMPONENT_LENGTH){
	sprintf(tmp_message,"?Filename is too long <%s>",cp2);
	error_message(tmp_message);
	return(FAIL);
    }/* End IF */
/*
 * Make a copy of the filename portion for safe keeping.
 */
    (void) strcpy(base_filename,cp2);
/*
 * And do the same for the path portion.
 */
    cp1 = path_name;
    cp2 = filename;
    for(i = 0; i < path_len; i++){
	*cp1++ = *cp2++;
    }/* End FOR */
    *cp1 = '\0';

/*
 * Now the first thing to do is see if the file exists, because this
 * will impact us on whether we want to attempt .BAK files
 */
    if(hbp->isbackedup == NO){

	fi = open(filename,O_RDONLY);
	if(fi >= 0){

#ifdef CREATE_OLD_FILES
#ifdef HAVE_LONG_FILE_NAMES
	    (void) strcpy(tmp_filename,base_filename);
	    (void) strcat(tmp_filename,".OLD");
	    status = cmd_writebak(fi,path_name,filename,tmp_filename,O_EXCL);
#else
	    (void) strcpy(tmp_filename,path_name);
	    (void) strcat(tmp_filename,".TECOLD");
	    status = cmd_writebak(fi,filename,tmp_filename,base_filename,O_EXCL);
#endif
	    if(status == SUCCESS) hbp->isbackedup = YES;

	    if(status == FAIL && errno != EEXIST){
	    	/* NOTE: cmd_writebak has already written the error_message */
		close(fi);
		return(FAIL);
	    }/* End IF */
#endif

	    if(hbp->isbackedup == NO){
#ifdef HAVE_LONG_FILE_NAMES
		(void) strcpy(tmp_filename,base_filename);
		(void) strcat(tmp_filename,".BAK");
		status = cmd_writebak(fi,path_name,filename,tmp_filename,0);
#else
		(void) strcpy(tmp_filename,path_name);
		(void) strcat(tmp_filename,".TECBAK");
		status = cmd_writebak(fi,filename,tmp_filename,base_filename,0);
#endif
		if(status == SUCCESS) hbp->isbackedup = YES;
		if(status == FAIL){
		    /* NOTE: cmd_writebak has already written the error_message */
		    close(fi);
		    return(FAIL);
	        }/* End IF */
	    }/* End IF */
	}/* End IF */

    close(fi);

    }/* End IF */

/* END OF UNIX CONDITIONAL CODE */

#endif

#ifdef VMS
    fi = creat(filename,0,"mrs = 0");
#else
    fi = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0666);
#endif

    if(fi < 0){
	sprintf(tmp_message,"?Error opening <%s>: %s",
	    filename,error_text(errno));
	error_message(tmp_message);
	return(FAIL);
    }/* End IF */

    status = buff_write(hbp,fi,0,hbp->zee);

    if(status == FAIL){
	sprintf(tmp_message,"?Error writing <%s>: %s",
	    filename,error_text(errno));
	error_message(tmp_message);
	close(fi);
	return(FAIL);
    }/* End IF */

    close(fi);

    hbp->ismodified = NO;
    if(hbp == curbuf){
	screen_label_line(hbp,"",LABEL_C_MODIFIED);
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Routine to open the BAK or OLD file
 *
 * This routine opens a channel to the backup file and creates any
 * subdirectories that may be necessary.
 */
int
cmd_writebak(
				int fi,
				char *pathname,
				char *input_filename,
				char *output_filename,
				int open_flags )
{
char tmp_filename[TECO_FILENAME_TOTAL_LENGTH + 1];
char tmp_message[LINE_BUFFER_SIZE];
char iobuf[IO_BUFFER_SIZE];
int fo;
register int i;
register int status;

    PREAMBLE();

#ifndef HAVE_LONG_FILE_NAMES
#ifdef MSDOS
    i = mkdir(pathname);
#else
    i = mkdir(pathname,0777);
#endif
    if(i){
	if(errno != EEXIST){
	    sprintf(tmp_message,"?Error creating subdirectory <%s>: %s",
		pathname,error_text(errno));
	    error_message(tmp_message);
	    errno = 0;
	    return(FAIL);
	}/* End IF */
    }/* End IF */
#endif

    (void) strcpy(tmp_filename,pathname);
    if(strlen(tmp_filename)){
	(void) strcat(tmp_filename,"/");
    }/* End IF */

    (void) strcat(tmp_filename,output_filename);
    chmod(tmp_filename,0666);
    fo = open(tmp_filename,O_WRONLY|O_CREAT|O_TRUNC|open_flags,0666);
    if(fo < 0){
	if(errno == EEXIST){
	    chmod(tmp_filename,0444);
	    errno = EEXIST;
	    return(FAIL);
	}/* End IF */
	snprintf(tmp_message,sizeof(tmp_message),"?Error opening <%s>: %s",
	    tmp_filename,error_text(errno));
	error_message(tmp_message);
	errno = 0;
	return(FAIL);
    }/* End IF */

    while(1){
	i = read(fi,iobuf,IO_BUFFER_SIZE);
	if(i < 0){
	    snprintf(tmp_message,sizeof(tmp_message),"?Error reading <%s>: %s",
		input_filename,error_text(errno));
	    error_message(tmp_message);
	    close(fo);
	    errno = 0;
	    return(FAIL);
	}/* End IF */

	status = write(fo,iobuf,(unsigned)i);
	if(status < 0){
	    snprintf(tmp_message,sizeof(tmp_message),"?Error writing <%s>: %s",
		tmp_filename,error_text(errno));
	    error_message(tmp_message);
	    close(fo);
	    errno = 0;
	    return(FAIL);
	}/* End IF */

	if(i < IO_BUFFER_SIZE) break;

    }/* End While */

    close(fo);
    chmod(tmp_filename,0444);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Here to move within the edit buffer by words
 *
 * Here in response to one of the 'word' commands. We move the
 * current edit position forward or backward by the specified
 * number of words.
 */
int
cmd_wordmove( int count )
{
    register int c;

    PREAMBLE();

    while(count > 0){
	while(1){
	    if(curbuf->dot == curbuf->zee) break;
	    c = buff_contents(curbuf,curbuf->dot);
	    if(isspace(c)) break;
	    curbuf->dot++;
	}/* End While */

	while(1){
	    if(curbuf->dot == curbuf->zee) break;
	    c = buff_contents(curbuf,curbuf->dot);
	    if(!isspace(c)) break;
	    curbuf->dot++;
	}/* End While */

	count -= 1;

    }/* End While */

    while(count < 0){
	while(1){
	    if(curbuf->dot == 0) break;
	    c = buff_contents(curbuf,curbuf->dot-1);
	    if(isspace(c)) break;
	    curbuf->dot--;
	}/* End While */

	while(1){
	    if(curbuf->dot == 0) break;
	    c = buff_contents(curbuf,curbuf->dot-1);
	    if(!isspace(c)) break;
	    curbuf->dot--;
	}/* End While */

	count += 1;

    }/* End While */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief We get here on a suspend signal from the tty
 *
 * Here when the user has typed ^Z. We want to suspend back to the
 * original shell.
 */
void
cmd_suspend()
{
#if defined(UNIX) || defined(VMS)
extern char susp_flag;
#endif

    PREAMBLE();

#ifdef UNIX
    if(waiting_for_input_flag == YES){
	pause_while_in_input_wait();
	return;
    }/* End IF */

#ifdef JOB_CONTROL
    if(susp_flag++ >= 4){
	signal(SIGTSTP,(void (*)(int))SIG_DFL);
    }/* End IF */
#endif

#endif

#ifdef VMS
    if(suspend_is_okay_flag == YES){
	susp_flag++;
    }/* End IF */
#endif

}/* End Routine */

/**
 * \brief Here when we are ready to suspend back to the shell
 *
 * Here when the parser has noticed that the suspend flag is set, and
 * is ready for us to suspend the process.
 */
void
cmd_pause()
{
extern char susp_flag;

#ifdef VMS
long owner_pid;
long status;

struct itmlst {
    short buffer_length;
    short item_code;
    char *buffer_address;
    long *length_address;
    long terminating_zero;
}list;
#endif

    PREAMBLE();

    restore_tty();

#if defined(JOB_CONTROL) || defined(VMS)

#ifdef CHECKPOINT
    alarm(0);
#endif /* CHECKPOINT */

#ifdef UNIX
    if(suspend_is_okay_flag == YES){
	kill(getpid(),SIGSTOP);
	signal(SIGTSTP,(void (*)(int))cmd_suspend);
    }
#endif /* UNIX */

#ifdef VMS
    owner_pid = 0;
    list.buffer_length = sizeof(owner_pid);
    list.item_code = JPI$_OWNER;
    list.buffer_address = (char *)&owner_pid;
    list.length_address = 0;
    list.terminating_zero = 0;
    sys$getjpiw(0,0,0,&list,0,0,0);

    if(owner_pid){
	status = lib$attach(&owner_pid);
	if(status != SS$_NORMAL){
	    error_message("?LIB$ATTACH failed");
	    susp_flag = 0;
	    return;
	}/* End IF */
    }/* End IF */

    else {
	error_message("?Cannot suspend. No parent process for LIB$ATTACH");
	susp_flag = 0;
	return;
    }/* End IF */
#endif

#ifdef CHECKPOINT
    alarm(checkpoint_interval);
#endif /* CHECKPOINT */

#endif /* JOB_CONTROL */

    initialize_tty();
    screen_redraw();
    susp_flag = 0;

}/* End Routine */



/**
 * \brief We get here when the OS says the window changed size
 *
 * Here when the OS says our window changed size. We just wanna
 * call the screen package's resize entry point so it can build
 * a new screen for us.
 */
void
cmd_winch()
{
    PREAMBLE();

#ifdef ATT_UNIX
    signal(SIGWINCH,(void (*)())cmd_winch);
#endif

#ifdef SEQUOIA
    screen_message("WINCH!\g\g");
    screen_refresh();
    return;
#endif /* SEQUOIA */

    resize_flag = YES;

    if(waiting_for_input_flag){
	screen_resize();
	screen_refresh();
    }/* End IF */

}/* End Routine */



/**
 * \brief We get here on an interrupt signal from the user
 *
 * Here when the user has typed ^C. We want to cause any current commands
 * to abort. This gives the user a way to break out of infinite iterations
 * and macros.
 */
void
cmd_interrupt()
{
    PREAMBLE();

#ifdef ATT_UNIX
    signal(SIGINT,(void (*)())cmd_interrupt);
#endif

    if(intr_flag++ >= 4){
	if(waiting_for_input_flag == NO){
	    restore_tty();
	    fprintf(stderr,"TECO aborted...\n");
#ifdef MSDOS
	    abort();
#else
	    signal(SIGINT,(void (*)(int))SIG_DFL);
	    kill(getpid(),SIGINT);
#endif
	}/* End IF */
    }/* End IF */

    screen_message("interrupt!");

}/* End Routine */



#ifdef CHECKPOINT

/**
 * \brief Here to when the interval timer expires
 *
 * This function is called periodically when the interval timer says
 * it is time for us to checkpoint all our buffers.
 */
cmd_alarm()
{

    PREAMBLE();

#if defined(ATT_UNIX) || defined(VMS)
    signal(SIGALRM,(void (*)())cmd_alarm);
#endif

    if(waiting_for_input_flag == 0){
	checkpoint_flag = YES;
    }/* End IF */

    else {
	if(checkpoint_enabled == YES){
	    cmd_checkpoint();
	}
	checkpoint_flag = NO;
    }/* End Else */

#ifdef VMS
    alarm(0);
#endif /* VMS */
    alarm(checkpoint_interval);

}/* End Routine */

#endif /* CHECKPOINT */



#ifdef CHECKPOINT

/**
 * \brief Here to write all our buffers to a checkpoint file
 *
 * Here we check out all the buffers and write out any which have
 * ever been modified.
 */
void
cmd_checkpoint()
{
register struct buff_header *bp;
int fd = 0;
char temp_buffer[TECO_FILENAME_TOTAL_LENGTH+20];
char message_save_buff[SCREEN_NOMINAL_LINE_WIDTH];
char filename_buffer[64];
register char *cp;
int i,status;

    PREAMBLE();

    if(checkpoint_modified == NO) return;
    checkpoint_modified = NO;

    if(checkpoint_filename == NULL){
	temp_buffer[0] = '\0';
	if(cp = (char *)getenv("HOME")){
	    (void) strcpy(temp_buffer,cp);
#ifndef VMS
	    (void) strcat(temp_buffer,"/");
#endif /* VMS */
	}/* End IF */

#ifndef VMS
	strcpy(filename_buffer,".tecXXXXXX");
#else
	strcpy(filename_buffer,"tecXXXXXX.CKP");
#endif /* VMS */
	cp = mktemp(filename_buffer);

	strcat(temp_buffer,cp);
	checkpoint_filename = tec_alloc(TYPE_C_CPERM,strlen(temp_buffer)+1);
	if(checkpoint_filename == NULL) return;
	strcpy(checkpoint_filename,temp_buffer);
    }/* End IF */

    status = 0;
    for(bp = buffer_headers; bp != NULL; bp = bp->next_header){
	if(bp->ismodified){
	    status = 1;
	}/* End IF */
    }/* End FOR */

    if(status == 0) return;

    screen_save_current_message(message_save_buff,sizeof(message_save_buff));
    screen_message("checkpointing...");
    screen_refresh();

    fd = open(checkpoint_filename,O_WRONLY|O_CREAT|O_TRUNC,0666);
    if(fd == 0 || fd == -1){
	sprintf(temp_buffer,"?Checkpoint file %s open failed %s",
	    checkpoint_filename,error_text(errno));
	error_message(temp_buffer);
	screen_refresh();
	sleep(2);
	return;
    }/* End IF */


    for(bp = buffer_headers; bp != NULL; bp = bp->next_header){
	if(bp->ismodified == NO && bp->buffer_number <= 0) continue;
	if(bp->buffer_number == 0)continue;
	i = (78 - strlen(bp->name) - 4) / 2;
	cp = temp_buffer;
	*cp++ = '\n';
	while(i-- > 0)*cp++ = '*';
	*cp++ = '(';
	strcpy(cp,bp->name);
	while(*cp)cp++;
	*cp++ = ')';
	if(bp->ismodified) *cp++ = '+';
	while(cp < &temp_buffer[78]) *cp++ = '*';
	*cp++ = '\n';
	*cp++ = '\0';
	i = strlen(temp_buffer);

	if(write(fd,temp_buffer,i) != i){
	    sprintf(temp_buffer,"?checkpoint failed %s",error_text(errno));
	    error_message(temp_buffer);
	    screen_refresh();
	    sleep(2);
	    close(fd);
	    return;
	}/* End IF */


	if(bp->ismodified){
	    status = buff_write(bp,fd,0,bp->zee);
	}/* End IF */

    }/* End FOR */

    close(fd);

    screen_message("checkpointing... done");
    screen_refresh();
    screen_message(message_save_buff);

}/* End Routine */

#endif /* CHECKPOINT */



#ifdef CHECKPOINT

/**
 * \brief On exit we clean up the checkpoint file
 *
 * This routine is called when a clean exit is assured. We remove
 * the checkpoint file so they don't clutter up the disk.
 */
void
remove_checkpoint_file()
{

    PREAMBLE();

    if(checkpoint_filename == NULL) return;
#ifndef VMS
    unlink(checkpoint_filename);
#endif

}/* End Routine */

#endif /* CHECKPOINT */



#if defined(VMS) || defined(STARDENT) || defined(STARDENT_860) || defined(SEQUOIA) || defined(LOCUS_SYSV) || defined(SCO_386)
/**
 * \brief Zero an array of bytes
 *
 * This routine zeros an array of bytes. For some reason the VMS
 * library doesn't seem to know about it.
 */
bzero(array,length)
register char *array;
register int length;
{
    PREAMBLE();

    while(length-- > 0) *array++ = '\0';

}/* End Routine */
#endif /*  VMS || STARDENT et al */



#ifdef UNIX
/**
 * \brief Issue a command to the operating system
 *
 * This routine is called in response to the \c EC command which allows the
 * user to execute operating system commands from within the editor.
 * Buffer content can either be piped into the process and replaced with its
 * output (filtering) or the command's output can be inserted at the current
 * buffer pointer position.
 *
 * \param uct		Command Token
 * \param arg_count	Number of arguments passed to \c EC. 0 results in
 *			unidirectional piping of the command's output into
 *			the edit buffer (at \e DOT). 1 or 2 indicates that
 *			\a arg1 and \a arg2 contain a buffer range that should
 *			be filtered through the command (single-arguments to
 *			\c EC indicating a relative buffer range in lines are
 *			already translated).
 * \param arg1		Start of buffer range. It is ensured to be less than
 *			\a arg2.
 * \param arg2		End of buffer range
 * \param cp		Command string that will be executed by \c /bin/sh
 *
 * \return		Status Code
 * \retval SUCCESS	Command succeeded
 * \retval FAIL		Command failed. Error message has been displayed.
 */
int
cmd_oscmd(struct cmd_token *uct, int arg_count, int arg1, int arg2, char *cp)
{
int last_intr_flag;
int line_cnt,w;
char tmpbuf[LINE_BUFFER_SIZE];
char pipebuff[IO_BUFFER_SIZE];
extern char susp_flag;
struct undo_token *ut;
int pid;
int bidir_flag = arg_count > 0;
int input_pipe[2], output_pipe[2];
int _errno;
int status;

    PREAMBLE();

/*
 * In bidirectional piping mode, the process will read stdin from input_pipe
 */
    if(bidir_flag){
	if (pipe(input_pipe)) {
		sprintf(tmpbuf,"?Error creating PIPE: %s",error_text(errno));
		error_message(tmpbuf);
		return(FAIL);
	}/* End IF */
    }/* End IF */

/*
 * otherwise the process will not get any input and only write stdout to
 * output_pipe
 */
    if(pipe(output_pipe)){
	sprintf(tmpbuf,"?Error creating PIPE: %s",error_text(errno));
	error_message(tmpbuf);
	return(FAIL);
    }/* End IF */

/*
 * Fork to get a process to run the shell
 */
    pid = fork();
    _errno = errno;
    if(pid == 0){
        if(bidir_flag){
            close(0); dup(input_pipe[0]);
            close(1); dup(output_pipe[1]);
            close(input_pipe[0]);
            close(input_pipe[1]);
	}else{
	    close(1); dup(output_pipe[1]);
	    close(0);
	}
	close(2); /* don't use stderr */
	close(output_pipe[0]);
	close(output_pipe[1]);

	/* Clean up */
	for (w = 3; w < 256; w++)
	    close(w);

	execl("/bin/sh","sh","-c",cp,NULL);
	exit(EXIT_FAILURE);
    }/* End IF */

/*
 * Close file descriptors which are no longer needed in the parent process
 */
    if(bidir_flag) close(input_pipe[0]);
    close(output_pipe[1]);

    if(pid == -1){
    	if(bidir_flag) close(input_pipe[1]);
	close(output_pipe[0]);

	sprintf(tmpbuf,"?Error forking child process: %s",error_text(_errno));
	error_message(tmpbuf);
	return(FAIL);
    }/* End IF */

/*
 * In bidirectional mode, write buffer range to input_pipe, delete buffer range
 * and care about the Undo structures
 */
    if(bidir_flag){
    	status = buff_write(curbuf,input_pipe[1],arg1,arg2);
    	close(input_pipe[1]);
    	if(status == FAIL){
    	    close(output_pipe[0]);
    	    goto failreap;
    	}/* End If */

	ut = allocate_undo_token(uct);
	if(ut == NULL){
	    close(output_pipe[0]);
	    goto failreap;
	}/* End If */
	ut->opcode = UNDO_C_CHANGEDOT;
	ut->iarg1 = curbuf->dot;

    	status = buff_delete_with_undo(uct,curbuf,arg1,arg2-arg1);
    	if(status == FAIL){
    	    close(output_pipe[0]);
    	    goto failreap;
    	}/* End If */
    }/* End If */

/*
 * Loop reading stuff coming back from the pipe until we get an
 * EOF which means the process has finished. Update the screen
 * on newlines so the user can see what is going on.
 *
 * In unidirectional mode, process output is written to DOT, otherwise to
 * the selected buffer range (it effectively gets replaced by the process output).
 */
    ut = allocate_undo_token(uct);
    if(ut == NULL){
    	close(output_pipe[0]);
    	goto failreap;
    }/* End If */
    ut->opcode = UNDO_C_DELETE;
    ut->carg1 = (char *)curbuf;
    ut->iarg1 = bidir_flag ? arg1 : curbuf->dot;
    ut->iarg2 = 0;

    last_intr_flag = intr_flag;
    line_cnt = 0;

    while((w = read(output_pipe[0],pipebuff,sizeof(pipebuff))) > 0){
    	char *p = pipebuff;

	status = buff_insert(curbuf,ut->iarg1 + ut->iarg2,pipebuff,w);
	if(status == FAIL){
	    close(output_pipe[0]);
    	    goto failreap;
        }/* End If */

	if(intr_flag != last_intr_flag){
	    kill(pid,SIGINT);
	    last_intr_flag = intr_flag;
	}/* End IF */

	if(susp_flag){
	    cmd_pause();
	}/* End IF */

	ut->iarg2 += w;

	while(w--){
	    if(intr_flag != last_intr_flag) break;
	    if(*p++ == '\n' && line_cnt++ > (term_lines / 4)){
		line_cnt = 0;
		if(tty_input_pending()) break;
		screen_format_windows();
		screen_refresh();
	    }/* End IF */
	}/* End While */
    }/* End While */
    _errno = errno;
    close(output_pipe[0]);
    if(w < 0){
    	sprintf(tmpbuf,"?Error reading from child process <%d>: %s",
    		pid,error_text(_errno));
	error_message(tmpbuf);
	goto failreap;
    }/* End If */
 
/*
 * The wait here is required so that the process we forked doesn't
 * stay around as a zombie. Also the command will only be Successful
 * if the process indicated Success.
 */
    if(waitpid(pid,&status,0) == -1){
    	sprintf(tmpbuf,"?Error reaping child process <%d>: %s",
    		pid,error_text(errno));
	error_message(tmpbuf);
	kill(pid,SIGKILL);
	return(FAIL);
    }/* End If */
    if (!WIFEXITED(status)){
    	sprintf(tmpbuf,"?Child process <%d> terminated abnormally",
    		pid);
	error_message(tmpbuf);
	return(FAIL);
    }/* End If */
    if(WEXITSTATUS(status) != EXIT_SUCCESS){
    	sprintf(tmpbuf,"?Child process <%d> terminated with status %d",
    		pid,WEXITSTATUS(status));
	error_message(tmpbuf);
	return(FAIL);
    }/* End If */

    return(SUCCESS);

/*
 * In case of errors, the process gets killed and is reaped.
 * NOTE: Error message has already been displayed
 */

failreap:
    kill(pid,SIGKILL);
    waitpid(pid,NULL,0);
    return(FAIL);

}/* End Routine */

/* END OF UNIX CONDITIONAL CODE */

#else /* UNIX */

/**
 * \brief Issue a command to the operating system
 *
 * This routine is called in response to the EC command which allows the
 * user to execute operating system commands from within the editor.
 */
int
cmd_oscmd(struct cmd_token *uct, int arg_count, int arg1, int arg2, char *cp)
{
    PREAMBLE();

    /*
     * FIXME: We could at least implement a subset with system(),
     * that would work on DOS as well.
     */
    error_message("?OS Does not currently support EC");
    return(FAIL);

}/* End Routine */

#endif



/**
 * \brief Loads buffer name into q-register *
 *
 * This routine is called by a command operating on the text
 * portion of a q-register if the specified q-register is *.
 * In this case, we load the name of the current edit buffer
 * into it so the user can easilly access it.
 */
void
load_qname_register()
{
register struct buff_header *hbp;
register struct buff_header *qbp;

    PREAMBLE();

    hbp = curbuf;
    qbp = buff_qfind('*',1);

    if(qbp->zee > 0){
	buff_delete(qbp,0,qbp->zee);
    }/* End IF */

    buff_insert(qbp,qbp->dot,hbp->name,strlen(hbp->name));

}/* End Routine */

/**
 * \brief Change the name of the specified edit buffer
 *
 * This routine is called by parser exec routines which have
 * loaded q-register *. Since the text portion of this q-register
 * is the buffer name, it has the effect of changing the name
 * of the buffer.
 */
int
rename_edit_buffer(
					struct buff_header *hbp,
					char *new_name,
					struct cmd_token *uct )
{
register int i;
register struct undo_token *ut;
int length;

    PREAMBLE();

    ut = allocate_undo_token(uct);
    if(ut == NULL) return(FAIL);
    ut->opcode = UNDO_C_RENAME_BUFFER;
    ut->carg1 = hbp->name;

    length = strlen(new_name);
    hbp->name = tec_alloc(TYPE_C_CBUFF,length+1);
    if(hbp->name == NULL) return(FAIL);
    for(i = 0; i < length; i++){
	hbp->name[i] = new_name[i];
    }/* End FOR */
    hbp->name[length] = '\0';

    buff_switch(hbp,0);
    return(SUCCESS);

}/* End Routine */



/**
 * \brief Set runtime options via the EJ command
 *
 * This routine is called when the user sets runtime variables using
 * the EJ command. Although the command is a classic TECO command, the
 * actual values are specific to Video TECO
 */
int
cmd_setoptions(
				int arg1,
				int arg2,
				struct undo_token *uct )
{
struct undo_token fake_token;
extern int tab_width;

    PREAMBLE();

    if(arg1 < SETOPTION_MIN_OPTION || arg1 > SETOPTION_MAX_OPTION){
	error_message("EJ option selector out of range");
	return(FAIL);
    }/* End IF */

    if(!uct) uct = &fake_token;

    switch(arg1){
	case SETOPTION_ALTERNATE_ESCAPE_CHAR:
	    uct->iarg1 = arg1;
	    uct->iarg2 = alternate_escape_character;
	    alternate_escape_character = arg2;
	    break;
	case SETOPTION_SCREEN_WIDTH:
	    uct->iarg1 = arg1;
	    uct->iarg2 = forced_width;
	    forced_width = arg2;
	    screen_resize();
	    break;
	case SETOPTION_SCREEN_HEIGHT:
	    if(arg2 != 0 && arg2 < 3){
		error_message("TECO requires a larger window");
		return(FAIL);
	    }/* End IF */
	    uct->iarg1 = arg1;
	    uct->iarg2 = forced_height;
	    forced_height = arg2;
	    screen_resize();
	    break;
	case SETOPTION_ALTERNATE_DELETE_CHAR:
	    uct->iarg1 = arg1;
	    uct->iarg2 = alternate_delete_character;
	    alternate_delete_character = arg2;
	    break;
	case SETOPTION_FORCE_8_BIT_CHARS:
	    uct->iarg1 = arg1;
	    uct->iarg2 = eight_bit_output_flag;
	    eight_bit_output_flag = arg2 ? YES : NO;
	    screen_reformat_windows();
	    break;
	case SETOPTION_TAB_WIDTH:
	    uct->iarg1 = arg1;
	    uct->iarg2 = tab_width;
	    tab_width = arg2 >= 1 && arg2 <= 16 ? arg2 : NORMAL_TAB_WIDTH;
	    screen_reformat_windows();
	    break;
	case SETOPTION_HIDE_CR_CHARS:
	    uct->iarg1 = arg1;
	    uct->iarg2 = hide_cr_flag;
	    hide_cr_flag = arg2 ? YES : NO;
	    screen_reformat_windows();
	    break;

    }/* End Switch */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Perform UNIX tags function
 *
 * This routine performs various UNIX tags functions. The following
 * functions are supported:
 *
 * <table>
 *	<tr>
 *		<td>\b arg1</td><td>\b arg2</td><td>\b string</td>
 *		<td>\b Description</td>
 *	</tr><tr>
 *		<td>0 \e (optional)</td><td>-</td><td>Tags file name</td>
 *		<td>Load tags file given in \a string.
 *		    It can be either in \e etags or \e ctags format.</td>
 *	</tr><tr>
 *		<td>1</td><td>\e optional (default: 0)</td><td>Symbol name</td>
 *		<td>Find tag entry whose symbol matches \a string,
 *		    skipping \a arg2 matching entries</td>
 *	</tr><tr>
 *		<td>2</td><td>-</td><td>-</td>
 *		<td>Check whether a tags file has been loaded</td>
 *	</tr><tr>
 *		<td>3</td><td>-</td><td>-</td>
 *		<td>Insert current tag entry's target file name into the edit buffer</td>
 *	</tr><tr>
 *		<td>4</td><td>-</td><td>-</td>
 *		<td>Insert current tag entry's search string into the edit buffer</td>
 *	</tr><tr>
 *		<td>5</td><td>-</td><td>-</td>
 *		<td>Insert current tag entry's line number into the edit buffer</td>
 *	</tr><tr>
 *		<td>6</td><td>-</td><td>-</td>
 *		<td>Dump entire tag database into the edit buffer</td>
 *	</tr>
 * </table>
 *
 * \param uct		Command Token
 * \param arg_count	Number of arguments: 0 \e none, 1 \a arg1 given,
 *			2 \a arg1 and \a arg2 given
 * \param arg1		Operation identifier
 * \param arg2		Operation parameter if required, depending on \a arg1
 * \param string	Tags file name or symbol depending on \a arg1
 *
 * \return		Status code
 * \retval SUCCESS	Operation was successful
 * \retval FAIL		Operation failed, an error message has been displayed
 */
int
cmd_tags(
			struct cmd_token *uct,
			int arg_count,
			int arg1,
			int arg2,
			char *string )
{
register struct undo_token *ut;
register struct tags *old_tags;
register struct tags *new_tags;
register struct tagent *tep = NULL;
int hashval,skip_cnt;

    PREAMBLE();

    if(arg_count == 0) arg1 = 0;
    if(arg_count < 2) arg2 = 0;

    if(arg1 < TAGS_MIN_OPTION || arg1 > TAGS_MAX_OPTION){
	error_message("FT option selector out of range");
	return(FAIL);
    }/* End IF */

    switch(arg1){
	case TAGS_LOAD_TAGS_FILE:
	    old_tags = current_tags;
	    new_tags = tag_load_file(string);
	    if(new_tags == NULL) return(FAIL);
	    current_tags = new_tags;
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_LOAD_TAGS;
	    ut->carg1 = (char *)old_tags;
	    break;
	case TAGS_SEARCH_TAGS_FILE:
	    if(current_tags == NULL) return(FAIL);
	    hashval = tag_calc_hash(string);
	    tep = current_tags->tagents[hashval];
	    skip_cnt = arg2;
	    while(tep){
		if(strcmp(string,tep->te_symbol) == 0){
		    if(skip_cnt-- <= 0){
			if(current_tags->current_entry != tep){
		    	    ut = allocate_undo_token(uct);
		    	    if(ut == NULL) return(FAIL);
		    	    ut->opcode = UNDO_C_SELECT_TAGS;
		    	    ut->carg1 = (char *)current_tags->current_entry;
			}/* End IF */
			current_tags->current_entry = tep;
			return(SUCCESS);
		    }/* End IF */
		}/* End IF */
		tep = tep->te_next;
	    }/* End While */
	    return(FAIL);
	case TAGS_TEST_FOR_LOADED_TAGS:
	    if(current_tags){
		return(SUCCESS);
	    }/* End IF */
	    return(FAIL);
	case TAGS_INSERT_TARGET_FILE:
	    if(current_tags->current_entry){
		if(current_tags->current_entry->te_filename){
		    buff_insert_with_undo(
			uct,
			curbuf,
			curbuf->dot,
			current_tags->current_entry->te_filename,
			strlen(current_tags->current_entry->te_filename)
		    );
		    return(SUCCESS);
		}/* End IF */
	    }/* End IF */
	    return(FAIL);
	case TAGS_INSERT_SEARCH_STRING:
	    if(current_tags->current_entry){
		if(current_tags->current_entry->te_search_string){
		    buff_insert_with_undo(
			uct,
			curbuf,
			curbuf->dot,
			current_tags->current_entry->te_search_string,
			strlen(current_tags->current_entry->te_search_string)
		    );
		    return(SUCCESS);
		}/* End IF */
	    }/* End IF */
	    return(FAIL);
	case TAGS_INSERT_LINENO:
	    if(current_tags->current_entry){
		if(current_tags->current_entry->te_lineno){
		    buff_insert_with_undo(
			uct,
			curbuf,
			curbuf->dot,
			current_tags->current_entry->te_lineno,
			strlen(current_tags->current_entry->te_lineno)
		    );
		    return(SUCCESS);
		}/* End IF */
	    }/* End IF */
	    return(FAIL);
	case TAGS_INSERT_ALL:
	    if(current_tags){
		tag_dump_database(current_tags,uct);
		return(SUCCESS);
	    }/* End IF */
	    return(FAIL);
    }/* End Switch */

    return(SUCCESS);

}/* End Routine */

/**
 * \brief Read in a tags file
 *
 * This is a huge monolithic nasty routine which reads either VI (\e ctags) or
 * EMACS (\e etags) tags files, and builds an internal representation.
 *
 * \param string	File name of tags file to load
 *
 * \return		Internal representation of tags database
 * \retval NULL		An error occurred, an error message has been displayed
 */
struct tags *
tag_load_file( char *string )
{
FILE *fd = NULL;
register struct tags *tp = NULL;
register struct tagent *tep = NULL;
register char *cp;
char *outcp = NULL;
char *filename_cp = NULL;
int state = 0;
int hashval;
int c;
char emacs_flag = NO;
char comma_seen = 0;
char tmp_buffer[LINE_BUFFER_SIZE];
char tmp_search_string[LINE_BUFFER_SIZE];
char tmp_filename[LINE_BUFFER_SIZE];
char tmp_symbol[LINE_BUFFER_SIZE];
char tmp_number[LINE_BUFFER_SIZE];
char tmp_message[LINE_BUFFER_SIZE];
int line = 0;

    fd = fopen(string,"r");
    if(fd == NULL){
	sprintf(
	    tmp_message,
	    "Could not open TAGS file <%s>: %s",
	    string,
	    error_text(errno)
	);
	error_message(tmp_message);
	return(NULL);
    }/* End IF */

    tp = (struct tags *)tec_alloc(TYPE_C_TAGS,sizeof(struct tags));
    if(tp == NULL) return(NULL);
    bzero((char *)tp,sizeof(struct tags));

/*
 * Loop reading tag entries
 */
    while(1){
	cp = tmp_buffer;
	bzero(cp,sizeof(tmp_buffer));
	if(fgets(cp,sizeof(tmp_buffer),fd) == NULL) break;
	line++;
/*
 * Ok, we have the start of an entry. Check for continuation.
 */
	while(cp[0]){
	    if(cp[0] == '\n'){
		cp[1] = '\0';
		break;
	    }/* End IF */
	    if(cp[0] == '\\' && cp[1] == '\n'){
		if(fgets(cp,sizeof(tmp_buffer)-(cp-tmp_buffer),fd) == NULL){
		    cp[0] = '\n'; cp[1] = '\0';
		}/* End IF */
		line++;
	    }/* End IF */
	    cp++;
	}/* End While */

/*
 * First step is to get the name of the symbol so that we can obtain the
 * hash value
 */
	cp = tmp_buffer;

	if(emacs_flag == NO) state = 0;

	while(*cp){
	    if(state < 0) break;
	    switch(state){
/*
 * Skip over symbol name until we hit whitespace. Then calculate the hash
 * value for that symbol, allocate the tag entry structure, and copy the
 * symbol name into it.
 */
		case 0:
		    if(cp[0] == '\f' && cp[1] == '\n'){
			emacs_flag = YES;
			state = 100;
			continue;
		    }/* End IF */

		    if(isspace((int)*cp)){
			c = *cp;
			*cp = '\0';
			hashval = tag_calc_hash(tmp_buffer);
			tep = (struct tagent *)tec_alloc(
			    TYPE_C_TAGENT,
			    sizeof(struct tagent)
			);
			if(tep == NULL) goto err;
			bzero((char *)tep,sizeof(*tep));

			tep->te_next = tp->tagents[hashval];
			tp->tagents[hashval] = tep;

			tep->te_symbol = (char *)tec_alloc(
			    TYPE_C_TAGSTR,
			    strlen(tmp_buffer) + 1
			);
			if(tep->te_symbol == NULL) goto err;
			strcpy(tep->te_symbol,tmp_buffer);

			*cp = c;
			state++;
		    }/* End IF */
		    cp++;
		    continue;
/*
 * Skip any amount of whitespace which seperates the symbol from the
 * filename.
 */
		case 1:
		    if(!isspace((int)*cp)){
			filename_cp = cp;
			state++;
			continue;
		    }/* End IF */
		    cp++;
		    continue;
/*
 * Find the length of the filename portion and copy it to the tagent
 */
		case 2:
		    if(isspace((int)*cp)){
			c = *cp;
			*cp = '\0';

			tep->te_filename = (char *)tec_alloc(
			    TYPE_C_TAGSTR,
			    strlen(filename_cp) + 1
			);
			if(tep->te_filename == NULL) goto err;
			strcpy(tep->te_filename,filename_cp);

			*cp = c;
			state++;
		    }/* End IF */
		    cp++;
		    continue;
/*
 * Skip any amount of whitespace which seperates the filename from the
 * search string
 */
		case 3:
		    if(!isspace((int)*cp)){
			state++;
			continue;
		    }/* End IF */
		    cp++;
		    continue;

/*
 * The next character should be a slash to begin the search string
 */
		case 4:
		    if(*cp == '/'){
			outcp = tmp_search_string;
			*outcp = '\0';
			state++;
		    }
		    cp++;
		    continue;
/*
 * Find the length of the search string portion and copy it to the tagent
 */
		case 5:
		    if(*cp == '/'){
			*outcp = '\0';

			tep->te_search_string = (char *)tec_alloc(
			    TYPE_C_TAGSTR,
			    strlen(tmp_search_string) + 1
			);
			if(tep->te_search_string == NULL) goto err;
			strcpy(tep->te_search_string,tmp_search_string);

			state = -1;
			continue;
		    }/* End IF */
		    if(*cp == '\\'){
			state = state + 1;
			cp++;
			continue;
		    }/* End IF */
		    *outcp++ = *cp++;
		    continue;

/*
 * Here on a backquote character. Just skip it and pass through the quoted
 * character without testing it for termination of the search string.
 */
		case 6:
		    *outcp++ = *cp++;
		    state = state - 1;
		    continue;

/*
 * Here if we encounter a form-feed. This is a giveaway that we have an
 * emacs style tags file, so we have to parse it differently.
 */
		case 100:
		    if(cp[0] != '\f' || cp[1] != '\n'){
			cp++;
			continue;
		    }/* End IF */
		    cp += 2;
		    state = 101;
		    outcp = tmp_filename;
		    comma_seen = 0;
		    continue;
/*
 * Here we get the name of the source file
 */
		case 101:
		    if(*cp == '\n'){
			if(comma_seen == 0){
			    state = 100;
			    continue;
			}/* End IF */
			outcp--;
			while(*outcp != ',') outcp--;
			*outcp = '\0';
			cp++;
			state = 102;
			outcp = tmp_symbol;
			*outcp++ = 127;
			continue;
		    }/* End IF */

		    if(*cp == ',') comma_seen = 1;
		    *outcp++ = *cp++;
		    continue;
/*
 * Now we get symbol names
 */
		case 102:
		    if(*cp == '\f'){
			state = 100;
			continue;
		    }/* End IF */

		    if(*cp != 127){
			*outcp++ = *cp++;
			continue;
		    }/* End IF */
/*
 * Here on a 127 code which terminates the symbol name. First eat back to
 * the first symbol character.
 */
		    while((c = outcp[-1])){
			if(c == 127) break;
			if(
			    isalnum(c) ||
			    isdigit(c) ||
			    c == '_' ||
			    c == '$' ||
			    c == '.'
			){
			    *outcp = '\0';
			    break;
			}/* End IF */
			outcp--;
		    }/* End While */
/*
 * Now go back until we find the first non-symbol character.
 */
		    while((c = outcp[-1])){
			if(c == 127) break;
			if(
			    isalnum(c) ||
			    isdigit(c) ||
			    c == '_' ||
			    c == '$' ||
			    c == '.'
			){
			    outcp--;
			    continue;
			}/* End IF */
			break;
		    }/* End While */

		    hashval = tag_calc_hash(outcp);
		    tep = (struct tagent *)tec_alloc(
			TYPE_C_TAGENT,
			sizeof(struct tagent)
		    );
		    if(tep == NULL) goto err;
		    bzero((char *)tep,sizeof(*tep));

		    tep->te_next = tp->tagents[hashval];
		    tp->tagents[hashval] = tep;

		    tep->te_symbol = (char *)tec_alloc(
			TYPE_C_TAGSTR,
			strlen(outcp) + 1
		    );
		    if(tep->te_symbol == NULL) goto err;
		    strcpy(tep->te_symbol,outcp);
		    tep->te_filename = (char *)tec_alloc(
			TYPE_C_TAGSTR,
			strlen(tmp_filename) + 1
		    );
		    if(tep->te_filename == NULL) goto err;
		    strcpy(tep->te_filename,tmp_filename);

		    state = 103;
		    continue;

/*
 * Eat until we reach a number, which is the line number
 */
		case 103:
		    if(isdigit((int)*cp)){
			state = 104;
			outcp = tmp_number;
			continue;
		    }/* End IF */

		    if(*cp == '\f'){
			state = 100;
			continue;
		    }/* End IF */

		    cp++;
		    continue;
/*
 * Read in the line number
 */
		case 104:
		    if(isdigit((int)*cp)){
			*outcp++ = *cp++;
			continue;
		    }/* End IF */

		    *outcp++ = '\0';
		    tep->te_lineno = (char *)tec_alloc(
			TYPE_C_TAGSTR,
			strlen(tmp_number) + 1
		    );
		    if(tep->te_lineno == NULL) goto err;
		    strcpy(tep->te_lineno,tmp_number);

		    state = 105;
		    continue;
/*
 * Eat until end of line
 */
		case 105:
		    if(*cp == '\n'){
			cp++;
			state = 102;
			outcp = tmp_symbol;
			continue;
		    }/* End IF */
		    if(*cp == '\f'){
			state = 100;
			continue;
		    }/* End IF */
		    cp++;
		    continue;
	    }/* End Switch */
	}/* End While */

    }/* End While */

    fclose(fd);
    return(tp);

err:

    tag_free_struct(tp);
    if(fd) fclose(fd);
    return(NULL);

}/* End Routine */

/**
 * \brief Release memory associated with a tags database
 *
 * \param tp Tags database
 */
void
tag_free_struct( struct tags *tp )
{
register struct tagent **tepp;
register struct tagent *tep;
register int i;

    if(tp == NULL) return;

    for(i = 0, tepp = &tp->tagents[0]; (unsigned)i < ELEMENTS(tp->tagents); i++,tepp++){
	while((tep = *tepp)){
	    *tepp = tep->te_next;

	    if(tep->te_symbol){
		tec_release(TYPE_C_TAGSTR,tep->te_symbol);
	    }/* End IF */

	    if(tep->te_filename){
		tec_release(TYPE_C_TAGSTR,tep->te_filename);
	    }/* End IF */

	    if(tep->te_search_string){
		tec_release(TYPE_C_TAGSTR,tep->te_search_string);
	    }/* End IF */

	    tec_release(TYPE_C_TAGENT,(char *)tep);

	}/* End While */

    }/* End FOR */

    tec_release(TYPE_C_TAGS,(char *)tp);

}/* End Routine */

/**
 * \brief Hash function for tag entries of a tags database
 *
 * \param string	Tag entry symbol name
 *
 * \return		Hash value
 */
int
tag_calc_hash( char *string )
{
register int hash = 0;
register int c;
register int shift = 0;

    while((c = *string++)){
	shift = shift == 8 ? 0 : shift + 1;
	hash += c << shift;
    }/* End While */

    return(hash % TAG_HASH_ENTRIES);

}/* End Routine */

/**
 * \brief Dump tags database into edit buffer
 *
 * \param tp	Tags database
 * \param uct	Command Token
 */
void
tag_dump_database( struct tags *tp, struct cmd_token *uct )
{
register struct tagent **tepp;
register struct tagent *tep;
register int i;
char tmp_output[LINE_BUFFER_SIZE];

    if(tp == NULL) return;

    for(i = 0, tepp = &tp->tagents[0]; (unsigned)i < ELEMENTS(tp->tagents); i++,tepp++){
	tep = *tepp;
	while(tep){

	    sprintf(
		tmp_output,
		"sym <%s> hash %d file <%s> line <%s> search <%s>\n",
		tep->te_symbol ? tep->te_symbol : "",
		tep->te_symbol ? tag_calc_hash(tep->te_symbol) : 0,
		tep->te_filename ? tep->te_filename : "",
		tep->te_lineno ? tep->te_lineno : "",
		tep->te_search_string ? tep->te_search_string : ""
	    );

	    buff_insert_with_undo(
		uct,
		curbuf,
		curbuf->dot,
		tmp_output,
		strlen(tmp_output)
	    );

	    tep = tep->te_next;

	}/* End While */
    }/* End FOR */

    return;

}/* End Routine */
