/*
 * $Date: 2007/12/10 21:59:20 $
 * $Source: /cvsroot/videoteco/videoteco/tecparse.h,v $
 * $Revision: 1.1 $
 * $Locker:  $
 */

/* tecparse.h
 * Definitions for TECO parser
 * %W% (PC) %G%
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

/*
 * This structure holds the context which must be passed forward as the
 * parse progresses. We split it out of the cmd_token structure so that
 * it can be easily duplicated.
 */
struct cmd_context {
	char	state;
	char	flags;
	char	go_flag;
	char	pnest;
	char	inest;
	char	cnest;
	char	iarg1_flag;
	char	iarg2_flag;
	char	delimeter;
	int	iarg1;
	int	iarg2;
	char	*carg;
	int	tmpval;
	int	return_state;
	struct	cmd_token *caller_token;
};

/*
 * Define the structure which we use to describe commands
 */
struct cmd_token {
	char	opcode;
	char	input_byte;
	char	q_register;
	char	flags;
	int	execute_state;
	struct	cmd_token *next_token;
	struct	cmd_token *prev_token;
	struct	undo_token *undo_list;
	struct	cmd_context ctx;
};

struct undo_token {
	char	opcode;
	int	iarg1;
	int	iarg2;
	char	*carg1;
	struct	undo_token *next_token;
};

#define TRACE_C_PARSE		1
#define TRACE_C_EXEC		2
#define TRACE_C_MACRO		3

#define TOK_M_EAT_TOKEN		(1 << 0)
#define TOK_M_WORDBOUNDARY	(1 << 1)
#define TOK_M_PARSELOAD_IARG1	(1 << 2)
#define TOK_M_PARSELOAD_IARG2	(1 << 3)
#define TOK_M_PARSELOAD_TMPVAL	(1 << 4)

#define CTOK_M_COLON_SEEN	(1 << 0)
#define CTOK_M_ATSIGN_SEEN	(1 << 1)
#define CTOK_M_STATUS_PASSED	(1 << 2)

#define TOK_C_UNUSED          0 /* Value before it gets set		*/
#define TOK_C_FIRSTTOKEN      1 /* The begining of it all		*/
#define TOK_C_INPUTCHAR       2 /* An input character was recieved here	*/
#define TOK_C_COPYTOKEN       3 /* A copy of a previous command token	*/
#define TOK_C_ITERATION_BEGIN 4	/* Marks the begining of an iteration	*/
#define TOK_C_ITERATION_END   5	/* Marks the end of an iteration	*/
#define TOK_C_CONDITIONAL_END 6 /* Marks end of a conditional cmd	*/
#define TOK_C_LABEL_BEGIN     7	/* Begining of a label tag !like this!	*/
#define TOK_C_LABEL_END       8	/* End of a label tag			*/
#define TOK_C_GOTO_BEGIN      9	/* Begining of an Omumble$ string	*/
#define TOK_C_GOTO_END       10	/* End of an Omumble$ string		*/
#define TOK_C_FINALTOKEN     11	/* Like FINALSTATE			*/
#define TOK_C_INITIALSTATE   12	/* Used by ^W to find begining of cmds	*/
#define TOK_C_CONDITIONAL_ELSE 13/* Marks begining of an ELSE clause	*/

#define STATE_C_INITIALSTATE	0
#define STATE_C_MAINCOMMANDS	1
#define STATE_C_ESCAPESEEN	2
#define STATE_C_ECOMMAND	3
#define STATE_C_ARG1		4
#define STATE_C_ARG2		5
#define STATE_C_EXPRESSION	6
#define STATE_C_OPERATOR	7
#define STATE_C_PLUS		8
#define STATE_C_MINUS		9
#define STATE_C_TIMES		10
#define STATE_C_DIVIDE		11
#define STATE_C_MINUSSEEN	14
#define STATE_C_SUBEXPRESSION	15
#define STATE_C_OPERAND		16
#define STATE_C_NUMBER_SUBSTATE	17
#define STATE_C_QOPERAND	18
#define STATE_C_INSERT		19
#define STATE_C_QUOTED_INSERT	20
#define STATE_C_UMINUS		21
#define STATE_C_LABEL		22
#define STATE_C_UQREGISTER	23
#define STATE_C_WRITEFILE	24
#define STATE_C_STRING		25
#define STATE_C_STRING1		26
#define STATE_C_SEARCH		27
#define STATE_C_FCOMMAND	28
#define STATE_C_FSPART1		29
#define STATE_C_FSPART2		30
#define STATE_C_EDITBUF		31
#define STATE_C_READFILE	32
#define STATE_C_XQREGISTER	33
#define STATE_C_GQREGISTER	34
#define STATE_C_MQREGISTER	35
#define STATE_C_VIEWBUF		36
#define STATE_C_FDCOMMAND	37
#define STATE_C_CONDITIONALS	38
#define STATE_C_GOTO		39
#define STATE_C_FRPART1		40
#define STATE_C_FRPART2		41
#define STATE_C_MESSAGE		42
#define STATE_C_FKCOMMAND	43
#define STATE_C_ECCOMMAND	44
#define STATE_C_SAVECOMMAND	45
#define STATE_C_PERCENT_OPERAND	46
#define STATE_C_ATINSERT	47
#define STATE_C_ATINSERT_PART2	48
#define STATE_C_ONE_EQUALS	49
#define STATE_C_TWO_EQUALS	50
#define STATE_C_SKIP_ELSE	51
#define STATE_C_PUSH_QREGISTER	52
#define STATE_C_POP_QREGISTER	53
#define STATE_C_NSEARCH		54
#define STATE_C_ACCEPT_ARGS	55
#define STATE_C_EQQREGISTER1	56
#define STATE_C_EQQREGISTER2	57
#define STATE_C_RADIX		58
#define STATE_C_HEX_NUMBER	59
#define STATE_C_OCTAL_NUMBER	60
#define STATE_C_HEX_NUMBER_SUBSTATE	61
#define STATE_C_OCTAL_NUMBER_SUBSTATE	62
#define STATE_C_BACKSLASH	63
#define STATE_C_DELAYED_MINUS	64
#define STATE_C_DELAYED_PLUS	65
#define STATE_C_FSPART3		66
#define STATE_C_FTAGS		67


#define STATE_C_RETURN		97
#define STATE_C_FINALSTATE	98
#define STATE_C_ERRORSTATE	99

/*
 * Here we define the execution time states
 */
#define EXEC_C_NULLSTATE	0
#define EXEC_C_DOTARG1		1
#define EXEC_C_ZEEARG1		2
#define EXEC_C_HARGUMENT	3
#define EXEC_C_EXITCOMMAND	4
#define EXEC_C_UQREGISTER	5
#define EXEC_C_JUMP		8
#define EXEC_C_INSERT		9
#define EXEC_C_LINE		10
#define EXEC_C_CHAR		11
#define EXEC_C_RCHAR		12
#define EXEC_C_DELETE		13
#define EXEC_C_HVALUE		14
#define EXEC_C_DOTVALUE		15
#define EXEC_C_ZEEVALUE		16
#define EXEC_C_QVALUE		17
#define EXEC_C_EQUALS		18
#define EXEC_C_REDRAW_SCREEN	19
#define EXEC_C_STOREVAL		20
#define EXEC_C_STORE1		21
#define EXEC_C_STORE2		22
#define EXEC_C_UMINUS		23
#define EXEC_C_PLUS		24
#define EXEC_C_MINUS		25
#define EXEC_C_TIMES		26
#define EXEC_C_DIVIDE		27
#define EXEC_C_KILL		28
#define EXEC_C_WRITEFILE	29
#define EXEC_C_SEARCH		30
#define EXEC_C_SETSEARCH	31
#define EXEC_C_FSREPLACE1	32
#define EXEC_C_ITERATION_BEGIN	33
#define EXEC_C_ITERATION_END	34
#define EXEC_C_READFILE		35
#define EXEC_C_EDITBUF		36
#define EXEC_C_XQREGISTER	37
#define EXEC_C_GQREGISTER	38
#define EXEC_C_SEMICOLON	39
#define EXEC_C_MQREGISTER	40
#define EXEC_C_CLOSEBUF		41
#define EXEC_C_VIEWBUF		42
#define EXEC_C_FDCOMMAND	43
#define EXEC_C_ACOMMAND		44
#define EXEC_C_BACKSLASH	45
#define EXEC_C_BACKSLASHARG	46
#define EXEC_C_COND_GT		47
#define EXEC_C_COND_LT		48
#define EXEC_C_COND_EQ		49
#define EXEC_C_COND_NE		50
#define EXEC_C_COND_DIGIT	51
#define EXEC_C_COND_ALPHA	52
#define EXEC_C_COND_LOWER	53
#define EXEC_C_COND_UPPER	54
#define EXEC_C_COND_SYMBOL	55
#define EXEC_C_GOTO		56
#define EXEC_C_FRREPLACE	57
#define EXEC_C_MESSAGE		58
#define EXEC_C_RESET_MESSAGE	59
#define EXEC_C_OUTPUT_MESSAGE	60
#define EXEC_C_FKCOMMAND	61
#define EXEC_C_REMEMBER_DOT	62
#define EXEC_C_ECCOMMAND	63
#define EXEC_C_SAVECOMMAND	64
#define EXEC_C_SCROLL		65
#define EXEC_C_UPDATE_SCREEN	66
#define EXEC_C_SET_IMMEDIATE_MODE 67
#define EXEC_C_PERCENT_VALUE	68
#define EXEC_C_WORD		69
#define EXEC_C_TWO_EQUALS	70
#define EXEC_C_THREE_EQUALS	71
#define EXEC_C_SKIP_ELSE	72
#define EXEC_C_PUSH_QREGISTER	73
#define EXEC_C_POP_QREGISTER	74
#define	EXEC_C_NSEARCH		75
#define EXEC_C_EQQREGISTER	76
#define EXEC_C_WINDOW_CONTROL	77
#define EXEC_C_NEXT_WINDOW	78
#define EXEC_C_RLINE		79
#define EXEC_C_DELWORD		80
#define EXEC_C_RDELWORD		81
#define EXEC_C_OPENBRACE	82
#define EXEC_C_CLOSEBRACE	83
#define EXEC_C_SKIPLABEL	84
#define EXEC_C_SETOPTIONS	85
#define EXEC_C_FSREPLACE2	86
#define EXEC_C_FSREPLACE3	87
#define EXEC_C_FTAGS		88

/*
 * Define the UNDO symbols
 */
#define UNDO_C_UNUSED		0
#define UNDO_C_CHANGEDOT	1
#define UNDO_C_DELETE		2
#define UNDO_C_INSERT		3
#define UNDO_C_UQREGISTER	4
#define UNDO_C_MEMFREE		5
#define UNDO_C_SHORTEN_STRING	6
#define UNDO_C_SET_SEARCH_STRING 7
#define UNDO_C_CHANGEBUFF	8
#define UNDO_C_MACRO		9
#define UNDO_C_SHORTEN_MESSAGE	10
#define UNDO_C_BULK_INSERT	11
#define UNDO_C_SET_IMMEDIATE_MODE 12
#define UNDO_C_PUSH		13
#define UNDO_C_POP		14
#define UNDO_C_MODIFIED		15
#define UNDO_C_CLOSEBUFF	16
#define UNDO_C_WINDOW_SWITCH	17
#define UNDO_C_WINDOW_SPLIT	18
#define UNDO_C_REOPENBUFF	19
#define UNDO_C_RENAME_BUFFER	20
#define	UNDO_C_PRESERVEARGS	21
#define	UNDO_C_SETOPTIONS	22
#define UNDO_C_SET_SEARCH_GLOBALS 23
#define UNDO_C_LOAD_TAGS	24
#define UNDO_C_SELECT_TAGS	25
#define UNDO_C_SET_EXIT_FLAG	26

int cmd_oscmd(struct cmd_token *ct);
int buff_insert_from_buffer_with_undo( struct cmd_token *,
			struct buff_header *,int,struct buff_header *,int,int);
int buff_delete_with_undo( struct cmd_token *,struct buff_header *,int,int);
int buff_insert_with_undo( struct cmd_token *,
					struct buff_header *,int,char *,int);
int rename_edit_buffer(struct buff_header *,char *,struct cmd_token *);
int cmd_setoptions(int,int,struct undo_token *);
void tag_dump_database(struct tags *tp,struct cmd_token *uct);
int buff_insert_char_with_undo(struct cmd_token *,
					struct buff_header *,int,char);
int set_search_string_with_undo(char *,struct cmd_token *uct);
int cmd_tags(struct cmd_token *uct,int,int,int,char *);
int tecmacro(struct buff_header *,struct cmd_token *,struct cmd_token **);
void parser_cleanup_ctlist(struct cmd_token *);
void free_cmd_token(struct cmd_token *);
int parser_undo(struct undo_token *);
void free_undo_token(struct undo_token *);
void screen_reset_echo(struct cmd_token *);
int parse_special_character(struct cmd_token *,int);
void parse_input_character(struct cmd_token *,struct cmd_token *);
void trace_mode(int, struct cmd_token *, struct cmd_token *);
int execute_a_state(struct cmd_token *, struct cmd_token *);
void tecundo_cleanup(struct undo_token *);
int parse_any_arguments(struct cmd_token *,char *);
int parse_more_than_one_arg(struct cmd_token *,char *);
int parse_check_qname(struct cmd_token *,char);
struct cmd_token *allocate_cmd_token(struct cmd_token *old_token);
struct undo_token *allocate_undo_token(struct cmd_token *ct);
void buff_free_line_buffer(struct buff_line *);

