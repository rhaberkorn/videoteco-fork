char *tecparse_c_version = "tecparse.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/tecparse.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/**
 * \file tecparse.c
 * \brief Subroutines to implement the finite state parser
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

    struct cmd_token *cmd_list = NULL;
    struct cmd_token *last_cmd_list = NULL;
    struct cmd_token *jump_to_token;
    struct cmd_token *resume_execute_ct;
    struct cmd_token *last_token_executed;
    struct search_buff search_string;
    char user_message[PARSER_STRING_MAX];
    unsigned long remembered_dot;
    char immediate_execute_flag = YES;
    char trace_mode_flag = NO;

    extern int tty_input_chan;
    extern int errno;
    extern char exit_flag;
    extern char intr_flag;
    extern char susp_flag;
    extern char input_pending_flag;
    extern char alternate_escape_character;
    extern char	main_delete_character;
    extern char alternate_delete_character;
    extern char checkpoint_flag;
    extern char checkpoint_enabled;
    extern char waiting_for_input_flag;
    extern char resize_flag;
    extern char ring_audible_bell;
    extern char suspend_is_okay_flag;
    extern struct buff_header *curbuf,*buffer_headers;
    extern int term_speed;

/*
 * Forward declarations
 */
    struct cmd_token *parse_rubout_character(struct cmd_token *,int);
    struct cmd_token *parse_rubout_cmd_token(struct cmd_token *);
    void preserve_rubout_char(char);
    void parser_clean_preserve_list(void);
    int parser_getc(void);
    int unpreserve_rubout_char(struct cmd_token *);
	char * trace_convert_opcode_to_name( int opcode );
	char * trace_convert_state_to_name( int state );
	char * trace_convert_exec_state_to_name( int state );



/**
 * \brief Main entry point of the parser
 *
 * This routine reads input characters and builds the token
 * list. It also calls the execute states as long as the go
 * flag is set. When it hits the final parse state, it then
 * executes any states which were not immediately executed.
 */
void
tecparse()
{
register struct cmd_token *ct;
int status;
int c;
static char no_mem = 0;

    PREAMBLE();

/*
 * We start our command token list with a special token that simply means that
 * this is the begining of the list. We also set the current state to be the
 * initial state of the parser.
 */
    resume_execute_ct = NULL;
    ct = allocate_cmd_token((struct cmd_token *)NULL);
    if(ct != NULL){
	no_mem = 0;
	ct->opcode = TOK_C_FIRSTTOKEN;
	ct->ctx.state  = STATE_C_INITIALSTATE;
	ct->ctx.go_flag = YES;
	if(immediate_execute_flag == NO) ct->ctx.go_flag = NO;
	cmd_list = ct;
	screen_reset_echo(cmd_list);
/*
 * Now we loop until we reach a final state, or something horrible happens.
 */
	while(1){
	    c = parser_getc();
	    status = tecparse_syntax(c);
	    if(status == FAIL) break;
	}/* End While */

	screen_format_windows();

    }/* End IF */

    else {
	if(no_mem){
	    tec_panic("no memory available");
	}/* End IF */
	no_mem = 1;
	error_message("<no memory>");
    }/* End Else */

    screen_refresh();

/*
 * Here to clean up old parse lists. Note that we always clean up the list
 * on last_cmd_list, and then move the current command list over there. This
 * is so that we always remember one command string back incase the user uses
 * the '*' command to save it all to a q-register. Besides unchaining command
 * blocks, the cleanup routine follows down undo lists and macro lists to
 * reclaim all the memory they are using.
 */
    parser_cleanup_ctlist(last_cmd_list);
    last_cmd_list = cmd_list;
    cmd_list = NULL;

/*
 * Also clean up the rubout Q-register.
 */
    parser_clean_preserve_list();

/*
 * Also, clean up any lookaside lists
 */
    tec_gc_lists();

}/* End Routine */



/**
 * \brief Here to manipulate the tree according to new input
 *
 * This routine is called with an input byte to manipulate the
 * parser tree and execute any associated code.
 */
int
tecparse_syntax( int c )
{
register struct cmd_token *ct;
struct cmd_token trace_ct;
register struct cmd_token *final_token,*last_token;
int status;
char token_used,token_marked;
char old_modified_state;
struct buff_header *old_curbuf;
struct undo_token *ut;

    PREAMBLE();

/*
 * Find the end of the current command list.
 */
    ct = cmd_list;
    while(ct->next_token){
	ct = ct->next_token;
    }/* End While */
/*
 * parse_special_character tests whether this character is an immediate
 * typing control character which should not be sent to the state machine but
 * handled locally. This is how we handle things like rubout, rubout word,
 * and Control-U.
 */
    if(parse_special_character(ct,c)){
	return(1);
    }/* End IF */
/*
 * We only echo the character if is is not a special character
 */
    screen_echo(c);
    if(input_pending_flag == NO){
	screen_format_windows();
	screen_refresh();
    }/* End IF */
/*
 * We set the token_used flag to indicate that we have not used the input
 * byte yet.
 */
    token_used = token_marked = NO;
/*
 * Now we loop until we use the input byte, or reach a terminating state
 */
    while(1){
/*
 * If the last state transition used the input byte, we return to our
 * caller.
 */
	if(token_used == YES){
	    return(1);
	}/* End IF */
/*
 * Allocate a command token to hold the next state transition in. The routine
 * will automatically link it onto the end of the list.
 */
	ct = allocate_cmd_token(ct);
	if(ct == NULL){
	    error_message("<out of memory>");
	    return(1);
	}/* End IF */
/*
 * Here we check for the special RETURN state. Although all this could be done
 * in the state machine itself, this was a good place to consolidate things.
 * We pop the state machine stack a level so as to return to the calling state.
 * Note that iarg1 and iarg2 are copied from the calling state, so that they
 * are preserved. This means that the substate must return it's value through
 * tmpval which is not preserved.
 */
	if(ct->ctx.state == STATE_C_RETURN){
	    ct->ctx.state = ct->ctx.return_state;
	    ct->ctx.return_state = ct->ctx.caller_token->ctx.return_state;
	    ct->ctx.iarg1_flag = ct->ctx.caller_token->ctx.iarg1_flag;
	    ct->ctx.iarg2_flag = ct->ctx.caller_token->ctx.iarg2_flag;
	    ct->ctx.iarg1 = ct->ctx.caller_token->ctx.iarg1;
	    ct->ctx.iarg2 = ct->ctx.caller_token->ctx.iarg2;
	    ct->ctx.caller_token = ct->ctx.caller_token->ctx.caller_token;
	}/* End IF */
/*
 * Set up the initial state of the command token. The COPYTOKEN opcode doesn't
 * really mean anything except that the opcode hasn't really been set, so it's
 * more for debugging purposes than anything else. Notice that since we are
 * actually placing an input byte in input_byte, we set the default that the
 * state will indeed eat the byte. If this is not the case, it is up to the
 * state to clear the flag.
 */
	ct->opcode = TOK_C_COPYTOKEN;
	ct->flags |= TOK_M_EAT_TOKEN;
	ct->input_byte = c;
/*
 * If this is the first time through here for this input character,
 * mark this is the input character token so that rubout works correctly.
 */
	if(token_marked == NO){
	    ct->opcode = TOK_C_INPUTCHAR;
	    token_marked = YES;
	}/* End IF */
/*
 * Call the state machine to process this character. Although more command
 * tokens may be added onto the list by the state machine, we leave ct where
 * it is for the moment so that we can work forward through all the generated
 * states.
 */
	parse_input_character(ct,ct);
/*
 * If the state machine indicates that the input character was eaten, remember
 * this fact.
 */
	if(ct->flags & TOK_M_EAT_TOKEN) token_used = YES;
/*
 * If this character would cause us to transition into an error state, we
 * simply don't allow it. We generate a rubout sequence to remove the offending
 * character and allow the user to make a decision about what to do next.
 */
	if(ct->ctx.state == STATE_C_ERRORSTATE){
	    token_used = YES;
	    while(ct->next_token) ct = ct->next_token;
	    ct = parse_rubout_character(ct,0);
	    screen_reset_echo(cmd_list);
	    return(1);
	}/* End IF*/

/*
 * If ? debug mode is on, record new syntax tokens in the ? buffer
 */
	if(trace_mode_flag == YES){
	    last_token = ct;
	    while(last_token){
		trace_mode(TRACE_C_PARSE,last_token,0);
		last_token = last_token->next_token;
	    }/* End While */
	}/* End IF */

/*
 * Ok, the state transition seems to have gone smoothly, if the go flag is set
 * we are in immediate execute mode and can perform the execution phase of the
 * state transition.
 */
	if(resume_execute_ct != NULL){
	    ct = resume_execute_ct;
	    ct->ctx.go_flag = YES;
	    resume_execute_ct = NULL;
	}

/*
 * We need a place to chain off undo tokens. If we use the current token,
 * loops will distribute undo tokens all over the parse tree, with no way
 * to tell which order they were created in. So we find the last token, and
 * chain everything off of that. That way, if a rubout is hit, all the undo
 * tokens will be undone in order back to the last keystroke token. As the
 * user types more keystrokes, we keep finding the new end of the command
 * chain, and chaining undo tokens off of that. Thus order will be preserved.
 */
	final_token = ct;
	while (final_token->next_token) final_token = final_token->next_token;
	last_token = ct->prev_token;

	while(ct){
/*
 * We still want to copy tmpvals forward from state to state unless the
 * parse stage is attempting a store.
 */
	    if(last_token){
/*
 * Carry the argument values forward to this state. If the last state was a
 * RETURN state, then only copy out the tmpval, and fetch the callers iargs.
 */
		if((ct->flags & TOK_M_PARSELOAD_IARG1) == 0){
		    ct->ctx.iarg1 = last_token->ctx.iarg1;
		}/* End IF */
		if((ct->flags & TOK_M_PARSELOAD_IARG2) == 0){
		    ct->ctx.iarg2 = last_token->ctx.iarg2;
		}/* End IF */

		if(ct->ctx.state == STATE_C_RETURN){
		    if((ct->flags & TOK_M_PARSELOAD_IARG1) == 0){
			ct->ctx.iarg1 = ct->ctx.caller_token->ctx.iarg1;
		    }/* End IF */
		    if((ct->flags & TOK_M_PARSELOAD_IARG2) == 0){
			ct->ctx.iarg2 = ct->ctx.caller_token->ctx.iarg2;
		    }/* End IF */
		}/* End IF */
/*
 * If the next state is STOREVAL, the syntax stage is setting storeval. The
 * typical example of this is when the syntax stage sees a constant like '1'.
 * It uses this state to set the temporary value to 1.
 */
		if(
		    ct->execute_state != EXEC_C_STOREVAL &&
		    ((ct->flags & TOK_M_PARSELOAD_TMPVAL) == 0)
		){
		    ct->ctx.tmpval = last_token->ctx.tmpval;
		}/* End IF */

	    }/* End IF */

	    if(ct->execute_state){
/*
 * This is sort of a kludge, but it's an easy place to see if this
 * state transition causes the buffer to become modified. If so, we
 * link on an undo token to that if this gets undone, the modified
 * status gets put back. If we didn't do it here, we would have to
 * either do it in every command, and that would get tedious.
 */
		old_modified_state = curbuf->ismodified;
		old_curbuf = curbuf;

		if(trace_mode_flag == YES){
		    trace_ct = *ct;
		}/* End IF */

		status = execute_a_state(ct,final_token);
		if(status == BLOCKED){
		    resume_execute_ct = ct;
		}
/*
 * An INVALIDATE status occurs when the execute engine changes the
 * command token list in such a way that we may no longer be pointing
 * at something valid. So we back out, and re-enter..
 */
		if(status == INVALIDATE){
		    return(SUCCESS);
		}

		if(trace_mode_flag == YES){
		    trace_mode(TRACE_C_EXEC,&trace_ct,ct);
		}/* End IF */
/*
 * If the modified state went from unmodified to modified, link on an
 * undo token incase this gets undone - then the modified status will
 * get put back to its original state.
 */
		if(old_modified_state == NO && old_curbuf->ismodified == YES){
		    if(ct->undo_list){
			ut = ct->undo_list;
			while(ut->next_token) ut = ut->next_token;
			ut->next_token = allocate_undo_token(NULL);
			ut = ut->next_token;
		    }/* End IF */

		    else ct->undo_list = ut = allocate_undo_token(NULL);
		    if(ut != NULL){
			ut->opcode = UNDO_C_MODIFIED;
			ut->carg1 = (char *)old_curbuf;
		    }/* End IF */
		}/* End IF */

		if(susp_flag) cmd_pause();
/*
 * Since we are in immediate execute mode, if we get an error during the
 * execution phase, we know it was generated by the most recently input
 * character. Therefore, we can handle it the same way we do during the syntax
 * parsing, by pretending a rubout character was input.
 */
		if(status == BLOCKED) break;
		if(intr_flag) break;

		if(status != SUCCESS){
		    token_used = YES;
		    while(ct->next_token) ct = ct->next_token;
		    ct = parse_rubout_character(ct,0);
		    screen_reset_echo(cmd_list);
		    break;
		}/* End IF */
	    }/* End IF */

	    last_token = ct;

	    if(jump_to_token){
		ct = jump_to_token;
		jump_to_token = NULL;
		continue;
	    }

	    if(ct->next_token) ct = ct->next_token;
	    else break;
	}/* End While */
/*
 * Now regardless of whether go_flag is set, chain ct forward to the end of
 * the token list.
 */
	while(ct->next_token) ct = ct->next_token;
/*
 * If we hit FINALSTATE, this is the way the state machine has of telling us
 * to reset to the begining parser state. We remember the final token address
 * so that all additional undo tokens can be chained off of it.
 */
	if(ct->ctx.state == STATE_C_FINALSTATE){
	    return(0);
	}/* End IF */

    }/* End While */

}/* End Routine */



/**
 * \brief Here to execute a Q register as a macro
 *
 * This routine executes the specified q register as a macro. It returns
 * SUCCESS or FAILURE depending on what happens within the macro. Note
 * that it is very similar to the main parse routine. There are two major
 * differences: First, the characters are gotton from the q-register
 * rather than the input stream, and second, the entire parse is done
 * to completion before any execution takes place. This means that the
 * macro can modify the contents of the q-register without changing the
 * way the macro will execute.
 */
int
tecmacro(
			struct buff_header *qbp,
			struct cmd_token *input_ct,
			struct cmd_token **macro_cmd_list )
{
register struct cmd_token *ct;
struct cmd_token trace_ct;
register struct cmd_token *last_token;
int c = 0;
int status;
char token_used;
int i;
char old_modified_state;
struct buff_header *old_curbuf;
struct undo_token *ut;

    PREAMBLE();

/*
 * We start our command token list with a special token that simply means that
 * this is the begining of the list. We also set the current state to be the
 * initial state of the parser.
 */
    ct = allocate_cmd_token((struct cmd_token *)NULL);
    if(ct == NULL) return(FAIL);

    ct->opcode = TOK_C_FIRSTTOKEN;
    ct->ctx.state  = STATE_C_INITIALSTATE;
    ct->ctx.go_flag = NO;
/*
 * By setting STATUS_PASSED, we insure that the first command in the macro will
 * get any arguments we have been passed.
 */
    ct->ctx.flags |= CTOK_M_STATUS_PASSED;
    ct->ctx.iarg1_flag = input_ct->ctx.iarg1_flag;
    ct->ctx.iarg2_flag = input_ct->ctx.iarg2_flag;
    ct->ctx.iarg1 = input_ct->ctx.iarg1;
    ct->ctx.iarg2 = input_ct->ctx.iarg2;
    *macro_cmd_list = ct;
/*
 * We set the token_used flag so that we will call the getc routine. This flag
 * in general lets us remember whether or not a state transistion ate an input
 * character or not.
 */
    token_used = YES;
/*
 * Now we loop until we reach a final state, or something horrible happens.
 */
    i = 0;

    while(i <= qbp->zee){
/*
 * Allocate a command token to hold the next state transition in. The routine
 * will automatically link it onto the end of the list.
 */
	ct = allocate_cmd_token(ct);
	if(ct == NULL) return(FAIL);
/*
 * Here we check for the special RETURN state. Although all this could be done
 * in the state machine itself, this was a good place to consolidate things.
 * We pop the state machine stack a level so as to return to the calling state.
 * Note that iarg1 and iarg2 are copied from the calling state, so that they
 * are preserved. This means that the substate must return it's value through
 * tmpval which is not preserved.
 */
	if(ct->ctx.state == STATE_C_RETURN){
	    ct->ctx.state = ct->ctx.return_state;
	    ct->ctx.return_state = ct->ctx.caller_token->ctx.return_state;
	    ct->ctx.iarg1_flag = ct->ctx.caller_token->ctx.iarg1_flag;
	    ct->ctx.iarg2_flag = ct->ctx.caller_token->ctx.iarg2_flag;
	    ct->ctx.iarg1 = ct->ctx.caller_token->ctx.iarg1;
	    ct->ctx.iarg2 = ct->ctx.caller_token->ctx.iarg2;
	    ct->ctx.caller_token = ct->ctx.caller_token->ctx.caller_token;
	}/* End IF */
/*
 * Set up the comand token a little
 */
	ct->opcode = TOK_C_COPYTOKEN;
	ct->flags |= TOK_M_EAT_TOKEN;
	ct->input_byte = c;
/*
 * If the last state ate the input character, we need to fetch another from the
 * command source.
 */
	if(token_used == YES){
	    if(i == qbp->zee) c = ESCAPE;
	    else c = buff_contents(qbp,i++);
#ifdef NEEDED_WHICH_I_DONT_THINK_IT_IS
	    if(parse_special_character(ct,c)){
		ct = *macro_cmd_list;
		while(ct->next_token) ct = ct->next_token;
		continue;
	    }/* End IF */
#endif
	    ct->opcode = TOK_C_INPUTCHAR;
	    ct->input_byte = c;
	}/* End IF */

	token_used = NO;
/*
 * Call the state machine to process this character. Although more command
 * tokens may be added onto the list by the state machine, we leave ct where
 * it is for the moment so that we can work forward through all the generated
 * states.
 */
	parse_input_character(ct,*macro_cmd_list);
/*
 * If the state machine indicates that the input character was eaten, remember
 * this fact.
 */
	if(ct->flags & TOK_M_EAT_TOKEN) token_used = YES;
/*
 * If this character would cause us to transition into an error state, we
 * terminate the macro and indicate where the error was.
 */
	if(ct->ctx.state == STATE_C_ERRORSTATE){
	    token_used = YES;
	    while(ct->next_token) ct = ct->next_token;
	    while(ct->opcode != TOK_C_FIRSTTOKEN){
		ct = parse_rubout_character(ct,0);
	    }/* End While */
	    return(FAIL);
	}/* End IF*/
/*
 * Always chain ct forward over any generated tokens
 */
	while(ct->next_token) ct = ct->next_token;
/*
 * If we hit FINALSTATE, this is the way the state machine has of telling us
 * to reset to the begining parser state.
 */
	if(ct->ctx.state == STATE_C_FINALSTATE) break;

    }/* End While */

/*
 * Now that the syntax parsing has been completed, we start execution of the
 * macro.
 */
    ct = last_token = *macro_cmd_list;

    while(ct){

/*
 * If we hit the final state, we don't need to go any further.
 */
	if(ct->ctx.state == STATE_C_FINALSTATE) break;

/*
 * Carry the argument values forward to this state. If the last state was a
 * RETURN state, then only copy out the tmpval, and fetch the callers iargs.
 */
	if((ct->flags & TOK_M_PARSELOAD_IARG1) == 0){
	    ct->ctx.iarg1 = last_token->ctx.iarg1;
	}/* End IF */
	if((ct->flags & TOK_M_PARSELOAD_IARG2) == 0){
	    ct->ctx.iarg2 = last_token->ctx.iarg2;
	}/* End IF */

	if(ct->ctx.state == STATE_C_RETURN){
	    if((ct->flags & TOK_M_PARSELOAD_IARG1) == 0){
		ct->ctx.iarg1 = ct->ctx.caller_token->ctx.iarg1;
	    }/* End IF */
	    if((ct->flags & TOK_M_PARSELOAD_IARG2) == 0){
		ct->ctx.iarg2 = ct->ctx.caller_token->ctx.iarg2;
	    }/* End IF */
	}/* End IF */
/*
 * If the next state is STOREVAL, the syntax stage is setting storeval. The
 * typical example of this is when the syntax stage sees a constant like '1'.
 * It uses this state to set the temporary value to 1.
 */
	if(
	    ct->execute_state != EXEC_C_STOREVAL &&
	    ((ct->flags & TOK_M_PARSELOAD_TMPVAL) == 0)
	){
	    ct->ctx.tmpval = last_token->ctx.tmpval;
	}/* End IF */

/*
 * If this token has an execute state, call the execute phase to implement it.
 */
	if(trace_mode_flag == YES){
	    trace_mode(TRACE_C_MACRO,ct,0);
	}/* End IF */

	if(ct->execute_state){

	    old_modified_state = curbuf->ismodified;
	    old_curbuf = curbuf;

	    if(trace_mode_flag == YES){
		trace_ct = *ct;
	    }/* End IF */

	    status = execute_a_state(ct,*macro_cmd_list);

	    if(trace_mode_flag == YES){
		trace_mode(TRACE_C_EXEC,&trace_ct,ct);
	    }/* End IF */

	    if(old_modified_state == NO && old_curbuf->ismodified == YES){
		if(ct->undo_list){
		    ut = ct->undo_list;
		    while(ut->next_token) ut = ut->next_token;
		    ut->next_token = allocate_undo_token(NULL);
		    ut = ut->next_token;
		}/* End IF */

		else ct->undo_list = ut = allocate_undo_token(NULL);
		if(ut != NULL){
		    ut->opcode = UNDO_C_MODIFIED;
		    ut->carg1 = (char *)old_curbuf;
		}/* End IF */
	    }/* End IF */

	    if(status != SUCCESS) goto cleanup;
	    if(exit_flag == YES) goto cleanup;
	    if(intr_flag) goto cleanup;
	    if(susp_flag) cmd_pause();
	}/* End IF */
/*
 * Ok, now chain to the next token, or quit if done
 */
	last_token = ct;
	ct = ct->next_token;
	if(jump_to_token){
	    ct = jump_to_token;
	    last_token = ct;
	    jump_to_token = NULL;
	}/* End IF */
    }/* End While */

cleanup:

    if(input_ct->ctx.flags & CTOK_M_STATUS_PASSED){
	input_ct->ctx.iarg1_flag = last_token->ctx.iarg1_flag;
	input_ct->ctx.iarg1 = last_token->ctx.iarg1;
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Test for and handle special characters
 *
 * This routine is called on each input character to determine whether
 * it requires special handling. It catches characters such as rubout,
 * ^W, ^U.
 */
int
parse_special_character( struct cmd_token *ct, int c )
{
char state_seen;
int tmp;

    PREAMBLE();

    switch(c){
	case RUBOUT:
	    ct = parse_rubout_character(ct,1);
	    break;
	case CNTRL_U:
/*
 * Get rid of any <CR> right at the deletion point
 */
	    if(ct->opcode == TOK_C_INPUTCHAR){
		ct = parse_rubout_character(ct,1);
	    }/* End IF */

	    while(ct != cmd_list){
		if(ct->opcode == TOK_C_INPUTCHAR && ct->input_byte == '\n'){
		    break;
		}/* End IF */
		ct = parse_rubout_character(ct,1);
	    }/* End While */
	    break;
	case CNTRL_W:
	    state_seen = 0;
	    tmp = 0;

	    while(ct != cmd_list){
		if(ct->flags & TOK_M_WORDBOUNDARY){
		    state_seen = 1;
		    break;
		}/* End IF */

		if(ct->opcode == TOK_C_INPUTCHAR){
		    tmp += 1;
		    if(tmp == 1 && ct->input_byte == '\n'){
			preserve_rubout_char(ct->input_byte);
			ct = parse_rubout_cmd_token(ct);
			break;
		    }/* End IF */

		    if(ct->input_byte != ' ' && ct->input_byte != '\t') break;
		    preserve_rubout_char(ct->input_byte);

		}/* End IF */

		ct = parse_rubout_cmd_token(ct);

	    }/* End While */

	    while(ct != cmd_list){
		if(ct->flags & TOK_M_WORDBOUNDARY) state_seen = 1;

		if(ct->opcode == TOK_C_INPUTCHAR){
		    if(state_seen){
			preserve_rubout_char(ct->input_byte);
			ct = parse_rubout_cmd_token(ct);
			break;
		    }/* End IF */

		    if(isspace((int)ct->input_byte)) break;
		    preserve_rubout_char(ct->input_byte);

		}/* End IF */

		ct = parse_rubout_cmd_token(ct);

	    }/* End While */

	    break;

	case CNTRL_R:
	    if(!unpreserve_rubout_char(ct)){
		error_message("^R no input tokens available");
	    }/* End IF */
	    return(1);

	default:
	    return(0);
    }/* End Switch */

    screen_reset_echo(cmd_list);
    return(1);

}/* End Routine */



/**
 * \brief Rubout the most recent character
 *
 * This routine is called when a rubout character is typed. We have to
 * back the parser up to the state it was in before the original char
 * was typed, as well as performing any undo functions to make sure
 * that the edit buffer also gets backed up.
 */
struct cmd_token *
parse_rubout_character( struct cmd_token *ct, int preserve_flag )
{
register struct cmd_token *oct;
struct undo_token *ut;
char saved_opcode;

    PREAMBLE();

    while(1){

/*
 * If the preserve flag is on, we want to save any deleted bytes in the
 * q-register that is used for this purpose. That way, the user can
 * correct for mistakenly typing the wrong rubout code.
 */
	if(preserve_flag && ct->opcode == TOK_C_INPUTCHAR){
	    preserve_rubout_char(ct->input_byte);
	}/* End IF */

/*
 * For each of the command tokens, we need to undo all the chained undo
 * tokens.
 */
	while( (ut = ct->undo_list) != NULL ){
/*
 * Take the top undo token off of the list and change the listhead to point
 * to it's child.
 */
	    ct->undo_list = ut->next_token;
	    ut->next_token = NULL;
/*
 * Now call the undo routine to back out the changes to the edit buffer that
 * this token calls for. Then place the token back on the free list.
 */
	    parser_undo(ut);
	    free_undo_token(ut);

	}/* End While */
/*
 * If this is TOK_C_FIRSTTOKEN, it means he is trying to rubout the head of the
 * list. This is probably not such a hot idea...
 */
	if(ct->opcode == TOK_C_FIRSTTOKEN) break;
/*
 * We save the opcode so that after cleaning up the token, we will know whether
 * it was an input character or not.
 */
	saved_opcode = ct->opcode;

	oct = ct->prev_token;
	if(oct == NULL) break;
	oct->next_token = NULL;
	ct->prev_token = NULL;
	free_cmd_token(ct);
	ct = oct;
/*
 * If this was an input character, then we have backed up enough.
 */
	if(saved_opcode == TOK_C_INPUTCHAR) break;

    }/* End While */

    return(ct);

}/* End Routine */



/**
 * \brief Rubout the most recent command token
 *
 * This routine is called to remove the last token on the command
 * list. It gets used in rubout / ^U / ^W processing.
 */
struct cmd_token *
parse_rubout_cmd_token( struct cmd_token *ct )
{
register struct cmd_token *oct;
struct undo_token *ut;

    PREAMBLE();

/*
 * We need to undo all the undo tokens chained off of this command token.
 */
    while( (ut = ct->undo_list) != NULL ){
/*
 * Take the top undo token off of the list and change the listhead to point
 * to it's child.
 */
	ct->undo_list = ut->next_token;
	ut->next_token = NULL;
/*
 * Now call the undo routine to back out the changes to the edit buffer that
 * this token calls for. Then place the token back on the free list.
 */
	parser_undo(ut);
	free_undo_token(ut);

    }/* End While */

    oct = ct->prev_token;
    if(oct) oct->next_token = NULL;
    ct->prev_token = NULL;
    free_cmd_token(ct);
    return(oct);

}/* End Routine */



/**
 * \brief Return next input character
 *
 * This routine is called by the parser when another input byte is needed.
 */
int
parser_getc()
{
register int i;
char inbuf[4];

#ifdef VMS
    short qio_iosb[4];
#endif /* VMS */

    PREAMBLE();

/*
 * If there are no unclaimed bytes in the buffer, we need to read one
 * from the command channel.
 */
    input_pending_flag = tty_input_pending();

    if(input_pending_flag == NO){
	if(ring_audible_bell){
	    term_putc(BELL);
	    ring_audible_bell = 0;
	}/* End IF */
	screen_echo('\0');
	screen_format_windows();
	screen_refresh();
    }/* End IF */

#ifdef CHECKPOINT
    if(checkpoint_enabled == YES && checkpoint_flag == YES){
	cmd_checkpoint();
	checkpoint_flag = NO;
    }/* End IF */
#endif /* CHECKPOINT */

    while(1){

#ifdef UNIX

	if(susp_flag){
	    pause_while_in_input_wait();
	    continue;
	}/* End IF */

	if(resize_flag){
	    screen_resize();
	}/* End IF */

	waiting_for_input_flag = YES;
	i = read(tty_input_chan,inbuf,1);
	waiting_for_input_flag = NO;

	intr_flag = 0;

	if(i >= 0) break;

	if(errno == EINTR) continue;
#endif
#ifdef MSDOS
	waiting_for_input_flag = YES;
	/* does not echo */
	i = getch();
	waiting_for_input_flag = NO;

	intr_flag = 0;

	if(i != EOF){
	    inbuf[0] = i == '\r' ? '\n' : i;
	    break;
	}
#endif
#ifdef VMS
	waiting_for_input_flag = YES;
	i = sys$qiow(0,tty_input_chan,IO$_READVBLK|IO$M_NOECHO|IO$M_NOFILTR,
	    qio_iosb,0,0,inbuf,1,0,0,0,0);
	waiting_for_input_flag = NO;
	if(!(i & STS$M_SUCCESS)) exit(i);
if(qio_iosb[0] != 1)
printf("!!! iosb[0] is %d\n",qio_iosb[0]);
	if(inbuf[0] == '\r') inbuf[0] = '\n';
	else if(inbuf[0] == '\n') inbuf[0] = '\r';
	if(inbuf[0] == '`') inbuf[0] = ESCAPE;
	if(inbuf[0] == CNTRL_Z && suspend_is_okay_flag == YES){
	    pause_while_in_input_wait();
	    continue;
	}/* End IF */

	break;
#endif
	perror("error reading command input");
	punt(errno);
    }/* End While */

    screen_reset_message();

    input_pending_flag = tty_input_pending();

    if(inbuf[0] == alternate_escape_character){
	return(ESCAPE);
    }/* End IF */

    if(inbuf[0] == main_delete_character){
	return(RUBOUT);
    }/* End IF */

    if(inbuf[0] == alternate_delete_character){
	return(RUBOUT);
    }/* End IF */

    return(inbuf[0]);

}/* End Routine */



/**
 * \brief Save rubbed out characters
 *
 * This routine is called in response to the user rubbing out input
 * characters with rubout, ^U, or ^W. Sometimes he does this by
 * mistake, and can delete large amounts of typing unintentially.
 * This routine saves these characters in a special Q-register so
 * that he can get them back if he wants.
 */
void
preserve_rubout_char( char the_byte )
{
register struct buff_header *qbp;

    PREAMBLE();

/*
 * Get a pointer to the special Q-register which holds the characters which
 * have been rubbed out.
 */
    qbp = buff_qfind('@',1);
    if(qbp == NULL){
	return;
    }/* End IF */

/*
 * Put the deleted byte into the Q-register where it can be retrieved by
 * the user if so desired.
 */
    buff_insert_char(qbp,0,the_byte);

}/* End Routine */



/**
 * \brief Poke a rubbed out char back into the parse
 *
 * This routine is called on behalf of a ^R command which causes the
 * most recent rubbed out character to be restored to the parse tree.
 * The return code determines whether this was done okay or not.
 */
int
unpreserve_rubout_char( struct cmd_token *ct )
{
register struct buff_header *qbp;
int c;

    PREAMBLE();

/*
 * Get a pointer to the special Q-register which holds the characters which
 * have been rubbed out.
 */
    qbp = buff_qfind('@',1);
    if(qbp == NULL){
	return(FAIL);
    }/* End IF */

/*
 * Make sure thre are some bytes in it
 */
    if(!qbp->zee) return(FAIL);

/*
 * Get the first byte saved in the Q-register
 */
    c = buff_contents(qbp,0);
    buff_delete(qbp,0,1);

    tecparse_syntax(c);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Zero the rubout preserve Q-register
 *
 * This routine is called at double-escape time to clean up the
 * special Q-register so that it doesn't grow without bounds.
 */
void
parser_clean_preserve_list( void )
{
register struct buff_header *qbp;

    PREAMBLE();

/*
 * Get a pointer to the special Q-register which holds the characters which
 * have been rubbed out.
 */
    qbp = buff_qfind('@',1);
    if(qbp == NULL){
	return;
    }/* End IF */

/*
 * Delete all the bytes in the Q-register
 */
    buff_delete(qbp,0,qbp->zee);

}/* End Routine */



/**
 * \brief Copy the current parse tree into a Q-register
 *
 * This routine copies the input bytes from the current command string
 * into the specified Q-register.
 */
void
parser_dump_command_line( struct buff_header *qbp )
{
register struct cmd_token *ct;

    PREAMBLE();

    ct = cmd_list;

    while(ct){
	if(ct->opcode == TOK_C_INPUTCHAR){
	    buff_insert_char(qbp,qbp->zee,ct->input_byte);
	}/* End IF */
	ct = ct->next_token;
    }/* End While */

}/* End Routine */



/**
 * \brief Replace current parse tree
 *
 * This routine replaces the current command string with the contents
 * the specified Q-register.
 */
int
parser_replace_command_line( struct buff_header *qbp )
{
register struct cmd_token *ct;
register struct cmd_token *first_different_ct;
register int c;
char temp;
register char *command_buffer;
register int cb_zee;

    PREAMBLE();

    cb_zee = 0;
    command_buffer = tec_alloc(TYPE_C_CBUFF,qbp->zee);
    if(command_buffer == NULL) return(FAIL);
/*
 * Walk the parse list to find the first place there is a difference
 * between the current parse list, and the one to be installed. When
 * we find the first difference, remember it, and then start copying
 * the changed bytes into our temporary Q-register.
 */
    first_different_ct = NULL;
    ct = cmd_list;
    for(c = 0; c < qbp->zee - 1; c++){
	temp = buff_contents(qbp,c);
	if(first_different_ct == NULL){
	    while(ct && ct->opcode != TOK_C_INPUTCHAR){
		ct = ct->next_token;
	    }/* End While */
	    if(ct && ct->input_byte != temp) first_different_ct = ct;
	    if(ct && ct->next_token) ct = ct->next_token;
	}/* End IF */
	if(first_different_ct){
	    command_buffer[cb_zee++] = temp;
	}/* End IF */
    }/* End FOR */

    if(ct != NULL && first_different_ct == NULL){
	while(ct->next_token){
	    if(ct->opcode == TOK_C_INPUTCHAR) break;
	    ct = ct->next_token;
	}
	first_different_ct = ct;
    }
/*
 * We only have to do the following if there were any changed bytes at
 * all. If he changed his mind and didn't modify the command list at all,
 * nothing really has to happen at all.
 */
    if(first_different_ct){
	while(first_different_ct->next_token != NULL &&
	  first_different_ct->next_token->opcode != TOK_C_INPUTCHAR){
	    first_different_ct = first_different_ct->next_token;
	}
	ct = cmd_list;
	while(ct->next_token) ct = ct->next_token;
/*
 * Remove from the tail of the parse list all the bytes up to and including
 * the first different character. This leaves us with a parse list which is
 * just the part that has not changed.
 */
	temp = 1;
	while(ct != cmd_list && temp != 0){
	    if(ct == first_different_ct) temp = 0;
	    ct = parse_rubout_character(ct,0);
	}/* End While */

/*
 * Now we walk through the Q-register, adding these bytes to the end of the
 * parse list. These are the bytes which are different from the original
 * parse list.
 */
	qbp->pos_cache.lbp = NULL;
	input_pending_flag = YES;
	for(c = 0; c < cb_zee; c++){
	    temp = command_buffer[c];
	    tecparse_syntax(temp);
	}/* End While */
    }/* End IF */
    tec_release(TYPE_C_CBUFF,command_buffer);
    parser_reset_echo();

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Allocate a command token structure
 *
 * This routine is called to allocate a command token structure. If there
 * is one on the free list, it is used, otherwise we allocate one. If
 * an old_token was specified, the context area is copied into the new
 * token, which has the effect of rippling the context forward through
 * the parse.
 */
struct cmd_token *
allocate_cmd_token( struct cmd_token *old_token )
{
register struct cmd_token *ct;

    PREAMBLE();

/*
 * Call the memory allocator for a command token block
 */
    ct = (struct cmd_token *)tec_alloc(TYPE_C_CMD,sizeof(struct cmd_token));
    if(ct == NULL) return(NULL);

/*
 * We set up the initial state to be that of an empty token, with fields
 * which are not set up yet set to their proper defaults.
 */
    memset(ct,0,sizeof(*ct));

/*
 * If there was an old token specified, two things must happen. First, we
 * have to link the new token onto the old token list. Second, we have to
 * copy the context area from the old token into the new one.
 */
    if(old_token){
	old_token->next_token = ct;
	ct->prev_token = old_token;
	ct->ctx = old_token->ctx;
    }/* End IF */

    return(ct);

}/* End Routine */



/**
 * \brief Routine to place a cmd token on the free list
 *
 * This routine is called with the address of the command token
 * to be placed on the free list.
 */
void
free_cmd_token( struct cmd_token *ct )
{

    PREAMBLE();

    tec_release(TYPE_C_CMD,(char *)ct);

}/* End Routine */

/**
 * \brief Pauses with the terminal in a good state
 *
 * This routine is called to pause the editor while we have been in
 * an input wait state. The big deal here is that we want to remove
 * the reverse video box that may be on the echo line before we pause
 * back to the system command processor.
 */
void
pause_while_in_input_wait()
{

    PREAMBLE();

    screen_reset_echo(cmd_list);
    screen_refresh();
    cmd_pause();
    screen_echo('\0');
    screen_refresh();

}/* End Routine */



/**
 * \brief Routine to deallocate all the blocks on a ct list
 *
 * This function deallocates all the blocks on a command token list,
 * including cmd_tokens and undo_tokens.
 */
void
parser_cleanup_ctlist( struct cmd_token *ct )
{
register struct cmd_token *oct;

    PREAMBLE();

/*
 * For each command token, if it has a list of undo tokens connected we
 * call the routine which knows how to clean them up.
 */
    while(ct){
	if(ct->undo_list){
	    tecundo_cleanup(ct->undo_list);
	}/* End IF */

	ct->undo_list = NULL;
/*
 * Now we remember what the next token in the list is, and then call the
 * routine to free the current one.
 */
	oct = ct;
	ct = ct->next_token;
	free_cmd_token(oct);

    }/* End While */

}/* End Routine */



/**
 * \brief Called by routines that don't want any arguments
 *
 * This function is called by states which don't want to accept any
 * arguments. If there are any present, it will generate an error message
 * and set the error state.
 */
int
parse_any_arguments( struct cmd_token *ct, char *cmd_name )
{
char tmp_message[LINE_BUFFER_SIZE];

    PREAMBLE();

/*
 * If either of the iarg flags is set, that means we received an argument.
 */
    if(ct->ctx.iarg1_flag == YES || ct->ctx.iarg2_flag == YES){
	(void) strcpy(tmp_message,"?The ");
	(void) strcat(tmp_message,cmd_name);
	(void) strcat(tmp_message," command accepts no arguments");
	error_message(tmp_message);
	ct->ctx.state = STATE_C_ERRORSTATE;
	return(1);
    }/* End IF */

    return(0);

}/* End Routine */

/**
 * \brief Called by routines that only want one arg
 *
 * This function is called by states which only want to accept one
 * argument. If there are two present, it will generate an error message
 * and set the error state.
 */
int
parse_more_than_one_arg( struct cmd_token *ct, char *cmd_name )
{
char tmp_message[LINE_BUFFER_SIZE];

    PREAMBLE();

/*
 * If the iarg2 flag is set, that means that we received two arguments when
 * we really only want one.
 */
    if(ct->ctx.iarg2_flag == YES){
	(void) strcpy(tmp_message,"?Two Arguments to ");
	(void) strcat(tmp_message,cmd_name);
	(void) strcat(tmp_message," not allowed");
	error_message(tmp_message);
	ct->ctx.state = STATE_C_ERRORSTATE;
	return(1);
    }/* End IF */

    return(0);

}/* End Routine */



/**
 * \brief Check for illegal buffer positions
 *
 * This routine is called to verify that buffer positions specified are
 * legal. If they are not, it generates an error message.
 */
int
parse_illegal_buffer_position( long pos1, long pos2, char *cmd_name )
{
char illegal_position;
char tmp_message[LINE_BUFFER_SIZE];

    PREAMBLE();

    illegal_position = 0;
    if(pos1 < 0) illegal_position = 1;
    if(pos2 < 0) illegal_position = 1;
    if(pos1 > curbuf->zee) illegal_position = 1;
    if(pos2 > curbuf->zee) illegal_position = 1;

    if(illegal_position == 0) return(0);

    (void) strcpy(tmp_message,"?Attempt to Move Pointer Off Page with ");
    (void) strcat(tmp_message,cmd_name);
    error_message(tmp_message);
    return(1);

}/* End Routine */



/**
 * \brief Re-echo the input line
 *
 * This is just an entry point to restore the echo line from a module
 * that doesn't want to know about the cmd_list structure.
 */
void
parser_reset_echo()
{
    PREAMBLE();

    screen_reset_echo(cmd_list);

}/* End Routine */



/**
 * \brief Trace execution of commands
 *
 * This routine is called at parse and execute time if tracing has been
 * enabled by the ? command. It causes q-register ? to be filled with
 * execution trace information.
 */
void
trace_mode( int phase, struct cmd_token *ct0, struct cmd_token *ct1 )
{
register struct buff_header *qbp;
char tmp_message[LINE_BUFFER_SIZE];
register char *cp;
register char *state_name;

    PREAMBLE();

    qbp = buff_qfind('?',1);
    if(qbp == NULL) return;

    switch(phase){
	case TRACE_C_PARSE:
	    state_name = trace_convert_opcode_to_name(ct0->opcode);
	    if(state_name == NULL) state_name = "UNKNOWN";
	    sprintf(tmp_message, "PARSE: %p Opcode %s %c",
	    	    ct0, state_name,
/*		    ct0->opcode == TOK_C_INPUTCHAR ? ct0->input_byte : ' '); */
		    ct0->flags & TOK_M_EAT_TOKEN ? ct0->input_byte : ' ');
	    if(ct0->flags & TOK_M_EAT_TOKEN){
		strcat(tmp_message," TOK_M_EAT_TOKEN");
	    }/* End IF */
	    if(ct0->flags & TOK_M_WORDBOUNDARY){
		strcat(tmp_message," TOK_M_WORDBOUNDARY");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_IARG1){
		strcat(tmp_message," TOK_M_PARSELOAD_IARG1");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_IARG2){
		strcat(tmp_message," TOK_M_PARSELOAD_IARG2");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_TMPVAL){
		strcat(tmp_message," TOK_M_PARSELOAD_TMPVAL");
	    }/* End IF */
	    strcat(tmp_message,"\n");
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

/*	int	execute_state; */
	    state_name = trace_convert_state_to_name(ct0->ctx.state);
	    if(state_name == NULL) state_name = "UNKNOWN";
	    sprintf(tmp_message,"    Parser State: %s '%c'",
		state_name,isprint((int)ct0->q_register) ? 
		    ct0->q_register : ' ');
	    if(ct0->ctx.flags & CTOK_M_COLON_SEEN){
		strcat(tmp_message," CTOK_M_COLON_SEEN");
	    }/* End IF */
	    if(ct0->ctx.flags & CTOK_M_ATSIGN_SEEN){
		strcat(tmp_message," CTOK_M_ATSIGN_SEEN");
	    }/* End IF */
	    if(ct0->ctx.flags & CTOK_M_STATUS_PASSED){
		strcat(tmp_message," CTOK_M_STATUS_PASSED");
	    }/* End IF */
	    strcat(tmp_message,"\n");
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    if(ct0->ctx.go_flag == 0){
		sprintf(tmp_message,"    Go flag clear\n");
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */

	    if(ct0->ctx.pnest || ct0->ctx.inest || ct0->ctx.cnest){
		sprintf(tmp_message,"    PNEST: %d INEST %d CNEST %d\n",
		    ct0->ctx.pnest,ct0->ctx.inest,ct0->ctx.cnest);
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */

	    sprintf(tmp_message,"    iarg1_flag %d iarg1 %ld (0x%lx)\n",
		ct0->ctx.iarg1_flag,ct0->ctx.iarg1,ct0->ctx.iarg1);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    iarg2_flag %d iarg2 %ld (0x%lx)\n",
		ct0->ctx.iarg2_flag,ct0->ctx.iarg2,ct0->ctx.iarg2);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    carg %p\n",ct0->ctx.carg);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    tmpval %ld (0x%lx)\n",
		ct0->ctx.tmpval,ct0->ctx.tmpval);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    if(ct0->ctx.return_state || ct0->ctx.caller_token){
		state_name = "";
		if(ct0->ctx.return_state){
		    state_name =
			trace_convert_state_to_name(ct0->ctx.return_state);
		    if(state_name == NULL) state_name = "UNKNOWN";
		}/* End IF */
		sprintf(tmp_message, "    return_state %s caller_token %p\n",
			state_name, ct0->ctx.caller_token);
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */
	    break;

	case TRACE_C_EXEC:
	    state_name = trace_convert_exec_state_to_name(ct0->execute_state);
	    if(state_name == NULL) state_name = "UNKNOWN";
	    sprintf(tmp_message, "EXEC: %p Exec State: %s\n",
	    	    ct1, state_name);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    iarg1_flag %d iarg1 %ld (0x%lx)",
		ct0->ctx.iarg1_flag,ct0->ctx.iarg1,ct0->ctx.iarg1);
	    while(strlen(tmp_message) < 39) strcat(tmp_message," ");
	    strcat(tmp_message," ");
	    cp = &tmp_message[strlen(tmp_message)];
	    sprintf(cp,"    iarg1_flag %d iarg1 %ld (0x%lx)\n",
		ct1->ctx.iarg1_flag,ct1->ctx.iarg1,ct1->ctx.iarg1);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    iarg2_flag %d iarg2 %ld (0x%lx)",
		ct0->ctx.iarg2_flag,ct0->ctx.iarg2,ct0->ctx.iarg2);
	    while(strlen(tmp_message) < 39) strcat(tmp_message," ");
	    strcat(tmp_message," ");
	    cp = &tmp_message[strlen(tmp_message)];
	    sprintf(cp,"    iarg2_flag %d iarg2 %ld (0x%lx)\n",
		ct1->ctx.iarg2_flag,ct1->ctx.iarg2,ct1->ctx.iarg2);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    carg %p",ct0->ctx.carg);
	    while(strlen(tmp_message) < 39) strcat(tmp_message," ");
	    strcat(tmp_message," ");
	    cp = &tmp_message[strlen(tmp_message)];
	    sprintf(cp,"    carg %p\n",ct1->ctx.carg);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    tmpval %ld (0x%lx)",
		ct0->ctx.tmpval,ct0->ctx.tmpval);
	    while(strlen(tmp_message) < 39) strcat(tmp_message," ");
	    strcat(tmp_message," ");
	    cp = &tmp_message[strlen(tmp_message)];
	    sprintf(cp,"    tmpval %ld (0x%lx)\n",
		ct1->ctx.tmpval,ct1->ctx.tmpval);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    break;

	case TRACE_C_MACRO:
	    state_name = trace_convert_opcode_to_name(ct0->opcode);
	    if(state_name == NULL) state_name = "UNKNOWN";
	    sprintf(tmp_message, "MACRO PLACEHOLDER: %p Opcode %s %c",
	    	    ct0, state_name,
/*		    ct0->opcode == TOK_C_INPUTCHAR ? ct0->input_byte : ' '); */
		    ct0->flags & TOK_M_EAT_TOKEN ? ct0->input_byte : ' ');
	    if(ct0->flags & TOK_M_EAT_TOKEN){
		strcat(tmp_message," TOK_M_EAT_TOKEN");
	    }/* End IF */
	    if(ct0->flags & TOK_M_WORDBOUNDARY){
		strcat(tmp_message," TOK_M_WORDBOUNDARY");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_IARG1){
		strcat(tmp_message," TOK_M_PARSELOAD_IARG1");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_IARG2){
		strcat(tmp_message," TOK_M_PARSELOAD_IARG2");
	    }/* End IF */
	    if(ct0->flags & TOK_M_PARSELOAD_TMPVAL){
		strcat(tmp_message," TOK_M_PARSELOAD_TMPVAL");
	    }/* End IF */
	    strcat(tmp_message,"\n");
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

/*	int	execute_state; */
	    state_name = trace_convert_state_to_name(ct0->ctx.state);
	    if(state_name == NULL) state_name = "UNKNOWN";
	    sprintf(tmp_message,"    Parser State: %s '%c'",
		state_name,
		isprint((int)ct0->q_register) ? ct0->q_register : ' ');
	    if(ct0->ctx.flags & CTOK_M_COLON_SEEN){
		strcat(tmp_message," CTOK_M_COLON_SEEN");
	    }/* End IF */
	    if(ct0->ctx.flags & CTOK_M_ATSIGN_SEEN){
		strcat(tmp_message," CTOK_M_ATSIGN_SEEN");
	    }/* End IF */
	    if(ct0->ctx.flags & CTOK_M_STATUS_PASSED){
		strcat(tmp_message," CTOK_M_STATUS_PASSED");
	    }/* End IF */
	    strcat(tmp_message,"\n");
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    if(ct0->ctx.go_flag == 0){
		sprintf(tmp_message,"    Go flag clear\n");
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */

	    if(ct0->ctx.pnest || ct0->ctx.inest || ct0->ctx.cnest){
		sprintf(tmp_message,"    PNEST: %d INEST %d CNEST %d\n",
		    ct0->ctx.pnest,ct0->ctx.inest,ct0->ctx.cnest);
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */

	    sprintf(tmp_message,"    iarg1_flag %d iarg1 %ld (0x%lx)\n",
		ct0->ctx.iarg1_flag,ct0->ctx.iarg1,ct0->ctx.iarg1);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    iarg2_flag %d iarg2 %ld (0x%lx)\n",
		ct0->ctx.iarg2_flag,ct0->ctx.iarg2,ct0->ctx.iarg2);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    carg %p\n",ct0->ctx.carg);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    sprintf(tmp_message,"    tmpval %ld (0x%lx)\n",
		ct0->ctx.tmpval,ct0->ctx.tmpval);
	    buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));

	    if(ct0->ctx.return_state || ct0->ctx.caller_token){
		state_name = "";
		if(ct0->ctx.return_state){
		    state_name =
			trace_convert_state_to_name(ct0->ctx.return_state);
		    if(state_name == NULL) state_name = "UNKNOWN";
		}/* End IF */
		sprintf(tmp_message, "    return_state %s caller_token %p\n",
			state_name, ct0->ctx.caller_token);
		buff_insert(qbp,qbp->zee,tmp_message,strlen(tmp_message));
	    }/* End IF */
	    break;

    }/* End Switch */

}

char *
trace_convert_opcode_to_name( int opcode )
{

    PREAMBLE();

    switch(opcode){
	case TOK_C_UNUSED:
	    return("TOK_C_UNUSED");
	case TOK_C_FIRSTTOKEN:
	    return("TOK_C_FIRSTTOKEN");
	case TOK_C_INPUTCHAR:
	    return("TOK_C_INPUTCHAR");
	case TOK_C_COPYTOKEN:
	    return("TOK_C_COPYTOKEN");
	case TOK_C_ITERATION_BEGIN:
	    return("TOK_C_ITERATION_BEGIN");
	case TOK_C_ITERATION_END:
	    return("TOK_C_ITERATION_END");
	case TOK_C_CONDITIONAL_END:
	    return("TOK_C_CONDITIONAL_END");
	case TOK_C_LABEL_BEGIN:
	    return("TOK_C_LABEL_BEGIN");
	case TOK_C_LABEL_END:
	    return("TOK_C_LABEL_END");
	case TOK_C_GOTO_BEGIN:
	    return("TOK_C_GOTO_BEGIN");
	case TOK_C_GOTO_END:
	    return("TOK_C_GOTO_END");
	case TOK_C_FINALTOKEN:
	    return("TOK_C_FINALTOKEN");
	case TOK_C_INITIALSTATE:
	    return("TOK_C_INITIALSTATE");
	case TOK_C_CONDITIONAL_ELSE:
	    return("TOK_C_CONDITIONAL_ELSE");
	default:
	    return(NULL);
    }/* End Switch */

}/* End Routine */

char *
trace_convert_state_to_name( int state )
{

    PREAMBLE();

    switch(state){
	case STATE_C_INITIALSTATE:
	    return("STATE_C_INITIALSTATE");
	case STATE_C_MAINCOMMANDS:
	    return("STATE_C_MAINCOMMANDS");
	case STATE_C_ESCAPESEEN:
	    return("STATE_C_ESCAPESEEN");
	case STATE_C_ECOMMAND:
	    return("STATE_C_ECOMMAND");
	case STATE_C_ARG1:
	    return("STATE_C_ARG1");
	case STATE_C_ARG2:
	    return("STATE_C_ARG2");
	case STATE_C_EXPRESSION:
	    return("STATE_C_EXPRESSION");
	case STATE_C_OPERATOR:
	    return("STATE_C_OPERATOR");
	case STATE_C_PLUS:
	    return("STATE_C_PLUS");
	case STATE_C_MINUS:
	    return("STATE_C_MINUS");
	case STATE_C_TIMES:
	    return("STATE_C_TIMES");
	case STATE_C_DIVIDE:
	    return("STATE_C_DIVIDE");
	case STATE_C_MINUSSEEN:
	    return("STATE_C_MINUSSEEN");
	case STATE_C_SUBEXPRESSION:
	    return("STATE_C_SUBEXPRESSION");
	case STATE_C_OPERAND:
	    return("STATE_C_OPERAND");
	case STATE_C_NUMBER_SUBSTATE:
	    return("STATE_C_NUMBER_SUBSTATE");
	case STATE_C_QOPERAND:
	    return("STATE_C_QOPERAND");
	case STATE_C_INSERT:
	    return("STATE_C_INSERT");
	case STATE_C_QUOTED_INSERT:
	    return("STATE_C_QUOTED_INSERT");
	case STATE_C_UMINUS:
	    return("STATE_C_UMINUS");
	case STATE_C_LABEL:
	    return("STATE_C_LABEL");
	case STATE_C_UQREGISTER:
	    return("STATE_C_UQREGISTER");
	case STATE_C_WRITEFILE:
	    return("STATE_C_WRITEFILE");
	case STATE_C_STRING:
	    return("STATE_C_STRING");
	case STATE_C_SEARCH:
	    return("STATE_C_SEARCH");
	case STATE_C_FCOMMAND:
	    return("STATE_C_FCOMMAND");
	case STATE_C_FSPART1:
	    return("STATE_C_FSPART1");
	case STATE_C_FSPART2:
	    return("STATE_C_FSPART2");
	case STATE_C_EDITBUF:
	    return("STATE_C_EDITBUF");
	case STATE_C_READFILE:
	    return("STATE_C_READFILE");
	case STATE_C_XQREGISTER:
	    return("STATE_C_XQREGISTER");
	case STATE_C_GQREGISTER:
	    return("STATE_C_GQREGISTER");
	case STATE_C_MQREGISTER:
	    return("STATE_C_MQREGISTER");
	case STATE_C_VIEWBUF:
	    return("STATE_C_VIEWBUF");
	case STATE_C_FDCOMMAND:
	    return("STATE_C_FDCOMMAND");
	case STATE_C_CONDITIONALS:
	    return("STATE_C_CONDITIONALS");
	case STATE_C_GOTO:
	    return("STATE_C_GOTO");
	case STATE_C_FRPART1:
	    return("STATE_C_FRPART1");
	case STATE_C_FRPART2:
	    return("STATE_C_FRPART2");
	case STATE_C_MESSAGE:
	    return("STATE_C_MESSAGE");
	case STATE_C_FKCOMMAND:
	    return("STATE_C_FKCOMMAND");
	case STATE_C_ECCOMMAND:
	    return("STATE_C_ECCOMMAND");
	case STATE_C_SAVECOMMAND:
	    return("STATE_C_SAVECOMMAND");
	case STATE_C_PERCENT_OPERAND:
	    return("STATE_C_PERCENT_OPERAND");
	case STATE_C_ATINSERT:
	    return("STATE_C_ATINSERT");
	case STATE_C_ATINSERT_PART2:
	    return("STATE_C_ATINSERT_PART2");
	case STATE_C_ONE_EQUALS:
	    return("STATE_C_ONE_EQUALS");
	case STATE_C_TWO_EQUALS:
	    return("STATE_C_TWO_EQUALS");
	case STATE_C_SKIP_ELSE:
	    return("STATE_C_SKIP_ELSE");
	case STATE_C_PUSH_QREGISTER:
	    return("STATE_C_PUSH_QREGISTER");
	case STATE_C_POP_QREGISTER:
	    return("STATE_C_POP_QREGISTER");
	case STATE_C_NSEARCH:
	    return("STATE_C_NSEARCH");
	case STATE_C_ACCEPT_ARGS:
	    return("STATE_C_ACCEPT_ARGS");
	case STATE_C_EQQREGISTER1:
	    return("STATE_C_EQQREGISTER1");
	case STATE_C_EQQREGISTER2:
	    return("STATE_C_EQQREGISTER2");
	case STATE_C_RADIX:
	    return("STATE_C_RADIX");
	case STATE_C_HEX_NUMBER:
	    return("STATE_C_HEX_NUMBER");
	case STATE_C_OCTAL_NUMBER:
	    return("STATE_C_OCTAL_NUMBER");
	case STATE_C_HEX_NUMBER_SUBSTATE:
	    return("STATE_C_HEX_NUMBER_SUBSTATE");
	case STATE_C_OCTAL_NUMBER_SUBSTATE:
	    return("STATE_C_OCTAL_NUMBER_SUBSTATE");
	case STATE_C_BACKSLASH:
	    return("STATE_C_BACKSLASH");
	case STATE_C_DELAYED_MINUS:
	    return("STATE_C_DELAYED_MINUS");
	case STATE_C_DELAYED_PLUS:
	    return("STATE_C_DELAYED_PLUS");
	case STATE_C_FSPART3:
	    return("STATE_C_FSPART3");


	case STATE_C_RETURN:
	    return("STATE_C_RETURN");
	case STATE_C_FINALSTATE:
	    return("STATE_C_FINALSTATE");
	case STATE_C_ERRORSTATE:
	    return("STATE_C_ERRORSTATE");
	default:
	    return(NULL);
    }/* End Switch */

}/* End Routine */



char *
trace_convert_exec_state_to_name( int state )
{

    PREAMBLE();

    switch(state){
	case EXEC_C_NULLSTATE:
	    return("EXEC_C_NULLSTATE");
	case EXEC_C_DOTARG1:
	    return("EXEC_C_DOTARG1");
	case EXEC_C_ZEEARG1:
	    return("EXEC_C_ZEEARG1");
	case EXEC_C_HARGUMENT:
	    return("EXEC_C_HARGUMENT");
	case EXEC_C_EXITCOMMAND:
	    return("EXEC_C_EXITCOMMAND");
	case EXEC_C_UQREGISTER:
	    return("EXEC_C_UQREGISTER");
	case EXEC_C_JUMP:
	    return("EXEC_C_JUMP");
	case EXEC_C_INSERT:
	    return("EXEC_C_INSERT");
	case EXEC_C_LINE:
	    return("EXEC_C_LINE");
	case EXEC_C_CHAR:
	    return("EXEC_C_CHAR");
	case EXEC_C_RCHAR:
	    return("EXEC_C_RCHAR");
	case EXEC_C_DELETE:
	    return("EXEC_C_DELETE");
	case EXEC_C_HVALUE:
	    return("EXEC_C_HVALUE");
	case EXEC_C_DOTVALUE:
	    return("EXEC_C_DOTVALUE");
	case EXEC_C_ZEEVALUE:
	    return("EXEC_C_ZEEVALUE");
	case EXEC_C_QVALUE:
	    return("EXEC_C_QVALUE");
	case EXEC_C_EQUALS:
	    return("EXEC_C_EQUALS");
	case EXEC_C_REDRAW_SCREEN:
	    return("EXEC_C_REDRAW_SCREEN");
	case EXEC_C_STOREVAL:
	    return("EXEC_C_STOREVAL");
	case EXEC_C_STORE1:
	    return("EXEC_C_STORE1");
	case EXEC_C_STORE2:
	    return("EXEC_C_STORE2");
	case EXEC_C_UMINUS:
	    return("EXEC_C_UMINUS");
	case EXEC_C_PLUS:
	    return("EXEC_C_PLUS");
	case EXEC_C_MINUS:
	    return("EXEC_C_MINUS");
	case EXEC_C_TIMES:
	    return("EXEC_C_TIMES");
	case EXEC_C_DIVIDE:
	    return("EXEC_C_DIVIDE");
	case EXEC_C_KILL:
	    return("EXEC_C_KILL");
	case EXEC_C_WRITEFILE:
	    return("EXEC_C_WRITEFILE");
	case EXEC_C_SEARCH:
	    return("EXEC_C_SEARCH");
	case EXEC_C_SETSEARCH:
	    return("EXEC_C_SETSEARCH");
	case EXEC_C_FSREPLACE1:
	    return("EXEC_C_FSREPLACE1");
	case EXEC_C_ITERATION_BEGIN:
	    return("EXEC_C_ITERATION_BEGIN");
	case EXEC_C_ITERATION_END:
	    return("EXEC_C_ITERATION_END");
	case EXEC_C_READFILE:
	    return("EXEC_C_READFILE");
	case EXEC_C_EDITBUF:
	    return("EXEC_C_EDITBUF");
	case EXEC_C_XQREGISTER:
	    return("EXEC_C_XQREGISTER");
	case EXEC_C_GQREGISTER:
	    return("EXEC_C_GQREGISTER");
	case EXEC_C_SEMICOLON:
	    return("EXEC_C_SEMICOLON");
	case EXEC_C_MQREGISTER:
	    return("EXEC_C_MQREGISTER");
	case EXEC_C_CLOSEBUF:
	    return("EXEC_C_CLOSEBUF");
	case EXEC_C_VIEWBUF:
	    return("EXEC_C_VIEWBUF");
	case EXEC_C_FDCOMMAND:
	    return("EXEC_C_FDCOMMAND");
	case EXEC_C_ACOMMAND:
	    return("EXEC_C_ACOMMAND");
	case EXEC_C_BACKSLASH:
	    return("EXEC_C_BACKSLASH");
	case EXEC_C_BACKSLASHARG:
	    return("EXEC_C_BACKSLASHARG");
	case EXEC_C_COND_GT:
	    return("EXEC_C_COND_GT");
	case EXEC_C_COND_LT:
	    return("EXEC_C_COND_LT");
	case EXEC_C_COND_EQ:
	    return("EXEC_C_COND_EQ");
	case EXEC_C_COND_NE:
	    return("EXEC_C_COND_NE");
	case EXEC_C_COND_DIGIT:
	    return("EXEC_C_COND_DIGIT");
	case EXEC_C_COND_ALPHA:
	    return("EXEC_C_COND_ALPHA");
	case EXEC_C_COND_LOWER:
	    return("EXEC_C_COND_LOWER");
	case EXEC_C_COND_UPPER:
	    return("EXEC_C_COND_UPPER");
	case EXEC_C_COND_SYMBOL:
	    return("EXEC_C_COND_SYMBOL");
	case EXEC_C_GOTO:
	    return("EXEC_C_GOTO");
	case EXEC_C_FRREPLACE:
	    return("EXEC_C_FRREPLACE");
	case EXEC_C_MESSAGE:
	    return("EXEC_C_MESSAGE");
	case EXEC_C_RESET_MESSAGE:
	    return("EXEC_C_RESET_MESSAGE");
	case EXEC_C_OUTPUT_MESSAGE:
	    return("EXEC_C_OUTPUT_MESSAGE");
	case EXEC_C_FKCOMMAND:
	    return("EXEC_C_FKCOMMAND");
	case EXEC_C_REMEMBER_DOT:
	    return("EXEC_C_REMEMBER_DOT");
	case EXEC_C_ECCOMMAND:
	    return("EXEC_C_ECCOMMAND");
	case EXEC_C_SAVECOMMAND:
	    return("EXEC_C_SAVECOMMAND");
	case EXEC_C_SCROLL:
	    return("EXEC_C_SCROLL");
	case EXEC_C_UPDATE_SCREEN:
	    return("EXEC_C_UPDATE_SCREEN");
	case EXEC_C_SET_IMMEDIATE_MODE:
	    return("EXEC_C_SET_IMMEDIATE_MODE");
	case EXEC_C_PERCENT_VALUE:
	    return("EXEC_C_PERCENT_VALUE");
	case EXEC_C_WORD:
	    return("EXEC_C_WORD");
	case EXEC_C_TWO_EQUALS:
	    return("EXEC_C_TWO_EQUALS");
	case EXEC_C_THREE_EQUALS:
	    return("EXEC_C_THREE_EQUALS");
	case EXEC_C_SKIP_ELSE:
	    return("EXEC_C_SKIP_ELSE");
	case EXEC_C_PUSH_QREGISTER:
	    return("EXEC_C_PUSH_QREGISTER");
	case EXEC_C_POP_QREGISTER:
	    return("EXEC_C_POP_QREGISTER");
	case EXEC_C_NSEARCH:
	    return("EXEC_C_NSEARCH");
	case EXEC_C_EQQREGISTER:
	    return("EXEC_C_EQQREGISTER");
	case EXEC_C_WINDOW_CONTROL:
	    return("EXEC_C_WINDOW_CONTROL");
	case EXEC_C_NEXT_WINDOW:
	    return("EXEC_C_NEXT_WINDOW");
	case EXEC_C_RLINE:
	    return("EXEC_C_RLINE");
	case EXEC_C_DELWORD:
	    return("EXEC_C_DELWORD");
	case EXEC_C_RDELWORD:
	    return("EXEC_C_RDELWORD");
	case EXEC_C_OPENBRACE:
	    return("EXEC_C_OPENBRACE");
	case EXEC_C_CLOSEBRACE:
	    return("EXEC_C_CLOSEBRACE");
	case EXEC_C_SKIPLABEL:
	    return("EXEC_C_SKIPLABEL");
	case EXEC_C_SETOPTIONS:
	    return("EXEC_C_SETOPTIONS");
	case EXEC_C_FSREPLACE2:
	    return("EXEC_C_FSREPLACE2");
	case EXEC_C_FSREPLACE3:
	    return("EXEC_C_FSREPLACE3");
	default:
	    return(NULL);
    }/* End Switch */

}/* End Routine */
