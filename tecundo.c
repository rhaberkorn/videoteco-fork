char *tecundo_c_version = "tecundo.c: $Revision: 1.2 $";

/*
 * $Date: 2007/12/10 22:13:08 $
 * $Source: /cvsroot/videoteco/videoteco/tecundo.c,v $
 * $Revision: 1.2 $
 * $Locker:  $
 */

/* tecundo.c
 * Subroutines to implement the undo capabilities of the parser
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

    extern struct search_buff search_string;
    extern char user_message[];
    extern char exit_flag;
    extern char immediate_execute_flag;
    extern int last_search_pos1,last_search_pos2;
    extern int last_search_status;

    extern struct buff_header *curbuf,*buffer_headers;
    extern struct buff_header *qregister_push_down_list;
    extern struct tags *current_tags;
    void tecundo_list(struct undo_token *);



/* PARSER_UNDO - Execute an UNDO command
 *
 * Function:
 *
 *	This routine is called with an undo token to reverse some changes
 *	to the program state.
 */
int
parser_undo(ut)
register struct undo_token *ut;
{
char tmp_message[LINE_BUFFER_SIZE];
register struct cmd_token *ct;

    PREAMBLE();

    switch(ut->opcode){
/*
 * CHANGEDOT is used to undo a movement of dot. iarg1 contains the buffer
 * position we want to go back to.
 */
	case UNDO_C_CHANGEDOT:
	    curbuf->dot = ut->iarg1;
	    break;
/*
 * DELETE is used to undo the addition of characters to the buffer. iarg1
 * contains the buffer position to begin deleting at, iarg2 contains the
 * number of characters that should be deleted.
 */
	case UNDO_C_DELETE:
	    if(ut->iarg2 > 0){
		buff_delete((struct buff_header *)ut->carg1,ut->iarg1,
								ut->iarg2);
	    }/* End IF */
	    break;
/*
 * INSERT is used to undo the deletion of characters from the buffer.
 * iarg1 contains the position in the buffer of the deletion. iarg2
 * contains a count of the number of bytes which were deleted. carg1
 * is a pointer to a tec_alloc'ed buffer which holds the deleted bytes.
 */
	case UNDO_C_INSERT:
	    buff_insert(curbuf,ut->iarg1,ut->carg1,ut->iarg2);
	    break;
/*
 * UQREGISTER is used to undo the loading of an integer value into a
 * Q register. iarg1 is the Q register name, iarg2 is the old value.
 */
	case UNDO_C_UQREGISTER:
	    {/* Local Block */
	    register struct buff_header *hbp;

	    hbp = buff_qfind(ut->iarg1,0);
	    if(hbp == NULL){
		sprintf(tmp_message,"?Cannot find Q-register <%c>",
		    ut->iarg1);
		error_message(tmp_message);
		return(FAIL);
	    }/* End IF */

	    hbp->ivalue = ut->iarg2;
	    break;
	    }/* End Local Block */
/*
 * MEMFREE allows us to free up memory allocated to a command token or an
 * undo token.
 */
	case UNDO_C_MEMFREE:
	    tec_release(TYPE_C_CBUFF,ut->carg1);
	    ut->carg1 = NULL;
	    return(SUCCESS);
/*
 * SHORTEN_STRING allows us to handle rubouts etc while a string was being
 * built.
 */
	case UNDO_C_SHORTEN_STRING:
	    {/* Local Block */
	    register int i;
	    register char *cp;
/*
 * IARG1 holds the length we want to shorten the string to. If the string is
 * already shorter than that, we leave it alone of course. Note that we use
 * IARG2 to hold the address of the string.
 */
	    i = ut->iarg1;
	    cp = ut->carg1;
	    while(i--){
		if(*cp) cp++;
	    }/* End While */
	    *cp = '\0';
	    return(SUCCESS);
	    }/* End Local Block */
/*
 * SET_SEARCH_STRING sets the default search string
 */
	case UNDO_C_SET_SEARCH_STRING:
	    return(SUCCESS);

/*
 * CHANGEBUFF changes the current buffer. We pass a flag to buff_openbuffnum
 * so that if we return to buffer zero, it doesn't build a new map.
 */
	case UNDO_C_CHANGEBUFF:

	    buff_openbuffnum(ut->iarg1,0);
	    return(SUCCESS);

/*
 * MACRO undoes all the changes a macro made
 */
	case UNDO_C_MACRO:
	    ct = (struct cmd_token *)ut->carg1;
	    while(ct->next_token) ct = ct->next_token;
	    while(ct){
/*
 * If this command token has an undo list associated with it, call the
 * routine which will undo the entire undo list under this token.
 */
		if(ct->undo_list){
		    tecundo_list(ct->undo_list);
		    ct->undo_list = NULL;
		}/* End IF */
/*
 * If the command token is not at the head of the list, back up one so
 * as to keep it's address while we free up this command token.
 */
		if(ct->prev_token){
		    ct = ct->prev_token;
		    free_cmd_token(ct->next_token);
		    ct->next_token = NULL;
		}/* End IF */
/*
 * Else, it IS the head of the list and we can simply free it up and break
 * out of the loop.
 */
		else {
		    free_cmd_token(ct);
		    ut->carg1 = NULL;
		    break;
		}/* End Else */
	    }/* End While */

	    return(SUCCESS);

/*
 * SHORTEN_MESSAGE handles rubouts in ^A messages
 */
	case UNDO_C_SHORTEN_MESSAGE:
	    {/* Local Block */
	    register int i;
	    i = strlen(user_message);
	    if(i > 0) user_message[i-1] = '\0';
	    return(SUCCESS);
	    }/* End Local Block */

/*
 * BULK_INSERT is used to undo a deleted region
 */
	case UNDO_C_BULK_INSERT:
	    buff_bulk_insert(curbuf,
		ut->iarg1,ut->iarg2,(struct buff_line *)ut->carg1);
	    return(SUCCESS);
/*
 * SET_IMMEDIATE_MODE resets the immediate execute state
 */
	case UNDO_C_SET_IMMEDIATE_MODE:
	    immediate_execute_flag = ut->iarg1;
	    return(SUCCESS);

/*
 * PUSH is used to re-push a Q register onto the Q register push down
 * list (stack).
 */
	case UNDO_C_PUSH:
	    push_qregister(ut->iarg1);
	    return(SUCCESS);
/*
 * POP is used to undo a Q register push. It just takes an entry off of
 * the Q register pushdown list and destroys it.
 */
	case UNDO_C_POP:
	    {/* Local Block */
	    register struct buff_header *hbp;

	    hbp = qregister_push_down_list;
	    if(hbp){
		qregister_push_down_list = hbp->next_header;
/*
 * Place it on the main buffer list so that buff_destroy can find it
 */
		hbp->next_header = buffer_headers;
		buffer_headers = hbp;
		buff_destroy(hbp);
	    }/* End IF */

	    return(SUCCESS);

	    }/* End Local Block */

/*
 * MODIFIED is used to flag a change in the state of the current edit buffers
 * modified status. We undo it by clearing the modified flag and clearing the
 * indication in the buffer status line display.
 */
	case UNDO_C_MODIFIED:
	    ((struct buff_header *)(ut->carg1))->ismodified = NO;
	    if(screen_label_line((struct buff_header *)ut->carg1,
					"",LABEL_C_MODIFIED) == FAIL){
		return(FAIL);
	    }/* End IF */
	    return(SUCCESS);
/*
 * This function undoes the creation of a buffer by the EB command
 */
	case UNDO_C_CLOSEBUFF:
	    buff_destroy((struct buff_header *)ut->carg1);
	    break;
/*
 * This function undoes a window switch
 */
	case UNDO_C_WINDOW_SWITCH:
	    window_switch(ut->iarg1);
	    break;
/*
 * This function undoes a window split
 */
	case UNDO_C_WINDOW_SPLIT:
	    window_switch(ut->iarg1);
	    screen_delete_window((struct window *)ut->carg1);
	    break;
/*
 * This function reopens a buffer which was closed with EF
 */
	case UNDO_C_REOPENBUFF:
	    buff_reopenbuff((struct buff_header *)ut->carg1);
	    break;
/*
 * This function replaces the buffer name which was changed by a rename
 * operation.
 */
	case UNDO_C_RENAME_BUFFER:
	    tec_release(TYPE_C_CBUFF,curbuf->name);
	    curbuf->name = ut->carg1;
	    break;

/*
 * This function replaces arguments which may have been overwritten during
 * a loop.
 */
	case UNDO_C_PRESERVEARGS:
	    ct = (struct cmd_token *)ut->carg1;
	    ct->ctx.iarg1 = ut->iarg1;
	    ct->ctx.iarg2 = ut->iarg2;
	    break;

/*
 * This function replaces runtime options changed by the EJ command
 */
	case UNDO_C_SETOPTIONS:
	    cmd_setoptions(ut->iarg1,ut->iarg2,NULL);
	    break;
/*
 * This function restores some globals used by the searching system
 */
	case UNDO_C_SET_SEARCH_GLOBALS:
	    last_search_pos1 = ut->iarg1;
	    last_search_pos2 = ut->iarg2;
	    last_search_status = (int)ut->carg1;
	    break;
/*
 * This function puts the previous tags file back
 */
	case UNDO_C_LOAD_TAGS:
	    tag_free_struct(current_tags);
	    current_tags = (struct tags *)ut->carg1;
	    break;

/*
 * This function puts the previous tag entry selection back
 */
	case UNDO_C_SELECT_TAGS:
	    current_tags->current_entry = (struct tagent *)ut->carg1;
	    break;

	case UNDO_C_SET_EXIT_FLAG:
	    exit_flag = 0;
	    break;

/*
 * Here when we get an undo code we don't know about. This is an internal
 * error to teco!
 */
	default:
	    sprintf(tmp_message,"?Unknown undo opcode %d",ut->opcode);
	    error_message(tmp_message);
	    return(FAIL);
    }/* End Switch */

    return(SUCCESS);

}/* End Routine */



/* ALLOCATE_UNDO_TOKEN - Allocate an undo token structure
 *
 * Function:
 *
 *	This routine is called to allocate an undo token structure. If there
 *	is one on the free list, it is used, otherwise we allocate one. UNDO
 *	tokens are used so that we can back out of operations which change
 *	some state other than that of the parser. The normal case is that we
 *	make some change to the edit buffer, and want to be able to back out
 *	of it if we want.
 */
struct undo_token *
allocate_undo_token(ct)
register struct cmd_token *ct;
{
register struct undo_token *ut;

    PREAMBLE();

    ut = (struct undo_token *)tec_alloc(TYPE_C_UNDO,sizeof(struct undo_token));
    if(ut == NULL) return(NULL);

    ut->opcode = UNDO_C_UNUSED;
    ut->iarg1 = 0;
    ut->iarg2 = 0;
    ut->carg1 = NULL;
    ut->next_token = NULL;

    if(ct != NULL){
	ut->next_token = ct->undo_list;
	ct->undo_list = ut;
    }/* End IF */

    return(ut);

}/* End Routine */



/* FREE_UNDO_TOKEN - Routine to place an undo token on the free list
 *
 * Function:
 *
 *	This routine is called with the address of the undo token
 *	to be placed on the free list.
 */
void
free_undo_token(ut)
register struct undo_token *ut;
{

    PREAMBLE();

    tec_release(TYPE_C_UNDO,(char *)ut);

}/* End Routine */



/* TECUNDO_LIST - Cause a list of undo structures to be backed out
 *
 * Function:
 *
 *	This routine is called with the head of a list of undo tokens that
 *	need to be undone. They are placed on the list such that the head
 *	of the list is the first that needs to be undone.
 */
void
tecundo_list(nut)
register struct undo_token *nut;
{
register struct undo_token *ut;

    PREAMBLE();

    while((ut = nut)){

/*
 * Remember what the next token address is and unchain it from the current
 * undo token.
 */
	nut = ut->next_token;
	ut->next_token = NULL;
/*
 * Now call the undo routine to back out the changes to the edit buffer that
 * this token calls for. Then place the token back on the free list.
 */
	parser_undo(ut);
	free_undo_token(ut);

    }/* End While */

}/* End Routine */



/* TECUNDO_CLEANUP - Place the list of undo tokens back on the free list
 *
 * Function:
 *
 *	This routine is called with the head of a list of undo tokens. It
 *	places them back on the undo free list while releasing any resources
 *	they may have allocated. For example, the MEMFREE token has to free
 *	the memory the block points to.
 */
void
tecundo_cleanup(tut)
register struct undo_token *tut;
{
register struct undo_token *ut;

    PREAMBLE();

    while((ut = tut)){
	tut = tut->next_token;
	switch(ut->opcode){
	    case UNDO_C_MEMFREE:
		if(ut->carg1) tec_release(TYPE_C_CBUFF,ut->carg1);
		ut->carg1 = NULL;
		free_undo_token(ut);
		break;
	    case UNDO_C_MACRO:
		if(ut->carg1){
		    parser_cleanup_ctlist((struct cmd_token *)ut->carg1);
		}/* End IF */
		ut->carg1 = NULL;
		free_undo_token(ut);
		break;
	    case UNDO_C_BULK_INSERT:
		buff_free_line_buffer_list((struct buff_line *)ut->carg1);
		ut->carg1 = NULL;
		free_undo_token(ut);
		break;
	    case UNDO_C_REOPENBUFF:
		{/* Local Block */
		register struct buff_header *hbp;
		hbp = (struct buff_header *)ut->carg1;
		hbp->next_header = buffer_headers;
		buffer_headers = hbp;
		buff_destroy(hbp);
		}/* End Local Block */
		free_undo_token(ut);
		break;
	    case UNDO_C_RENAME_BUFFER:
		tec_release(TYPE_C_CBUFF,ut->carg1);
		free_undo_token(ut);
		break;
	    case UNDO_C_LOAD_TAGS:
		tag_free_struct((struct tags *)ut->carg1);
		break;
	    default:
		free_undo_token(ut);
	}/* End Switch */
    }/* End While */
}/* End Routine */
