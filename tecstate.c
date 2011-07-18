char *tecstate_c_version = "tecstate.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:31 $
 * $Source: /cvsroot/videoteco/videoteco/tecstate.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/**
 * \file tecstate.c
 * \brief Main SWITCH/CASE statements to implement the parser syntax stage
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

    extern char immediate_execute_flag;
    extern char trace_mode_flag;
    extern char suspend_is_okay_flag;



/**
 * \brief Continue the parse with the supplied character
 *
 * We re-enter the parser with a new character at this point. We jump back
 * to the state we left before.
 */
void
parse_input_character( struct cmd_token *ct, struct cmd_token *uct )
{
char tmp_message[LINE_BUFFER_SIZE];
register struct cmd_token *oct = NULL;

    PREAMBLE();

    switch(ct->ctx.state){
/*
 * Here on initial command state. This is the begining of a command, so we are
 * looking for arguments that might go with a command. If there are no args,
 * we just transfer into the main command loop.
 */
	case STATE_C_INITIALSTATE:
	    ct->ctx.flags &= ~(CTOK_M_COLON_SEEN | CTOK_M_ATSIGN_SEEN);
	    ct->flags |= TOK_M_WORDBOUNDARY;
	case STATE_C_ACCEPT_ARGS:
	    ct->ctx.carg = NULL;
	    switch(ct->input_byte){
/*
 * If it looks like an argument, then transfer into a subexpression parser to
 * get the value of the expression. ARG1 will stuff the result into iarg1 and
 * also check for a comma (',') incase he is specifing a twin argument command
 */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'Q': case 'q': case '%':
#ifdef CLASSIC_B_BEHAVIOR
		case 'B': case 'b':
#endif
		case 'Z': case 'z':
		case '\\':
		case '.':
		case '-':
		case '(':
		case '^':
		    ct->ctx.flags &= ~CTOK_M_STATUS_PASSED;
		    ct->ctx.iarg1_flag = ct->ctx.iarg2_flag = NO;
		    ct->ctx.state = STATE_C_EXPRESSION;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_ARG1;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
/*
 * H is a special case, since the single argument actually implies two args.
 * (H actually is the same as 0,z<cmd>)
 */
		case 'H': case 'h':
		    ct->ctx.flags &= ~CTOK_M_STATUS_PASSED;
		    ct->ctx.iarg1_flag = ct->ctx.iarg2_flag = YES;
		    ct->ctx.state = STATE_C_MAINCOMMANDS;
		    ct->execute_state = EXEC_C_HVALUE;
		    return;
/*
 * Here on the @ command. This says to use user specified delimeters for
 * strings, rather than just terminating with escape.
 */
		case '@':
		    ct->ctx.flags |= CTOK_M_ATSIGN_SEEN;
		    ct->ctx.state = STATE_C_ACCEPT_ARGS;
		    return;
/*
 * Here on the : command. This is just a flag to many commands to tell them
 * to work in a slightly different way. The most common usage is to mean that
 * the command should return a value which says whether it worked or not.
 */
		case ':':
		    ct->ctx.flags |= CTOK_M_COLON_SEEN;
		    ct->ctx.state = STATE_C_ACCEPT_ARGS;
		    return;
/*
 * Well, it doesn't look like he is going to be specifying any arguments, so we
 * just go and decode the command.
 */
		default:
		    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){
			ct->ctx.state = STATE_C_MAINCOMMANDS;
			ct->flags &= ~TOK_M_EAT_TOKEN;
			ct->ctx.flags &= ~CTOK_M_STATUS_PASSED;
			return;
		    }/* End IF */
		    ct->ctx.state = STATE_C_MAINCOMMANDS;
		    ct->ctx.iarg1_flag = ct->ctx.iarg2_flag = NO;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;

	    }/* End Switch */
/*
 * Here to parse the major commands (i.e., ones that have no lead-in)
 */
	case STATE_C_MAINCOMMANDS:
	    switch(ct->input_byte){
/*
 * Space is a nop so that it can be used to make macros more readable
 */
		case ' ':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    return;
/*
 * Carriage Return is a nop so that it can be used in a macro to avoid
 * extremely long lines wrapping.
 */
		case '\n':
		    ct->ctx.state = STATE_C_MAINCOMMANDS;
		    return;
/*
 * Here on an escape. First of all, we have to remember that we have seen an
 * escape since two in a row will terminate the command. Also, escape has the
 * effect of blocking arguments from commands i.e. 2L will move two lines, but
 * 2$L will only move one since the escape eats the argument.
 */
		case ESCAPE:
		    ct->ctx.iarg1_flag = ct->ctx.iarg2_flag = NO;
		    ct->ctx.state = STATE_C_ESCAPESEEN;
		    return;		    
/*
 * Here on an Asterisk command. This causes the last double-escaped command
 * sequence to be saved in the named q-register.
 */
		case '*':
		    if(parse_any_arguments(ct,"*")) return;
		    ct->ctx.state = STATE_C_SAVECOMMAND;
		    return;
/*
 * Here on the @ command. This says to use user specified delimeters for
 * strings, rather than just terminating with escape.
 */
		case '@':
		    ct->ctx.flags |= CTOK_M_ATSIGN_SEEN;
		    return;
/*
 * Here on the : command. This is just a flag to many commands to tell them
 * to work in a slightly different way. The most common usage is to mean that
 * the command should return a value which says whether it worked or not.
 */
		case ':':
		    ct->ctx.flags |= CTOK_M_COLON_SEEN;
		    return;
/*
 * Here on the [ command. This causes the specified Q register to be pushed
 * onto the Q register pushdown stack.
 */
		case '[':
		    ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    ct->ctx.state = STATE_C_PUSH_QREGISTER;
		    return;

/*
 * Here on the ] command. This causes a Q register to be popped off of the
 * Q register pushdown stack, and replaces the specified Q register.
 */
		case ']':
		    ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    ct->ctx.state = STATE_C_POP_QREGISTER;
		    return;
/*
 * The B command moves backwards by lines. It is strictly a Video TECO
 * enhancement. In normal TECO, the B command returns the address of the
 * begining of the buffer, i.e. 0.
 */
		case 'B': case 'b':
		    if(parse_more_than_one_arg(ct,"B")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_RLINE;
		    return;
/*
 * Here on the C command. The C command is a relative move command, i.e., the
 * argument says how many spaces from the current position to move.
 */
		case 'C': case 'c':
		    if(parse_more_than_one_arg(ct,"C")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_CHAR;
		    return;
/*
 * The D command deletes the specified number of characters in the indicated
 * direction.
 */
		case 'D': case 'd':
		    if(parse_more_than_one_arg(ct,"D")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_DELETE;
		    return;
/*
 * Here on an E command. This is simply the lead-in to any one of the many
 * E commands.
 */
		case 'E': case 'e':
		    ct->ctx.state = STATE_C_ECOMMAND;
		    return;
/*
 * Here on an F command. This is a lead-in to one of the F? commands.
 */
		case 'F': case 'f':
		    ct->ctx.state = STATE_C_FCOMMAND;
		    return;
/*
 * The G command copies the contents of the specified q-register into the
 * edit buffer at the current position.
 */
		case 'G': case 'g':
		    if(parse_any_arguments(ct,"G")) return;
		    ct->ctx.state = STATE_C_GQREGISTER;
		    return;
/*
 * The I command inserts characters until an escape is input. If this is
 * entered with the tab command, not only do we enter insert mode, we also
 * insert the tab itself.
 */
		case '\t':
		    if(ct->ctx.iarg1_flag == NO) ct->flags &= ~TOK_M_EAT_TOKEN;


		case 'I': case 'i':
		    if(parse_more_than_one_arg(ct,"I")) return;

		    if(ct->ctx.iarg1_flag == YES){
			ct->ctx.state = STATE_C_INITIALSTATE;
			ct->execute_state = EXEC_C_INSERT;
			return;
		    }/* End IF */

		    if(ct->ctx.flags & CTOK_M_ATSIGN_SEEN){
			ct->ctx.state = STATE_C_ATINSERT;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_INSERT;
		    return;
/*
 * Here on the J command. The J command jumps to an absolute position in the
 * buffer.
 */
		case 'J': case 'j':
		    if(parse_more_than_one_arg(ct,"J")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_JUMP;
		    return;
/*
 * The K command acts just like the L command except that it deletes instead
 * of moving over. Also, it is legal to specify an a,b range to K
 */
		case 'K': case 'k':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_KILL;
		    return;
/*
 * The L command moves over the specified number of new-line characters. An
 * argument of zero means get to the begining of the current line.
 */
		case 'L': case 'l':
		    if(parse_more_than_one_arg(ct,"L")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_LINE;
		    return;
/*
 * The M command executes the contents of the specified Q register as a macro.
 */
		case 'M': case 'm':
		    ct->ctx.state = STATE_C_MQREGISTER;
		    return;
/*
 * The N command will search for the specified string across multiple edit
 * buffers.
 */
		case 'N': case 'n':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_NSEARCH;
		    return;
/*
 * The O command goes to the specified label
 */
		case 'O': case 'o':
		    if(parse_any_arguments(ct,"O")) return;
		    ct->ctx.state = STATE_C_GOTO;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_GOTO_BEGIN;
		    return;
/*
 * The P command selects the next window
 */
		case 'P': case 'p':
		    if(parse_more_than_one_arg(ct,"P")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_NEXT_WINDOW;
		    return;
/*
 * The R command is exactly like the C command, except that the direction
 * is reversed.
 */
		case 'R': case 'r':
		    if(parse_more_than_one_arg(ct,"R")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_RCHAR;
		    return;
/*
 * The S command will search for the specified string
 */
		case 'S': case 's':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_SEARCH;
		    return;
/*
 * The ^L command is temporarily used to redraw the screen
 */
		case '\f':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_REDRAW_SCREEN;
		    return;
/*
 * The U command loads the argument into the specified Q register
 */
		case 'U': case 'u':
		    if(parse_more_than_one_arg(ct,"U")) return;
		    ct->ctx.state = STATE_C_UQREGISTER;
		    return;
/*
 * The V command deletes words. This is a Vido TECO enhancement. In normal
 * TECO, the V command was equivalent to 0TT
 */
		case 'V': case 'v':
		    if(parse_more_than_one_arg(ct,"V")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_DELWORD;
		    return;
/*
 * The W command moves by words
 */
		case 'W': case 'w':
		    if(parse_more_than_one_arg(ct,"W")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_WORD;
		    return;
/*
 * The X command moves a block of characters from the edit buffer into
 * the specified q register.
 */
		case 'X': case 'x':
		    ct->ctx.state = STATE_C_XQREGISTER;
		    return;
/*
 * The Y command deletes words in reverse direction. This is a Video TECO
 * enhancement. In classic TECO, the Y command 'yanked' input data into the
 * edit buffer.
 */
		case 'Y': case 'y':
		    if(parse_more_than_one_arg(ct,"Y")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_RDELWORD;
		    return;
/*
 * The ^A command allows you to print a message to the message line
 */
		case CNTRL_A:
		    if(parse_any_arguments(ct,"^A")) return;
		    ct->ctx.state = STATE_C_MESSAGE;
		    ct->execute_state = EXEC_C_RESET_MESSAGE;
		    return;
/*
 * The < command opens an iteration
 */
		case '<':
		    if(parse_more_than_one_arg(ct,"<")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_ITERATION_BEGIN;
		    ct->ctx.inest += 1;
		    ct->execute_state = EXEC_C_ITERATION_BEGIN;
		    return;
/*
 * The > command closes an iteration
 */
		case '>':
		    if(parse_more_than_one_arg(ct,">")) return;
		    if(ct->ctx.inest == 0){
			error_message("?No Iteration Present");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_ITERATION_END;
		    while((oct = oct->prev_token)){
			if(oct->opcode != TOK_C_ITERATION_BEGIN) continue;
			if(oct->ctx.inest != ct->ctx.inest) continue;
			ct->ctx.caller_token = oct;
			break;
		    }/* End While */
/*
 * Preserve the arguments so that if the loop gets undone, it will get redone
 * the correct number of iterations when the > is typed again.
 */
		    {
		    register struct undo_token *ut;
		    ut = allocate_undo_token(ct);
		    if(ut == NULL){
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ut->opcode = UNDO_C_PRESERVEARGS;
		    ut->iarg1 = oct->ctx.iarg1;
		    ut->iarg2 = oct->ctx.iarg2;
		    ut->carg1 = (char *)oct;

		    ct->ctx.inest -= 1;
		    ct->execute_state = EXEC_C_ITERATION_END;
		    ct = allocate_cmd_token(ct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_COPYTOKEN;
		    return;
		    }
/*
 * The ; command terminates an iteration if the argument is >= 0.
 * If there is no argument supplied, it uses the state of the last
 * search operation performed as a value.
 */
		case ';':
		    if(parse_more_than_one_arg(ct,";")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_SEMICOLON;
		    return;
/*
 * The = command prints out the value of the supplied expression.
 * A single = prints out in decimal, == prints in octal and === in hex.
 */
		case '=':
		    if(parse_more_than_one_arg(ct,"=")) return;
		    ct->ctx.state = STATE_C_ONE_EQUALS;
		    ct->execute_state = EXEC_C_EQUALS;
		    return;
/*
 * The " command is the conditional 'IF' operator. The following commands only
 * get executed if the condition is satisfied.
 */
		case '"':
		    if(parse_more_than_one_arg(ct,"\"")) return;
		    ct->ctx.state = STATE_C_CONDITIONALS;
		    ct->ctx.cnest += 1;
		    return;
/*
 * The | command provides an else clause to a conditional expression
 */
		case '|':
		    if(parse_any_arguments(ct,"|")) return;
		    if(ct->ctx.cnest <= 0){
			error_message("?Not in a conditional");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_SKIP_ELSE;
		    ct->execute_state = EXEC_C_SKIP_ELSE;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
/*
 * The ' command ends a conditional expression.
 */
		case '\'':
		    if(parse_any_arguments(ct,"'")) return;
		    if(ct->ctx.cnest <= 0){
			error_message("?Not in a conditional");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_INITIALSTATE;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_CONDITIONAL_END;
		    ct->ctx.cnest -= 1;
		    return;
/*
 * The Label command allows you to set a tag which can be jumped to with
 * the 'O' command. It also provides a way to comment macros.
 */
		case '!':
		    if(parse_any_arguments(ct,"!")) return;
		    ct->ctx.state = STATE_C_LABEL;
		    ct->execute_state = EXEC_C_SKIPLABEL;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_LABEL_BEGIN;
		    return;
/*
 * The backslash command with an argument inserts the decimal
 * representation of the argument into the buffer at the current position.
 */
		case '\\':
		    if(ct->ctx.iarg2_flag == NO){
			ct->ctx.iarg2 = 10;
			ct->flags |= TOK_M_PARSELOAD_IARG2;
		    }/* End IF */

		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_BACKSLASH;
		    return;
/*
 * The open-brace command copies the current command string into a special
 * Q-register and places the user there so he can edit it.
 */
		case '{':
		    if(parse_any_arguments(ct,"{")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_OPENBRACE;
		    return;

/*
 * The close-brace command replaces the command string with the string in
 * the special Q-register.
 */
		case '}':
		    if(parse_any_arguments(ct,"}")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_CLOSEBRACE;
		    return;

/*
 * Here on a system which doesn't really do suspend correctly
 */
		case CNTRL_Z:
		    if(suspend_is_okay_flag == YES){
			cmd_suspend();
		    }/* End IF */
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    return;

/*
 * Here to toggle trace mode. This inserts information into q-register ?
 * to help us track execution.
 */
		case '?':
		    trace_mode_flag = !trace_mode_flag;
			{
				char traceString[ 64 ];
				if( trace_mode_flag ) strcpy( traceString, "Trace Mode ON" );
				else strcpy( traceString, "Trace Mode OFF" );
			    screen_message( traceString );
			}
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    return;

/*
 * We only get here on an error, since we should never dispatch a command that
 * does not have a corresponding case statement.
 */
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,
			"?MAINCOMMANDS: unknown command <%c>",
			UPCASE((int)ct->input_byte));
		    error_message(tmp_message);
		    return;
	    }/* End Switch */
/*
 * Here when we have seen the begining of a conditional '"'. Now we have to
 * look at the next character to determine what the actual condition test is.
 */
	case STATE_C_CONDITIONALS:
	    switch(ct->input_byte){
		case 'G': case 'g':
		case '>':
		    ct->execute_state = EXEC_C_COND_GT;
		    break;
		case 'L': case 'l':
		case 'T': case 't':
		case 'S': case 's':
		case '<':
		    ct->execute_state = EXEC_C_COND_LT;
		    break;
		case 'E': case 'e':
		case 'F': case 'f':
		case 'U': case 'u':
		case '=':
		    ct->execute_state = EXEC_C_COND_EQ;
		    break;
		case 'N': case 'n':
		case '!':
		    ct->execute_state = EXEC_C_COND_NE;
		    break;
		case 'C': case 'c':
		    ct->execute_state = EXEC_C_COND_SYMBOL;
		    break;
		case 'D': case 'd':
		    ct->execute_state = EXEC_C_COND_DIGIT;
		    break;
		case 'A': case 'a':
		    ct->execute_state = EXEC_C_COND_ALPHA;
		    break;
		case 'V': case 'v':
		    ct->execute_state = EXEC_C_COND_LOWER;
		    break;
		case 'W': case 'w':
		    ct->execute_state = EXEC_C_COND_UPPER;
		    break;
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,
			"?Unknown conditional command '%c'",
			UPCASE((int)ct->input_byte));
		    error_message(tmp_message);
		    return;
	    }/* End Switch */

	    ct->ctx.state = STATE_C_INITIALSTATE;
	    return;
/*
 * Here to watch for the end of a label
 */
	case STATE_C_LABEL:
	    switch(ct->input_byte){
		case '!':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_LABEL_END;
		    return;
		default:
		    ct->ctx.state = STATE_C_LABEL;
		    return;
	    }/* End Switch */
/*
 * Here to eat the characters in a GOTO command
 */
	case STATE_C_GOTO:
	    switch(ct->input_byte){
		case ESCAPE:
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_GOTO_END;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->execute_state = EXEC_C_GOTO;
		    return;
		default:
		    ct->ctx.state = STATE_C_GOTO;
		    return;
	    }/* End Switch */
/*
 * Here if we have seen an escape. If we see another, then he wants us to
 * complete the parse and execute any remaining commands.
 */
	case STATE_C_ESCAPESEEN:
	    switch(ct->input_byte){
		case ESCAPE:
		    if(ct->ctx.inest){
			error_message("?Unterminated Iteration");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    if(ct->ctx.cnest){
			error_message("?Unterminated Conditional");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_FINALSTATE;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->opcode = TOK_C_FINALTOKEN;
		    return;

		default:
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */
/*
 * Here if we have seen an E as a lead-in to one of the E commands.
 */
	case STATE_C_ECOMMAND:
	    switch(ct->input_byte){
/*
 * The EB command creates a new edit buffer and reads the specified file in.
 * If the command is given an argument, then this is a shorthand switch to
 * the buffer that has that number.
 */
		case 'B': case 'b':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    if(ct->ctx.iarg1_flag == YES){
			ct->execute_state = EXEC_C_EDITBUF;
			if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			    oct = ct;
			    ct = allocate_cmd_token(oct);
			    if(ct == NULL){
				oct->ctx.state = STATE_C_ERRORSTATE;
				return;
			    }/* End IF */
			    ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
			    ct->execute_state = EXEC_C_STORE1;
			}/* End IF */
			ct->ctx.state = STATE_C_INITIALSTATE;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_EDITBUF;
		    return;
/*
 * The EC command allows you to execute a command, and the output is placed
 * into the edit buffer.
 */
		case 'C': case 'c':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_ECCOMMAND;
		    return;
/*
 * The EF command closes the current edit buffer. If the current buffer
 * is modified, the command will fail unless the user specifies an argument
 * to the command.
 */
		case 'F': case 'f':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->execute_state = EXEC_C_CLOSEBUF;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			oct = ct;
			ct = allocate_cmd_token(oct);
			if(ct == NULL){
			    oct->ctx.state = STATE_C_ERRORSTATE;
			    return;
			}/* End IF */
			ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
			ct->execute_state = EXEC_C_STORE1;
		    }/* End IF */
		    return;
/*
 * The EI command allows you to alter immediate execution mode. With no
 * argument, execute mode is turned off for the remainder of the current
 * command. An argument of 0 turns it off until further notice and an
 * argument of 1 turns it back on.
 */
		case 'I': case 'i':
		    if(parse_more_than_one_arg(ct,"EI")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;

		    if(ct->ctx.iarg1_flag == NO){
			ct->ctx.go_flag = NO;
			return;
		    }/* End IF */

		    ct->execute_state = EXEC_C_SET_IMMEDIATE_MODE;
		    return;
/*
 * The EJ command allows you to load runtime options into TECO.
 */
		case 'J': case 'j':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_SETOPTIONS;
		    return;
		    
/*
 * The EP command splits the current window in half. If an argument is
 * supplied, the command deletes the current window.
 */
		case 'P': case 'p':
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_WINDOW_CONTROL;
		    return;
/*
 * The EV command works exactly like the EB command except that the buffer
 * which is created is 'readonly'. Thus this command stands for 'VIEW' file.
 */
		case 'V': case 'v':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    if(ct->ctx.iarg1_flag == YES){
			ct->execute_state = EXEC_C_VIEWBUF;
			ct->ctx.state = STATE_C_INITIALSTATE;
			return;
		    }/* End IF */

		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_VIEWBUF;
		    return;
/*
 * The EQ command reads a file into the specified Q-register.
 */
		case 'Q': case 'q':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    if(parse_any_arguments(ct,"EQ")) return;
		    ct->ctx.state = STATE_C_EQQREGISTER1;
		    return;
/*
 * The ER command reads the specified file into the current buffer location.
 */
		case 'R': case 'r':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_READFILE;
		    return;
/*
 * The ES command causes the screen to scroll
 */
		case 'S': case 's':
		    if(parse_more_than_one_arg(ct,"ES")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_SCROLL;
		    return;
/*
 * The ET command causes the screen to update
 */
		case 'T': case 't':
		    if(parse_any_arguments(ct,"ET")) return;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_UPDATE_SCREEN;
		    return;
/*
 * The EW command writes out the current buffer
 */
		case 'W': case 'w':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_WRITEFILE;
		    return;
/*
 * The EX command causes the editor to exit
 */
		case 'X': case 'x':
		    ct->execute_state = EXEC_C_EXITCOMMAND;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->ctx.go_flag = NO;
		    return;

		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,
			"?Unknown command E%c",UPCASE((int)ct->input_byte));
		    error_message(tmp_message);
		    return;
	    }/* End Switch */
/*
 * Here if we have seen an F as a lead-in to one of the F commands.
 */
	case STATE_C_FCOMMAND:
	    switch(ct->input_byte){
/*
 * The FD command finds the string and deletes it.
 */
		case 'D': case 'd':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_FDCOMMAND;
		    return;
/*
 * The FK command clears the text between the current position and the position
 * of the searched for text. The searched for text is left unmodified.
 */
		case 'K': case 'k':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_FKCOMMAND;
		    return;
/*
 * The FS command finds the first string and replaces it with the second
 */
		case 'S': case 's':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_FSPART1;
		    return;
/*
 * The FT command is a Video TECO extension to read & use a unix style
 * tags file.
 */
		case 'T': case 't':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->ctx.state = STATE_C_STRING;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_FTAGS;
		    return;

/*
 * The FR command acts like the FS command except that if the second argument
 * is NULL, the string is replaced with the last replace value.
 */
		case 'R': case 'r':
		    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
			ct->ctx.flags |= CTOK_M_STATUS_PASSED;
		    }/* End IF */
		    ct->execute_state = EXEC_C_SEARCH;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->execute_state = EXEC_C_FRREPLACE;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){  /* mch */
			ct = allocate_cmd_token(ct);
			ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
			ct->execute_state = EXEC_C_STORE1;
		    }/* End IF */

		    return;
/*
 * Here when we have an 'F' command with an unknown second character. This
 * makes it an illegal command, and we don't waste any time letting the user
 * know about it.
 */
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,
			"?Unknown command F%c",UPCASE((int)ct->input_byte));
		    error_message(tmp_message);
		    return;
	    }/* End Switch */
/*
 * The following state is the return-state from STRING when the first part
 * of an FS command has been parsed.
 */
	case STATE_C_FSPART1:
	    ct->ctx.state = STATE_C_FSPART2;
	    ct->execute_state = EXEC_C_SEARCH;
	    oct = ct;
	    ct = allocate_cmd_token(oct);
	    if(ct == NULL){
		oct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ct->execute_state = EXEC_C_FSREPLACE1;
	    return;

	case STATE_C_FSPART2:
	    ct->flags |= TOK_M_WORDBOUNDARY;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_FSPART3;
	    return;

	case STATE_C_FSPART3:
	    if(ct->ctx.flags & CTOK_M_ATSIGN_SEEN){
		if(ct->input_byte == ct->ctx.delimeter){
		    ct->ctx.state = STATE_C_ESCAPESEEN;
		    ct->execute_state = EXEC_C_FSREPLACE3;
		    return;
		}

		else {
		    ct->ctx.state = STATE_C_FSPART3;
		    ct->execute_state = EXEC_C_FSREPLACE2;
		    return;

		}
	    }

	    if(ct->input_byte == ESCAPE){
		ct->ctx.state = STATE_C_ESCAPESEEN;
		ct->execute_state = EXEC_C_FSREPLACE3;
		if(ct->ctx.flags & CTOK_M_STATUS_PASSED){  /* mch */
		    ct = allocate_cmd_token(ct);
		    ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		    ct->execute_state = EXEC_C_STORE1;
		}/* End IF */
		return;
	    }

	    else {
		ct->ctx.state = STATE_C_FSPART3;
		ct->execute_state = EXEC_C_FSREPLACE2;
		return;

	    }

#ifdef WIMPY_CODER_WHO_HAS_NOT_HANDLED_THIS_YET
		case CNTRL_V:
		    ct->ctx.state = STATE_C_QUOTED_INSERT;
		    return;
#endif /* WIMPY_CODER_WHO_HAS_NOT_HANDLED_THIS_YET */

/*
 * This state is the return state from STRING and the TAGS command is
 * ready to execute.
 */
	case STATE_C_FTAGS:
	    ct->execute_state = EXEC_C_FTAGS;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;

/*
 * The following state is the return-state from STRING when a search string
 * has been completely specified.
 */
	case STATE_C_SEARCH:
	    ct->execute_state = EXEC_C_SEARCH;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is the return-state from STRING when a search string
 * has been completely specified for the 'N' search command.
 */
	case STATE_C_NSEARCH:
	    ct->execute_state = EXEC_C_NSEARCH;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is the return-state from STRING when the search part
 * of an FD command has been parsed.
 */
	case STATE_C_FDCOMMAND:
	    ct->execute_state = EXEC_C_SEARCH;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    oct = ct;
	    ct = allocate_cmd_token(oct);
	    if(ct == NULL){
		oct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ct->ctx.carg = NULL;
	    ct->execute_state = EXEC_C_FDCOMMAND;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){  /* mch */
		ct = allocate_cmd_token(ct);
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is the return-state from STRING when the search part
 * of an FK command has been parsed.
 */
	case STATE_C_FKCOMMAND:
	    ct->execute_state = EXEC_C_REMEMBER_DOT;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    oct = ct;
	    ct = allocate_cmd_token(oct);
	    if(ct == NULL){
		oct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ct->execute_state = EXEC_C_SEARCH;
	    oct = ct;
	    ct = allocate_cmd_token(oct);
	    if(ct == NULL){
		oct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ct->ctx.carg = NULL;
	    ct->execute_state = EXEC_C_FKCOMMAND;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){  /* mch */
		ct = allocate_cmd_token(ct);
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */

	    return;
/*
 * The following state is a return-state from STRING when we have seen an
 * escape terminated string which in this case is the name of the file to
 * write.
 */
	case STATE_C_WRITEFILE:
	    ct->execute_state = EXEC_C_WRITEFILE;
	    ct->ctx.go_flag = NO;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is a return-state from STRING when we have seen an
 * escape terminated string which in this case is the name of the file to
 * read into the current buffer location.
 */
	case STATE_C_READFILE:
	    ct->execute_state = EXEC_C_READFILE;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is a return-state from STRING when we have seen an
 * escape terminated string which in this case is the name of the file which
 * we want to create a buffer for and load.
 */
	case STATE_C_EDITBUF:
	    ct->execute_state = EXEC_C_EDITBUF;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		oct = ct;
		ct = allocate_cmd_token(ct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is a return-state from STRING when we have seen an
 * escape terminated string which in this case is the name of the file which
 * we want to create a readonly buffer for and load.
 */
	case STATE_C_VIEWBUF:
	    ct->execute_state = EXEC_C_VIEWBUF;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		oct = ct;
		ct = allocate_cmd_token(ct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state is a return-state from STRING when we have seen an
 * escape terminated string which in this case is the command the user wants
 * to execute.
 */
	case STATE_C_ECCOMMAND:
	    ct->execute_state = EXEC_C_ECCOMMAND;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    if(ct->input_byte == ESCAPE) ct->ctx.state = STATE_C_ESCAPESEEN;
	    if(ct->ctx.flags & CTOK_M_STATUS_PASSED){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state gets entered when the user has typed ^A
 * to print out a message.
 */
	case STATE_C_MESSAGE:
	    switch(ct->input_byte){
		case CNTRL_A:
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    ct->execute_state = EXEC_C_OUTPUT_MESSAGE;
		    return;
		default:
		    ct->execute_state = EXEC_C_MESSAGE;
		    ct->ctx.state = STATE_C_MESSAGE;
		    return;
	    }/* End Switch */

/*
 * The following state is a return-state from EXPRESSION when we have seen
 * what appears to be a first argument. It stashes the argument and tests if
 * there is a second argument available (indicated by a comma).
 */
	case STATE_C_ARG1:
	    ct->ctx.iarg1_flag = YES;
	    ct->execute_state = EXEC_C_STORE1;
	    switch(ct->input_byte){
		case ',':
		    ct->ctx.state = STATE_C_EXPRESSION;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_ARG2;
		    return;

		default:
		    ct->ctx.state = STATE_C_MAINCOMMANDS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */
/*
 * Here if the first argument was followed by a comma. This indicates that a
 * second argument is being specified. Only certain commands accept a double
 * argument.
 */
	case STATE_C_ARG2:
	    ct->ctx.iarg2_flag = YES;
	    ct->execute_state = EXEC_C_STORE2;
	    switch(ct->input_byte){
		case ',':
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    error_message("?Maximum of two arguments exceeded");
		    return;

		default:
		    ct->ctx.state = STATE_C_MAINCOMMANDS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */
/*
 * The next state is the expression substate. States outside of the expression
 * parser call through here so that pnest (the current nesting level of
 * parentheses) gets set to zero. Then it calls the expression parser's own
 * internal sub-expression state.
 */
	case STATE_C_EXPRESSION:
	    ct->ctx.pnest = 0;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_SUBEXPRESSION;
	    return;
/*
 * This is the sub-expression state. It gets called from within the expression
 * parser when we want to recursively parse an expression. This lets us handle
 * precedence and parenthesis correctly.
 */
	case STATE_C_SUBEXPRESSION:
	    switch(ct->input_byte){
/*
 * When we see an open parenthesis, we want to recursively call the
 * subexpression state to parse the contents of the parenthesis. Since the open
 * parenthesis always occurs in place of an operand, we set that to be the next
 * state to call.
 */
		case '(':
		    ct->ctx.pnest += 1;
		    ct->ctx.state = STATE_C_OPERAND;

		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_OPERATOR;

		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_OPERATOR;
		    return;
/*
 * If we see a minus sign here, it is a unary minus. The difference between
 * the unary minus and the normal minus is one of precedence. The unary minus
 * is very high precedence.
 */
		case '-':
		    ct->ctx.state = STATE_C_MINUSSEEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_UMINUS;
		    return;
/*
 * Here on a leading % command. This just means he is going to default to
 * incrementing the queue register by one.
 */
		case '%':
		    ct->ctx.tmpval = 1;
		    ct->flags |= TOK_M_PARSELOAD_TMPVAL;
/* ;;; the following line shouldn't be required, but is. A bug. */
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.state = STATE_C_PERCENT_OPERAND;
		    return;
/*
 * Well, not much exciting going on here, so we just call the normal operand
 * state to parse the current token.
 */
		default:
		    ct->ctx.state = STATE_C_OPERAND;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_OPERATOR;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */
/*
 * Here if we saw a unary minus. The reason for calling this state at all is to
 * catch constructs like -L which really implies -1L. In a case like this,
 * the OPERAND state will not see anything it understands, and will return.
 * So as to make the defaulting work correctly, we catch that case here and
 * default so that we get our -1.
 */
	case STATE_C_MINUSSEEN:
	    switch(ct->input_byte){
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		case 'Q': case 'q': case '%':
		case '.':
#ifdef CLASSIC_B_BEHAVIOR
		case 'B': case 'b':
#endif
		case 'Z': case 'z':
		case '(':
		case '^':
		    ct->ctx.state = STATE_C_OPERAND;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;

		default:
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = 1;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
	    }/* End Switch */
/*
 * Here on the return from the OPERAND state. Whatever it parsed, we want to
 * make it minus.
 */
	case STATE_C_UMINUS:
	    ct->execute_state = EXEC_C_UMINUS;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_OPERATOR;
	    return;
/*
 * Here when we are expecting an operator like + = * /. Note that ')' also gets
 * handled here since it always appears in the place an operator would be
 * expected to appear.
 */
	case STATE_C_OPERATOR:
	    switch(ct->input_byte){
/*
 * Here on an a-b case. Note that since minus is a low precedence operator,
 * we stash the temporary value, and call subexpression to parse the rest
 * of the expression. In that way, if the next operator was * or /, it will
 * get processed first.
 */
		case '-':
		    ct->execute_state = EXEC_C_STORE2;
		    ct->ctx.state = STATE_C_OPERAND;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_MINUS;
		    return;
/*
 * Here on an a+b case. This is similar to the case above in that we want to
 * just stash the first value, and then recursively call the parser to handle
 * the rest of the expression first. Note that this causes a+b+c to actually
 * get handled as a+(b+c) which is a little weird, but works out ok.
 */
		case '+':
		    ct->execute_state = EXEC_C_STORE2;
		    ct->ctx.state = STATE_C_OPERAND;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_PLUS;
		    return;
/*
 * Here on the a*b case. Unlike the above two cases, we want to immediately
 * handle this case since it is the highest precedence of the four operators.
 */
		case '*':
		    ct->execute_state = EXEC_C_STORE1;
		    ct->ctx.state = STATE_C_OPERAND;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_TIMES;
		    return;
/*
 * Here on the a/b case. This is just like the multiply state
 */
		case '/':
		    ct->execute_state = EXEC_C_STORE1;
		    ct->ctx.state = STATE_C_OPERAND;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_DIVIDE;
		    return;

/*
 * Here on the n%q case. This just collapses to the value of the Q-register
 * after it has been incremented by 'n'
 */
		case '%':
		    ct->ctx.state = STATE_C_PERCENT_OPERAND;
		    return;
/*
 * Here on the 'A' command which returns a value
 */
		case 'A': case 'a':
		    ct->execute_state = EXEC_C_ACOMMAND;
		    ct->ctx.state = STATE_C_OPERATOR;
		    return;
/*
 * Here on a close parenthesis. This will force the end of a subexpression
 * parse even though it would not normally have terminated yet. Note the check
 * for the guy typing too many of these.
 */
		case ')':
		    if(ct->ctx.pnest == 0){
			error_message("?No Matching Open Parenthesis");
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.pnest -= 1;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
/*
 * If this is not an operator character, it must be the end of the expression.
 * In this case, we must be back at the zero level for parenthesis nesting.
 */
		default:
		    if(ct->ctx.pnest > 0){
			sprintf(tmp_message,"?Missing %d Close Parenthesis",
			    ct->ctx.pnest);
			error_message(tmp_message);
			ct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
	    }/* End Switch */
/*
 * Here when we have the 'b' part of an a+b expression. This happens either
 * when the end of the expression has been completed, or a parenthesis has
 * forced a temporary end as in: (a+b)
 */
	case STATE_C_PLUS:
	    switch(ct->input_byte){
		case '-':
		case '+':
		case ')':
		    ct->execute_state = EXEC_C_PLUS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_OPERATOR;
		    return;
		default:
		    ct->ctx.state = STATE_C_OPERATOR;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_DELAYED_PLUS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */

	case STATE_C_DELAYED_PLUS:
	    ct->execute_state = EXEC_C_PLUS;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_OPERATOR;
	    return;

/*
 * Here when we have the 'b' part of an a-b expression. This happens either
 * when the end of the expression has been completed, or a parenthesis has
 * forced a temporary end as in: (a-b)
 */
	case STATE_C_MINUS:
	    switch(ct->input_byte){
		case '-':
		case '+':
		case ')':
		    ct->execute_state = EXEC_C_MINUS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_OPERATOR;
		    return;
		default:
		    ct->ctx.state = STATE_C_OPERATOR;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    oct = ct;
		    ct = allocate_cmd_token(oct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->ctx.caller_token = oct;
		    ct->ctx.return_state = STATE_C_DELAYED_MINUS;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
	    }/* End Switch */

	case STATE_C_DELAYED_MINUS:
	    ct->execute_state = EXEC_C_MINUS;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_OPERATOR;
	    return;

/*
 * Here when we have both arguments to a multiply. Because multiply is a higher
 * precedence operator than +-, it happens immediately unless parenthesis get
 * involved.
 */
	case STATE_C_TIMES:
	    ct->execute_state = EXEC_C_TIMES;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_OPERATOR;
	    return;
/*
 * Here when we have both arguments to a divide. Because division is a higher
 * precedence operator than -+, it generally happens immediately. Note that we
 * check to be sure that the divisor is non-zero.
 */
	case STATE_C_DIVIDE:
	    ct->execute_state = EXEC_C_DIVIDE;
	    ct->flags &= ~TOK_M_EAT_TOKEN;
	    ct->ctx.state = STATE_C_OPERATOR;
	    return;
/*
 * Here to parse any legal TECO operand. These would include numbers and
 * commands that return values. Since open parenthesis occur in the same place
 * as operands, we also catch them here.
 */
	case STATE_C_OPERAND:
	    switch(ct->input_byte){
/*
 * If we see a numeric digit, call a substate which will continue eating bytes
 * until a non-digit is seen.
 */
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    ct->ctx.state = STATE_C_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->input_byte - '0';
		    return;
/*
 * Here if he wants to input in a different radix
 */
		case '^':
		    ct->ctx.state = STATE_C_RADIX;
		    ct->ctx.tmpval = 0;
		    return;
/*
 * Here if he is specifying the numeric value of a q-register. We have to go
 * to another state to determine which q-register he wants to access.
 */
		case 'Q': case 'q':
		    ct->ctx.state = STATE_C_QOPERAND;
		    return;
/*
 * Here when he has specified <dot>. This will return as a value the current
 * position in the edit buffer.
 */
		case '.':
		    ct->ctx.state = STATE_C_RETURN;
		    ct->execute_state = EXEC_C_DOTVALUE;
		    return;
#ifdef CLASSIC_B_BEHAVIOR
/*
 * B is a special operand which returns the address of the first character
 * in the buffer. This is currently always 0, but may change if some really
 * obscure features get put in one of these days.
 */
		case 'B': case 'b':
		    ct->ctx.state = STATE_C_RETURN;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = 0;
		    return;
#endif
/*
 * Z is a special operand which is the number of characters currently in the
 * edit buffer.
 */
		case 'Z': case 'z':
		    ct->ctx.state = STATE_C_RETURN;
		    ct->execute_state = EXEC_C_ZEEVALUE;
		    return;
/*
 * BACKSLASH with no argument actually looks in the buffer at the current
 * position and eats numeric digits until it hits a non-digit. It then
 * returns the number as a value.
 */
		case '\\':
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_BACKSLASH;
		    ct->ctx.tmpval = 10;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct = allocate_cmd_token(ct);
		    if(ct == NULL){
			oct->ctx.state = STATE_C_ERRORSTATE;
			return;
		    }/* End IF */
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->execute_state = EXEC_C_STORE1;
		    return;
/*
 * If we see a parenthesis in place of an operand, we want to parse it as
 * a complete expression of its own until a matching close parenthesis.
 */
		case '(':
		    ct->ctx.state = STATE_C_SUBEXPRESSION;
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    return;
/*
 * We really should not get here if the guy is typing good syntax, so we
 * tell him it is junk and we don't allow it.
 */
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,
			"?Unexpected character <%c> in operand",
			ct->input_byte);
		    error_message(tmp_message);
		    return;
	    }/* End Switch */
/*
 * The following is a substate to parse a number. It will continue until
 * it sees a non-digit, and then return to the calling state.
 */
	case STATE_C_NUMBER_SUBSTATE:
	    switch(ct->input_byte){
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    ct->ctx.state = STATE_C_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->ctx.tmpval * 10 + ct->input_byte - '0';
		    return;
		default:
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
	    }/* End Switch*/
/*
 * The following is a substate to parse a hex number. It will continue until
 * it sees a non hex digit, and then return to the calling state.
 */
	case STATE_C_HEX_NUMBER_SUBSTATE:
	    switch(ct->input_byte){
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval *= 16;
		    ct->ctx.tmpval += ct->input_byte - '0';
		    return;
		case 'a': case 'b': case 'c': case 'd': case 'e':
		case 'f':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval *= 16;
		    ct->ctx.tmpval += ct->input_byte - 'a' + 10;
		    return;
		case 'A': case 'B': case 'C': case 'D': case 'E':
		case 'F':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval *= 16;
		    ct->ctx.tmpval += ct->input_byte - 'A' + 10;
		    return;
		default:
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
	    }/* End Switch*/
/*
 * The following is a substate to parse a octal number. It will continue until
 * it sees a non octal digit, and then return to the calling state.
 */
	case STATE_C_OCTAL_NUMBER_SUBSTATE:
	    switch(ct->input_byte){
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		    ct->ctx.state = STATE_C_OCTAL_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval *= 8;
		    ct->ctx.tmpval += ct->input_byte - '0';
		    return;
		case '8': case '9':
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,"?Illegal octal digit <%c> in operand",
			ct->input_byte);
		    error_message(tmp_message);
		    return;
		default:
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_RETURN;
		    return;
	    }/* End Switch*/
/*
 * The following state gets the value of the specified q-register
 */
	case STATE_C_QOPERAND:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_RETURN;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_QVALUE;
	    return;
/*
 * This is just like QOPERAND except the execute state is different
 */
	case STATE_C_PERCENT_OPERAND:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_OPERATOR;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_PERCENT_VALUE;
	    return;
/*
 * Here on a input radix character
 */
	case STATE_C_RADIX:
	    switch(ct->input_byte){
		case 'X': case 'x':
		    ct->ctx.state = STATE_C_HEX_NUMBER;
		    ct->execute_state = EXEC_C_STOREVAL;
		    return;
		case 'O': case 'o':
		    ct->ctx.state = STATE_C_OCTAL_NUMBER;
		    ct->execute_state = EXEC_C_STOREVAL;
		    return;
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,"?Unknown radix ^<%c> in operand",
			ct->input_byte);
		    error_message(tmp_message);
		    return;
	    }/* End Switch */

	case STATE_C_HEX_NUMBER:
	    switch(ct->input_byte){
		case '\\':
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_BACKSLASH;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = 16;
		    return;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->input_byte - '0';
		    return;
		case 'a': case 'b': case 'c': case 'd': case 'e':
		case 'f':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->input_byte - 'a' + 10;
		    return;
		case 'A': case 'B': case 'C': case 'D': case 'E':
		case 'F':
		    ct->ctx.state = STATE_C_HEX_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->input_byte - 'A' + 10;
		    return;
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,"?Illegal hex digit <%c> in operand",
			ct->input_byte);
		    error_message(tmp_message);
		    return;
	}/* End Switch */

	case STATE_C_OCTAL_NUMBER:
	    switch(ct->input_byte){
		case '\\':
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_BACKSLASH;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = 8;
		    return;
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7':
		    ct->ctx.state = STATE_C_OCTAL_NUMBER_SUBSTATE;
		    ct->execute_state = EXEC_C_STOREVAL;
		    ct->ctx.tmpval = ct->input_byte - '0';
		    return;
		default:
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    sprintf(tmp_message,"?Illegal octal digit <%c> in operand",
			ct->input_byte);
		    error_message(tmp_message);
		    return;
	    }/* End Switch*/

/*
 * The next state is the string substate. Outside states call in through here
 * which does the initial tec_alloc of the string storage. Strings are limited
 * to PARSER_STRING_MAX bytes at this time.
 */
	case STATE_C_STRING:
	    if((ct->ctx.flags & CTOK_M_ATSIGN_SEEN) == 0){
		ct->ctx.delimeter = ESCAPE;
		ct->flags &= ~TOK_M_EAT_TOKEN;
	    }
	    else {
		ct->ctx.delimeter = ct->input_byte;
	    }
	    ct->ctx.state = STATE_C_STRING1;
	    return;

	case STATE_C_STRING1:
/*
 * First, check if this is the termination character. This would normally be
 * an escape, but could be another character if the user used the @ form.
 */
	    if( ( (ct->input_byte == ESCAPE) &&
		  (!(ct->ctx.flags & CTOK_M_ATSIGN_SEEN))
		) ||
		( (ct->ctx.flags & CTOK_M_ATSIGN_SEEN) &&
		  (ct->input_byte == ct->ctx.delimeter) &&
		  (ct->ctx.carg != NULL)
		)
	      ){
		ct->flags &= ~TOK_M_EAT_TOKEN;
		ct->ctx.state = STATE_C_RETURN;
		return;
	    }/* End IF */

/*
 * Here when we see a normal character inside of the string. Note that we
 * have to do the undo stuff to manage the string since it gets stored in
 * a regular string array rather than being spread through the command
 * tokens.
 */
	    {/* Local Block */
	    register char *cp;
	    register struct undo_token *ut;
/*
 * Get the address of the string
 */
	    cp = ct->ctx.carg;
/*
 * If there is no string allocated yet, then we have to allocate one.
 * Also, if an @ was seen, then record the delimeter character.
 */
	    if(cp == NULL){
		ct->ctx.carg = cp = tec_alloc(TYPE_C_CBUFF,PARSER_STRING_MAX);
		if(cp == NULL){
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		*cp = '\0';
		ut = allocate_undo_token(uct);
		if(ut == NULL){
		    ct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ut->opcode = UNDO_C_MEMFREE;
		ut->carg1 = cp;
	    }/* End IF */
/*
 * Each time we append a character to the string, we also have to allocate
 * an undo token which will shorten the string if the input character gets
 * rubbed out.
 */
	    ut = allocate_undo_token(uct);
	    if(ut == NULL){
		ct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ut->opcode = UNDO_C_SHORTEN_STRING;
	    ut->carg1 = cp;
	    ut->iarg1 = 0;
	    while(*cp){
		ut->iarg1 += 1;
		cp++;
	    }/* End While */
	    if(ut->iarg1 >= (PARSER_STRING_MAX-1)){
		sprintf(tmp_message,"?Exceeded maximum string length of %d",
		    PARSER_STRING_MAX-1);
		error_message(tmp_message);
		ct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
/*
 * Finally, we append the input byte to the string
 */
	    *cp++ = ct->input_byte;
	    *cp++ = '\0';
	    ct->ctx.state = STATE_C_STRING1;
	    return;
	    }/* End Local Block */
/*
 * The following state gets the Q register that will be used with the
 * G command.
 */
	case STATE_C_GQREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_GQREGISTER;
	    return;
/*
 * The following state gets the Q register that will be used with the
 * M command.
 */
	case STATE_C_MQREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_MQREGISTER;
	    return;
/*
 * The following two states implement the EQ command which reads a file into
 * the specified Q-register.
 */
	case STATE_C_EQQREGISTER1:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->execute_state = EXEC_C_STOREVAL;
	    ct->ctx.tmpval = ct->input_byte;
	    ct->ctx.state = STATE_C_STRING;

	    oct = ct;
	    ct = allocate_cmd_token(oct);
	    if(ct == NULL){
		oct->ctx.state = STATE_C_ERRORSTATE;
		return;
	    }/* End IF */
	    ct->ctx.caller_token = oct;
	    ct->ctx.return_state = STATE_C_EQQREGISTER2;
	    return;
	case STATE_C_EQQREGISTER2:
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->execute_state = EXEC_C_EQQREGISTER;
	    if(ct->ctx.flags & CTOK_M_COLON_SEEN){
		oct = ct;
		ct = allocate_cmd_token(oct);
		if(ct == NULL){
		    oct->ctx.state = STATE_C_ERRORSTATE;
		    return;
		}/* End IF */
		ct->ctx.iarg1_flag = YES; ct->ctx.iarg2_flag = NO;
		ct->execute_state = EXEC_C_STORE1;
	    }/* End IF */
	    return;
/*
 * The following state gets the Q register that will be loaded with the
 * U command.
 */
	case STATE_C_UQREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_UQREGISTER;
	    return;
/*
 * The following state gets the Q register that will be loaded with the
 * X command.
 */
	case STATE_C_XQREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_XQREGISTER;
	    return;
/*
 * The following state gets the Q register that will be loaded with the
 * previous command line in response to the '*' command.
 */
	case STATE_C_SAVECOMMAND:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_SAVECOMMAND;
	    return;
/*
 * The following state gets the Q register which will be pushed onto the
 * Q register stack.
 */
	case STATE_C_PUSH_QREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_PUSH_QREGISTER;
	    return;
/*
 * The following state gets the Q register which will be popped from the
 * Q register stack.
 */
	case STATE_C_POP_QREGISTER:
	    if(!parse_check_qname(ct,ct->input_byte)) return;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->q_register = ct->input_byte;
	    ct->execute_state = EXEC_C_POP_QREGISTER;
	    return;

/*
 * Here to insert characters. The insert state continues until an escape
 * is seen. Escapes can also be quoted if they are to be inserted into the
 * buffer. If there are many escapes to be inserted, it may be easier to use
 * the @ insert command...
 */
	case STATE_C_INSERT:
	    switch(ct->input_byte){
/*
 * Here when we see a non-quoted escape. This terminates the insert.
 */
		case ESCAPE:
		    ct->flags &= ~TOK_M_EAT_TOKEN;
		    ct->ctx.state = STATE_C_INITIALSTATE;
		    return;
/*
 * Here to enter a quote character. This lets you insert characters that would
 * normally be handled otherwise. ESCAPE is a good example of this.
 */
		case CNTRL_V:
		    ct->ctx.state = STATE_C_QUOTED_INSERT;
		    return;
/*
 * Here to handle a normal character. It will simply be inserted into the
 * edit buffer.
 */
		default:
		    ct->ctx.state = STATE_C_INSERT;
		    ct->execute_state = EXEC_C_INSERT;
		    return;
	    }/* End Switch */
/*
 * Here to insert a character directly following the quote character. If
 * this is a special character such as an escape, it will be inserted and
 * will not terminate the insert command.
 */
	case STATE_C_QUOTED_INSERT:
	    ct->ctx.state = STATE_C_INSERT;
	    ct->execute_state = EXEC_C_INSERT;
	    return;

/*
 * Here for the alternate form of insert, @I/text/
 */
	case STATE_C_ATINSERT:
	    ct->ctx.tmpval = ct->input_byte;
	    ct->execute_state = EXEC_C_STOREVAL;
	    ct->ctx.state = STATE_C_ATINSERT_PART2;
	    return;

	case STATE_C_ATINSERT_PART2:
	    if(ct->input_byte == ct->ctx.tmpval){
		ct->ctx.state = STATE_C_INITIALSTATE;
		return;
	    }/* End IF */

	    ct->ctx.state = STATE_C_ATINSERT_PART2;
	    ct->execute_state = EXEC_C_INSERT;
	    return;
/*
 * Here after seeing an equals command. This lets us test for two or three
 * in a row which output in octal and hex respectively.
 */
	case STATE_C_ONE_EQUALS:
	    if(ct->input_byte != '='){
		ct->ctx.state = STATE_C_INITIALSTATE;
		ct->flags &= ~TOK_M_EAT_TOKEN;
		return;
	    }/* End IF */
	    ct->ctx.state = STATE_C_TWO_EQUALS;
	    ct->execute_state = EXEC_C_TWO_EQUALS;
	    return;
	case STATE_C_TWO_EQUALS:
	    if(ct->input_byte != '='){
		ct->ctx.state = STATE_C_INITIALSTATE;
		ct->flags &= ~TOK_M_EAT_TOKEN;
		return;
	    }/* End IF */
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    ct->execute_state = EXEC_C_THREE_EQUALS;
	    return;

/*
 * Here when we have seen a vertical bar. We have to put a mark here so
 * that if the condition is not met, the else clause can be found.
 */
	case STATE_C_SKIP_ELSE:
	    ct->opcode = TOK_C_CONDITIONAL_ELSE;
	    ct->ctx.state = STATE_C_INITIALSTATE;
	    return;

	case STATE_C_BACKSLASH:
	    ct->ctx.state = STATE_C_RETURN;
	    ct->execute_state = EXEC_C_BACKSLASHARG;
	    return;

	default:
	    sprintf(tmp_message,"?Dispatched unknown state %d",ct->ctx.state);
	    error_message(tmp_message);
	    return;
    }/* End Switch */

}/* End Routine */



/**
 * \brief Check that the specified Q-register name is ok
 *
 * This routine is called when a parse state wants to verify that the
 * specified Q-register name is syntactically correct. It does not
 * verify that the Q-register exists, because the execute phase may just
 * not have got around to that yet.
 */
int
parse_check_qname( struct cmd_token *ct, char name )
{
char tmp_message[LINE_BUFFER_SIZE];

    PREAMBLE();

    switch(name){
	case '*':
	case '_':
	case '-':
	case '@':
	case '?':
	    return(SUCCESS);
    }/* End Switch */

    if(isalnum((int)name)) return(SUCCESS);

    sprintf(tmp_message,"?Illegal Q-register Name <%c>",name);
    error_message(tmp_message);
    ct->ctx.state = STATE_C_ERRORSTATE;
    return(FAIL);

}/* End Routine */
