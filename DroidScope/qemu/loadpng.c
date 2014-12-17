#include <stdio.h>
#include <stdlib.h>

#include <png.h>

#if 0
#define LOG(x...) fprintf(stderr,"error: " x)
#else
#define LOG(x...) do {} while (0)
#endif

void *loadpng(const char *fn, unsigned *_width, unsigned *_height)
{
    FILE *fp = 0;
    unsigned char header[8];
    unsigned char *data = 0;
    unsigned char **rowptrs = 0;
    png_structp p = 0;
    png_infop pi = 0;
    
    png_uint_32 width, height;
    int bitdepth, colortype, imethod, cmethod, fmethod, i;

    p = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(p == 0) {
        LOG("%s: failed to allocate png read struct\n", fn);
        return 0;
    }

    pi = png_create_info_struct(p);
    if(pi == 0) {
        LOG("%s: failed to allocate png info struct\n", fn);
        goto oops;
    }
        
    fp = fopen(fn, "rb");
    if(fp == 0) {
        LOG("%s: failed to open file\n", fn);
        return 0;
    }

    if(fread(header, 8, 1, fp) != 1) {
        LOG("%s: failed to read header\n", fn);
        goto oops;
    }
    
    if(png_sig_cmp(header, 0, 8)) {
        LOG("%s: header is not a PNG header\n", fn);
        goto oops;
    }

    if(setjmp(png_jmpbuf(p))) {
        LOG("%s: png library error\n", fn);
    oops:
        png_destroy_read_struct(&p, &pi, 0);
        if(fp != 0) fclose(fp);
        if(data != 0) free(data);
        if(rowptrs != 0) free(rowptrs);
        return 0;
    }

    png_init_io(p, fp);
    png_set_sig_bytes(p, 8);

    png_read_info(p, pi);

    png_get_IHDR(p, pi, &width, &height, &bitdepth, &colortype,
                 &imethod, &cmethod, &fmethod);
//    printf("PNG: %d x %d (d=%d, c=%d)\n",
//           width, height, bitdepth, colortype);

    switch(colortype){
    case PNG_COLOR_TYPE_PALETTE:
        png_set_palette_to_rgb(p);
        break;

    case PNG_COLOR_TYPE_RGB:
        if(png_get_valid(p, pi, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(p);
        } else {
            png_set_filler(p, 0xff, PNG_FILLER_AFTER);
        }
        break;
        
    case PNG_COLOR_TYPE_RGB_ALPHA:
        break;

    case PNG_COLOR_TYPE_GRAY:
        if(bitdepth < 8) {
            png_set_gray_1_2_4_to_8(p);
        }
        
    default:
        LOG("%s: unsupported (grayscale?) color type\n");
        goto oops;
    }

    if(bitdepth == 16) {
        png_set_strip_16(p);
    }

    data = (unsigned char*) malloc((width * 4) * height);
    rowptrs = (unsigned char **) malloc(sizeof(unsigned char*) * height);

    if((data == 0) || (rowptrs == 0)){
        LOG("could not allocate data buffer\n");
        goto oops;
    }

    for(i = 0; i < height; i++) {
        rowptrs[i] = data + ((width * 4) * i);
    }
    
    png_read_image(p, rowptrs);
    
    png_destroy_read_struct(&p, &pi, 0);
    fclose(fp);
    if(rowptrs != 0) free(rowptrs);

    *_width = width;
    *_height = height;
    
    return (void*) data;    
}


typedef struct
{
    const unsigned char*  base;
    const unsigned char*  end;
    const unsigned char*  cursor;

} PngReader;

static void
png_reader_read_data( png_structp  png_ptr,
                      png_bytep   data,
                      png_size_t  length )
{
  PngReader* reader = png_get_io_ptr(png_ptr);
  png_size_t avail  = (png_size_t)(reader->end - reader->cursor);

  if (avail > length)
      avail = length;

  memcpy( data, reader->cursor, avail );
  reader->cursor += avail;
}


void *readpng(const unsigned char *base, size_t   size, unsigned *_width, unsigned *_height)
{
    PngReader  reader;
    unsigned char *data = 0;
    unsigned char **rowptrs = 0;
    png_structp p = 0;
    png_infop pi = 0;

    png_uint_32 width, height;
    int bitdepth, colortype, imethod, cmethod, fmethod, i;

    p = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
    if(p == 0) {
        LOG("%s: failed to allocate png read struct\n", fn);
        return 0;
    }

    pi = png_create_info_struct(p);
    if(pi == 0) {
        LOG("%s: failed to allocate png info struct\n", fn);
        goto oops;
    }

    reader.base   = base;
    reader.end    = base + size;
    reader.cursor = base;

    if(size < 8 || png_sig_cmp((unsigned char*)base, 0, 8)) {
        LOG("%s: header is not a PNG header\n", fn);
        goto oops;
    }

    reader.cursor += 8;

    if(setjmp(png_jmpbuf(p))) {
        LOG("%s: png library error\n", fn);
    oops:
        png_destroy_read_struct(&p, &pi, 0);
        if(data != 0) free(data);
        if(rowptrs != 0) free(rowptrs);
        return 0;
    }

    png_set_read_fn (p, &reader, png_reader_read_data);
    png_set_sig_bytes(p, 8);

    png_read_info(p, pi);

    png_get_IHDR(p, pi, &width, &height, &bitdepth, &colortype,
                 &imethod, &cmethod, &fmethod);
//    printf("PNG: %d x %d (d=%d, c=%d)\n",
//           width, height, bitdepth, colortype);

    switch(colortype){
    case PNG_COLOR_TYPE_PALETTE:
        png_set_palette_to_rgb(p);
        break;

    case PNG_COLOR_TYPE_RGB:
        if(png_get_valid(p, pi, PNG_INFO_tRNS)) {
            png_set_tRNS_to_alpha(p);
        } else {
            png_set_filler(p, 0xff, PNG_FILLER_AFTER);
        }
        break;

    case PNG_COLOR_TYPE_RGB_ALPHA:
        break;

    case PNG_COLOR_TYPE_GRAY:
        if(bitdepth < 8) {
            png_set_gray_1_2_4_to_8(p);
        }

    default:
        LOG("%s: unsupported (grayscale?) color type\n");
        goto oops;
    }

    if(bitdepth == 16) {
        png_set_strip_16(p);
    }

    data    = (unsigned char*) malloc((width * 4) * height);
    rowptrs = (unsigned char **) malloc(sizeof(unsigned char*) * height);

    if((data == 0) || (rowptrs == 0)){
        LOG("could not allocate data buffer\n");
        goto oops;
    }

    for(i = 0; i < height; i++) {
        rowptrs[i] = data + ((width * 4) * i);
    }

    png_read_image(p, rowptrs);

    png_destroy_read_struct(&p, &pi, 0);
    if(rowptrs != 0) free(rowptrs);

    *_width = width;
    *_height = height;

    return (void*) data;    
}


#if 0
int main(int argc, char **argv)
{
    unsigned w,h;
    unsigned char *data;
    
    if(argc < 2) return 0;


    data = loadpng(argv[1], &w, &h);

    if(data != 0) {
        printf("w: %d  h: %d\n", w, h);
    }

    return 0;
}
#endif
