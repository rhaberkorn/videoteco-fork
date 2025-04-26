char *tecbuf_c_version = "tecbuf.c: $Revision: 1.4 $";

struct buff_header *buff_create( char *name, char internal_flag );

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/tecbuf.c,v $
 * $Revision: 1.4 $
 * $Locker:  $
 */

/**
 * \file tecbuf.c
 * \brief Subroutines to handle the edit buffers
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

char rcs_date[] = AUTO_DATE;

/*
 * Global Storage is defined here for lack of a better place.
 * First off are all the flags.
 */

/*
 * Global Variables
 */
    struct buff_header *buffer_headers;
    struct buff_header *curbuf;
    struct buff_header *qregister_push_down_list;

/*
 * Global structures
 */
    struct buff_line *line_buffer_lookaside_list = NULL;
/*
 * Forward References
 */
    struct buff_line *allocate_line_buffer(int);
    void movc3(char *,char *,int);
    void buff_buffer_map( void );
    unsigned int stringHash( char *str );

    extern FILE *teco_debug_log;
    extern int term_columns;
    extern struct window *curwin;
    extern char checkpoint_modified;

/**
 * \brief Find the named buffer
 *
 * This routine is called with the name of the buffer that we want to
 * find. It searches all the buffer headers until it finds the name.
 * It will either return the address of the buffer header, or null if
 * it could not find one with the proper name.
 */
struct buff_header *
buff_find( char *name )
{
register struct buff_header *bp;
register char *cp1,*cp2;
unsigned int hash = stringHash( name );

    PREAMBLE();
    for(bp = buffer_headers; bp != NULL; bp = bp->next_header){
	if( hash != bp->buf_hash )
	{
//	    printf("skipping because hash 0x%08x != 0x%08x %s %s\n",
//		hash, bp->buf_hash, name, bp->name);
	    continue;
	}

//	printf("hash match! %s %s\n",name,bp->name);
/*
 * This loop implements the equivalent of strcmp, except that in the
 * case of VMS and MS-DOS, it is case insensitive.
 */
	for(cp1 = name, cp2 = bp->name;;cp1++,cp2++){
#if defined(VMS) || defined(MSDOS)
	    if(UPCASE(*cp1) != UPCASE(*cp2)) break;
#else
	    if(*cp1 != *cp2) break;
#endif
	    if(*cp1 == '\0') return(bp);
	}/* End FOR */

    }/* End FOR */

    return(NULL);

}/* End Routine */



/**
 * \brief Find the named Q register
 *
 * This routine is called with the name of the Q register that we
 * want to find. It constructs the internal name of the Q register
 * and then calls buff_find to find the buffer. If the buffer is not
 * found and the create flag is set, we call buff_create to cause
 * the Q register to be created.
 */
struct buff_header *
buff_qfind( char name, char create_flag )
{
register struct buff_header *hbp;
register int i;
char tmp_buffer[LINE_BUFFER_SIZE],tmp_message[LINE_BUFFER_SIZE];
//struct buff_header *buff_create();

    PREAMBLE();
    (void) strcpy(tmp_buffer,TECO_INTERNAL_BUFFER_NAME);
    i = strlen(tmp_buffer);
    name = UPCASE((int)name);
    tmp_buffer[i] = name; tmp_buffer[i+1] = '\0';
    hbp = buff_find(tmp_buffer);
    if(hbp == NULL){
	if(create_flag){
	    hbp = buff_create(tmp_buffer,1);
	    if(hbp == (struct buff_header *)NULL){
		sprintf(tmp_message,"?Cannot create Q-register %c",name);
		error_message(tmp_message);
		return((struct buff_header *)NULL);
	    }/* End IF */
	}/* End IF */
    }/* End IF */

    return(hbp);

}/* End Routine */



/**
 * \brief Create a new buffer
 *
 * This routine is called when we need to create a new buffer. The caller
 * should have already verified that this doesn't already exist. The
 * routine returns the address of the new buffer, or NULL if there is a
 * problem of some sort.
 */
struct buff_header *
buff_create( char *name, char internal_flag )
{
register struct buff_header *bp;
register struct buff_header *obp;
register struct buff_line *lbp;
register int i = 0;

    PREAMBLE();

    if(buff_find(name)) return(NULL);

/*
 * Create the buffer header itself.
 */
    bp = (struct buff_header *)tec_alloc(
	TYPE_C_BHDR,
	sizeof(struct buff_header)
    );

    if(bp == NULL) return(NULL);

    memset(bp,0,sizeof(*bp));
    bp->buf_magic = MAGIC_BUFFER;
    bp->buf_hash = stringHash( name );

    bp->name = tec_alloc(TYPE_C_CBUFF,strlen(name)+1);
    if(bp->name == NULL){
	bp->buf_magic = 0;
	tec_release(TYPE_C_BHDR,(char *)bp);
	return(NULL);
    }/* End IF */
    (void) strcpy(bp->name,name);

    obp = buffer_headers;
    if(obp) i = obp->buffer_number;
    while(obp){
	if(internal_flag){
	    if(obp->buffer_number < i) i = obp->buffer_number;
	}
	else {
	    if(obp->buffer_number > i) i = obp->buffer_number;
	}
	obp = obp->next_header;
    }/* End While */	    

    if(internal_flag) bp->buffer_number = i - 1;
    else bp->buffer_number = i + 1;

/*
 * Create the first line buffer structure
 */
    lbp = allocate_line_buffer(1);
    if(lbp == NULL){
	bp->buf_magic = 0;
	tec_release(TYPE_C_CBUFF,bp->name);
	tec_release(TYPE_C_BHDR,(char *)bp);
	return((struct buff_header *)NULL);
    }/* End IF */
    bp->first_line = lbp;

/*
 * Link it into the list of buffer headers, in order by buffer number
 */
    obp = buffer_headers;
    while(obp){
	if(obp->next_header == NULL) break;
	if(obp->next_header->buffer_number > bp->buffer_number) break;
	obp = obp->next_header;
    }/* End While */

    if(obp == NULL || buffer_headers->buffer_number > bp->buffer_number){
	bp->next_header = buffer_headers;
	buffer_headers = bp;
    }/* End IF */

    else {
	bp->next_header = obp->next_header;
	obp->next_header = bp;
    }/* End Else */

    return(bp);

}/* End Routine */



/**
 * \brief Make a duplicate of a buffer
 *
 * This routine is called to duplicate the specified buffer and return
 * a pointer to the duplicate. The current requirement for this is for
 * the push Q register command ('[').
 */
struct buff_header *
buff_duplicate( struct buff_header *sbp )
{
register struct buff_header *dbp;
register struct buff_line *slp;
register struct buff_line *dlp;

    PREAMBLE();
/*
 * Create the buffer header itself.
 */
    dbp = (struct buff_header *)
	tec_alloc(TYPE_C_BHDR,sizeof(struct buff_header));
    if(dbp == NULL){
	return((struct buff_header *)NULL);
    }/* End IF */

    dbp->name = tec_alloc(TYPE_C_CBUFF,strlen(sbp->name)+1);
    if(dbp->name == NULL){
	tec_release(TYPE_C_BHDR,(char *)dbp);
	return((struct buff_header *)NULL);
    }/* End IF */

    (void) strcpy(dbp->name,sbp->name);

    dbp->buf_magic = MAGIC_BUFFER;
    dbp->ismodified = sbp->ismodified;
    dbp->isreadonly = sbp->isreadonly;
    dbp->isbackedup = sbp->isbackedup;
    dbp->dot = sbp->dot;
    dbp->zee = sbp->zee;
    dbp->ivalue = sbp->ivalue;
    dbp->pos_cache.lbp = NULL;
    dbp->pos_cache.base = 0;
    dbp->first_line = NULL;

/*
 * Copy the line buffer structures. If an allocation error occurs, we
 * have to return all the allocated memory, and return an error.
 */
    slp = sbp->first_line;

    if(slp){
	dbp->first_line = dlp = allocate_line_buffer(slp->byte_count);
	if(dlp == NULL){
	    buff_destroy(dbp);
	    return((struct buff_header *)NULL);
	}/* End IF */
	movc3(slp->buffer,dlp->buffer,slp->byte_count);
	dlp->byte_count = slp->byte_count;

	while((slp = slp->next_line)){
	    dlp->next_line = allocate_line_buffer(slp->byte_count);
	    if(dlp->next_line == NULL){
		buff_destroy(dbp);
		return((struct buff_header *)NULL);
	    }/* End IF */
	    dlp->next_line->prev_line = dlp;
	    dlp = dlp->next_line;
	    movc3(slp->buffer,dlp->buffer,slp->byte_count);
	    dlp->byte_count = slp->byte_count;
	}/* End While */

    }/* End IF */

    return(dbp);

}/* End Routine */



/**
 * \brief Copy the specified number of bytes
 *
 * This routine copies 'n' bytes from the source to the
 * destination.
 */
void
movc3( char *source, char *dest, int count )
{

    PREAMBLE();
    while(count-- > 0) *dest++ = *source++;

}/* End Routine */



/**
 * \brief Delete an existing buffer
 *
 * This routine is called to delete an existing buffer. The routine is
 * supplied the address of the buffer structure. We have to clean up
 * all the lines in the buffer, all the display lines, etc.
 */
void
buff_destroy( struct buff_header *hbp )
{
register struct buff_header *bp;
register struct buff_line *lbp;

    PREAMBLE();
/*
 * There are certain buffers we don't allow to be destroyed
 */
    if(strcmp(hbp->name,"TECO_INTERNAL-Main") == 0){
	return;
    }/* End IF */
/*
 * Clean up all the data in the buffer first
 */
    lbp = hbp->first_line;
    hbp->pos_cache.lbp = NULL;
    hbp->first_line = NULL;
    buff_free_line_buffer_list(lbp);

/*
 * Now unlink this buffer header from the header list.
 * Check to see if it is at the head of the list.
 */
    bp = buffer_headers;
    if(bp == hbp){
	buffer_headers = bp->next_header;
    }/* End IF */

/*
 * If not at the head of the list, we have to search the list till
 * we find it's father. Then we make it's father's child be it's
 * child.
 */
    else {
	while(bp){
	    if(bp->next_header == hbp){
		bp->next_header =  hbp->next_header;
		break;
	    }/* End IF */
	    bp = bp->next_header;
	}/* End While */

    }/* End Else */

/*
 * Now give back the storage for the name, and for the structure itself.
 */
    if(hbp->name) tec_release(TYPE_C_CBUFF,(char *)hbp->name);
    tec_release(TYPE_C_BHDR,(char *)hbp);

    return;

}/* End Routine */



/**
 * \brief Find the line structure that \a position is on
 *
 * This routine is called to find the \p line_buffer structure that
 * the buffer position resides on.
 */
struct buff_line *
buff_find_line( struct buff_header *hbp, unsigned long position )
{
register struct buff_line *lbp;
register unsigned long i;

    PREAMBLE();

    if(hbp == NULL) return(NULL);

/*
 * First step is to see if the current position cache in the buffer header will
 * help us out. Our position must be on this line if it is to be of help to us.
 */
    lbp = hbp->pos_cache.lbp;
    if(lbp){

	i = hbp->pos_cache.base + lbp->byte_count;
/*
 * Test to see if it is a currently existing character position that is mapped
 * by our position cache.
 */
	if(position >= hbp->pos_cache.base && position < i){
	    return(lbp);
	}/* End IF */
/*
 * If we are inserting at the end of the buffer, the position doesn't really
 * exist, but we can still resolve it if we happen to have the final line
 * cached. The only case where we don't want to do this is when the line
 * already has a trailing newline. Then we want to fall down into the code
 * below which will recognize this and construct a new line buffer.
 */
	if(position == hbp->zee && position == i){
	    if(lbp->byte_count == 0) return(lbp);
	    if(lbp->buffer[lbp->byte_count - 1] != '\n') return(lbp);
	}/* End IF */

    }/* End IF */

/*
 * We get here if the current cached location does not help us out. In this
 * case we need to hunt till we find the correct place in the buffer. We also
 * update our cache to remember this.
 */
    i = position;
    lbp = hbp->first_line;

    if(lbp == NULL){
	return(NULL);
    }/* End IF */

/*
 * Now, even though the cache did not give us the correct line, it can still
 * help us out in the search if it maps a position before the one we are
 * looking for, since we can begin our search at it's base.
 */
    if(hbp->pos_cache.lbp && hbp->pos_cache.base < position){
	i -= hbp->pos_cache.base;
	lbp = hbp->pos_cache.lbp;
    }/* End IF */

    while(1){
	if(lbp->byte_count >= i) break;
	i -= lbp->byte_count;
	if(lbp->next_line == NULL){
	    char panic_string[LINE_BUFFER_SIZE];
	    sprintf(panic_string,
		"buff_find_line: position %lu specified in buffer %s, z=%lu",
		position,hbp->name,hbp->zee);
	    tec_panic(panic_string);
	    printf("NULL ptr %lu bytes still to go\n",i);
	}/* End IF */
	lbp = lbp->next_line;
    }/* End While */

    if(i == lbp->byte_count && i && lbp->buffer[i-1] == '\n'){
	i = 0;
	if(lbp->next_line == NULL){
	    lbp->next_line = allocate_line_buffer(1);
	    if(lbp->next_line == NULL) return((struct buff_line *)NULL);
	    lbp->next_line->prev_line = lbp;
	}/* End IF */
	lbp = lbp->next_line;
    }/* End IF */

    hbp->pos_cache.lbp = lbp;
    hbp->pos_cache.base = position - i;

    return(lbp);

}/* End Routine */



/**
 * \brief Find offset onto line structure that \a position is on
 *
 * This routine is called to find the offset into the \p line_buffer of
 * the specified position.
 */
int
buff_find_offset(	struct buff_header *hbp, 
					struct buff_line *lbp, long position )
{

    PREAMBLE();
    if(hbp == NULL) return(-1);
/*
 * First step is to see if the current position cache in the buffer header will
 * help us out. Our position must be on this line if it is to be of help to us.
 */
    if( lbp == NULL || hbp->pos_cache.lbp != lbp){
	lbp = buff_find_line(hbp,position);
	if(lbp == NULL) return(-1);
    }/* End IF */

/*
 * Now we simply calculate the position's offset from the line structure base.
 */
    return( position - hbp->pos_cache.base );

}/* End Routine */



/**
 * \brief Return the character at the specified position
 *
 * This routine is called to fetch the single character which is at
 * the specified buffer position. This routine is generally called
 * by routines which are low frequence or only handle a small number
 * of characters at any one time.
 */
int
buff_contents( struct buff_header *hbp, long position )
{
register struct buff_line *lbp;
register int i;

    PREAMBLE();
/*
 * Insure that the specified position is legal
 */
    if(position > hbp->zee || position < 0){
	char panic_string[LINE_BUFFER_SIZE];
	sprintf(panic_string,
	    "buff_contents: illegal position %ld specified in buffer %s",
	    position,hbp->name);
	tec_panic(panic_string);
    }/* End IF */

/*
 * Call buff_find_line to find the line structure that the specified
 * position resides on.
 */
    lbp = buff_find_line(hbp,position);
    i = buff_find_offset(hbp,lbp,position);
    if(i == -1) return(-1);

    return(lbp->buffer[i] & 0xFF);

}/* End Routine */

/**
 * \brief Return the character and positional information
 *
 * This routine is called to fetch the single character which is at
 * the specified buffer position. This routine is generally called
 * by routines which are low frequence or only handle a small number
 * of characters at any one time.
 */
int
buff_cached_contents(	struct buff_header *hbp, 
						unsigned long position, struct position_cache *cache )
{
register struct buff_line *lbp;
register int i;

    PREAMBLE();
/*
 * Insure that the specified position is legal
 */
    if(position > hbp->zee || position < 0){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(tmp_message,"buff_contents: illegal position %lu %s Z = %lu\n",
	    position,hbp->name,hbp->zee);
	tec_panic(tmp_message);
    }/* End IF */

/*
 * Call buff_find_line to find the line structure that the specified
 * position resides on.
 */
    lbp = buff_find_line(hbp,position);
    if(lbp == NULL) return(-1);
    i = buff_find_offset(hbp,lbp,position);
    if(i == -1) return(-1);
/*
 * If he has specified a local position cache, copy that information back to
 * his local cache. It's up to him to invalidate it if the buffer gets changed.
 */
    if(cache){
	cache->lbp = lbp;
	cache->base = i;
    }/* End IF */

    return(lbp->buffer[i] & 0xFF);

}/* End Routine */



/**
 * \brief Routine to initialize the buffer routines
 *
 * This routine is called before the first buffer operations to
 * initialize the buffer routines.
 */
int
buff_init()
{
    register struct buff_header *hbp;
    char tmp_buffer[LINE_BUFFER_SIZE];

    PREAMBLE();
/*
 * Create the buffer map buffer
 */
    (void) strcpy(tmp_buffer,TECO_INTERNAL_BUFFER_NAME);
    (void) strcat(tmp_buffer,"BUFFER-MAP");
    hbp = buff_create(tmp_buffer,1);
    if(hbp == NULL) return(FAIL);
    hbp->buffer_number = 0;

/*
 * Create a default edit buffer
 */
    (void) strcpy(tmp_buffer,TECO_INTERNAL_BUFFER_NAME);
    (void) strcat(tmp_buffer,"Main");
    curbuf = hbp = buff_create(tmp_buffer,1);
    if(hbp == NULL) return(FAIL);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Routine to read a file into a TECO edit buffer
 *
 * This routine is called to load a file into the named buffer. If the
 * buffer already exists, we simply switch to it and don't modify it.
 */
int
buff_openbuffer(	char *name, int buffer_number, int readonly_flag )
{
    register struct buff_header *hbp = NULL;

    PREAMBLE();
/*
 * If no name is supplied then the buffer_number argument is significant,
 * otherwise we ignore it.
 */
    if(name == NULL){
	hbp = buffer_headers;
	while(hbp){
	    if(hbp->buffer_number == buffer_number){
		name = hbp->name;
		break;
	    }/* End IF */
	    hbp = hbp->next_header;
	}/* End While */
    }/* End IF */
/*
 * If no name is supplied, the buffer_number argument must already exist
 * or an error is declared.
 */
    if(name == NULL){
	error_message("?No such buffer number exists");
	return(FAIL);
    }/* End IF */
/*
 * First determine if a buffer of that name already exists
 */
    if(hbp == NULL){
	hbp = buff_find(name);
    }/* End IF */

    if(hbp){
	buff_switch(hbp,1);
 	return(SUCCESS);
    }/* End IF */

/*
 * If there is not such buffer in existence, we need to create one
 */
    hbp = buff_create(name,0);
    if(hbp == NULL) return(FAIL);
    if(readonly_flag == YES) hbp->isreadonly = YES;

/*
 * Ignore possible open error, since he may be creating a new file
 */
    buff_read(hbp,name);

    hbp->dot = 0;
    hbp->ismodified = NO;

    buff_switch(hbp,1);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Routine to open buffer number 'n'
 *
 * This routine is called with the number of a buffer which is to be
 * made the 'current' edit buffer.
 */
int
buff_openbuffnum( int buffer_number, int map_flag )
{
    register struct buff_header *hbp;

    PREAMBLE();
/*
 * Search the list of buffers for the specified buffer number
 */
    hbp = buffer_headers;
    while(hbp){
	if(hbp->buffer_number == buffer_number){
	    buff_switch(hbp,map_flag);
	    return(SUCCESS);
	}/* End IF */
	hbp = hbp->next_header;
    }/* End While */

    return(FAIL);

}/* End Routine */



/**
 * \brief Called by undo to re-create an edit buffer
 *
 * This routine is called to place a buffer back onto the buffer
 * list.
 */
void
buff_reopenbuff( struct buff_header *bp )
{
register struct buff_header *obp;

    PREAMBLE();
/*
 * Make sure there isn't another buffer of this name already here. The
 * way this can happen is automatically created Q-registers like the
 * search string and rubout string Q-registers might be created. Since
 * they don't get undone, they would stick around and then we might
 * try to undo the removal of an earlier copy of one of them. This way,
 * we make sure that any earlier copy replaces any more recent copy.
 */
    obp = buffer_headers;
    while(obp){
	if(strcmp(obp->name,bp->name) == 0){
	    buff_destroy(obp);
	    break;
	}/* End IF */
	obp = obp->next_header;
    }/* End While */
/*
 * Insert this buffer in the proper place on the buffer list.
 */
    obp = buffer_headers;
    while(obp){
	if(obp->next_header == NULL) break;
	if(obp->next_header->buffer_number > bp->buffer_number) break;
	obp = obp->next_header;
    }/* End While */

    if(obp == NULL || buffer_headers->buffer_number > bp->buffer_number){
	bp->next_header = buffer_headers;
	buffer_headers = bp;
    }/* End IF */

    else {
	bp->next_header = obp->next_header;
	obp->next_header = bp;
    }/* End Else */

}/* End Routine */



/**
 * \brief Routine to read a file into an existing TECO edit buffer
 *
 * This routine is called to read a file into the specified edit buffer.
 * The edit buffer must already exist.
 */
int
buff_read( struct buff_header *hbp, char *name )
{
    int iochan;
    char tmp_message[LINE_BUFFER_SIZE];
    int status;

    PREAMBLE();
/*
 * Attempt to open the file
 */
    iochan = open(name,O_RDONLY);
    if(iochan < 0){
	sprintf(tmp_message,"?Cannot open <%s>: %s",name,error_text(errno));
	error_message(tmp_message);
	return(FAIL);
    }/* End IF */

    status = buff_readfd(hbp,name,iochan);
    close(iochan);
    return(status);

}/* End Routine */



/**
 * \brief Reads the file descriptor into the specified buffer
 *
 * This routine is called with a buffer and a file descriptor. The
 * entire contents of the file descriptor are read into the specified
 * buffer.
 */
int
buff_readfd( struct buff_header *hbp, char *name, int iochan )
{
    char iobuf[IO_BUFFER_SIZE];
    char linebuf[IO_BUFFER_SIZE];
    int linebuf_cnt;
    char tmp_message[LINE_BUFFER_SIZE];
    register int bcount;
    register char *cp,*iop = 0;
    register int i;
    register struct buff_line *lbp;
    struct buff_line *olbp;
    int original_zee = hbp->zee;
    int error_status = SUCCESS;

    PREAMBLE();
/*
 * Read the file and insert the characters into the buffer
 */
    cp = linebuf;
    linebuf_cnt = bcount = 0;

    while(1){
	if(bcount <= 0){
	    bcount = read(iochan,iobuf,sizeof(iobuf));
	    iop = iobuf;
	    if(bcount == 0) break;
	    if(bcount < 0){
		sprintf(tmp_message,"?Error reading <%s>: %s",
		  name,error_text(errno));
		error_message(tmp_message);
		error_status = FAIL;
		break;
	    }/* End IF */
	}/* End IF */

	linebuf_cnt += 1;
	bcount -= 1;
	if((*cp++ = *iop++) == '\n' || (unsigned)linebuf_cnt >= sizeof(linebuf)){
	    lbp = buff_find_line(hbp,hbp->dot);
	    if(lbp == NULL){
		error_status = FAIL;
		break;
	    }/* End IF */
	    i = buff_find_offset(hbp,lbp,hbp->dot);
	    if(i == -1){
		error_status = FAIL;
		break;
	    }/* End IF */
	    if(i == 0){
		olbp = lbp;
		lbp = allocate_line_buffer(linebuf_cnt);
		if(lbp == NULL){
		    error_status = FAIL;
		    break;
		}/* End IF */

		lbp->next_line = olbp;
		lbp->prev_line = olbp->prev_line;
		olbp->prev_line = lbp;
		if(lbp->prev_line) lbp->prev_line->next_line = lbp;
		if(hbp->first_line == olbp) hbp->first_line = lbp;
		movc3(linebuf,lbp->buffer,linebuf_cnt);
		lbp->byte_count = linebuf_cnt;
		hbp->pos_cache.lbp = lbp;
		hbp->pos_cache.base = hbp->dot;
		hbp->dot += linebuf_cnt;
		hbp->zee += linebuf_cnt;
	    }/* End IF */

	    else {
		if(buff_insert(hbp,hbp->dot,linebuf,linebuf_cnt) == FAIL){
		    error_status = FAIL;
		    break;
		}/* End IF */
	    }/* End Else */

	    cp = linebuf;
	    linebuf_cnt = 0;
	    continue;
	}/* End IF */

    }/* End While */

    if(error_status == SUCCESS && linebuf_cnt != 0){
	if(buff_insert(hbp,hbp->dot,linebuf,linebuf_cnt) == FAIL){
	    error_status = FAIL;
	}/* End IF */
    }/* End IF */

/*
 * If the buffer just became modified for the first time, we need to change the
 * label line to reflect this.
 */
    if(hbp->ismodified == NO && hbp->zee != original_zee){
	if(hbp == curbuf){
	    if(screen_label_line(hbp,"(modified)",LABEL_C_MODIFIED) == FAIL){
		error_status = FAIL;
	    }/* End IF */
	}/* End IF */
	hbp->ismodified = YES;
    }/* End IF */

    checkpoint_modified = YES;

    return(error_status);

}/* End Routine */



/**
 * \brief Write the specified buffer out to a file
 *
 * This routine is generally called on an EW command when the user
 * wishes to write out the contents of the buffer.
 */
int
buff_write( struct buff_header *hbp, int chan, unsigned long start, unsigned long end )
{
register int bcount;
register struct buff_line *lbp;
register char *cp1,*cp2;
register int line_bcount;
char iobuf[IO_BUFFER_SIZE];
int status;

    PREAMBLE();

    bcount = 0;
    cp2 = iobuf;

    lbp = buff_find_line(hbp,start);
    line_bcount = buff_find_offset(hbp,lbp,start);
    cp1 = lbp->buffer + line_bcount;
    line_bcount = lbp->byte_count - line_bcount;

    while(start < end){
	if(bcount == sizeof(iobuf)){
	    status = write(chan,iobuf,sizeof(iobuf));
	    if(status != sizeof(iobuf)){
	    	/* NOTE: status *should* be -1, errno *should* be set */
		return(FAIL);
	    }/* End IF */
	    bcount = 0;
	    cp2 = iobuf;
	}/* End IF */

	if(line_bcount == 0){
	    lbp = lbp->next_line;
	    cp1 = lbp->buffer;
	    line_bcount = lbp->byte_count;
	}/* End IF */

	*cp2++ = *cp1++;
	bcount += 1;
	line_bcount -= 1;
        start += 1;

    }/* End While */

    if(bcount > 0){
	status = write(chan,iobuf,(unsigned)bcount);
	if(status != bcount){
	    /* NOTE: status *should* be -1, errno *should* be set */
	    return(FAIL);
	}/* End IF */
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Switch the current edit buffer
 *
 * This routine is called when we wish to switch the current buffer
 * to some other edit buffer.
 */
int
buff_switch( struct buff_header *hbp, int map_flag )
{
char tmp_message[LINE_BUFFER_SIZE];
char *cp;

    PREAMBLE();

    curbuf = curwin->win_buffer = hbp;

    sprintf(tmp_message,"<Buffer %d> %s",hbp->buffer_number,hbp->name);
    screen_label_line(hbp,tmp_message,LABEL_C_FILENAME);

    cp = "";
    if(hbp->isreadonly) cp = "(READONLY)";
    screen_label_line(hbp,cp,LABEL_C_READONLY);

    cp = "";
    if(hbp->ismodified) cp = "(modified)";
    screen_label_line(hbp,cp,LABEL_C_MODIFIED);

    if(hbp->buffer_number == 0 && map_flag){
	buff_buffer_map();
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Build a map of the existing buffers.
 *
 * This routine is called when the buffer map buffer is entered. It is
 * used to fill this routine with the appropriate text.
 */
void
buff_buffer_map()
{
register struct buff_header *hbp;
char tmp_buffer[LINE_BUFFER_SIZE],padd_buffer[LINE_BUFFER_SIZE];
register int i;
int count;
register char *cp;
int max_length;

    PREAMBLE();

    buff_delete(curbuf,0,curbuf->zee);

    for(i = 0; (unsigned)i < sizeof(padd_buffer); i++){
	padd_buffer[i] = ' ';
    }/* End FOR */
    padd_buffer[sizeof(padd_buffer)-1] = '\0';

    max_length = 0;
    for(hbp = buffer_headers; hbp != NULL ; hbp = hbp->next_header){
	if(hbp->buffer_number == 0) continue;
	i = strlen(hbp->name);
	if(i > max_length) max_length = i;
    }/* End FOR */

    for(hbp = buffer_headers; hbp != NULL ; hbp = hbp->next_header){
	if(hbp->buffer_number <= 0) continue;
	i = max_length - strlen(hbp->name);
	snprintf(tmp_buffer,sizeof(tmp_buffer),"<Buffer %-4d> %s%s %s %6lu bytes\n",
	    hbp->buffer_number,hbp->name,&padd_buffer[sizeof(padd_buffer)-1-i],
	    hbp->ismodified ? "(modified)" : "          ",hbp->zee);
	i = strlen(tmp_buffer);
	buff_insert(curbuf,curbuf->dot,tmp_buffer,i);
    }/* End FOR */

    buff_insert(curbuf,curbuf->dot,"\n",1);

    for(hbp = buffer_headers; hbp != NULL ; hbp = hbp->next_header){
	if(hbp->buffer_number >= 0) continue;
	i = max_length - strlen(hbp->name);
	snprintf(tmp_buffer,sizeof(tmp_buffer),"<Buffer %-4d> %s%s %s %6lu bytes\n",
	    hbp->buffer_number,hbp->name,&padd_buffer[sizeof(padd_buffer)-1-i],
	    hbp->ismodified ? "(modified)" : "          ",hbp->zee);
	i = strlen(tmp_buffer);
	buff_insert(curbuf,curbuf->dot,tmp_buffer,i);
    }/* End FOR */

/*
 * Here to tell him if any q registers are currently on the push down
 * list.
 */
    i = count = 0;
    for(hbp = qregister_push_down_list; hbp != NULL ; hbp = hbp->next_header){
	i += 1;
	count += hbp->zee;
    }/* End FOR */

    if(i > 0){
	sprintf(tmp_buffer,
	    "\nQ register push down list contains %d register%s, %d bytes\n",
	    i,(i > 1)?"s":"",count);
	buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));
    }/* End IF */

/*
 * Insert our TITLE
 */
    max_length = term_columns <= 80 ? term_columns : 80;

    buff_insert(curbuf,curbuf->dot,"\n\n",2);
    (void) strcpy(tmp_buffer,"Video TECO");
    i = (max_length - strlen(tmp_buffer))/2;
    if(i < 0) i = 0;
    (void) strcat(tmp_buffer,"\n");
    buff_insert(curbuf,curbuf->dot,padd_buffer,i);
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    (void) strcpy(tmp_buffer," by Paul Cantrell & Joyce Nishinaga");
    i = (max_length - strlen(tmp_buffer))/2;
    if(i < 0) i = 0;
    (void) strcat(tmp_buffer,"\n");
    buff_insert(curbuf,curbuf->dot,padd_buffer,i);
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

#if 0
    (void) strcpy(tmp_buffer,id_string);
    if(tmp_buffer[0] == '%'){
	tmp_buffer[0] = '\0';
	cp = rcs_id;
	i = 0;
	if(*cp == '$'){
	    cp++;
	    while(cp[i] == "Revision: "[i]) i += 1;
	    cp += i;
	}/* End IF */
	i = 0;
	while(*cp != '$' && *cp != '\0'){
	    tmp_buffer[i++] = *cp++;
	}/* End While */
	tmp_buffer[i] = '\0';
	if(i <= 2) sprintf(tmp_buffer,"*%d.%d* ",VMAJOR,VMINOR);

	cp = rcs_date;
	i = 0;
	if(*cp == '$'){
	    cp++;
	    while(cp[i] == "Date: "[i]) i += 1;
	    cp += i;
	}/* End IF */
	i = strlen(tmp_buffer);
	while(*cp != '$' && *cp != '\0'){
	    tmp_buffer[i++] = *cp++;
	}/* End While */
	tmp_buffer[i] = '\0';

    }/* End IF */
#endif
    sprintf(tmp_buffer,"*%d.%d* ",VMAJOR,VMINOR);
    cp = rcs_date;
    i = 0;
    if(*cp == '$'){
	cp++;
	while(cp[i] == "Date: "[i]) i += 1;
	cp += i;
    }/* End IF */
    i = strlen(tmp_buffer);
    while(*cp != '$' && *cp != '\0'){
	tmp_buffer[i++] = *cp++;
    }/* End While */
    tmp_buffer[i] = '\0';

    i = (max_length - strlen(tmp_buffer))/2;
    if(i < 0) i = 0;
    (void) strcat(tmp_buffer,"\n");
    buff_insert(curbuf,curbuf->dot,padd_buffer,i);
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    tecmem_stats();

    curbuf->dot = 0;
    return;

}/* End Routine */



/**
 * \brief Insert a string at current position
 *
 * This routine is called to insert a string into the specified buffer
 * at the buffer's current location.
 */
int
buff_insert( struct buff_header *hbp, unsigned long position, char *buffer, unsigned long length )
{
struct buff_header fake_header;
struct buff_line *fake_line;
register char *cp;

    PREAMBLE();

    fake_line =
	(struct buff_line *)tec_alloc(TYPE_C_LINE,sizeof(struct buff_line));

    if(fake_line == NULL) return(FAIL);

    memset(&fake_header,0,sizeof(fake_header));
    fake_header.pos_cache.lbp = fake_line;
    fake_header.first_line = fake_line;

    memset(fake_line,0,sizeof(*fake_line));
    fake_line->buffer_size = LINE_BUFFER_SIZE;
    fake_line->buffer = buffer;

    while(length){
	cp = fake_line->buffer;
	fake_line->byte_count = 0;
	while(1){
	    fake_line->byte_count += 1;
	    length -= 1;
	    if(*cp == '\n' || length == 0){
		buff_insert_from_buffer_with_undo(
		    NULL,
		    hbp,
		    position,
		    &fake_header,
		    0,
		    fake_line->byte_count
		);
/*
 * The following is a terrible kludge. The buff_find_line code ends up
 * inserting a null line buffer after our fake line, in order to
 * represent the location following the last character. If we don't
 * free it, it will get lost and consume memory...
 */
		if(fake_line->next_line){
		    buff_free_line_buffer(fake_line->next_line);
		    fake_line->next_line = NULL;
		}/* End IF */
		fake_line->buffer += fake_line->byte_count;
		position += fake_line->byte_count;
		break;
	    }/* End IF */
	    cp++;
	}/* End While */
    }/* End While */

    tec_release(TYPE_C_LINE,(char *)fake_line);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Copy bytes from another buffer
 *
 * This routine copies bytes from one buffer to another, arranging
 * undo capability so that we can reverse the process. Because it
 * knows the lengths of each line, this should be a very efficient
 * method of inserting data, and should certainly have preference
 * over using the byte-at-a-time routines.
 */
int
buff_insert_from_buffer_with_undo(	struct cmd_token *ct,
									struct buff_header *dbp,
									unsigned long dest_position,
									struct buff_header *sbp,
									unsigned long src_position,
									size_t length )
{
register int i;
struct undo_token *ut = 0;
struct buff_line *dlbp,*sb_lbp,*se_lbp,*nlbp;
int doff,sb_off,se_off;
int newline_included_in_src_data;
int bytes_to_copy_from_source;
int bytes_required_for_line,bytes_to_allocate;
char *new_buffer;
register char *dcp,*scp;
int bytes_inserted_so_far = 0;

#ifdef DEBUG
char outbuf[1024];
    term_goto(0,0);
    term_clrtobot();
    term_flush();
#endif /* DEBUG */

    PREAMBLE();
/*
 * Insure that the specified position is legal
 */
    if(src_position > sbp->zee){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(
	    tmp_message,
	    "buff_insert_from_buffer_with_undo: bad src pos %lu %s Z = %lu\n",
	    src_position,
	    sbp->name,sbp->zee
	);
	tec_panic(tmp_message);
    }/* End IF */
    if(dest_position > dbp->zee){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(
	    tmp_message,
	    "buff_insert_from_buffer_with_undo: bad dest pos %lu %s Z = %lu\n",
	    dest_position,
	    dbp->name,dbp->zee
	);
	tec_panic(tmp_message);
    }/* End IF */

    if(ct){
	ut = allocate_undo_token(ct);
	if(ut == NULL) return(FAIL);
    }/* End IF */

/*
 * Determine which line the current insert point is on by calling the
 * find routines.
 */
    dlbp = buff_find_line(dbp,dest_position);
    doff = buff_find_offset(dbp,dlbp,dest_position);

    sb_lbp = buff_find_line(sbp,src_position);
    sb_off = buff_find_offset(sbp,sb_lbp,src_position);

    se_lbp = buff_find_line(sbp,src_position+length);
    se_off = buff_find_offset(sbp,se_lbp,src_position+length);

#ifdef DEBUG
    movc3(dlbp->buffer,outbuf,dlbp->byte_count);
    outbuf[dlbp->byte_count] = '\0';
    if(outbuf[dlbp->byte_count-1] == '\n') outbuf[dlbp->byte_count-1] = '\0';
    fprintf(stderr,"'%s' dest begin doff %d\n",outbuf,doff);
    for(i = 0; i < sizeof(outbuf); i++) outbuf[i] = ' ';
    outbuf[doff+1] = '^'; outbuf[doff+2] = '\0';
    fprintf(stderr,"%s\n",outbuf);

    movc3(sb_lbp->buffer,outbuf,sb_lbp->byte_count);
    outbuf[sb_lbp->byte_count] = '\0';
    if(outbuf[sb_lbp->byte_count-1] == '\n'){
	outbuf[sb_lbp->byte_count-1] = '\0';
    }
    fprintf(stderr,"'%s' source begin sb_off %d\n",outbuf,sb_off);
    for(i = 0; i < sizeof(outbuf); i++) outbuf[i] = ' ';
    outbuf[sb_off+1] = '^'; outbuf[sb_off+2] = '\0';
    fprintf(stderr,"%s\n",outbuf);

    movc3(se_lbp->buffer,outbuf,se_lbp->byte_count);
    outbuf[se_lbp->byte_count] = '\0';
    if(outbuf[se_lbp->byte_count-1] == '\n'){
	outbuf[se_lbp->byte_count-1] = '\0';
    }
    fprintf(stderr,"'%s' source end se_off %d\n",outbuf,se_off);
    for(i = 0; i < sizeof(outbuf); i++) outbuf[i] = ' ';
    outbuf[se_off+1] = '^'; outbuf[se_off+2] = '\0';
    fprintf(stderr,"%s\n",outbuf);
#endif /* DEBUG */

/*
 * If the buffer just became modified for the first time, we need to change the
 * label line to reflect this.
 */
    if(dbp->ismodified == NO){
	if(dbp == curbuf){
	    screen_label_line(dbp,"(modified)",LABEL_C_MODIFIED);
	}/* End IF */
	dbp->ismodified = YES;
    }/* End IF */

    checkpoint_modified = YES;

/*
 * Since we are modifying the destination line buffer structure, we
 * want to delete any format structures so that the screen display
 * code will know to reformat it.
 */
    if(dlbp->format_line){
	screen_free_format_lines(dlbp->format_line);
	dlbp->format_line = NULL;
    }/* End IF */
/*
 * The cases we have to handle are much more complex if there are newlines
 * being inserted, so we determine now whether this is the case or not.
 */
    newline_included_in_src_data = 1;
    if(sb_lbp->buffer[sb_lbp->byte_count-1] != '\n'){
	newline_included_in_src_data = 0;
    }/* End IF */

#ifdef DEBUG
    fprintf(
	stderr,
	"newline_included_in_src_data = %d\n",
	newline_included_in_src_data
    );
#endif /* DEBUG */

    bytes_to_copy_from_source = sb_lbp->byte_count - sb_off;
    if(bytes_to_copy_from_source > length){
#ifdef DEBUG
	fprintf(
	    stderr,
	    "bytes_to_copy_from_source %d > length %d\n",
	    bytes_to_copy_from_source,
	    length
	);
#endif /* DEBUG */
	bytes_to_copy_from_source = length;
	newline_included_in_src_data = 0;
    }/* End IF */
#ifdef DEBUG
    fprintf(
	stderr,
	"bytes_to_copy_from_source is now %d\n",
	bytes_to_copy_from_source
    );
#endif /* DEBUG */

/*
 * Test for a simple case - one in which there are no newlines in the source
 * data. In this case, we know that we simply have to insert it into the
 * current line buffer. If the buffer area is not big enough, we may have
 * to allocate a new buffer.
 */
    if(newline_included_in_src_data == 0){
/*
 * The total line length will be the current line length, plus the amount
 * of data being inserted.
 */
	bytes_required_for_line = dlbp->byte_count + bytes_to_copy_from_source;

#ifdef DEBUG
	fprintf(
	    stderr,
	    "no newline, bytes_required_for_line %d \n",
	    bytes_required_for_line
	);
#endif /* DEBUG */

/*
 * If the current buffer is too small, we allocate a new buffer and copy
 * all the data into it, and then free the old buffer.
 */
	if(bytes_required_for_line > dlbp->buffer_size){
#ifdef DEBUG
	    fprintf(
		stderr,
		"new allocation required\n"
	    );
#endif /* DEBUG */
	    bytes_to_allocate =
		bytes_required_for_line + INCREMENTAL_LINE_BUFFER_SIZE - 1;
	    bytes_to_allocate -=
		bytes_to_allocate % INCREMENTAL_LINE_BUFFER_SIZE;
#if LARGEST_LOOKASIDE_BLOCK > 0
	    if(dlbp->buffer_size > LARGEST_LOOKASIDE_BLOCK){
		bytes_to_allocate = dlbp->buffer_size * 2;
	    }/* End IF */
#endif

	    if(bytes_to_allocate < bytes_required_for_line){
		char tmp_message[1024];
		sprintf(
		    tmp_message,
		    "buff_insert_from_buffer_with_undo need %d alloced %d\n",
		    bytes_required_for_line,
		    bytes_to_allocate
		);
		tec_panic(tmp_message);
	    }/* End IF */

	    new_buffer = 
		tec_alloc(
		    TYPE_C_LINEBUF,
		    bytes_to_allocate
		);

	    if(new_buffer == NULL){
		return(FAIL);
	    }/* End IF */

	    dcp = new_buffer;
/*
 * Copy up to the current insertion point in the new buffer, then the
 * bytes coming from the source line, then if the source line didn't
 * contribute a newline, the rest of the bytes coming from the original
 * destination line.
 */
	    movc3(dlbp->buffer,dcp,doff);
	    dcp += doff;
	    movc3(sb_lbp->buffer+sb_off,dcp,bytes_to_copy_from_source);
	    dcp += bytes_to_copy_from_source;
	    movc3(&dlbp->buffer[doff],dcp,dlbp->byte_count-doff);
	    bytes_inserted_so_far += bytes_to_copy_from_source;
	    
	    tec_release(TYPE_C_LINEBUF,dlbp->buffer);
	    dlbp->buffer = new_buffer;
	    dlbp->buffer_size = bytes_to_allocate;
	    dlbp->byte_count = bytes_required_for_line;
	}/* End IF */
/*
 * If the current buffer is large enough, make a hole to insert the new
 * data into, and copy in the new data.
 */
	else {
#ifdef DEBUG
	    fprintf(
		stderr,
		"it will fit into the current size of %d\n",
		dlbp->buffer_size
	    );
#endif /* DEBUG */
	    dcp = &dlbp->buffer[bytes_required_for_line-1];
	    scp = &dlbp->buffer[dlbp->byte_count-1];
	    for(i = 0; i < dlbp->byte_count - doff; i++){
		*dcp-- = *scp--;
	    }/* End FOR */
	    movc3(
		&sb_lbp->buffer[sb_off],
		&dlbp->buffer[doff],
		bytes_to_copy_from_source
	    );
	    dlbp->byte_count = bytes_required_for_line;
	    bytes_inserted_so_far += bytes_to_copy_from_source;
	}/* End Else */

#ifdef DEBUG
	doff += bytes_to_copy_from_source;
	movc3(dlbp->buffer,outbuf,dlbp->byte_count);
	outbuf[dlbp->byte_count] = '\0';
	if(outbuf[dlbp->byte_count-1] == '\n'){
	    outbuf[dlbp->byte_count-1] = '\0';
	}
	fprintf(stderr,"'%s' new dest doff %d\n",outbuf,doff);
	for(i = 0; i < sizeof(outbuf); i++) outbuf[i] = ' ';
	outbuf[doff+1] = '^'; outbuf[doff+2] = '\0';
	fprintf(stderr,"%s\n",outbuf);
#endif /* DEBUG */
    }/* End IF */

/*
 * Else, if there is a newline involved, things get more complicated...
 * The first thing we do is construct a line buffer which holds any
 * trailing part of the source data (i.e. not terminated by a newline)
 * along with the trailing part of the line we are inserting into.
 */
    else {
	bytes_required_for_line = dlbp->byte_count - doff + se_off;
	if(bytes_required_for_line > 0){
	    nlbp = allocate_line_buffer(bytes_required_for_line);
	    if(nlbp == NULL) return(FAIL);

	    nlbp->next_line = dlbp->next_line;
	    dlbp->next_line = nlbp;
	    if(nlbp->next_line) nlbp->next_line->prev_line = nlbp;
	    nlbp->prev_line = dlbp;
	    movc3(se_lbp->buffer,nlbp->buffer,se_off);
	    movc3(
		&dlbp->buffer[doff],
		&nlbp->buffer[se_off],
		dlbp->byte_count-doff
	    );
	    nlbp->byte_count = bytes_required_for_line;
	    dlbp->byte_count -= ( dlbp->byte_count - doff );
	    bytes_inserted_so_far += se_off;
	}/* End IF */
/*
 * Now we want to merge the data from the begining of the original
 * destinatio line, and the data up to the first newline from the
 * source data.
 */
	bytes_required_for_line =
	    dlbp->byte_count + sb_lbp->byte_count - sb_off;
	if(bytes_required_for_line > dlbp->buffer_size){
	    bytes_to_allocate =
		bytes_required_for_line + INCREMENTAL_LINE_BUFFER_SIZE - 1;
	    bytes_to_allocate -=
		bytes_to_allocate % INCREMENTAL_LINE_BUFFER_SIZE;
#if LARGEST_LOOKASIDE_BLOCK > 0
	    if(dlbp->buffer_size > LARGEST_LOOKASIDE_BLOCK){
		bytes_to_allocate = dlbp->buffer_size * 2;
	    }/* End IF */
#endif

	    if(bytes_to_allocate < bytes_required_for_line){
		char tmp_message[1024];
		sprintf(
		    tmp_message,
		    "buff_insert_from_buffer_with_undo needs %d alloced %d\n",
		    bytes_required_for_line,
		    bytes_to_allocate
		);
		tec_panic(tmp_message);
	    }/* End IF */

	    new_buffer = 
		tec_alloc(
		    TYPE_C_LINEBUF,
		    bytes_to_allocate
		);
	    if(new_buffer != NULL){
		dcp = new_buffer;
		movc3(dlbp->buffer,dcp,dlbp->byte_count);
		dcp += dlbp->byte_count;
		movc3(&sb_lbp->buffer[sb_off],dcp,sb_lbp->byte_count - sb_off);
		tec_release(TYPE_C_LINEBUF,dlbp->buffer);
		dlbp->buffer = new_buffer;
		dlbp->buffer_size = bytes_to_allocate;
		dlbp->byte_count = bytes_required_for_line;
		bytes_inserted_so_far += (sb_lbp->byte_count - sb_off);
	    }/* End IF */
	}/* End IF */

	else {
	    movc3(
		&sb_lbp->buffer[sb_off],
		&dlbp->buffer[doff],
		sb_lbp->byte_count - sb_off
	    );
	    dlbp->byte_count = bytes_required_for_line;
	    bytes_inserted_so_far += (sb_lbp->byte_count - sb_off);
	}/* End Else */

/*
 * Now we want to copy all the full-length lines in between the starting
 * and ending line buffers.
 */
	sb_lbp = sb_lbp->next_line;
	while(sb_lbp){
	    if(sb_lbp == se_lbp) break;
	    bytes_required_for_line = sb_lbp->byte_count;
	    nlbp = allocate_line_buffer(bytes_required_for_line);
	    if(nlbp == NULL) break;
	    nlbp->next_line = dlbp->next_line;
	    dlbp->next_line = nlbp;
	    if(nlbp->next_line) nlbp->next_line->prev_line = nlbp;
	    nlbp->prev_line = dlbp;
	    movc3(sb_lbp->buffer,nlbp->buffer,bytes_required_for_line);
	    nlbp->byte_count = bytes_required_for_line;
	    bytes_inserted_so_far += bytes_required_for_line;
	    dlbp = nlbp;
	    sb_lbp = sb_lbp->next_line;
	}/* End While */

#ifdef DEBUG
	movc3(dlbp->buffer,outbuf,dlbp->byte_count);
	outbuf[dlbp->byte_count] = '\0';
	if(outbuf[dlbp->byte_count-1] == '\n'){
	    outbuf[dlbp->byte_count-1] = '\0';
	}
	fprintf(stderr,"'%s' new dest doff %d\n",outbuf,doff);
	for(i = 0; i < sizeof(outbuf); i++) outbuf[i] = ' ';
	outbuf[doff+1] = '^'; outbuf[doff+2] = '\0';
	fprintf(stderr,"%s\n",outbuf);

	movc3(nlbp->buffer,outbuf,nlbp->byte_count);
	outbuf[nlbp->byte_count] = '\0';
	if(outbuf[nlbp->byte_count-1] == '\n'){
	    outbuf[nlbp->byte_count-1] = '\0';
	}
	fprintf(stderr,"'%s' new line\n",outbuf);
#endif /* DEBUG */
    }/* End Else */


    if(ct){
	ut->opcode = UNDO_C_DELETE;
	ut->carg1 = (char *)dbp;
	ut->iarg1 = dest_position;
	ut->iarg2 = bytes_inserted_so_far;
    }/* End IF */

/*
 * Depending on whether or not the insert position precedes dot or not, we
 * may have to increment dot.
 */
    if(dest_position <= dbp->dot) dbp->dot += bytes_inserted_so_far;
    dbp->zee += bytes_inserted_so_far;
/*
 * If we are inserting characters before the position cache, it will
 * shift our cached position down by the length, so we adjust the cache.
 */
    if(dest_position < dbp->pos_cache.base){
	dbp->pos_cache.base += bytes_inserted_so_far;
    }/* End IF */

    return(SUCCESS);

}/* End Routine */


/**
 * \brief Insert a string and arrange for undo capability
 *
 * This routine is called when characters need to be inserted, and
 * also need to be un-inserted if the command is undone.
 */
int
buff_insert_with_undo(	struct cmd_token *ct,
						struct buff_header *hbp,
						unsigned long position,
						char *buffer,
						unsigned long length )
{
struct undo_token *ut;

    PREAMBLE();

    ut = allocate_undo_token(ct);
    if(ut == NULL) return(FAIL);

    ut->opcode = UNDO_C_DELETE;
    ut->carg1 = (char *)hbp;
    ut->iarg1 = position;
    ut->iarg2 = length;

    if(buff_insert(hbp,position,buffer,length) == FAIL){
	ut->iarg2 = 0;
	return(FAIL);
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Insert the specified character into the current position
 *
 * This routine is called to insert the single character into the buffer
 * at the current position (dot). It has to worry about such things as
 * noting that the buffer has been modified, the screen display may be
 * invalid, etc.
 */
int
buff_insert_char(	struct buff_header *hbp,
					unsigned long position,
					char data )
{
register struct buff_line *lbp;
struct buff_line *nlbp;
register char *cp,*ocp;
register int i,j;

    PREAMBLE();

/*
 * Insure that the specified position is legal
 */
    if(position > hbp->zee){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(tmp_message,"buff_insert_char: bad position %lu %s Z = %lu\n",
	    position,hbp->name,hbp->zee);
	tec_panic(tmp_message);
    }/* End IF */
/*
 * Determine which line the current insert point is on by calling the
 * find routines.
 */
    lbp = buff_find_line(hbp,position);
    i = buff_find_offset(hbp,lbp,position);
/*
 * Depending on whether or not the specified position precedes dot or not, we
 * may have to decrement dot.
 */
    if(position <= hbp->dot) hbp->dot += 1;
    hbp->zee += 1;
/*
 * If the buffer just became modified for the first time, we need to change the
 * label line to reflect this.
 */
    if(hbp->ismodified == NO){
	if(hbp == curbuf){
	    screen_label_line(hbp,"(modified)",LABEL_C_MODIFIED);
	}/* End IF */
	hbp->ismodified = YES;
    }/* End IF */

    checkpoint_modified = YES;

/*
 * If we are inserting a character before the position cache, it will
 * shift our cached position down by one, so we adjust the cache.
 */
    if(position < hbp->pos_cache.base){
	hbp->pos_cache.base += 1;
    }/* End IF */
/*
 * If there is a format_line structure associated with this line, we make it
 * go away so that the screen display routine will rebuild the display contents
 * of this line.
 */
    if(lbp->format_line){
	screen_free_format_lines(lbp->format_line);
	lbp->format_line = NULL;
    }/* End IF */

/*
 * If this is a newline, we do things a little differently because this will
 * actually cause the line to be broken up. In this case we actually have to
 * create a new line buffer structure.
 */
    if(data == '\n'){
/*
 * Calculate how many bytes are to move to the new line
 */
	j = lbp->byte_count - i;
	if(j > 0){
	    nlbp = allocate_line_buffer(j);
	    if(nlbp == NULL) return(FAIL);
	    nlbp->next_line = lbp->next_line;
	    lbp->next_line = nlbp;
	    if(nlbp->next_line) nlbp->next_line->prev_line = nlbp;
	    nlbp->prev_line = lbp;

/*
 * Now copy the data into the new line buffer
 */
	    cp = lbp->buffer + i;
	    ocp = nlbp->buffer;

	    while(nlbp->byte_count < j){
		*ocp++ = *cp++;
		nlbp->byte_count++;
		lbp->byte_count--;
	    }/* End While */
	}/* End IF */
    }/* End IF */

/*
 * If this line buffer does not have room for the data, we need to expand it
 */
    if(lbp->byte_count == lbp->buffer_size){
	ocp =
	    tec_alloc(
		TYPE_C_LINEBUF,
		lbp->buffer_size + INCREMENTAL_LINE_BUFFER_SIZE
	    );
	if(ocp == NULL) return(FAIL);
	movc3(lbp->buffer,ocp,lbp->buffer_size);
	tec_release(TYPE_C_LINEBUF,lbp->buffer);
	lbp->buffer = ocp;
	lbp->buffer_size += INCREMENTAL_LINE_BUFFER_SIZE;
    }/* End IF */


/*
 * Make a hole in which to place the byte.
 */
    if(lbp->byte_count){
	ocp = lbp->buffer + lbp->byte_count;
	cp = ocp - 1;
	j = lbp->byte_count - i;
	while(j--){
	    *ocp-- = *cp--;
	}/* End While */
    }/* End IF */

    cp = lbp->buffer + i;
/*
 * Ok, now insert the data into the hole
 */
    *cp = data;
    lbp->byte_count += 1;

    return(SUCCESS);

}/* End Routine */

/**
 * \brief Insert a single char with undo capability
 *
 * This is an envelope routine which guarentees that the character
 * to be input can be undone if necessary.
 */
int
buff_insert_char_with_undo(	struct cmd_token *ct,
							struct buff_header *hbp,
							unsigned long position,
							char data )
{
struct undo_token *ut;

    PREAMBLE();

    ut = allocate_undo_token(ct);
    if(ut == NULL) return(FAIL);
    ut->opcode = UNDO_C_DELETE;
    ut->carg1 = (char *)hbp;
    ut->iarg1 = position;
    ut->iarg2 = 1;

    if(buff_insert_char(hbp,position,data) == FAIL){
	ut->iarg2 = 0;
	return(FAIL);
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Delete specified number of characters
 *
 * This routine is called to delete the specified number of characters
 * at the specified location in the buffer.
 */
void
buff_delete(	struct buff_header *hbp,
				unsigned long position,
				unsigned long count )
{

    PREAMBLE();
/*
 * Insure that there are that many character in the buffer after the specified
 * position.
 */
    if((position + count) > hbp->zee){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(tmp_message,"buff_delete: illegal range %lu-%lu %s Z = %lu\n",
	    position,position+count,hbp->name,hbp->zee);
	tec_panic(tmp_message);
    }/* End IF */
/*
 * For now we are cheap and just repeatedly call the single character delete
 * routine, because this is simple. It is also very expensive and will have to
 * be fixed eventually.
 */
    while(count){
	buff_delete_char(hbp,position);
	count -= 1;
    }/* End While */

}/* End Routine */



/**
 * \brief Delete the character at the current position
 *
 * This routine is called to delete the single character which is at
 * the current buffer position.
 */
int
buff_delete_char(	struct buff_header *hbp,
					unsigned long position )
{
register struct buff_line *lbp;
struct buff_line *nlbp;
register char *cp,*ocp;
register int i,j;

    PREAMBLE();
/*
 * Insure that the specified position is legal
 */
    if(position >= hbp->zee){
	char tmp_message[LINE_BUFFER_SIZE];
	sprintf(tmp_message,"buff_delete_char: bad position %lu %s Z = %lu\n",
	    position,hbp->name,hbp->zee);
	tec_panic(tmp_message);
    }/* End IF */
/*
 * Call buff_find_line to find the line structure that the specified
 * position resides on.
 */
    lbp = buff_find_line(hbp,position);
    i = buff_find_offset(hbp,lbp,position);
/*
 * If there is a format_line structure associated with this line, we make it
 * go away so that the screen display routine will rebuild the display contents
 * of this line.
 */
    if(lbp->format_line){
	screen_free_format_lines(lbp->format_line);
	lbp->format_line = NULL;
    }/* End IF */
/*
 * Get a pointer to the character that is about to be deleted
 */
    cp = lbp->buffer + i;
/*
 * Depending on whether or not the specified position precedes dot or not, we
 * may have to decrement dot.
 */
    if(position < hbp->dot) hbp->dot -= 1;
    hbp->zee -= 1;
/*
 * If this has modified the buffer for the first time, it needs to be displayed
 * in the label line.
 */
    if(hbp->ismodified == NO){
	hbp->ismodified = YES;
	if(hbp == curbuf){
	    screen_label_line(hbp,"(modified)",LABEL_C_MODIFIED);
	}/* End IF */
    }/* End IF */

    checkpoint_modified = YES;

/*
 * If we are deleting a character before the position cache, it will
 * shift our cached position up by one, so we adjust the cache.
 */
    if(position < hbp->pos_cache.base){
	hbp->pos_cache.base -= 1;
    }/* End IF */
/*
 * If this is not a newline, then things are pretty simple. Just
 * make the character go away...
 */
    if(*cp != '\n'){
	movc3(cp+1,cp,lbp->byte_count-i-1);
	lbp->byte_count -= 1;
	return(SUCCESS);
    }/* End IF */

/*
 * If this is a newline, we do things a little differently because this
 * actually causes the two lines to become concatenated (unless this is the
 * final line in the buffer). If the buffer area is not big enough to hold
 * both lines, we have to create a bigger area.
 */
    nlbp = lbp->next_line;
    if(nlbp == NULL){
	lbp->byte_count -= 1;
	return(SUCCESS);
    }/* End IF */

/*
 * If there is a format_line structure associated with the second line, we make
 * it go away since this line is about to be trashed.
 */
    if(nlbp->format_line){
	screen_free_format_lines(nlbp->format_line);
	nlbp->format_line = NULL;
    }/* End IF */

/*
 * Test whether or not the buffer is big enough to hold the data from both
 * lines.
 */
    j = lbp->byte_count + nlbp->byte_count - 1;
    if(lbp->buffer_size < j){
	i = j + INCREMENTAL_LINE_BUFFER_SIZE - 1;
	i -= i % INCREMENTAL_LINE_BUFFER_SIZE;
	cp = tec_alloc(TYPE_C_LINEBUF,i);
	if(cp == NULL) return(FAIL);
	movc3(lbp->buffer,cp,lbp->buffer_size);
	tec_release(TYPE_C_LINEBUF,lbp->buffer);
	lbp->buffer = cp;
	lbp->buffer_size = i;
    }/* End IF */

/*
 * Ok, now concatenate the two line buffers by copying the second line
 * into the first. Because we copy on top of the newline, it effectively
 * goes away, so we have to decrement the line count by one.
 */
    lbp->byte_count -= 1;
    cp = lbp->buffer + lbp->byte_count;
    ocp = nlbp->buffer;
    i = nlbp->byte_count;
    while(i--){
	*cp++ = *ocp++;
	lbp->byte_count++;
    }/* End While */

/*
 * Fixup the next and previous pointers so that they point around the
 * line that we are about to remove.
 */
    lbp->next_line = nlbp->next_line;
    if(nlbp->next_line) nlbp->next_line->prev_line = lbp;

    if(hbp->pos_cache.lbp == nlbp) hbp->pos_cache.lbp = NULL;
    buff_free_line_buffer(nlbp);

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Clobber buffer positions and build undo list
 *
 * This routine is called when we want to delete a range of bytes in the
 * buffer, and build an undo list so that we could replace them if we need
 * to. This happens to be pretty efficient for a bulk delete since we can
 * just move the line buffers over to the undo list.
 */
int
buff_delete_with_undo(	struct cmd_token *ct,
						struct buff_header *hbp,
						unsigned long position,
						long count )
{
register char *cp;
struct undo_token *ut;
register struct buff_line *lbp;
struct buff_line *top_lbp;
unsigned long top_offset,local_count,bulk_position;
unsigned long bytes_deleted_so_far = 0;

    PREAMBLE();

/* ???
 * I think I may need this to insure that dot doesn't get changed. For
 * instance, if we had <dot>abc and deleted 'a', we would have <dot>bc.
 * Then if we undo, it's an insert of 'a' at position zero which causes
 * dot to shift so we would now have a<dot>bc.
 */
    if(hbp->dot == (position+count)){
	ut = allocate_undo_token(ct);
	if(ut == NULL) return(FAIL);
	ut->opcode = UNDO_C_CHANGEDOT;
	ut->iarg1 = curbuf->dot;
    }/* End IF */

/*
 * Because some of the undo functions only work in the 'current' buffer, if
 * the user is deleting from a buffer other than the 'current' one, we have
 * to set up some undo tokens to switch the current buffer back and forth
 * during the undo process, should it occur.
 */
    if(hbp != curbuf){
	ut = allocate_undo_token(ct);
	if(ut == NULL) return(FAIL);
	ut->opcode = UNDO_C_CHANGEBUFF;
	ut->iarg1 = curbuf->buffer_number;
    }/* End IF */

    top_lbp = buff_find_line(hbp,position);
    top_offset = buff_find_offset(hbp,top_lbp,position);

/*
 * First step will be to delete the (hopefully) large block of line buffer
 * structures in the middle. This will leave a partial line at the top and
 * also at the bottom. To start, if the begining is in the middle of a line,
 * we need to skip over the bytes on that line.
 */
    local_count = count;
    lbp = top_lbp;
    bulk_position = position;
    if(top_offset){
	local_count -= lbp->byte_count - top_offset;
	bulk_position = position + lbp->byte_count - top_offset;
	lbp = lbp->next_line;
    }/* End IF */
    if(lbp) top_lbp = lbp->prev_line;

/*
 * Ok, now we want to chain over all the line buffer structures which can be
 * removed in bulk. lbp gets left pointing at the final line which can be bulk
 * removed.
 */
    if(lbp && lbp->byte_count <= local_count){
/*
 * We will get to bulk delete at least one line, so allocate an undo buffer.
 * Point the undo token at the head of the list of line buffers. Remember
 * the buffer position of the first byte of bulk data in iarg1, and keep a
 * running count of the number of bytes of bulk data in iarg2.
 */
	ut = allocate_undo_token(ct);
	if(ut == NULL) return(FAIL);
	ut->opcode = UNDO_C_BULK_INSERT;
	ut->carg1 = (char *)lbp;
	ut->iarg1 = bulk_position;
	ut->iarg2 = 0;
/*
 * Now scan down until we reach the bottom of the bulk region. Keep track of
 * how many characters this all represents, and clean up any associated format
 * line structures.
 */
	while(1){
	    ut->iarg2 += lbp->byte_count;
	    local_count -= lbp->byte_count;
	    if(lbp->format_line){
		screen_free_format_lines(lbp->format_line);
		lbp->format_line = NULL;
	    }/* End IF */
	    if(lbp->next_line == NULL) break;
	    if(lbp->next_line->byte_count > local_count) break;
	    lbp = lbp->next_line;
	}/* End While */
/*
 * Unchain the bulk area from the rest of the buffer. First, make the parent
 * structure's forward pointer point to the next line buffer after the final
 * one in the bulk deletion region.
 */
	if(top_lbp) top_lbp->next_line = lbp->next_line;
	else hbp->first_line = lbp->next_line;
	if(hbp->first_line == NULL) hbp->first_line = allocate_line_buffer(1);
/*
 * Now we have to fixup the backwards pointer of the trailing line buffer
 * structure, if it exists.
 */
	if(lbp->next_line){
	    lbp->next_line->prev_line = top_lbp;
	}/* End IF */
/*
 * Finally, clobber the 'next' pointer in lbp which is the end of the bulk
 * area.
 */
	lbp->next_line = NULL;
/*
 * Update the remaining count of bytes to be deleted.
 */
	bytes_deleted_so_far += ut->iarg2;
/*
 * Depending on whether or not the bulk position precedes dot or not, we
 * may have to decrement dot. In any case, we have to update zee.
 */
	if(bulk_position < hbp->dot){
	    if(hbp->dot > bulk_position + ut->iarg2) hbp->dot -= ut->iarg2;
	    else hbp->dot = bulk_position;
	}/* End IF */
	hbp->zee -= ut->iarg2;
/*
 * If this has modified the buffer for the first time, it needs to be displayed
 * in the label line.
 */
	if(hbp->ismodified == NO){
	    hbp->ismodified = YES;
	    if(hbp == curbuf){
		screen_label_line(hbp,"(modified)",LABEL_C_MODIFIED);
	    }/* End IF */
	}/* End IF */

    checkpoint_modified = YES;

/*
 * If we are deleting from before the position cache, it will invalidate the
 * information there, so we have to set it invalid so it does not get used.
 */
	if(hbp->pos_cache.base >= bulk_position){
	    hbp->pos_cache.lbp = NULL;
	}/* End IF */

    }/* End IF */

/*
 * Now, just brute force clobber the remaining few bytes
 */
    if(bytes_deleted_so_far < count){
	ut = allocate_undo_token(ct);
	if(ut == NULL) return(FAIL);
	ut->opcode = UNDO_C_MEMFREE;
	ut->carg1 = cp = tec_alloc(TYPE_C_CBUFF,count-bytes_deleted_so_far);
	if(cp != NULL){
/*
 * Set up the undo token to put the text back into the q-register, and copy
 * it into our save buffer as we delete it from the q-register.
 */
	    ut = allocate_undo_token(ct);
	    if(ut == NULL) return(FAIL);
	    ut->opcode = UNDO_C_INSERT;
	    ut->iarg1 = position;
	    ut->iarg2 = 0;
	    ut->carg1 = cp;
	    while(count > bytes_deleted_so_far){
		*cp++ = buff_contents(hbp,position);
		if(buff_delete_char(hbp,position) == FAIL) return(FAIL);
		bytes_deleted_so_far += 1;
		ut->iarg2 += 1;
	    }/* End While */
	}/* End IF */
    }/* End IF */

/*
 * Insure that the undo commands are working on the buffer the user specified
 */
    if(hbp != curbuf){
	ut = allocate_undo_token(ct);
	if(ut == FAIL) return(FAIL);
	ut->opcode = UNDO_C_CHANGEBUFF;
	ut->iarg1 = hbp->buffer_number;
    }/* End IF */

    return(SUCCESS);

}/* End Routine */



/**
 * \brief Insert a list of line buffer structures
 *
 * This routine is used to insert a list of line buffer structures at
 * the specified position in the buffer.
 */
void
buff_bulk_insert(	struct buff_header *hbp,
					unsigned long position,
					long count,
					struct buff_line *lbp )
{
register struct buff_line *olbp;
int offset;

    PREAMBLE();
/*
 * If this will modify the buffer for the first time, it needs to be displayed
 * in the label line.
 */
    if(hbp->ismodified == NO){
	hbp->ismodified = YES;
	if(hbp == curbuf){
	    screen_label_line(hbp,"(modified)",LABEL_C_MODIFIED);
	}/* End IF */
    }/* End IF */

    checkpoint_modified = YES;

/*
 * We special case position zero because this is an easy case, and is common
 * when HK is being used a lot
 */
    if(position == 0){
	if((olbp = hbp->first_line) != NULL){
	    if(olbp->byte_count == 0){
		olbp = olbp->next_line;
		buff_free_line_buffer(hbp->first_line);
	    }/* End IF */
	}/* End IF */	    
	hbp->first_line = lbp;
	while(lbp->next_line) lbp = lbp->next_line;
	lbp->next_line = olbp;
	if(olbp) olbp->prev_line = lbp;
	hbp->zee += count;
	hbp->dot += count;
	hbp->pos_cache.lbp = NULL;
	return;
    }/* End IF */

/*
 * Also special check for position zee since this is also an easy case
 */
    if(position == hbp->zee){
	olbp = hbp->first_line;
	if(hbp->pos_cache.lbp) olbp = hbp->pos_cache.lbp;
	while(olbp->next_line) olbp = olbp->next_line;
	if(olbp->byte_count == 0){
	    olbp = olbp->prev_line;
	    buff_free_line_buffer(olbp->next_line);
	}/* End IF */
	olbp->next_line = lbp;
	lbp->prev_line = olbp;
	if(hbp->dot == hbp->zee) hbp->dot += count;
	hbp->zee += count;
	hbp->pos_cache.lbp = NULL;
	return;
    }/* End IF */

/*
 * Ok, if the insertion is not at the begining or the end of the buffer, then
 * we just have to do it the hard way...
 */
    olbp = buff_find_line(hbp,position);
    offset = buff_find_offset(hbp,olbp,position);

/*
 * Bulk inserts should always be guarenteed aligned.
 */
    if(offset){
	error_message("BULK_INSERT: internal error! non-aligned bulk insert");
	return;
    }/* End IF */

    if(olbp->prev_line) olbp->prev_line->next_line = lbp;
    lbp->prev_line = olbp->prev_line;

    while(lbp->next_line) lbp = lbp->next_line;
    lbp->next_line = olbp;
    olbp->prev_line = lbp;

    if(hbp->dot >= position) hbp->dot += count;
    if(hbp->pos_cache.base >= position) hbp->pos_cache.base += count;
    hbp->zee += count;

    return;

}/* End Routine */



/**
 * \brief Routine to allocate a line buffer structure
 *
 * This routine is used to allocate a line buffer structure with
 * a data buffer large enough to accommodate the specified number
 * of bytes.
 */
struct buff_line *
allocate_line_buffer(	int size )
{
register struct buff_line *lbp;

    PREAMBLE();
/*
 * If he is asking for a size which will result in the minimum allocation, try
 * to satisfy the request off of the lookaside list.
 */
    if(size <= INITIAL_LINE_BUFFER_SIZE){
	if((lbp = line_buffer_lookaside_list) != NULL){
	    line_buffer_lookaside_list = lbp->next_line;
	    lbp->next_line = NULL;
	    lbp->prev_line = NULL;
	    lbp->format_line = NULL;
	    lbp->byte_count = 0;
	    lbp->lin_magic = MAGIC_LINE;
	    return(lbp);
	}/* End IF */
    }/* End IF */

/*
 * Lookaside list is empty, or he is asking for a non-standard size
 */
    lbp = (struct buff_line *)tec_alloc(TYPE_C_LINE,sizeof(struct buff_line));
    if(lbp == NULL) return(NULL);

    memset(lbp,0,sizeof(*lbp));
    if(size <= INITIAL_LINE_BUFFER_SIZE){
	lbp->buffer_size = INITIAL_LINE_BUFFER_SIZE;
    }/* End IF */
    else if(size < INITIAL_LINE_BUFFER_SIZE + INCREMENTAL_LINE_BUFFER_SIZE){
	lbp->buffer_size = INITIAL_LINE_BUFFER_SIZE +
				INCREMENTAL_LINE_BUFFER_SIZE;
    }
    else {
	lbp->buffer_size = size + INCREMENTAL_LINE_BUFFER_SIZE - 1;
	lbp->buffer_size -= lbp->buffer_size % INCREMENTAL_LINE_BUFFER_SIZE;
    }/* End IF */

    lbp->buffer = tec_alloc(TYPE_C_LINEBUF,lbp->buffer_size);
    if(lbp->buffer == NULL){
	tec_release(TYPE_C_LINE,(char *)lbp);
	return(NULL);
    }/* End IF */

    lbp->lin_magic = MAGIC_LINE;
    return(lbp);

}/* End Routine */


/**
 * \brief Free up the associated storage
 *
 * This routine is called when a line buffer is deleted. It cleans up any
 * associated storage as well as the buffer itself.
 */
void
buff_free_line_buffer(	struct buff_line *lbp )
{

    PREAMBLE();

    if(lbp->format_line){
	screen_free_format_lines(lbp->format_line);
	lbp->format_line = NULL;
    }/* End IF */

/*
 * If this is a standard size line buffer, place it back on the lookaside list
 * rather than giving it back to tec_alloc.
 */
    lbp->lin_magic = 0;

    if(lbp->buffer_size == INITIAL_LINE_BUFFER_SIZE){
	lbp->lin_magic = MAGIC_LINE_LOOKASIDE;
	lbp->next_line = line_buffer_lookaside_list;
	line_buffer_lookaside_list = lbp;
	return;
    }/* End IF */

    tec_release(TYPE_C_LINEBUF,lbp->buffer);
    tec_release(TYPE_C_LINE,(char *)lbp);

}/* End Routine */

/**
 * \brief Release a whole list of line buffers
 *
 * This routine will delete an entire list of line buffers
 */
void
buff_free_line_buffer_list(	struct buff_line *lbp )
{
register struct buff_line *olbp;

    PREAMBLE();

    while((olbp = lbp)){
	lbp = lbp->next_line;
	buff_free_line_buffer(olbp);
    }/* End While */

}/* End Routine */

/**
 * \brief Free up lookaside list
 *
 * This routine frees up any format_lines we've cached on our local
 * lookaside list.
 */
void
buff_deallocate_line_buffer_lookaside_list()
{
register struct buff_line *lbp;

    PREAMBLE();

    while((lbp = line_buffer_lookaside_list) != NULL){
	line_buffer_lookaside_list = lbp->next_line;
	lbp->lin_magic = 0;
	tec_release(TYPE_C_LINEBUF,lbp->buffer);
	tec_release(TYPE_C_LINE,(char *)lbp);
    }/* End While */

}/* End Routine */



#ifdef DEBUG_DUMPING
buff_dump_buffer(who_string,hbp,channel)
char *who_string;
register struct buff_header *hbp;
FILE *channel;
{
register struct buff_line *lbp;
register int i,j;

    PREAMBLE();

    fprintf(channel,"%s requested buffer dump:\n");

    for(i = 0, lbp = hbp->first_line; lbp != NULL; i++,lbp = lbp->next_line){
	fprintf(channel,"line %4d (count %4d)'",i,lbp->byte_count);
	buff_dump_line(lbp,channel);
	fprintf(channel,"'\n");
    }/* End FOR */

    fflush(debug_chan);

}/* End Routine */

buff_dump_line(lbp,channel)
register struct buff_line *lbp;
FILE *channel;
{
register struct buff_header *hbp = curbuf;
register int i,j;

    PREAMBLE();

    for(j = 0; j < lbp->byte_count; j++){
	if(lbp->buffer[j] == '\n'){
	    fprintf(channel,"<newline>");
	}/* End IF */
	else fprintf(channel,"%c",lbp->buffer[j]);
    }/* End FOR */

}/* End Routine */

#endif

unsigned int
stringHash( char *str )
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++) & 0xFF )
    {
	hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
    }

    return hash;
}
