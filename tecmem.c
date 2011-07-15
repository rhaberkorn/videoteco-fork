char *tecmem_c_version = "tecmem.c: $Revision: 1.3 $";

/*
 * $Date: 2007/12/26 13:28:30 $
 * $Source: /cvsroot/videoteco/videoteco/tecmem.c,v $
 * $Revision: 1.3 $
 * $Locker:  $
 */

/* tecmem.c
 * Subroutines to manage memory allocation and deallocation
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

#if HAVE_SBRK
#ifndef HAVE_UNISTD_H
    char *sbrk();
#endif
#endif

    char *starting_break;

#ifndef HAVE_UNISTD_H
    char *malloc();
    void free();
    void exit();
#endif

    extern struct buff_header *curbuf;
    extern FILE *teco_debug_log;

struct memblock {
    unsigned char type;
    int size;
};

struct memlist {
    unsigned char type;
    struct memlist *next_block;
};

char *malloc_leftover;
int malloc_leftover_size;
struct memlist *lookaside_lists[LOOKASIDE_COUNT];
int lookaside_stat_allocated[LOOKASIDE_COUNT];
int lookaside_stat_available[LOOKASIDE_COUNT];
int malloc_stat_call_count,malloc_stat_inuse;

int memstat_by_type[TYPE_C_MAXTYPE-TYPE_C_MINTYPE+1];

void tecmem_verify(unsigned char,char *,char *);

/**
 * \brief Routine to allocate some memory
 *
 * This routine is called to request memory
 */
char *
tec_alloc( int type, int size )
{
int actual_size;
register int i,j;
register char *cp;
register struct memblock *mp;
register struct memlist *lp;
extern char outof_memory;

    PREAMBLE();

/*
 * First we check that the caller is requesting a known memory block
 * type. This is just a consitency check.
 */
    if(type < TYPE_C_MINTYPE || type > TYPE_C_MAXTYPE){
	printf("\nTECO: UNKNOWN MEMORY BLOCK TYPE %d in TEC_ALLOC\n",type);
#ifdef UNIX
	kill(getpid(),SIGQUIT);
#endif
	exit(ENOMEM);
    }/* End IF */


#ifdef INSERT_RANDOM_MALLOC_ERRORS
    {
	int chance;
	chance = rand() % 100;
	if(chance == 0){
	    return(NULL);
	}/* End IF */
    }
#endif /* INSERT_RANDOM_MALLOC_ERRORS */

/*
 * If the size required is larger than the largest lookaside block we support,
 * we just call malloc with it.
 */
    actual_size = size + sizeof(struct memblock);
    if(actual_size > LARGEST_LOOKASIDE_BLOCK){
	mp = (struct memblock *)malloc((unsigned)actual_size);

	if(mp == NULL){
	    outof_memory = YES;
	    return(NULL);
	}/* End IF */

	malloc_stat_call_count += 1;
	malloc_stat_inuse += actual_size;
	mp->type = type;
	mp->size = actual_size;
	cp = (char *)mp + sizeof(struct memblock);
	return(cp);
    }/* End IF */

/*
 * Round up to the next larger lookaside block size
 */
    if(actual_size & (MINIMUM_ALLOCATION_BLOCK - 1)){
	actual_size += MINIMUM_ALLOCATION_BLOCK;
	actual_size &= ~(MINIMUM_ALLOCATION_BLOCK - 1);
    }/* End IF */

/*
 * Check whether we have a lookaside entry of that size. If not, we allocate
 * some number of entries of that size from our large memory hunk.
 */
    i = (actual_size / MINIMUM_ALLOCATION_BLOCK) - 1;

    if(lookaside_lists[i] == NULL){
	tec_gc_lists();
    }/* End IF */

    if(lookaside_lists[i] == NULL){
	if(malloc_leftover != NULL){
	    if(malloc_leftover_size < actual_size){
		i = (malloc_leftover_size / MINIMUM_ALLOCATION_BLOCK) - 1;
		lp = (struct memlist *)malloc_leftover;
		lp->next_block = lookaside_lists[i];
		lookaside_lists[i] = lp;
		lookaside_stat_available[i] += 1;
		i = (actual_size / MINIMUM_ALLOCATION_BLOCK) - 1;
		malloc_leftover = NULL;
		malloc_leftover_size= 0;
	    }/* End IF */
	    else {
		lp = (struct memlist *)malloc_leftover;
		malloc_leftover += actual_size;
		malloc_leftover_size -= actual_size;
		if(malloc_leftover_size == 0) malloc_leftover = NULL;
		lp->next_block = NULL;
		lookaside_lists[i] = lp;
		lookaside_stat_available[i] += 1;
	    }/* End Else */
	}/* End IF */
    }/* End IF */

    if(lookaside_lists[i] == NULL){
	j = BIG_MALLOC_HUNK_SIZE / actual_size;
	cp = (char *)malloc((unsigned)(BIG_MALLOC_HUNK_SIZE));

	if(cp == NULL){
	    printf("\nTECO: malloc failed!\n");
	    outof_memory = YES;
	    return(NULL);
	}/* End IF */

/*
 * Place the one block on the lookaside list, and the rest as a malloc
 * leftover.
 */
	lp = (struct memlist *)cp;
	lookaside_stat_available[i] += 1;
	lp->next_block = NULL;
	lookaside_lists[i] = lp;
	malloc_leftover = cp + actual_size;
	malloc_leftover_size = BIG_MALLOC_HUNK_SIZE - actual_size;
	     
    }/* End IF */

    lp = lookaside_lists[i];
    lookaside_lists[i] = lp->next_block;
    lookaside_stat_allocated[i] += 1;
    lookaside_stat_available[i] -= 1;
    memstat_by_type[type-TYPE_C_MINTYPE] += 1;

    mp = (struct memblock *)lp;
    mp->type = type;
    mp->size = actual_size;
    cp = (char *)mp + sizeof(struct memblock);
    bzero(cp,size);
    return(cp);

}/* End Routine */

/**
 * \brief Envelope routine for free()
 *
 * This routine is called to release memory which was previously allocated
 * by calling the tec_alloc routine.
 */
void
tec_release( unsigned char type, char *addr )
{
register struct memblock *mp;
register struct memlist *lp;
register int i;

    PREAMBLE();

    mp = (struct memblock *)( addr - sizeof(struct memblock) );
    if(mp->type != type){
	printf("\nTEC_RELEASE: TYPE Mismatch: Supplied %d Stored %d addr 0x%x\n",
	    type,mp->type,(unsigned int)addr);
#ifdef UNIX
	kill(getpid(),SIGQUIT);
#endif
	exit(ENOMEM);
    }/* End IF */

    if(mp->size > LARGEST_LOOKASIDE_BLOCK){
	malloc_stat_inuse -= mp->size;
	free(mp);
	return;
    }/* End IF */

    mp->type += 1000;
    i = mp->size / MINIMUM_ALLOCATION_BLOCK - 1;
    lp = (struct memlist *)mp;
    lp->next_block = lookaside_lists[i];
    lookaside_lists[i] = lp;

    lookaside_stat_available[i] += 1;
    lookaside_stat_allocated[i] -= 1;
    memstat_by_type[type-TYPE_C_MINTYPE] -= 1;

    return;

}/* End Routine */



/**
 * \brief Verify that structure type hasn't been corrupted
 *
 * This debug routine checks to see whether the memory block has been
 * overwritten by checking the type code.
 */
void
tecmem_verify( unsigned char type, char *addr, char *message )
{
register struct memblock *mp;
#if 0
register struct memlist *lp;
#endif
#if 0
register int i;
#endif

    PREAMBLE();

    mp = (struct memblock *)( addr - sizeof(struct memblock) );
    if(mp->type != type){
	printf(
	    "\nTYPE Mismatch: Supplied %d Stored %d addr %x '%s'\n",
	    type,mp->type,(unsigned int)addr,message);
	exit(1);
    }/* End IF */

    return;

}/* End Routine */



/**
 * \brief Garbage collect any local lookaside lists
 *
 * This routine is called before we call malloc for more memory in
 * an attempt to find the memory on some local lookaside lists.
 */
void
tec_gc_lists()
{

    PREAMBLE();

    screen_deallocate_format_lookaside_list();
    buff_deallocate_line_buffer_lookaside_list();

}/* End Routine */


void
initialize_memory_stats()
{
    starting_break = 0;

#if HAVE_SBRK
    if(starting_break == NULL){
	starting_break = (char *)sbrk(0);
    }/* End IF */
#endif

}


/**
 * \brief Insert memory statistics into the current buffer
 *
 * This routine is called by the buffer map routine to allow us to
 * insert some memory statistics into the map.
 */
void
tecmem_stats()
{
char tmp_buffer[LINE_BUFFER_SIZE];
register int i;
register int size;
int total_memory_in_use;
char *current_break;
int bss_in_use;

    PREAMBLE();

    total_memory_in_use = malloc_stat_inuse;

    sprintf(
	tmp_buffer,
	"\n\n%d non-lookaside allocations, %d bytes outstanding\n\n",
	malloc_stat_call_count,
	malloc_stat_inuse
    );
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    for(i = 0; i < LOOKASIDE_COUNT; i++){
	if(lookaside_stat_allocated[i] == 0 &&
	   lookaside_stat_available[i] == 0) continue;
	size = ( i + 1 ) * MINIMUM_ALLOCATION_BLOCK;
	total_memory_in_use += lookaside_stat_allocated[i] * size;

	sprintf(
	    tmp_buffer,
	    "LA%2d, size %4d, bytes in use %6d, bytes available %6d\n",
	    i,
	    size,
	    lookaside_stat_allocated[i] * size,
	    lookaside_stat_available[i] * size
	);

	buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    }/* End FOR */

    sprintf(
	tmp_buffer,
	"Malloc Leftover %d bytes\n",
	malloc_leftover_size
    );
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    buff_insert(curbuf,curbuf->dot,"\n",1);

    for(i = 0; i < TYPE_C_MAXTYPE-TYPE_C_MINTYPE; i++){
	static char *type_name_list[] = {
	    "CBUFF",
	    "CPERM",
	    "CMD",
	    "UNDO",
	    "SCR",
	    "SCRBUF",
	    "SCREEN",
	    "SCREENBUF",
	    "LINE",
	    "LINEBUF",
	    "BHDR",
	    "WINDOW",
	    "LABELFIELD",
	    "WILD",
	    "TAGS",
	    "TAGENT",
	    "TAGSTR",
	    "MAXTYPE"
	};

	if((unsigned)i >= ELEMENTS(type_name_list)){
	    sprintf(
		tmp_buffer,
		"Unknown memory type index %d(%d)\n",
		i,
		i+TYPE_C_MINTYPE
	    );
	}/* End IF */

	else {
	    sprintf(
		tmp_buffer,
		"Type %10s, blocks in use %d\n",
		type_name_list[i],
		memstat_by_type[i]
	    );
	}/* End Else */

	buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

    }/* End FOR */

#if HAVE_SBRK
    current_break = (char *)sbrk(0);
    if(current_break != sbrk(0)){
	tec_panic("sbrk(0) seems to be allocating space!\n");
    }/* End IF */
    bss_in_use = current_break - starting_break;
#else
    bss_in_use = 0;
#endif /* UNIX */

    if( bss_in_use ){
	sprintf(
	    tmp_buffer,
	    "\nTotal memory in use %d, Total allocated bss %d\n",
	    total_memory_in_use,
	    bss_in_use
	);
    } else {
	sprintf(tmp_buffer,"\nTotal memory in use %d\n",total_memory_in_use);
    }
    buff_insert(curbuf,curbuf->dot,tmp_buffer,strlen(tmp_buffer));

}/* End Routine */
