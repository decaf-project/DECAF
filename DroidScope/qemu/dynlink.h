/* Copyright (c) 2008 The Android Open Source Project
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

/*
 * Lazy Dynamic Linking Support
 *
 * This header file is meant to be included multiple times.
 *
 * It is used to define function pointers to symbols in external
 * shared objects (Unix dynamic libraries) which will be lazily resolved
 * at runtime, by calling a specific initialization function.
 *
 * You must define, before including this header, a DYNLINK_FUNCTIONS
 * macro which must contain a sequence of DYNLINK_FUNC(ret,name,sig)
 * statements.
 *
 * In each statement, 'ret' is a function return type, 'name' is
 * the function's name as provided by the library, and 'sig' is
 * the function signature, including enclosing parentheses.
 *
 * Here's an example:
 *
 *  #define DYNLINK_FUNCTIONS \
 *     DYNLINK_FUNC(int,open,(const char*, int)) \
 *     DYNLINK_FUNC(int,read,(int,char*,int)) \
 *     DYNLINK_FUNC(int,close,(int)) \
 *
 *
 * You must also define a DYNLINK_FUNCTIONS_INIT macro which contains the
 * name of a generated function used to initialize the function pointers.
 * (see below)
 */

#ifndef DYNLINK_FUNCTIONS
#error DYNLINK_FUNCTIONS should be defined when including this file
#endif

#ifndef DYNLINK_FUNCTIONS_INIT
#error DYNLINK_FUNCTIONS_INIT should be defined when including this file
#endif

/* just in case */
#undef DYNLINK_FUNC

/* define pointers to dynamic library functions as static pointers.
 */
#define  DYNLINK_FUNC(ret,name,sig) \
      static ret  (*_dynlink_##name) sig ;

#define  DYNLINK_STR(name)   DYNLINK_STR_(name)
#define  DYNLINK_STR_(name)  #name

DYNLINK_FUNCTIONS
#undef DYNLINK_FUNC

/* now define a function that tries to load all dynlink function
 * pointers. returns 0 on success, or -1 on error (i.e. if any of
 * the functions could not be loaded).
 *
 * 'library' must be the result of a succesful dlopen() call
 *
 * You must define DYNLINK_FUNCTIONS_INIT
 */
static int
DYNLINK_FUNCTIONS_INIT(void*  library)
{
#define  DYNLINK_FUNC(ret,name,sig) \
    do { \
        _dynlink_##name = dlsym( library, DYNLINK_STR(name) ); \
        if (_dynlink_##name == NULL) goto Fail; \
    } while (0);

    DYNLINK_FUNCTIONS
#undef DYNLINK_FUNC

    return 0;
Fail:
    return -1;
}

/* in user code, use  FF(function_name) to invoke the
 * corresponding dynamic function named 'function_name'
 * after initialization succeeded.
 */
#ifndef FF
#define FF(name)   (*_dynlink_##name)
#endif

/* clear macros */
#undef DYNLINK_FUNC
#undef DYNLINK_FUNCTIONS
#undef DYNLINK_FUNCTIONS_INIT
