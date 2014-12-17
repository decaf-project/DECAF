/* Copyright (c) 2010 The Android Open Source Project
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/* dummy dlopen()/dlclose()/dlsym() implementations to be used in static builds */
#include <stddef.h>

void* dlopen(void)
{
    /* Do not return NULL to route around a bug in our SDL configure script */
    /* mimick succesful load, then all calls to dlsym/dlvsym will fail */
    return (void*)"XXX";
}

void dlclose(void)
{
}

void* dlsym(void)
{
    return NULL;
}

void* dlvsym(void)
{
    return NULL;
}

const char* dlerror(void)
{
    return "Dynamic linking not enabled !";
}
