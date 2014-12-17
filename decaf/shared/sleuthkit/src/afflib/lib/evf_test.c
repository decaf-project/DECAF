////////////////////////////////////////////////////////////////////////////////
////// headers.c
//////
////// Outputs all sections (sans headers) except "sectors" section from EVF 
////// file specified in argv[1], storing each as a separate file in directory
////// specified by argv[2], numbering each in order of its appearance in EVF 
////// file.
//////
////// David J. Malan
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//// libraries
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zlib.h"


////////////////////////////////////////////////////////////////////////////////
//// constants
////////////////////////////////////////////////////////////////////////////////

#define SIZE (512 * 64)


////////////////////////////////////////////////////////////////////////////////
// main driver
////////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
    // comments
    unsigned char *comments = NULL, *ccomments = NULL;

    // inflated comments' size
    unsigned long long comments_size = 0;

    // counter
    unsigned short counter = 0;

    // EOF flag
    short eof = 0;

    // file header
    unsigned char filehdr[13];

    // file pointers
    FILE *finfile = NULL, *foutfile = NULL;

    // number of bytes we have to inflate
    unsigned int have;

    // index variable
    unsigned long long i;

    // command-line arguments
    char *infile = NULL, *outdir = NULL;

    // size of reads
    unsigned long long n;

    // current offset in file
    unsigned long offset = 0;

    // outfile
    unsigned char outfile[1024];

    // raw data
    unsigned char raw[SIZE];

    // return value
    int result;

    // section header
    unsigned char sectionhdr[76];

    // a section's size
    unsigned long long size = 0;

    // stream for inflate
    z_stream strm;

    // a section's type
    char type[17] = "";

    // ensure proper usage
    if (argc == 3)
    {
        infile = argv[1];
        outdir = argv[2];
    }
    else
    {
        fprintf(stderr, "usage: headers infile outdir\n");
        return 1;
    }

    // attempt to open input file
    if ((finfile = fopen(infile, "rb")) == NULL)
    {
        fprintf(stderr, "error opening infile: %s\n", infile);
        return 1;
    }

    // attempt to read file header
    if (fread(filehdr, sizeof(filehdr), 1, finfile) != 1)
    {
        fprintf(stderr, "error reading infile: %s\n", infile);
        return 1;
    }
    
    // ensure file header's signature is correct
    if ((filehdr[0]  != 'E')  ||
        (filehdr[1]  != 'V')  ||
        (filehdr[2]  != 'F')  ||
        (filehdr[3]  != 0x09) ||
        (filehdr[4]  != 0x0d) ||
        (filehdr[5]  != 0x0a) ||
        (filehdr[6]  != 0xff) ||
        (filehdr[7]  != 0x00) ||
        (filehdr[8]  != 0x01) ||
        (filehdr[11] != 0x00) ||
        (filehdr[12] != 0x00)) 
    {
        fprintf(stderr, "invalid signature in file header\n");
        return 1;
    }

    // report
    fprintf(stdout, "Found file header at 0x0\n");
    fprintf(stdout, "This is segment %u\n", *((short *) &filehdr[9]));

    // initialize EOF flag
    eof = 0;

    // initialize current offset
    offset = 13;

    // iterate over file's sections
    while (!eof)
    {
        // attempt to read beginning of a section
        if (fread(sectionhdr, sizeof(sectionhdr), 1, finfile) != 1) 
        {
            fprintf(stderr,"error reading beginning of section\n");
            return 1;
        }

        // increment counter
        counter++;

        // remember section's type
        strncpy(type, sectionhdr, 16);

        // report
        fprintf(stdout, "Found section at 0x%x: %s\n", offset, type);

        // remember section's size
        size = *((long *) (&sectionhdr[24]));

        // open outfile
        sprintf(outfile, "%s/%.2d-%s", outdir, counter, type);
        foutfile = fopen(outfile, "wb");
        if (foutfile == NULL)
        {
            fprintf(stderr, "error opening outfile: %s\n", outfile);
            return 1;
        }

        // convert section to raw
        if (!strcmp("header", type) || !strcmp("header2", type))
        {
            // read deflated comments
            ccomments = malloc((size - 76) * sizeof(unsigned char));
            if (ccomments == NULL)
            {
                fprintf(stderr, "error allocating memory for comments\n");
                return 1;
            }
            n = fread(ccomments, 1, size - 76, finfile);

            // allocate memory for inflated comments
            comments_size = SIZE * sizeof(unsigned char);
            if ((comments = malloc(comments_size)) == NULL)
            {
                fprintf(stderr, 
                       "error allocating memory for comments\n");
                return 1;
            }

            // prepare state for inflate
            strm.zalloc = Z_NULL;
            strm.zfree = Z_NULL;
            strm.opaque = Z_NULL;
            strm.avail_in = n;
            strm.next_in = ccomments;
            if (inflateInit(&strm) != Z_OK)
            {
                fprintf(stderr, "error allocating inflate state\n");
                return 1;
            }
            strm.avail_out = comments_size;
            strm.next_out = comments;

            // inflate comments
            do 
            {
                // inflate a buffer's worth of comments
                result = inflate(&strm, Z_SYNC_FLUSH);
                if (result != Z_OK && result != Z_STREAM_END)
                {
                    (void) inflateEnd(&strm);
                    fprintf(stderr, "error decompressing chunk\n");
                    return 1;
                }

                // allocate additional memory for comments as needed
                if (result == Z_OK && strm.avail_out == 0)
                {
                    // double our buffer's size
                    strm.avail_out = comments_size;
                    comments_size *= 2;
                    if ((comments = realloc(comments, comments_size)) == NULL)
                    {
                        fprintf(stderr, 
                                "error allocating memory for comments\n");
                        return 1;
                    }

                    // don't clobber comments already inflated
                    strm.next_out = comments + strm.avail_out;
                }
            }
            while (result != Z_STREAM_END);

            // output inflated comments
            if (fwrite(comments, comments_size - strm.avail_out, 1, foutfile) != 1 ||
                ferror(foutfile))
            {
                fprintf(stderr, "error writing to outfile: %s\n", outfile);
                (void) inflateEnd(&strm);
                return 1;
            }

            // we're done with current section
            if (ccomments != NULL)
                free(ccomments);
            (void) inflateEnd(&strm);
        }
        else if (strcmp("sectors", type))
            for (i = 76; i < size; i++)
                fputc(fgetc(finfile), foutfile);
        
        if (!strcmp("done", type) || !strcmp("next", type))
            eof = 1;

        // close outfile
        fclose(foutfile);

        // determine offset of next section
        offset = *((long *) (&sectionhdr[16]));

        // attempt to seek to next section
        if (fseek(finfile, offset, SEEK_SET) != 0) 
        {
            fprintf(stderr, "error seeking to next section\n");
            return 1;
        }
    }

    // close input file
    if (finfile && fclose(finfile) != 0)
    {
        fprintf(stderr, "error closing infile: %s\n", infile);
        return 1;
    }

    // That's all folks!
    return 0;
}


////////////////////////////////////////////////////////////////////////////////
////// EOF
////////////////////////////////////////////////////////////////////////////////

