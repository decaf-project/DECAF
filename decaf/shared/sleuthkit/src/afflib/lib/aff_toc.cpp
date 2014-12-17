/*
 * afflib_dir.cpp:
 * 
 * Functions for the manipulation of the AFF directories
 */

#include "afflib.h"
#include "afflib_i.h"

int	af_toc_free(AFFILE *af)
{
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(af->toc[i].name) free(af->toc[i].name);
	}
	free(af->toc);
	af->toc = 0;
	af->toc_count = 0;
    }
    return 0;
}


void	af_toc_print(AFFILE *af)
{
    printf("AF DIRECTORY:\n");
    for(int i=0;i<af->toc_count;i++){
	if(af->toc[i].name){
	    printf("%-32s %" I64d " \n",af->toc[i].name, af->toc[i].offset);
	}
    }
}

void	af_toc_append(AFFILE *af,const char *segname,int64 offset)
{
    af->toc = (af_toc_mem *)realloc(af->toc,sizeof(*af->toc)*(af->toc_count+1));
    af->toc[af->toc_count].offset = offset;

    af->toc[af->toc_count].name = strdup(segname); // make a copy of the string
    af->toc_count++;
}

/*
 * af_toc_build:
 * Build the directory by reading the existing file...
 */
int	af_toc_build(AFFILE *af)	// build the dir if we couldn't find it
{
    af_toc_free(af);
    af_rewind_seg(af);			// start at the beginning
    af->toc = (af_toc_mem *)malloc(0);
    while(1){
	char segname[AF_MAX_NAME_LEN];
	size_t segname_len=sizeof(segname);
	int64 pos = ftello(af->aseg);

	if(af_get_next_seg(af,segname,segname_len,0,0,0)!=0) break;
	af_toc_append(af,segname,pos);
    }
    return 0;
}

/*
 * return the named entry in the directory
 */

struct af_toc_mem *af_toc(AFFILE *af,const char *segname)
{
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(af->toc[i].name && strcmp(af->toc[i].name,segname)==0) return &af->toc[i];
	}
    }
    return 0;
}

/* Delete something from the directory, but don't bother reallocating.*/
void af_toc_del(AFFILE *af,const char *segname)
{
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(strcmp(af->toc[i].name,segname)==0){
		free(af->toc[i].name);
		af->toc[i].name=0;
		return;
	    }
	}
    }
}

void af_toc_insert(AFFILE *af,const char *segname,int64 offset)
{
    if(af->toc){
	for(int i=0;i<af->toc_count;i++){
	    if(af->toc[i].name==0 || strcmp(af->toc[i].name,segname)==0){
		if(af->toc[i].name==0){	// if name was empty, copy it over
		    af->toc[i].name = strdup(segname);
		}
		af->toc[i].offset = offset;
		return;
	    }
	}
	/* Need to append it to the directory */
	af_toc_append(af,segname,offset);
    }
}
