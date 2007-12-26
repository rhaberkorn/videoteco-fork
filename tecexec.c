char *tecexec_c_version = "tecexec.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/tecexec.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/* tecexec.c
 * The SWITCH/CASE statements which implement execution stage of the parser
 *
 *
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

    extern struct cmd_token *jump_to_token;
    extern struct cmd_token *last_token_executed;
    extern struct cmd_token *last_cmd_list;
    extern struct search_buff search_string;
    extern char user_message[PARSER_STRING_MAX];

    extern char exit_flag;
    extern struct window *window_list;
    extern struct buff_header *curbuf,*buffer_headers;
    extern struct buff_header *qregister_push_down_list;
    extern int last_search_pos1,last_search_pos2;
    extern int last_search_status;
    extern int remembered_dot;
    extern char intr_flag;
    extern char immediate_execute_flag;
    extern struct window *curwin;

    int find_conditional_else(struct cmd_token *);
    int find_conditional_end(struct cmd_token *);
    int compare_label(struct cmd_token *,struct cmd_token *);
    void extract_label(struct cmd_token *,char *);
	struct wildcard_expansion *expand_filename( char *wildcard_string );



/* EXECUTE_A_STATE - Do the runtime execution part of a command
 *
 * Function:
 *
 *	This routine is called to execute the runtime part of a command token.
 *	This could be during immediate execution mode, directly after a token
 *	gets parsed, or it could be during final stages when we are either
 *	running commands which cannot be undone (like ex), or simply complex
 *	things like an iteration. The execute_state which we dispatch on was
 *	set by the parse state during syntax analysis.
 */
int
execute_a_state( struct cmd_token *ct, struct cmd_token *uct )
{
register struct undo_token *ut;
char tmp_buffer[LINE_BUFFER_SIZE],tmp_message[LINE_BUFFER_SIZE];

    PREAMBLE();

    last_token_executed = ct;

    switch(ct->execute_state){
/*
 * Here to return the numeric value of a q-register. This has to be done
 * at run time because the value of the q-register might be modified during
 * the course of the command. Note that we return an error if the q-register
 * has never been loaded. Otherwise, we return the value in tmpval so that
 * it can be the input to further arithmetic expressions.
 */
	case EXEC_C_QVALUE:
	    {/* Local Block */
	    register struct buff_header *hbp;

	    if(ct->q_register == '*'){
		ct->ctx.tmpval = curbuf->buffer_number;
		return(SUCCESS);
	    }/* End IF */

	    hbp = buff_qfind(ct->q_register,0);
	    if(hbp == NULL){
		sprintf(tmp_message,"?Q-register %c empty",ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
	    ct->ctx.tmpval = hbp->ivalue;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to return the numeric value of a q-register and also increment it's
 * value. Other than the incrementing, this is exactly like the QVALUE state.
 */
	case EXEC_C_PERCENT_VALUE:
	    {/* Local Block */
	    register struct buff_header *hbp;

	    hbp = buff_qfind(ct->q_register,0);
	    if(hbp == NULL){
		sprintf(tmp_message,"?Q-register %c empty",ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * Set up an undo block so that we can put the previous value back into
 * the q-register if this command gets undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);

	    ut->opcode = UNDO_C_UQREGISTER;
	    ut->iarg1 = ct->q_register;
	    ut->iarg2 = hbp->ivalue;

	    hbp->ivalue += ct->ctx.tmpval;
	    ct->ctx.tmpval = hbp->ivalue;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to read a file into a q-register. We also set up undo tokens to
 * restore the previous contents of the q-register if he undoes this.
 */
	case EXEC_C_EQQREGISTER:
	    {/* Local Block */
	    register struct buff_header *qbp;
	    struct wildcard_expansion *name_list,*np;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = curbuf->buffer_number;

	    qbp = buff_qfind(ct->ctx.tmpval,1);

	    if(ct->ctx.tmpval == '*'){
		if(ct->ctx.carg){
		    rename_edit_buffer(curbuf,ct->ctx.carg,uct);
		}/* End IF */
		else {
		    load_qname_register();
		    buff_switch(qbp,1);
		}/* End Else */

		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = SUCCESS;
		}/* End IF */
		return(SUCCESS);
	    }/* End IF */

	    if(ct->ctx.carg == NULL || ct->ctx.carg[0] == '\0'){
		buff_switch(qbp,1);
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = SUCCESS;
		}/* End IF */
		return(SUCCESS);
	    }/* End IF */
/*
 * Delete the previous contents of the q-register, but allow it to be
 * reconstructed if this gets undone. Note that we don't have to do
 * this if there was nothing there. Also, if the colon modifier was
 * specified, then we append to the buffer instead of replacing it.
 */
	    if((ct->ctx.flags & CTOK_M_COLON_SEEN) == 0 && qbp->zee){
		buff_delete_with_undo(uct,qbp,0,qbp->zee);
	    }/* End IF */

/*
 * Set up an undo argument to delete any characters which get inserted as
 * part of the buff_read we do to load the q-register.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)qbp;
	    ut->iarg1 = qbp->dot;
	    ut->iarg2 = qbp->zee;

	    np = expand_filename(ct->ctx.carg);
	    if(np == NULL){
		sprintf(tmp_message,"EQ No such file <%s>",ct->ctx.carg);
		error_message(tmp_message);
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    while(np){
		buff_read(qbp,np->we_name);
		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,(char *)name_list);
	    }/* End While */

	    ut->iarg2 = qbp->zee - ut->iarg2;

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = SUCCESS;
	    }/* End IF */
	    return(SUCCESS);
	    }/* End Local Block */

/*
 * Here to load a q-register with a numeric value. Note that not only do we
 * have to put the new number into it, we also have to set up the undo token
 * so that if this command is backed out of we can restore the previous value
 * to the q-register.
 */
	case EXEC_C_UQREGISTER:
	    {/* Local Block */
	    register struct buff_header *hbp;
/*
 * Find the q-register. If it does not exist yet, create it.
 */
	    hbp = buff_qfind(ct->q_register,1);
	    if(hbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Insure that we have an argument to store
 */
	    if(ct->ctx.iarg1_flag == NO){
		error_message("?Argument required for U command");
		return(FAIL);
	    }/* End IF */
/*
 * Set up an undo block so that we can put the previous value back into
 * the q-register if this command gets undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_UQREGISTER;
	    ut->iarg1 = ct->q_register;
	    ut->iarg2 = hbp->ivalue;

	    hbp->ivalue = ct->ctx.iarg1;
	    return(SUCCESS);
	    }/* End Local Block */

/*
 * Here to load the text contents of the specified q-register into the
 * edit buffer at the current location.
 */
	case EXEC_C_GQREGISTER:
	    {/* Local Block */
	    register struct buff_header *hbp;
	    register struct buff_header *qbp;

	    hbp = curbuf;
/*
 * If this is the buffer name q-register, we have to do some special
 * stuff.
 */
	    if(ct->q_register == '*') load_qname_register();
/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, declare an error.
 */
	    qbp = buff_qfind(ct->q_register,0);
	    if(qbp == NULL){
		sprintf(tmp_message,"?Q-register <%c> does not exist",
		    ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * Test if the q-register is empty, and declare an error if it is
 */
	    if(qbp->zee == 0){
		sprintf(tmp_message,"?Q-register <%c> is empty",
		    ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * Make sure the Q register and the edit buffer are not the same buffer
 */
	    if(qbp == hbp){
		strcpy(tmp_message,
		    "?Cannot use G to copy from a Q-register to itself");
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * Now copy the text out of the q-register and insert it into the edit buffer
 */
	    buff_insert_from_buffer_with_undo(uct,hbp,hbp->dot,qbp,0,qbp->zee);

	    return(SUCCESS);
	    }/* End Local Block */

/*
 * Here to execute the macro in the specified Q register
 */
	case EXEC_C_MQREGISTER:
	    {/* Local Block */
	    register struct buff_header *qbp;
/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, declare an error.
 */
	    qbp = buff_qfind(ct->q_register,0);
	    if(qbp == NULL){
		sprintf(tmp_message,"?Q-register <%c> does not exist",
		    ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * It's an error if the q-register is empty
 */
	    if(qbp->zee == 0){
		sprintf(tmp_message,"?Q-register <%c> is empty",
		    ct->q_register);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * Set up the undo block, and pass its address to the tecmacro routine so
 * that the tecmacro can chain the command list off of it.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_MACRO;
	    ut->carg1 = NULL;

	    return(tecmacro(qbp,ct,(struct cmd_token **)&ut->carg1));

	    }/* End Local Block */

/*
 * Here to load text into a q-register in response to the X command.
 * This clears any existing text from the q-register, and then copies
 * the specified text into it.
 */
	case EXEC_C_XQREGISTER:
	    {/* Local Block */
	    register struct buff_header *hbp;
	    register struct buff_header *qbp;
	    register struct buff_line *lbp;
	    register int i,j;
	    int pos1,pos2;

	    hbp = curbuf;
/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, create it.
 */
	    qbp = buff_qfind(ct->q_register,1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Make sure the Q register and the edit buffer are not the same buffer
 */
	    if(qbp == hbp){
		strcpy(tmp_message,
		    "?Cannot use X to copy from a Q-register to itself");
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */
/*
 * If two arguments were specified, then he has specified an a,b range to
 * be copied.
 */
	    if(ct->ctx.iarg1_flag == YES && ct->ctx.iarg2_flag == YES){
		pos1 = ct->ctx.iarg1;
		pos2 = ct->ctx.iarg2;
		if(parse_illegal_buffer_position(pos1,pos2,"X")){
		    return(FAIL);
		}/* End IF */
		if(pos2 < pos1){
		    pos2 = ct->ctx.iarg1;
		    pos1 = ct->ctx.iarg2;
		}/* End IF */
	    }/* End IF */
/*
 * Else, if it was a single command, it is a relative copy which affects a
 * portion of the buffer in a similar way as if it was an L command. Our job
 * now is to turn it into a a,b range.
 */
	    else {
		lbp = buff_find_line(hbp,hbp->dot);
		if(lbp == NULL) return(FAIL);
/*
 * Start by pretending it was a 0L command, and get to the begining of
 * the line. That allows us to not worry about offset onto current line.
 */
		j = 0 - buff_find_offset(hbp,lbp,hbp->dot);
		i = ct->ctx.iarg1;
		if(ct->ctx.iarg1_flag == NO) i = 1;
/*
 * If argument is positive, movement is forward
 */
		if(i > 0){
		    while(i-- && lbp){
			j += lbp->byte_count;
			lbp = lbp->next_line;
		    }/* End WHILE */
		    if(lbp == NULL){
			error_message("?Attempt to Move Pointer Off Page with X");
			return(FAIL);
		    }/* End IF */
		    pos1 = hbp->dot;
		    pos2 = hbp->dot + j;
		}/* End IF */
/*
 * Else, it is backwards
 */
		else {

		    while(i++ && lbp){
			lbp = lbp->prev_line;
			if(lbp == NULL){
			    error_message("?Attempt to Move Pointer Off Page with X");
			    return(FAIL);
			}/* End IF */
			j -= lbp->byte_count;
		    }/* End WHILE */
		    pos1 = hbp->dot + j;
		    pos2 = hbp->dot;
		}/* End Else */
	    }/* End Else */
/*
 * Now that we have calculated the buffer positions to be used by the command,
 * we can actually do the work.
 */
	    j = pos2 - pos1;
/*
 * Delete the previous contents of the q-register, but allow it to be
 * reconstructed if this gets undone. Note that we don't have to do
 * this if there was nothing there. Also, if the colon modifier was
 * specified, then we append to the buffer instead of replacing it.
 */
	    if((ct->ctx.flags & CTOK_M_COLON_SEEN) == 0 && qbp->zee){
		buff_delete_with_undo(uct,qbp,0,qbp->zee);
	    }/* End IF */
/*
 * Now copy the new text from the edit buffer into the q-register
 */
	    buff_insert_from_buffer_with_undo(uct,qbp,qbp->zee,hbp,pos1,j);
/*
 * If the q-register specified is '*', then the actual result is to rename
 * the current buffer.
 */
	    if(ct->q_register == '*'){
		rename_edit_buffer(hbp,qbp->name,uct);
	    }/* End IF */

	    return(SUCCESS);
	    }/* End Local Block */

/*
 * Here to load text into a q-register in response to the * command.
 * This clears any existing text from the q-register, and then copies
 * the previous command string into it.
 */
	case EXEC_C_SAVECOMMAND:
	    {/* Local Block */
	    register struct buff_header *qbp;
	    register struct cmd_token *oct;
	    register int i;

/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, create it.
 */
	    qbp = buff_qfind(ct->q_register,1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Delete the previous contents of the q-register, but allow it to be
 * reconstructed if this gets undone. Note that we don't have to do
 * this if there was nothing there.
 */
	    if(qbp->zee) buff_delete_with_undo(uct,qbp,0,qbp->zee);
/*
 * Set up an undo token to delete the contents of the q-register that we
 * are about to load.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)qbp;
	    ut->iarg1 = 0;
	    ut->iarg2 = 0;

/*
 * Now we go through the last command list and anytime we see an input
 * byte, we copy it into the q-register.
 */
	    oct = last_cmd_list;
	    i = 0;
	    while(oct){
		if(oct->flags & TOK_M_EAT_TOKEN){
		    buff_insert_char(qbp,i++,oct->input_byte);
		    ut->iarg2 += 1;
		}/* End IF */
		oct = oct->next_token;
	    }/* End While */

	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to push the specified q-register onto the q-register stack
 */
	case EXEC_C_PUSH_QREGISTER:
	    if(ct->q_register == '*') load_qname_register();
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    if(push_qregister(ct->q_register) != SUCCESS) return(FAIL);
	    ut->opcode = UNDO_C_POP;
	    return(SUCCESS);
/*
 * Here to pop a Q register off of the stack and replace a Q register
 * with it. Since this destroys the Q register we pop onto, we first
 * must save the entire Q register.
 */
	case EXEC_C_POP_QREGISTER:
	    {/* Local Block */
	    register struct buff_header *pbp;
	    register struct buff_header *qbp;
/*
 * Get a pointer to the pushed buffer structure. If there are none on
 * the stack, then he has tried to pop too many, and this is an error.
 */
	    pbp = qregister_push_down_list;

	    if(pbp == NULL){
		error_message("?Q-register push down list is empty");
		return(FAIL);
	    }/* End IF */
/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, create it.
 */
	    qbp = buff_qfind(ct->q_register,1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Arrange to restore the numeric contents of the Q register if this
 * should be undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_UQREGISTER;
	    ut->iarg1 = ct->q_register;
	    ut->iarg2 = qbp->ivalue;

	    qbp->ivalue = pbp->ivalue;
/*
 * Delete the previous contents of the q-register, but allow it to be
 * reconstructed if this gets undone. Note that we don't have to do
 * this if there was nothing there.
 */
	    if(qbp->zee){
		if(buff_delete_with_undo(uct,qbp,0,qbp->zee) == FAIL){
		    return(FAIL);
		}/* End IF */
	    }/* End IF */
/*
 * Arrange for it to be deleted if this gets undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)qbp;
	    ut->iarg1 = 0;
	    ut->iarg2 = pbp->zee;
/*
 * Transfer the contents of the pushed register into the actual Q register
 */
	    buff_bulk_insert(qbp,0,pbp->zee,pbp->first_line);
	    pbp->first_line = NULL;
	    qbp->pos_cache.lbp = NULL;
	    qbp->pos_cache.base = 0;

/*
 * Arrange that the push be re-done if the pop is undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_PUSH;
	    ut->iarg1 = ct->q_register;
/*
 * Place it on the main buffer list so that buff_destroy can find it
 */
	    qregister_push_down_list = pbp->next_header;
	    pbp->next_header = buffer_headers;
	    buffer_headers = pbp;
	    buff_destroy(pbp);
/*
 * If the q-register specified is '*', then the actual result is to rename
 * the current buffer.
 */
	    if(ct->q_register == '*'){
		rename_edit_buffer(curbuf,qbp->name,uct);
	    }/* End IF */

	    return(SUCCESS);
	    }/* End Local Block */

/*
 * Here to return the current position of 'dot'
 */
	case EXEC_C_DOTVALUE:
	    ct->ctx.tmpval = curbuf->dot;
	    return(SUCCESS);
/*
 * Here to return the number of bytes in the edit buffer
 */
	case EXEC_C_ZEEVALUE:
	    ct->ctx.tmpval = curbuf->zee;
	    return(SUCCESS);
/*
 * Here to return the value 0,Z which H is shorthand for
 */
	case EXEC_C_HVALUE:
	    ct->ctx.iarg1 = 0;
	    ct->ctx.iarg2 = curbuf->zee;
	    return(SUCCESS);
/*
 * Print out the numeric value of an expression
 */
	case EXEC_C_EQUALS:
	    if(ct->ctx.iarg1_flag == NO){
		error_message("?Argument required for = command");
		return(FAIL);
	    }/* End IF */

	    sprintf(tmp_buffer,"value = %d",ct->ctx.iarg1);
	    screen_message(tmp_buffer);
	    return(SUCCESS);

	case EXEC_C_TWO_EQUALS:
	    sprintf(tmp_buffer,"value = 0%o",ct->ctx.iarg1);
	    screen_message(tmp_buffer);
	    return(SUCCESS);

	case EXEC_C_THREE_EQUALS:
	    sprintf(tmp_buffer,"value = 0x%X",ct->ctx.iarg1);
	    screen_message(tmp_buffer);
	    return(SUCCESS);
/*
 * Here to insert the value of the expression into the edit buffer as a
 * decimal representation.
 */
	case EXEC_C_BACKSLASH:
	    {/* Local Block */
	    register int i,j;
	    char *cp;
	    int length = 0;

	    if(ct->ctx.iarg2 < 2 || ct->ctx.iarg2 > 36) ct->ctx.iarg2 = 10;
	    j = ct->ctx.iarg1;
	    cp = tmp_buffer + sizeof(tmp_buffer);
	    do {
		i = j % ct->ctx.iarg2;
		j = (j - i) / ct->ctx.iarg2;
		if(i > 9) i += ('A' - 10);
		else i += '0';
		*(--cp) = i;
		length += 1;
	    } while(j);

	    buff_insert_with_undo(uct,curbuf,curbuf->dot,cp,length);
	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to interpret the ascii digits following 'dot' in the edit buffer
 * as a decimal number, and return this number as the value of this expression,
 * leaving 'dot' positioned after the digits.
 */
	case EXEC_C_BACKSLASHARG:
	    {/* Local Block */
	    register int i;
	    register int radix;
	    
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    radix = ct->ctx.tmpval;
	    ct->ctx.tmpval = 0;

	    for(;;){
		i = buff_contents(curbuf,curbuf->dot);

		if(i >= '0' && i <= '9') i -= '0';
		else if(i >= 'a' && i <= 'z') i -= ('a' - 10);
		else if(i >= 'A' && i <= 'Z') i -= ('A' - 10);
		else break;

		if(i < 0 || i >= radix) break;

		ct->ctx.tmpval = ct->ctx.tmpval * radix + i;
		if(curbuf->dot == curbuf->zee) break;
		curbuf->dot++;
	    }/* End FOR */

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * The poor bastard wants to leave...
 */
	case EXEC_C_EXITCOMMAND:
	    {/* Local Block */
	    register struct buff_header *hbp;

	    if(ct->ctx.iarg1_flag == NO || ct->ctx.iarg1 != -1){
		for(hbp = buffer_headers; hbp != NULL; hbp = hbp->next_header){
		    if(hbp->buffer_number <= 0) continue;
		    if(hbp->ismodified != 0 && hbp->isreadonly == 0){
			error_message("?Modified buffers exist");
			return(FAIL);
		    }/* End IF */
		}/* End FOR */
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_SET_EXIT_FLAG;

	    exit_flag = YES;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to jump to an absolute position in the buffer
 */
	case EXEC_C_JUMP:
	    if(ct->ctx.iarg1_flag == YES && ct->ctx.iarg1 < 0){
		error_message("?Negative Argument to J");
		return(FAIL);
	    }/* End IF */

	    if(ct->ctx.iarg1_flag == NO) ct->ctx.iarg1 = 0;

	    if(parse_illegal_buffer_position(ct->ctx.iarg1,0,"J")){
		return(FAIL);
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    if(ct->ctx.iarg1_flag == NO) curbuf->dot = 0;
	    else curbuf->dot = ct->ctx.iarg1;
	    return(SUCCESS);
/*
 * Here to move dot by a relative number of buffer positions
 */
	case EXEC_C_CHAR:
	    {/* Local Block */
	    register int i;

	    i = curbuf->dot + 1;

	    if(ct->ctx.iarg1_flag == YES){
		i = curbuf->dot + ct->ctx.iarg1;
	    }/* End IF */

	    if(parse_illegal_buffer_position(i,0,"C")) return(FAIL);

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    curbuf->dot = i;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to move dot by a relative number of buffer positions, just like
 * the "C" command, except that the direction is by default backwards.
 */
	case EXEC_C_RCHAR:
	    {/* Local Block */
	    register int i;

	    i = curbuf->dot - 1;
	    if(ct->ctx.iarg1_flag == YES){
		i = curbuf->dot - ct->ctx.iarg1;
	    }/* End IF */

	    if(parse_illegal_buffer_position(i,0,"R")) return(FAIL);

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    curbuf->dot = i;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to delete the specified number of characters following 'dot'.
 * A negative value indicates we should delete characters BEFORE 'dot'.
 */
	case EXEC_C_DELETE:
	    {/* Local Block */
	    register int i;

	    i = 1;
	    if(ct->ctx.iarg1_flag == YES) i = ct->ctx.iarg1;

	    if(parse_illegal_buffer_position(curbuf->dot+i,0,"D")){
		return(FAIL);
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    if(i < 0){
		i = 0 - i;
		curbuf->dot -= i;
	    }/* End IF */

/*
 * Call the standard routine which deletes the specified buffer positions
 * and constructs the undo list so that it can be rebuilt if necessary.
 */
	    if(i) buff_delete_with_undo(uct,curbuf,curbuf->dot,i);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to move 'dot' by the specified number of words. Note that words
 * can have a pretty nebulous definition, depending on what you are doing.
 * This should probably have a q-register which contains all of the delimeters.
 */
	case EXEC_C_WORD:
	    {/* Local Block */
	    register int count;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    count = ct->ctx.iarg1;
	    if(ct->ctx.iarg1_flag == NO) count = 1;

	    cmd_wordmove(count);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to delete the specified number of words. Note that words
 * can have a pretty nebulous definition, depending on what you are doing.
 * This should probably have a q-register which contains all of the delimeters.
 */
	case EXEC_C_DELWORD:
	    {/* Local Block */
	    register int count;
	    int original_position;
	    int pos;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = original_position = curbuf->dot;

	    count = ct->ctx.iarg1;
	    if(ct->ctx.iarg1_flag == NO) count = 1;

	    cmd_wordmove(count);

	    pos = original_position;
	    count = curbuf->dot - original_position;
	    if(count < 0){
		count = 0 - count;
		pos = curbuf->dot;
	    }/* End IF */

	    if(count) buff_delete_with_undo(uct,curbuf,pos,count);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to delete backward the specified number of words. Note that
 * words can have a pretty nebulous definition, depending on what you are
 * doing. This should probably have a q-register which contains all of the
 * delimeters.
 */
	case EXEC_C_RDELWORD:
	    {/* Local Block */
	    register int count;
	    int original_position;
	    int pos;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = original_position = curbuf->dot;

	    count = 0 - ct->ctx.iarg1;
	    if(ct->ctx.iarg1_flag == NO) count = -1;

	    cmd_wordmove(count);

	    pos = original_position;
	    count = curbuf->dot - original_position;
	    if(count < 0){
		count = 0 - count;
		pos = curbuf->dot;
	    }/* End IF */

	    if(count) buff_delete_with_undo(uct,curbuf,pos,count);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to move the 'dot' by the specified number of lines.
 */
	case EXEC_C_LINE:
	    {/* Local Block */
	    register struct buff_header *hbp;
	    register struct buff_line *lbp;
	    register int i,j;

	    hbp = curbuf;
	    lbp = buff_find_line(hbp,hbp->dot);
/*
 * Start by pretending it was a 0L command, and get to the begining of
 * the line. That allows us to not worry about offset onto current line.
 */
	    j = 0 - buff_find_offset(hbp,lbp,hbp->dot);
	    i = ct->ctx.iarg1;
	    if(ct->ctx.iarg1_flag == NO) i = 1;
/*
 * If argument is positive, movement is forward
 */
	    if(i > 0){
		while(i-- && lbp){
		    j += lbp->byte_count;
		    lbp = lbp->next_line;
		}/* End WHILE */
		if(lbp == NULL){
		    error_message("?Attempt to Move Pointer Off Page with L");
		    return(FAIL);
		}/* End IF */
	    }/* End IF */

	    else {
		while(i++ && lbp){
		    lbp = lbp->prev_line;
		    if(lbp == NULL){
			error_message("?Attempt to Move Pointer Off Page with L");
			return(FAIL);
		    }/* End IF */
		    j -= lbp->byte_count;
		}/* End WHILE */
	    }/* End Else */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = hbp->dot;

	    hbp->dot += j;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to move the 'dot' backward by the specified number of lines.
 */
	case EXEC_C_RLINE:
	    {/* Local Block */
	    register struct buff_header *hbp;
	    register struct buff_line *lbp;
	    register int i,j;

	    hbp = curbuf;
	    lbp = buff_find_line(hbp,hbp->dot);
/*
 * Start by pretending it was a 0L command, and get to the begining of
 * the line. That allows us to not worry about offset onto current line.
 */
	    j = 0 - buff_find_offset(hbp,lbp,hbp->dot);
	    i = 0 - ct->ctx.iarg1;
	    if(ct->ctx.iarg1_flag == NO) i = -1;
/*
 * If argument is positive, movement is forward
 */
	    if(i > 0){
		while(i-- && lbp){
		    j += lbp->byte_count;
		    lbp = lbp->next_line;
		}/* End WHILE */
		if(lbp == NULL){
		    error_message("?Attempt to Move Pointer Off Page with B");
		    return(FAIL);
		}/* End IF */
	    }/* End IF */

	    else {
		while(i++ && lbp){
		    lbp = lbp->prev_line;
		    if(lbp == NULL){
			error_message("?Attempt to Move Pointer Off Page with B");
			return(FAIL);
		    }/* End IF */
		    j -= lbp->byte_count;
		}/* End WHILE */
	    }/* End Else */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = hbp->dot;

	    hbp->dot += j;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to kill the specified number of lines.
 */
	case EXEC_C_KILL:
	    {/* Local Block */
	    register struct buff_header *hbp;
	    register struct buff_line *lbp;
	    register int i,j;
	    int pos1,pos2;

	    hbp = curbuf;
/*
 * If two arguments were specified, then he has specified an a,b range to
 * be killed.
 */
	    if(ct->ctx.iarg1_flag == YES && ct->ctx.iarg2_flag == YES){
		pos1 = ct->ctx.iarg1;
		pos2 = ct->ctx.iarg2;
		if(parse_illegal_buffer_position(pos1,pos2,"K")){
		    return(FAIL);
		}/* End IF */
		if(pos2 < pos1){
		    pos2 = ct->ctx.iarg1;
		    pos1 = ct->ctx.iarg2;
		}/* End IF */
	    }/* End IF */
/*
 * Else, if it was a single command, it is a relative kill which affects a
 * portion of the buffer in a similar way as if it was an L command. Our job
 * now is to turn it into a a,b range.
 */
	    else {
		lbp = buff_find_line(hbp,hbp->dot);
/*
 * Start by pretending it was a 0L command, and get to the begining of
 * the line. That allows us to not worry about offset onto current line.
 */
		j = 0 - buff_find_offset(hbp,lbp,hbp->dot);
		i = ct->ctx.iarg1;
		if(ct->ctx.iarg1_flag == NO) i = 1;
/*
 * If argument is positive, movement is forward
 */
		if(i > 0){
		    while(i-- && lbp){
			j += lbp->byte_count;
			lbp = lbp->next_line;
		    }/* End WHILE */
		    if(lbp == NULL){
			error_message("?Attempt to Move Pointer Off Page with K");
			return(FAIL);
		    }/* End IF */
		    pos1 = hbp->dot;
		    pos2 = hbp->dot + j;
		}/* End IF */

		else {

		    while(i++ && lbp){
			lbp = lbp->prev_line;
			if(lbp == NULL){
			    error_message("?Attempt to Move Pointer Off Page with K");
			    return(FAIL);
			}/* End IF */
			j -= lbp->byte_count;
		    }/* End WHILE */
		    pos1 = hbp->dot + j;
		    pos2 = hbp->dot;
		}/* End Else */
	    }/* End Else */

	    j = pos2 - pos1;
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = hbp->dot;

/*
 * Call the standard routine which deletes the specified buffer positions
 * and constructs the undo list so that it can be rebuilt if necessary.
 */
	    if(j) buff_delete_with_undo(uct,hbp,pos1,j);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to insert a character as part of an insert command
 */
	case EXEC_C_INSERT:
	    buff_insert_char_with_undo(
		uct,
		curbuf,
		curbuf->dot,
		ct->ctx.iarg1_flag ? ct->ctx.iarg1 : ct->input_byte
	    );
	    return(SUCCESS);
/*
 * This state is used when a subroutine state is trying to return a
 * value to the calling state. It gets specially noticed by the parser,
 * so it really doesn't do much except act as a flag.
 */
	case EXEC_C_STOREVAL:
	    return(SUCCESS);
/*
 * Store the current expression value as the first argument to a command
 */
	case EXEC_C_STORE1:
	    ct->ctx.iarg1 = ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Store the current expression value as the second argument to a command
 */
	case EXEC_C_STORE2:
	    ct->ctx.iarg2 = ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here when we have a leading minus sign in an expression such as -3+4
 */
	case EXEC_C_UMINUS:
	    ct->ctx.tmpval = 0 - ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here to add the two parts of the expression to produce a new temporary
 * value.
 */
	case EXEC_C_PLUS:
	    ct->ctx.tmpval = ct->ctx.iarg2 + ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here to subtract the two parts of the expression to produce a new temporary
 * value.
 */
	case EXEC_C_MINUS:
	    ct->ctx.tmpval = ct->ctx.iarg2 - ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here to multiply the two parts of the expression to produce a new temporary
 * value.
 */
	case EXEC_C_TIMES:
	    ct->ctx.tmpval = ct->ctx.iarg1 * ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here to divide the two parts of the expression to produce a new temporary
 * value.
 */
	case EXEC_C_DIVIDE:
	    if(ct->ctx.tmpval == 0){
		error_message("?Attempt to Divide by Zero");
		return(FAIL);
	    }/* End IF */
	    ct->ctx.tmpval = ct->ctx.iarg1 / ct->ctx.tmpval;
	    return(SUCCESS);
/*
 * Here in response to the "A" command which returns as a value the ASCII
 * value of the character at 'dot' + offset.
 */
	case EXEC_C_ACOMMAND:
	    {/* Local Block */
	    register int i;

	    i = curbuf->dot + ct->ctx.tmpval;

	    if(parse_illegal_buffer_position(i,i+1,"A")){
		return(FAIL);
	    }/* End IF */

	    ct->ctx.tmpval = buff_contents(curbuf,i);

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to cause a repaint of the screen incase it got messed up by
 * something
 */
	case EXEC_C_REDRAW_SCREEN:
	    screen_redraw();
	    return(SUCCESS);
/*
 * Here to search for a given string. This gets used not only in the
 * "S" command, but as a substate in any command which requires searching
 * for a string (such as FD, FK, FS, FR, etc.)
 */
	case EXEC_C_SEARCH:
	    {/* Local Block */
	    int arg1,arg2;
#if 0
	    struct buff_header *qbp;
#endif

	    if(set_search_string_with_undo(ct->ctx.carg,uct) == FAIL){
		return(FAIL);
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

	    arg1 = 1;
	    arg2 = -1;
	    if(ct->ctx.iarg1_flag) arg1 = ct->ctx.iarg1;
	    if(ct->ctx.iarg2_flag) arg2 = ct->ctx.iarg2;

/*
 * If there are two arguments, then it is a constrained search and we must
 * insure that the two positions specified are valid buffer positions.
 */
	    if(ct->ctx.iarg1_flag && ct->ctx.iarg2_flag){
		if(arg1 < 0 || arg1 > curbuf->zee){
		    sprintf(tmp_message,
			"?Illegal buffer position %d in search command",arg1);
		    error_message(tmp_message);
		    return(FAIL);
		}/* End IF */
		if(arg2 < 0 || arg2 > curbuf->zee){
		    sprintf(tmp_message,
			"?Illegal buffer position %d in search command",arg2);
		    error_message(tmp_message);
		    return(FAIL);
		}/* End IF */
	    }/* End IF */

/*
 * That done, we call the search command to perform the search
 */
	    if(cmd_search(arg1,arg2,&search_string) == FAIL){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */

		if(!ct->ctx.inest && search_string.error_message_given == NO){
		    sprintf(tmp_message,
			"?Cannot find '%.40s'",search_string.input);
		    error_message(tmp_message);
		}/* End IF */

		return(SUCCESS);
	    }/* End IF */

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = -1;
	    }/* End IF */

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to implement the FR command. The FR command is similar to the FS
 * command (Find-Substitute), except that if the second string argument is
 * null, rather than simply deleting the first string argument, it is replaced
 * by the default replace string.
 */
	case EXEC_C_FRREPLACE:
	    {/* Local Block */
	    register struct buff_header *qbp;
	    register int j;

	    if(last_search_status == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		if(ct->ctx.inest != 0) return(SUCCESS);
		return(FAIL);
	    }/* End IF */

	    j = last_search_pos2 - last_search_pos1;
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

/*
 * Call the standard routine which deletes the specified buffer positions
 * and constructs the undo list so that it can be rebuilt if necessary.
 */
	    if(j) buff_delete_with_undo(uct,curbuf,last_search_pos1,j);

	    qbp = buff_qfind('-',1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */

	    buff_insert_from_buffer_with_undo(
		uct,
		curbuf,			/* destination is current edit buf */
		last_search_pos1,	/* at the search position          */
		qbp,			/* source is replacement q-register*/
		0,			/* from position 0 to the end      */
		qbp->zee
	    );

	    if(ct->ctx.iarg1_flag == YES){
		if( ct->ctx.iarg1 < 0){
		    curbuf->dot = last_search_pos1;
		}/* End IF */
		else if(ct->ctx.iarg2_flag == YES){
		    if(ct->ctx.iarg2 < ct->ctx.iarg1){
			curbuf->dot = last_search_pos1;
		    }/* End IF */
		}/* End Else */
	    }/* End IF */

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = -1;
	    }/* End IF */

	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to implement the FS (Find-Substitute) command which searches for
 * the first string argument and replaces it with the second.
 */
	case EXEC_C_FSREPLACE1:
	    {/* Local Block */
	    register int j;
	    register struct buff_header *qbp;

	    ct->ctx.tmpval = 0;

	    if(last_search_status == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		if(ct->ctx.inest != 0) return(SUCCESS);
		return(FAIL);
	    }/* End IF */

#ifdef INTERACTIVE_FS
	    j = last_search_pos2 - last_search_pos1;
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;
	    if(j) buff_delete_with_undo(uct,curbuf,last_search_pos1,j);
#endif /* INTERACTIVE_FS */

	    return(SUCCESS);

	case EXEC_C_FSREPLACE2:

	    if(last_search_status == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		if(ct->ctx.inest != 0) return(SUCCESS);
		return(FAIL);
	    }/* End IF */

	    qbp = buff_qfind('-',1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */

#ifdef INTERACTIVE_FS

	    buff_insert_char_with_undo(uct,curbuf,curbuf->dot,ct->input_byte);

#endif /* INTERACTIVE_FS */

/*
 * What this does is hope that the replacement string is the same as the
 * previous one. Only if we get a miscompare do we delete the contents of
 * the replacement q-register, and insert the new string.
 */
	    if(qbp->zee > ct->ctx.tmpval){
		if(ct->input_byte == buff_contents(qbp,ct->ctx.tmpval)){
		    ct->ctx.tmpval += 1;
		    return(SUCCESS);
		}

		buff_delete_with_undo(uct,qbp,ct->ctx.tmpval,
		    qbp->zee-ct->ctx.tmpval);

	    }

	    buff_insert_char_with_undo(uct,qbp,ct->ctx.tmpval,ct->input_byte);

	    ct->ctx.tmpval += 1;
	    return(SUCCESS);

	case EXEC_C_FSREPLACE3:

	    qbp = buff_qfind('-',1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */

	    if(qbp->zee > ct->ctx.tmpval){
		buff_delete_with_undo(uct,qbp,ct->ctx.tmpval,
		    qbp->zee-ct->ctx.tmpval);
	    }

#ifndef INTERACTIVE_FS
	    j = last_search_pos2 - last_search_pos1;
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;
	    if(j) buff_delete_with_undo(uct,curbuf,last_search_pos1,j);

	    buff_insert_from_buffer_with_undo(uct,
		curbuf,
		curbuf->dot,
		qbp,
		0,
		qbp->zee
	    );

#endif /* !INTERACTIVE_FS */

	    if(ct->ctx.iarg1_flag == YES){
		if( ct->ctx.iarg1 < 0){
		    ut = allocate_undo_token(uct);
		    if(ut == NULL) return(FAIL);
		    ut->opcode = UNDO_C_CHANGEDOT;
		    ut->iarg1 = curbuf->dot;
		    curbuf->dot = last_search_pos1;
		}/* End IF */
		else if(ct->ctx.iarg2_flag == YES){
		    if(ct->ctx.iarg2 < ct->ctx.iarg1){
			ut = allocate_undo_token(uct);
			if(ut == NULL) return(FAIL);
			ut->opcode = UNDO_C_CHANGEDOT;
			ut->iarg1 = curbuf->dot;
			curbuf->dot = last_search_pos1;
		    }/* End IF */
		}/* End Else */
	    }/* End IF */

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = last_search_status;
	    }/* End IF */

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to execute the FT (tags) command. Since this is a fairly complicated
 * command, we call a subroutine to perform the actions.
 */
	case EXEC_C_FTAGS:
	    {/* Local Block */
	    register int arg_count = 0;
	    register int status;

	    if(ct->ctx.iarg1_flag == YES) arg_count = 1;
	    if(ct->ctx.iarg2_flag == YES) arg_count = 2;

	    status = cmd_tags(
		uct,
		arg_count,
		ct->ctx.iarg1,
		ct->ctx.iarg2,
		ct->ctx.carg
	    );

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = status;
		return(SUCCESS);
	    }/* End IF */

	    return(status);

	    }/* End Local Block */

/*
 * Here to execute the FD command which searches for a string and deletes
 * it. It is like FS but saves you from typing the second escape.
 */
	case EXEC_C_FDCOMMAND:
	    {/* Local Block */
	    register int j;

	    if(last_search_status == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		if(ct->ctx.inest != 0) return(SUCCESS);
		return(FAIL);
	    }/* End IF */

	    j = last_search_pos2 - last_search_pos1;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

/*
 * Call the standard routine which deletes the specified buffer positions
 * and constructs the undo list so that it can be rebuilt if necessary.
 */
	    if(j) buff_delete_with_undo(uct,curbuf,last_search_pos1,j);

	    curbuf->dot = last_search_pos1;

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = -1;
	    }/* End IF */

	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to implement the FK command. The FK command remembers the current
 * position in the buffer and searches for the specified string. It then
 * deletes the characters from the old current position up to but not
 * including the search string.
 */
	case EXEC_C_FKCOMMAND:
	    {/* Local Block */
	    register int j;

	    if(last_search_status == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		if(ct->ctx.inest != 0) return(SUCCESS);
		return(FAIL);
	    }/* End IF */

	    if(remembered_dot <= last_search_pos1){
		last_search_pos2 = last_search_pos1;
		last_search_pos1 = remembered_dot;
	    }/* End IF */

	    else {
		last_search_pos1 = last_search_pos2;
		last_search_pos2 = remembered_dot;
		curbuf->dot = last_search_pos1;
	    }/* End IF */

	    j = last_search_pos2 - last_search_pos1;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = curbuf->dot;

/*
 * Call the standard routine which deletes the specified buffer positions
 * and constructs the undo list so that it can be rebuilt if necessary.
 */
	    if(j) buff_delete_with_undo(uct,curbuf,last_search_pos1,j);

	    curbuf->dot = last_search_pos1;

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = -1;
	    }/* End IF */

	    return(SUCCESS);
	    }/* End Local Block */
/*
 * This state is used by the FK command to remember the current dot
 * position before it is changed by the search command.
 */
	case EXEC_C_REMEMBER_DOT:
	    remembered_dot = curbuf->dot;
	    return(SUCCESS);
/*
 * Here to implement the N search which searches across edit buffers for
 * the given string.
 */
	case EXEC_C_NSEARCH:
	    {/* Local Block */
	    int count;
	    struct buff_header *original_buffer;
	    int original_dot;
	    int saved_dot,saved_top,saved_bottom;
	    int first_buffer;
	    int last_buffer = 0;
	    char back_in_original_buffer;
/*
 * We set the search string, just like all the other search commands
 */
	    if(set_search_string_with_undo(ct->ctx.carg,uct) == FAIL){
		return(FAIL);
	    }/* End IF */
/*
 * Remember the buffer that we started in, and be ready to change back to
 * it if we undo.
 */
	    original_buffer = curbuf;
	    original_dot = curbuf->dot;

	    count = 1;
	    if(ct->ctx.iarg1_flag) count = ct->ctx.iarg1;

/*
 * If he says 1,5n<string>$ then we search for the string in buffers 1
 * through 5. We check that both buffers exist before we begin the search.
 */
	    if(ct->ctx.iarg2_flag){
		count = 1;
		first_buffer = ct->ctx.iarg1;
		last_buffer = ct->ctx.iarg2;

		if(first_buffer != curbuf->buffer_number){
		    if(buff_openbuffer(NULL,last_buffer,0) != SUCCESS){
			return(FAIL);
		    }/* End IF */
		    if(buff_openbuffer(NULL,first_buffer,0) != SUCCESS){
			return(FAIL);
		    }/* End IF */
		    curbuf->dot = 0;
		}/* End IF */
	    }/* End IF */

/*
 * In the starting buffer, we want to search from 'dot' to the end. Then
 * we would search all the other buffers, and then the original one again,
 * but from the top to 'dot'. The saved_dot stuff is necessary because
 * the cmd_search routine has the side effect of changing 'dot' in the
 * buffer being searched. However, we only want to affect 'dot' in the
 * final buffer that the final occurance of the search string is found in.
 */
	    saved_dot = curbuf->dot;
	    saved_top = curbuf->dot;
	    saved_bottom = curbuf->zee;
	    back_in_original_buffer = 0;

	    while(count > 0){

		if(cmd_search(saved_top,saved_bottom,&search_string) != FAIL){
		    saved_top = curbuf->dot;
		    count -= 1;
		    continue;
		}/* End IF */
/*
 * Here if the search failed. We want to switch over to the next buffer and
 * try again, until we exhaust all the buffers. If we reach the final buffer
 * without finding the string, we return an error to the user, leaving him
 * back in his original buffer.
 */
		if(ct->ctx.iarg2_flag && curbuf->buffer_number == last_buffer){
		    if(buff_openbuffer(NULL,original_buffer->buffer_number,0) != SUCCESS){
			error_message("Boom! lost original buffer");
			return(FAIL);
		    }/* End IF */
		    back_in_original_buffer = 1;
		}/* End IF */

		if(back_in_original_buffer){
		    curbuf->dot = saved_dot;
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.tmpval = FAIL;
			return(SUCCESS);
		    }/* End IF */
		    sprintf(tmp_message,
			"?Cannot find '%.40s'",search_string.input);
		    if(!ct->ctx.inest && !intr_flag &&
		      search_string.error_message_given == NO){
			error_message(tmp_message);
		    }/* End IF */
		    return(SUCCESS);
		}/* End IF */
/*
 * We havn't tried all the buffers yet, switch to the next one.
 */
		curbuf->dot = saved_dot;

		while(1){
		    curbuf = curbuf->next_header;
		    if(curbuf == NULL) curbuf = buffer_headers;
		    if(curbuf->buffer_number == -1) break;
		    if(curbuf->buffer_number > 0) break;
		    if(ct->ctx.iarg2_flag) continue;
		    if(curbuf == original_buffer) break;
		}/* End While */

		saved_dot = curbuf->dot;
		saved_top = 0;
		saved_bottom = curbuf->zee;

		if(ct->ctx.iarg2_flag == NO){
		    if(curbuf->buffer_number == original_buffer->buffer_number){
			back_in_original_buffer = 1;
			saved_bottom = original_dot;
		    }/* End IF */
		}/* End IF */

	    }/* End While */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = original_buffer->buffer_number;

	    buff_switch(curbuf,1);

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEDOT;
	    ut->iarg1 = saved_dot;

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = -1;
	    }/* End IF */

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to begin an iteration < ... >
 */
	case EXEC_C_ITERATION_BEGIN:
	    {/* Local Block */
	    register struct cmd_token *oct;
	    if(ct->ctx.iarg1_flag == YES){
		if(ct->ctx.iarg1 <= 0){
		    oct = ct;
		    while((oct = oct->next_token)){
			if(oct->opcode != TOK_C_ITERATION_END) continue;
			if(oct->ctx.inest != (ct->ctx.inest-1)) continue;
			jump_to_token = oct;
			return(SUCCESS);
		    }/* End While */
		    ct->ctx.go_flag = NO;
		    return(SUCCESS);
		}/* End IF */
		ct->ctx.iarg1 -= 1;
	    }/* End IF */
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to provide the execution code for the end of an iteration.
 * This must search backwards in the parse tokens to find the begining
 * of the iteration, and set up to jump to that token
 */
	case EXEC_C_ITERATION_END:
	    {/* Local Block */
	    register struct cmd_token *oct;
/*
 * The parser left us a pointer to the begining of the iteration so we can
 * find it without having to search.
 */
	    oct = ct->ctx.caller_token;

	    if(oct->ctx.iarg1_flag == YES){
		if(oct->ctx.iarg1 <= 0){
		    return(SUCCESS);
		}/* End IF */
	    }/* End IF */
/*
 * The following line grabs the old iteration value so that when the
 * begining of iteration code grabs the "previous" iarg1, this will
 * be it. Think of it this way: the code has to do something different
 * the first time into the iteration (it has to load the results of the
 * STORE1, then the subsequent times it has to decrement what's already
 * there. That's why this seems a little kludgy.
 */
	    ct->ctx.iarg1 = oct->ctx.iarg1;
	    jump_to_token = oct;
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to implement the semicolon command which is used to jump out
 * of an iteration if the argument is >= 0.
 */
	case EXEC_C_SEMICOLON:
	{/* Local Block */
	register struct cmd_token *oct;
	int value;

	value = last_search_status;
	if(ct->ctx.iarg1_flag == YES) value = ct->ctx.iarg1;

	if(value < 0) return(SUCCESS);

	oct = ct;
	while((oct = oct->next_token)){
	    if(oct->opcode != TOK_C_ITERATION_END) continue;
	    if(oct->ctx.inest != (ct->ctx.inest-1)) continue;
	    if(oct->next_token){
		jump_to_token = oct->next_token;
		return(SUCCESS);
	    }/* End IF */
	}/* End While */

	ct->ctx.go_flag = NO;
	return(BLOCKED);

	}/* End Local Block */
/*
 * Here to implement the various conditional operators
 */
	case EXEC_C_COND_GT:
	    if(ct->ctx.iarg1 <= 0){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_LT:
	    if(ct->ctx.iarg1 >= 0){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_EQ:
	    if(ct->ctx.iarg1 != 0){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_NE:
	    if(ct->ctx.iarg1 == 0){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_DIGIT:
	    if(!isdigit(ct->ctx.iarg1)){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_ALPHA:
	    if(!isalpha(ct->ctx.iarg1)){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_LOWER:
	    if(!islower(ct->ctx.iarg1)){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_UPPER:
	    if(!isupper(ct->ctx.iarg1)){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_COND_SYMBOL:
	    if(!isalnum(ct->ctx.iarg1) && ct->ctx.iarg1 != '_'){
		return(find_conditional_else(ct));
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_SKIP_ELSE:
	    return(find_conditional_end(ct));
/*
 * Here to implement the GOTO command OLABEL$
 */
	case EXEC_C_GOTO:
	    {/* Local Block */
	    struct cmd_token *goto_ptr;
	    struct cmd_token *test_ptr;
/*
 * First step is to find the begining of this goto string
 */
	    goto_ptr = ct;
	    while((goto_ptr = goto_ptr->prev_token)){
		if(goto_ptr->opcode != TOK_C_GOTO_BEGIN) continue;
		break;
	    }/* End While */
/*
 * Now try to find the label. First search backwards for it
 */
	    test_ptr = goto_ptr;
	    while((test_ptr = test_ptr->prev_token)){
		if(test_ptr->opcode != TOK_C_LABEL_BEGIN) continue;
		if(compare_label(goto_ptr,test_ptr) == YES){
		    ct->ctx.go_flag = NO;
		    jump_to_token = test_ptr;
		    return(SUCCESS);
		}/* End IF */
	    }/* End While */
/*
 * The label is not in front of us, look for it afterwards
 */
	    test_ptr = goto_ptr;
	    while((test_ptr = test_ptr->next_token)){
		if(test_ptr->opcode == TOK_C_FINALTOKEN){
		    char string1[PARSER_STRING_MAX];

		    extract_label(goto_ptr,string1);
		    (void) sprintf(tmp_message,"?Can't find label <%s>",
			string1);
		    error_message(tmp_message);
		    return(FAIL);
		}/* End IF */

		if(test_ptr->opcode != TOK_C_LABEL_BEGIN) continue;

		if(compare_label(goto_ptr,test_ptr) == YES){
		    jump_to_token = test_ptr;
		    return(SUCCESS);
		}/* End IF */
	    }/* End While */

	    ct->ctx.go_flag = NO;
	    return(BLOCKED);

	    }/* End Local Block */
/*
 * Here to write out the buffer. If the filename is specified, write it
 * to that name, otherwise write it to the default name of the buffer.
 */
	case EXEC_C_WRITEFILE:
	    {/* Local Block */
	    int status;
	    struct wildcard_expansion *name_list,*np;
	    char *filename;

	    if(curbuf->isreadonly){
		error_message("?File is Readonly");
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    if(ct->ctx.carg) filename = ct->ctx.carg;
	    else filename = curbuf->name;

	    np = expand_filename(filename);
	    if(np == NULL){
		sprintf(tmp_message,"EW No such file <%s>",filename);
		error_message(tmp_message);
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    if(np->we_next){
		sprintf(tmp_message,"EW: Non-unique filename <%s>",filename);
		error_message(tmp_message);
		while(np){
		    name_list = np;
		    np = np->we_next;
		    tec_release(TYPE_C_WILD,(char *)name_list);
		}/* End While */
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    status = cmd_write(curbuf,np->we_name);
	    tec_release(TYPE_C_WILD,(char *)np);
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = status;
		return(SUCCESS);
	    }/* End IF */
	    return(status);

	    }/* End Local Block */
/*
 * Read in the named file to the current position in the edit buffer
 */
	case EXEC_C_READFILE:
	    {/* Local Block */
	    struct wildcard_expansion *name_list,*np;

	    if(ct->ctx.carg == NULL || ct->ctx.carg[0] == '\0'){
		error_message("?ER Requires a filename");
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)curbuf;
	    ut->iarg1 = curbuf->dot;
	    ut->iarg2 = curbuf->zee;

	    np = expand_filename(ct->ctx.carg);
	    if(np == NULL){
		sprintf(tmp_message,"ER No such file <%s>",ct->ctx.carg);
		error_message(tmp_message);
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		return(FAIL);
	    }/* End IF */

	    while(np){
		buff_read(curbuf,np->we_name);
		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,(char *)name_list);
	    }/* End While */


	    ut->iarg2 = curbuf->zee - ut->iarg2;

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = SUCCESS;
		return(SUCCESS);
	    }/* End IF */
	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to change the current edit buffer. If a numeric argument is
 * supplied, change to that buffer, otherwise a string should have been
 * specified, and this is the name of the file/buffer to be loaded.
 */
	case EXEC_C_EDITBUF:
	    {/* Local Block */
	    struct buff_header *hbp,*old_buffer,*first_buffer;
	    struct wildcard_expansion *name_list,*np;

/*
 * Here if there is no string argument. It better be a numeric buffer
 * number argument. If so, switch to that buffer, and set the undo to
 * switch back to the old one if this gets undone.
 */
	    if(ct->ctx.carg == NULL || ct->ctx.carg[0] == '\0'){
		if(ct->ctx.iarg1_flag == NO){
		    error_message("?EB Requires a filename or argument");
		    return(FAIL);
		}/* End IF */

		old_buffer = curbuf;
		if(buff_openbuffer(0,ct->ctx.iarg1,0) == FAIL){
		    return(FAIL);
		}/* End IF */

		if(curbuf->buffer_number != ct->ctx.iarg1){
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.tmpval = FAIL;
			return(SUCCESS);
		    }
		    return(FAIL);
		}/* End IF */

		ut = allocate_undo_token(uct);
		if(ut == NULL) return(FAIL);
		ut->opcode = UNDO_C_CHANGEBUFF;
		ut->iarg1 = old_buffer->buffer_number;

		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = SUCCESS;
		}
		return(SUCCESS);

	    }/* End IF */

	    old_buffer = curbuf;
	    first_buffer = NULL;

	    np = expand_filename(ct->ctx.carg);
	    if(np == NULL){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}
		sprintf(tmp_message,"EB No such file <%s>",ct->ctx.carg);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */

	    while(np){
/*
 * The following hack detects whether a new buffer was created so that
 * we know whether or not to set up an undo to un-create it.
 */
		hbp = buff_find(np->we_name);
		if(buff_openbuffer(np->we_name,0,0) == SUCCESS){
		    if(hbp == NULL){
			ut = allocate_undo_token(uct);
			if(ut == NULL) return(FAIL);
			ut->opcode = UNDO_C_CLOSEBUFF;
			ut->carg1 = (char *)curbuf;
		    }/* End IF */
		    if(first_buffer == NULL) first_buffer = curbuf;
		}/* End IF */

		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,(char *)name_list);
	    }/* End While */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = old_buffer->buffer_number;

	    buff_openbuffer(0,first_buffer->buffer_number,0);
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = SUCCESS;
		return(SUCCESS);
	    }
	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to implement the EF command which removes an edit buffer.
 */
	case EXEC_C_CLOSEBUF:
	    {/* Local Block */
	    register struct buff_header *hbp,*bp;
	    register int i;

	    hbp = curbuf;
	    if(curbuf->buffer_number == 0){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}/* End IF */
		error_message("?EF Not allowed on special buffers");
		return(FAIL);
	    }/* End IF */

	    if(hbp->ismodified && !hbp->isreadonly){
		if(ct->ctx.iarg1_flag == NO ||
		   ct->ctx.iarg1 != -1){
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.tmpval = FAIL;
			return(SUCCESS);
		    }/* End IF */
		    sprintf(tmp_message,"?EF Buffer %s is modified",
			curbuf->name);
		    error_message(tmp_message);
		    return(FAIL);
		}/* End IF */
	    }/* End IF */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = hbp->buffer_number;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_REOPENBUFF;
	    ut->iarg1 = hbp->buffer_number;
	    ut->carg1 = (char *)hbp;

/*
 * Unlink this buffer header from the header list. Check to see if it is
 * at the head of the list. If not at the head of the list, we have to
 * search the list till we find it's father. Then we make it's father's
 * child be it's child.
 */
	    i = 0;
	    bp = buffer_headers;
	    if(bp == hbp) buffer_headers = bp->next_header;
	    else {
		while(bp){
		    if(bp->next_header == hbp) break;
		    bp = bp->next_header;
		}/* End While */
		bp->next_header =  hbp->next_header;
		curbuf = NULL;
		if(bp->buffer_number > 0 && hbp->buffer_number > 0){
		    i = bp->buffer_number;
		}/* End IF */
		else if(bp->buffer_number < 0 && hbp->buffer_number < 0){
		    i = bp->buffer_number;
		}/* End Else */
		else if(hbp->buffer_number > 0 && hbp->next_header){
		    i = hbp->next_header->buffer_number;
		}/* End Else */
		else i = -1;
	    }/* End Else */
/*
 * Pick a different buffer to be the current buffer
 */
	    if(i){
		if(buff_openbuffer(NULL,i,0) == FAIL) i = 0;
	    }/* End IF */
	    if(!i){
		if(buff_openbuffer(NULL,-1,0) == FAIL){
		    buff_openbuffer(NULL,0,0);
		}/* End IF */
	    }/* End IF */

	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = SUCCESS;
	    }/* End IF */
	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to implement the EV command which is identical to the EB command
 * except that if the buffer does not exist, when it is created it is created
 * as a read only buffer.
 */
	case EXEC_C_VIEWBUF:
	    {/* Local Block */
	    struct buff_header *hbp,*old_buffer,*first_buffer;
	    struct wildcard_expansion *name_list,*np;

/*
 * Here if there is no string argument. It better be a numeric buffer
 * number argument. If so, switch to that buffer, and set the undo to
 * switch back to the old one if this gets undone.
 */
	    if(ct->ctx.carg == NULL || ct->ctx.carg[0] == '\0'){
		if(ct->ctx.iarg1_flag == NO){
		    error_message("?EV Requires a filename or argument");
		    return(FAIL);
		}/* End IF */

		old_buffer = curbuf;
		buff_openbuffer(0,ct->ctx.iarg1,1);

		ut = allocate_undo_token(uct);
		if(ut == NULL) return(FAIL);
		ut->opcode = UNDO_C_CHANGEBUFF;
		ut->iarg1 = old_buffer->buffer_number;

		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = SUCCESS;
		}
		return(SUCCESS);

	    }/* End IF */

	    old_buffer = curbuf;
	    first_buffer = NULL;

	    np = expand_filename(ct->ctx.carg);
	    if(np == NULL){
		if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		    ct->ctx.tmpval = FAIL;
		    return(SUCCESS);
		}
		sprintf(tmp_message,"EV No such file <%s>",ct->ctx.carg);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */

	    while(np){
/*
 * The following hack detects whether a new buffer was created so that
 * we know whether or not to set up an undo to un-create it.
 */
		hbp = buff_find(np->we_name);
		if(buff_openbuffer(np->we_name,0,1) == SUCCESS){
		    if(hbp == NULL){
			ut = allocate_undo_token(uct);
			if(ut == NULL) return(FAIL);
			ut->opcode = UNDO_C_CLOSEBUFF;
			ut->carg1 = (char *)curbuf;
		    }/* End IF */
		    if(first_buffer == NULL) first_buffer = curbuf;
		}/* End IF */

		name_list = np;
		np = np->we_next;
		tec_release(TYPE_C_WILD,(char *)name_list);
	    }/* End While */

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = old_buffer->buffer_number;

	    buff_openbuffer(0,first_buffer->buffer_number,1);
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		ct->ctx.tmpval = SUCCESS;
		return(SUCCESS);
	    }
	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to scroll the screen the specified number of lines
 */
	case EXEC_C_SCROLL:
	    if(ct->ctx.iarg1_flag == NO) ct->ctx.iarg1 = 1;
	    screen_scroll(ct->ctx.iarg1);
	    return(SUCCESS);
/*
 * Here to cause the screen to update
 */
	case EXEC_C_UPDATE_SCREEN:
	    screen_format_windows();
	    screen_refresh();
	    return(SUCCESS);
/*
 * Here to either split or combine windows
 */
	case EXEC_C_WINDOW_CONTROL:
	    {/* Local Block */
	    struct window *old_wptr;
	    struct window *new_wptr;

	    if(ct->ctx.iarg1_flag == NO) screen_delete_window(curwin);

	    if(ct->ctx.iarg1_flag == YES && ct->ctx.iarg2_flag == YES){
		old_wptr = curwin;
		ut = allocate_undo_token(uct);
		if(ut == NULL) return(FAIL);

		new_wptr =
		    screen_split_window(
			curwin,
			ct->ctx.iarg1,
			ct->ctx.iarg2
		    );

		if(new_wptr == NULL) return(FAIL);

		ut->opcode = UNDO_C_WINDOW_SPLIT;
		ut->iarg1 = old_wptr->win_window_number;
		ut->carg1 = (char *)new_wptr;
	    }/* End Else */

	    return(SUCCESS);
	    }/* End Local Block */
/*
 * Here to select the next window as active
 */
	case EXEC_C_NEXT_WINDOW:
	    {/* Local Block */
	    int status;
	    int new_window;

	    new_window = ct->ctx.iarg1_flag == YES ? ct->ctx.iarg1 : 0;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_WINDOW_SWITCH;
	    ut->iarg1 = curwin->win_window_number;

	    status = window_switch(new_window);

	    return(status);

	    }/* End Local Block */
/*
 * Here to reset a ^A message to NULL
 */
	case EXEC_C_RESET_MESSAGE:
	    user_message[0] = '\0';
	    return(SUCCESS);
/*
 * Here to add a byte to the length of the current user message string
 */
	case EXEC_C_MESSAGE:
	    {/* Local Block */
	    register int i;

	    if(strlen(user_message) > (sizeof(user_message) - 1)){
		error_message("?^A message length exceeded");
		return(FAIL);
	    }/* End IF */

	    i = strlen(user_message);
	    user_message[i] = ct->input_byte;
	    user_message[i+1] = '\0';

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_SHORTEN_MESSAGE;

	    return(SUCCESS);

	    }/* End Local Block */
/*
 * Here to print the current user message to the status line of the screen
 */
	case EXEC_C_OUTPUT_MESSAGE:
	    screen_message(user_message);
	    screen_refresh();
	    return(SUCCESS);
/*
 * Here to execute the specified operating system command, and insert the
 * generated output into the edit buffer.
 */
	case EXEC_C_ECCOMMAND:
	    {/* Local Block */
	    int status;

	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)curbuf;
	    ut->iarg1 = curbuf->dot;
	    ut->iarg2 = 0;

	    status = cmd_oscmd(ct);
	    ut->iarg2 = curbuf->dot - ut->iarg1;
	    return(status);

	    }/* End Local Block */

	case EXEC_C_SET_IMMEDIATE_MODE:
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_SET_IMMEDIATE_MODE;
	    ut->iarg1 = immediate_execute_flag;

	    immediate_execute_flag = YES;
	    if(ct->ctx.iarg1 == 0){
		ct->ctx.go_flag = NO;
		immediate_execute_flag = NO;
	    }/* End IF */
	    return(SUCCESS);

	case EXEC_C_OPENBRACE:
	    {/* Local Block */
	    register struct buff_header *qbp;

/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, create it.
 */
	    qbp = buff_qfind('$',1);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Set up the undo token to remember the current edit buffer, and then switch
 * to the Q-register.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_CHANGEBUFF;
	    ut->iarg1 = curbuf->buffer_number;

	    buff_switch(qbp,1);
/*
 * Delete the previous contents of the q-register, but allow it to be
 * reconstructed if this gets undone. Note that we don't have to do
 * this if there was nothing there.
 */
	    if(qbp->zee) buff_delete_with_undo(uct,qbp,0,qbp->zee);
/*
 * Arrange for it to be deleted if this gets undone.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
/*
 * Copy the parser command line into the Q-register
 */
	    parser_dump_command_line(qbp);

	    ut->opcode = UNDO_C_DELETE;
	    ut->carg1 = (char *)qbp;
	    ut->iarg1 = 0;
	    ut->iarg2 = qbp->zee;

	    return(SUCCESS);
	    }/* End Local Block */

	case EXEC_C_CLOSEBRACE:
	    {/* Local Block */
	    register struct buff_header *qbp;

/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, that is an error
 */
	    qbp = buff_qfind('$',0);
	    if(qbp == NULL){
		return(FAIL);
	    }/* End IF */
/*
 * Replace the command line with the contents of the Q-register
 */
	    parser_replace_command_line(qbp);

	    return(INVALIDATE);

	    }/* End Local Block */

/*
 * Here to skip a label (this occurs when we run into a label)
 */
	case EXEC_C_SKIPLABEL:
	    while(ct){
		if(ct->opcode == TOK_C_LABEL_END){
		    jump_to_token = ct;
		    return(SUCCESS);
		}/* End IF */
		ct = ct->next_token;
	    }/* End While */
	    return(SUCCESS);

/*
 * Here if he wants to set some options up
 */
	case EXEC_C_SETOPTIONS:
	    ut = allocate_undo_token(uct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_SETOPTIONS;

	    return(cmd_setoptions(ct->ctx.iarg1,ct->ctx.iarg2,ut));

	default:
	    sprintf(tmp_message,"?TECEXEC: unknown state %d dispatched",
		ct->execute_state);
	    error_message(tmp_message);
	    return(FAIL);

    }/* End Switch */

}/* End Routine */



/* PUSH_QREGISTER - Push a Q register onto the stack
 *
 * Function:
 *
 *	This routine is called to make a copy of a Q register and place it
 *	on the Q register stack.
 */
int
push_qregister( char letter )
{
register struct buff_header *hbp;
register struct buff_header *qbp;

    PREAMBLE();

/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, create it.
 */
    qbp = buff_qfind(letter,1);
    if(qbp == NULL){
	return(FAIL);
    }/* End IF */

    hbp = buff_duplicate(qbp);

    hbp->next_header = qregister_push_down_list;
    qregister_push_down_list = hbp;

    return(SUCCESS);

}/* End Routine */


/* EXTRACT_LABEL - Build a string with the contents of the label in it
 *
 * Function:
 *
 *	This routine is called to build a string with the label name
 *	in it. The current use is for error messages.
 */
void
extract_label( struct cmd_token *label_ptr, char *string1 )
{
register char *cp1;

    cp1 = string1;
    *cp1 = '\0';
    while(label_ptr){
	if(cp1 >= &string1[PARSER_STRING_MAX-1]) break;
	if(label_ptr->opcode == TOK_C_INPUTCHAR){
	    if(label_ptr->input_byte == '!') break;
	    *cp1++ = label_ptr->input_byte;
	    *cp1 = '\0';
	}/* End IF */
	if(label_ptr->opcode == TOK_C_LABEL_END) break;
	label_ptr = label_ptr->next_token;
    }/* End While */

}/* End Routine */

/* COMPARE_LABEL - Test if this is our label
 *
 * Function:
 *
 *	This routine is called by the GOTO function to compare labels
 *	to see if they match.
 */
int
compare_label( struct cmd_token *goto_ptr, struct cmd_token *label_ptr )
{
char string1[PARSER_STRING_MAX],string2[PARSER_STRING_MAX];
char *cp1;

    PREAMBLE();

    cp1 = string1;

    *cp1 = '\0';
    while(goto_ptr){
	if(cp1 >= &string1[PARSER_STRING_MAX-1]) break;
	if(goto_ptr->opcode == TOK_C_INPUTCHAR){
	    if(goto_ptr->input_byte == ESCAPE) break;
	    *cp1++ = goto_ptr->input_byte;
	    *cp1 = '\0';
	}/* End IF */
	if(goto_ptr->opcode == TOK_C_GOTO_END) break;
	goto_ptr = goto_ptr->next_token;
    }/* End While */

    cp1 = string2;

    *cp1 = '\0';
    while(label_ptr){
	if(cp1 >= &string2[PARSER_STRING_MAX-1]) break;
	if(label_ptr->opcode == TOK_C_INPUTCHAR){
	    if(label_ptr->input_byte == '!') break;
	    *cp1++ = label_ptr->input_byte;
	    *cp1 = '\0';
	}/* End IF */
	if(label_ptr->opcode == TOK_C_LABEL_END) break;
	label_ptr = label_ptr->next_token;
    }/* End While */

    if(strcmp(string1,string2) == 0) return(YES);

    return(NO);

}/* End Routine */



/* FIND_CONDITIONAL_ELSE - Routine to find the else of a conditional
 *
 * Function:
 *
 *	This routine is called by the conditional expressions when the
 *	specified condition has not been met and we want to execute the
 *	else clause of the conditional. The routine searches until it
 *	finds either the else, or the end of the conditional.
 */
int
find_conditional_else( struct cmd_token *ct )
{
    register struct cmd_token *oct;

    PREAMBLE();

    oct = ct;
    while((oct = oct->next_token)){
	if(oct->opcode == TOK_C_CONDITIONAL_ELSE){
	    if(oct->ctx.cnest == ct->ctx.cnest){
		jump_to_token = oct;
		return(SUCCESS);
	    }/* End IF */
	}/* End IF */

	if(oct->opcode == TOK_C_CONDITIONAL_END){
	    if(oct->ctx.cnest == (ct->ctx.cnest-1)){
		jump_to_token = oct;
		return(SUCCESS);
	    }/* End IF */
	}/* End IF */

    }/* End While */
    ct->ctx.go_flag = NO;
    return(BLOCKED);

}/* End Routine */

/* FIND_CONDITIONAL_END - Routine to find the close of a conditional
 *
 * Function:
 *
 *	This routine is called to skip over the else clause and find the
 *	end of the conditional.
 */
int
find_conditional_end( struct cmd_token *ct )
{
    register struct cmd_token *oct;

    PREAMBLE();

    oct = ct;
    while((oct = oct->next_token)){
	if(oct->opcode != TOK_C_CONDITIONAL_END) continue;
	if(oct->ctx.cnest != (ct->ctx.cnest-1)) continue;
	jump_to_token = oct;
	return(SUCCESS);
    }/* End While */
    ct->ctx.go_flag = NO;
    return(BLOCKED);

}/* End Routine */



/* EXEC_DOQ0 - Execute Q register zero as a macro
 *
 * Function:
 *
 *	This routine is called to execute Q register zero as a macro on
 *	startup. This allows the user to initialize things from his teco.ini
 *	before teco reaches command level.
 */
int
exec_doq0()
{
    register struct buff_header *qbp;
    register struct undo_token *ut;
    struct cmd_token *ct;

    PREAMBLE();

/*
 * Get a pointer to the q-register buffer structure. If it doesn't
 * exist, declare an error.
 */
    qbp = buff_qfind('0',0);
    if(qbp == NULL){
	error_message("?Internal Error! Q register 0 disappeared during init");
	return(FAIL);
    }/* End IF */
/*
 * If the Q register is empty, just return with no error.
 */
    if(qbp->zee == 0){
	return(SUCCESS);
    }/* End IF */
/*
 * Set up a fake command token and undo token structure for the parser to
 * chain undo blocks off of. We don't ever undo this stuff, but the routine
 * expects it to be there.
 */
    ct = allocate_cmd_token((struct cmd_token *)NULL);
    if(ct == NULL) return(FAIL);

    ut = allocate_undo_token(ct);
    if(ut == NULL){
	free_cmd_token(ct);
	return(FAIL);
    }/* End IF */

    ut->opcode = UNDO_C_MACRO;
    ut->carg1 = NULL;

    tecmacro(qbp,ct,(struct cmd_token **)&ut->carg1);
    parser_cleanup_ctlist(ct);
    return(SUCCESS);

}/* End Routine */
