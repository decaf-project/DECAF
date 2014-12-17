/*
 * atest.cpp:
 * test suite for the AFF Library.
 */

#include "afflib.h"
#include "afflib_i.h"


#define MAX_FMTS 10000			// how many formats we should write

char *fmt = "This is format string %8d.\n"; // must be constant size
const char *progname = 0;

char *opt_ext = "aff";


/* Create the segment that we need */

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif


const char *filename(char *buf,const char *base)
{
    sprintf(buf,"%s.%s",base,opt_ext);
    return buf;
}

void enable_writing(AFFILE *af)
{
    af_enable_writing(af,1);
    af_set_pagesize(af,1024);
    af_set_maxsize(af,(int64)65536);		// force splitting of raw and afd files
}

int sequential_test()
{
    char buf[1024];

    printf("Sequential test...\n");

    char fn[1024];

    filename(fn,"test_sequential");
    unlink(fn);				// make sure it is gone
    AFFILE *af = af_open(fn,O_CREAT|O_RDWR|O_TRUNC,0666);
    if(!af) err(1,"af_open");
    enable_writing(af);
    for(int i=0;i<MAX_FMTS;i++){
	printf("\rwriting %d/%d...",i,MAX_FMTS);
	sprintf(buf,fmt,i);
	if(af_write(af,(unsigned char *)buf,strlen(buf))!=(int)strlen(buf)){
	    err(1,"Attempt to write buffer %d failed\n",i);
	}
    }
    af_close(af);
    printf("\nSequential test passes.\n");
    return 0;
}

int reverse_test()
{
    char buf[1024];
    char fn[1024];

    printf("Reverse write test...\n");
    filename(fn,"test_reverse");
    unlink(fn);
    AFFILE *af = af_open(fn,O_CREAT|O_RDWR|O_TRUNC,0666);
    if(!af) err(1,"af_open");
    enable_writing(af);
    for(int i=MAX_FMTS-1;i>=0;i--){
	sprintf(buf,fmt,i);
	af_seek(af,strlen(buf)*i,SEEK_SET);
	if(af_write(af,(unsigned char *)buf,strlen(buf))!=(int)strlen(buf)){
	    err(1,"Attempt to write buffer %d failed\n",i);
	}
	printf("\r%d  ",i);
	fflush(stdout);
    }
    af_close(af);
    printf("\nReverse test passes.\n");
    return 0;
}


int random_write_test()
{
    char fn[1024];
    char buf[1024];
    char tally[MAX_FMTS];
    int i;

    memset(tally,0,sizeof(tally));

    /* Create the AFF file */
    sprintf(buf,fmt,0);		// figure out how big fmt string is
    int fmt_size = strlen(buf);

    printf("Random write test...\n");
    printf("Creating test file with  %d byte records.\n", fmt_size);

    filename(fn,"test_random");
    unlink(fn);				// make sure it is gone
    AFFILE *af = af_open(fn,O_CREAT|O_RDWR|O_TRUNC,0666);
    if(!af) err(1,"af_open");
    enable_writing(af);


    if(af_write(af,(unsigned char *)buf,fmt_size)!=fmt_size){
	err(1,"af_write");
    }
    for(i=0;i<MAX_FMTS;i++){
	/* Find a random spot that's available */
	int pos = rand() % MAX_FMTS;
	while(tally[pos]==1){		//  if this one is used, find next
	    pos = (pos + 1) % MAX_FMTS;
	}
	tally[pos] = 1;
	sprintf(buf,fmt,pos);
	assert((int)strlen(buf)==fmt_size);	// make sure
	af_seek(af,fmt_size*pos,SEEK_SET);
	int wrote = af_write(af,(unsigned char *)buf,fmt_size);
	if(wrote !=fmt_size){
	    fprintf(stderr,"Attempt to write buffer #%d \n",pos);
	    fprintf(stderr,"wrote %d bytes instead of %d bytes\n",wrote,fmt_size);
	    exit(1);
	}
	if(i%25==0) printf("\r%d ...",i);
	fflush(stdout);
    }
    af_close(af);
    
    /* Now verify what was written */
    printf("Verifying write test...\n");
    af = af_open(fn,O_RDONLY,0);
    if(!af) err(1,"af_open");
    
    for(i=0;i<MAX_FMTS;i++){
	char should[256];		// what we should get
	sprintf(should,fmt,i);
	int got = af_read(af,(unsigned char *)buf,fmt_size);
	if(got != fmt_size){
	    fprintf(stderr,"Attempt to read %d bytes; got %d\n",fmt_size,got);
	    exit(1);
	}
	if(i%25==24) printf("\r%d .. %d okay",i-24,i);
    }
    af_close(af);
    printf("\n");
    return 0;
}

int random_read_test(int total_bytes,int data_segment_size)
{
    printf("\n\n\nrandom read test. filesize=%d, segment_size=%d\n",
	   total_bytes,data_segment_size);

    /* Create a regular file and an AFF file */

    printf("Creating random_contents.img and random_contents.%s, both with %d bytes of user data...\n",opt_ext,total_bytes);

    int    fd = open("test_random_contents.img",O_CREAT|O_RDWR|O_TRUNC,0666);
    if(fd<0) err(1,"fopen");

    char fn[1024];
    AFFILE *af = af_open(filename(fn,"test_random_contents"),O_CREAT|O_RDWR|O_TRUNC,0666);
    if(!af) err(1,"af_open");
    enable_writing(af);

    /* Just write it out as one big write */

    unsigned char *buf = (unsigned char *)malloc(total_bytes);
    unsigned char *buf2 = (unsigned char *)malloc(total_bytes);
    RAND_pseudo_bytes(buf,total_bytes);
    if(write(fd,buf,total_bytes)!=total_bytes) err(1,"fwrite");
    if(af_write(af,buf,total_bytes)!=(int)total_bytes) err(1,"af_write");

    /* Now try lots of seeks and reads */
    for(int i=0;i<MAX_FMTS;i++){
	unsigned int loc = rand() % total_bytes;
	unsigned int len = rand() % total_bytes;
	memset(buf,0,total_bytes);
	memset(buf2,0,total_bytes);

	printf("\r#%d  reading %u bytes at %u    ...",i,loc,len);
	fflush(stdout);

	unsigned long l1 = lseek(fd,loc,SEEK_SET);
	unsigned long l2 = af_seek(af,loc,SEEK_SET);


	if(l1!=l2){
	    err(1,"l1 (%lu) != l2 (%lu)",l1,l2);
	}

	int r1 = read(fd,buf,len);
	int r2 = af_read(af,buf2,len);

	if(r1!=r2){
	    err(1,"r1 (%d) != r2 (%d)",r1,r2);
	}
    }
    af_close(af);
    close(fd);
    printf("\nRandom read test passes\n");
    return 0;
}

void large_file_test()
{
    int pagesize = 1024*1024;		// megabyte sized segments
    int64 num_segments = 5000;
    int64 i;
    char fn[1024];

    printf("Large file test... Creating a %"I64d"MB file...\n",pagesize*num_segments/(1024*1024));
    filename(fn,"large_file");
    AFFILE *af = af_open(fn,O_CREAT|O_RDWR|O_TRUNC,0666);

    unsigned char *buf = (unsigned char *)malloc(pagesize);

    memset(buf,'E', pagesize);
    af_enable_writing(af,1);
    af_set_pagesize(af,pagesize);
    af_set_maxsize(af,(int64)pagesize * 600);

    for(i=0;i<num_segments;i++){
	sprintf((char *)buf,"%"I64d" page is put here",i);
	printf("\rWriting page %"I64d"\r",i);
	if(af_write(af,buf,pagesize)!=pagesize){
	    err(1,"Can't write page %"I64d,i);
	}
    }
    printf("\n\n");
    /* Now let's just read some test locations */
    for(i=0;i<num_segments;i+=num_segments/25){	// check a few places
	int r;
	af_seek(af,pagesize*i,SEEK_SET);
	r = af_read(af,buf,1024);		// just read a bit
	if(r!=1024){
	    err(1,"Tried to read 1024 bytes; got %d\n",r);
	}
	if(atoi((char *)buf)!=i){
	    err(1,"at page %"I64d", expected %"I64d", got %s\n",i,i,buf);
	}
	printf("Page %"I64d" validates\n",i);
    }

    af_close(af);
    if(unlink("large_file.aff")){
	err(1,"Can't delete large_file.aff");
    }
    printf("Large file test passes\n");
}

void maxsize_test()
{
    char segname[16];
    char buf[1024];
    char fn[1024];
    int numpages = 1000;

    printf("Maxsize test. Writing %d pages\n",numpages);
    AFFILE *af = af_open(filename(fn,"maxsize"),O_CREAT|O_RDWR|O_TRUNC,0666);
    af_enable_writing(af,1);		// just take the segment defaults
    memset(buf,0,sizeof(buf));
    for(int64 i=0;i<numpages;i++){
	sprintf(buf,"This is page %"I64d". ****************************************************\n",i);
	sprintf(segname,AF_PAGE,i);
	af_update_seg(af,segname,0,buf,sizeof(buf),1);
    }
    af_close(af);
}

void sparse_test()
{
    char buf[1024];
    memset(buf,0,sizeof(buf));
    strcpy(buf,"This is at location 600,000,000.\n");

    char fn[1024];
    AFFILE *af = af_open(filename(fn,"sparse"),O_CREAT|O_RDWR|O_TRUNC,0666);
    af_enable_writing(af,1);
    af_set_maxsize(af,(int64)1024*1024*256);
    af_set_pagesize(af,1024*1024*16);
    af_seek(af,600000000,SEEK_SET);
    af_write(af,(unsigned char *)buf,sizeof(buf));
    af_close(af);
}

void usage()
{
    printf("usage: %s [options]\n",progname);
    printf("    -e ext = use ext for extension (default is %s)\n",opt_ext);
    printf("    -a = do all tests (except -L)\n");
    printf("    -1 = do sequential test\n");
    printf("    -2 = do reverse test\n");
    printf("    -3 = do random write test\n");
    printf("    -4 = do random read test\n");
    printf("    -5 = do maxsize multi-file test\n");
    printf("    -6 = sparse file test\n");
    printf("    -L = run large file test (needs 5GB of disk)\n");
    printf("    -r# = Repeat the random tests # times.\n");
    printf("    -d<dir> = use <dir> as the working dir for files\n");
    printf("    -f<dev> = run af_figure_media on dev and print the results\n");
}



void figure(const char *fn)
{
    struct af_figure_media_buf afb;

    int fd = open(fn,O_RDONLY);
    if(fd<0) err(1,fn);
    if(af_figure_media(fd,&afb)){
	err(1,"af_figure_media");
    }
    printf("sector size: %d\n",afb.sector_size);
    printf("total sectors: %qd\n",afb.total_sectors);
    printf("max read blocks: %d\n",afb.max_read_blocks);
    exit(0);
}


int main(int argc,char **argv)
{
    progname = argv[0];
    int do_sequential = 0;
    int do_reverse    = 0;
    int do_random_write_test = 0;
    int do_random_read_test  = 0;
    int do_large_file = 0;
    int do_maxsize_test = 0;
    int random_repeat = 1;
    int do_sparse_test = 0;
    int do_all=0;
    int ch;
    const char *dir = getenv("BIGTMP");	// use by default

    setvbuf(stdout,0,_IONBF,0);

    while ((ch = getopt(argc, argv, "123456ar:Ld:h?f:e:")) != -1) {
	switch(ch){
	case '1':
	    do_sequential = 1;
	    break;
	case '2':
	    do_reverse = 1;
	    break;
	case '3':
	    do_random_write_test = 1;
	    break;
	case '4':
	    do_random_read_test = 1;
	    break;
	case '5':
	    do_maxsize_test = 1;
	    break;
	case '6':
	    do_sparse_test = 1;
	    break;
	case 'l':
	    random_repeat = atoi(optarg);
	    break;
	case 'L':
	    do_large_file = 1;
	    break;
	case 'a':
	    do_all = 1;
	    break;
	case 'd':
	    dir = optarg;
	    break;
	case 'f':
	    figure(optarg);
	    break;
	case 'e':
	    opt_ext = optarg;
	    break;
	case 'h':
	case '?':
	default:
	    usage();
	}
    }
	
    if(dir){
	fprintf(stderr,"Changing to %s\n",dir);
	if(chdir(dir)) err(1,"Can't change directory to %s",optarg);
    }

    if(do_sequential || do_all) sequential_test();
    if(do_reverse || do_all ) reverse_test();
    if(do_maxsize_test || do_all) maxsize_test();
    if(do_sparse_test || do_all) sparse_test();

    for(int i=0;i<random_repeat;i++){
	if(do_random_read_test  || do_all) random_read_test(256*1024,rand() % 65536); 
	if(do_random_write_test || do_all) random_write_test();
    }

    if(do_large_file) large_file_test();

    return 0;
}
