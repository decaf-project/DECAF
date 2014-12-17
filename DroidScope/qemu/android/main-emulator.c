/* Copyright (C) 2011 The Android Open Source Project
**
** This software is licensed under the terms of the GNU General Public
** License version 2, as published by the Free Software Foundation, and
** may be copied, distributed, and modified under those terms.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
*/

/* This is the source code to the tiny "emulator" launcher program
 * that is in charge of starting the target-specific emulator binary
 * for a given AVD, i.e. either 'emulator-arm' or 'emulator-x86'
 *
 * This program will be replaced in the future by what is currently
 * known as 'emulator-ui', but is a good placeholder until this
 * migration is completed.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <android/utils/panic.h>
#include <android/utils/path.h>
#include <android/utils/bufprint.h>
#include <android/avd/util.h>

/* Required by android/utils/debug.h */
int android_verbose;


#define DEBUG 1

#if DEBUG
#  define D(...)  do { if (android_verbose) printf("emulator:" __VA_ARGS__); } while (0)
#else
#  define D(...)  do{}while(0)
#endif

/* Forward declarations */
static char* getTargetEmulatorPath(const char* progName, const char* avdArch);

/* Main routine */
int main(int argc, char** argv)
{
    const char* avdName = NULL;
    char*       avdArch = NULL;
    char*       emulatorPath;

    /* Define ANDROID_EMULATOR_DEBUG to 1 in your environment if you want to
     * see the debug messages from this launcher program.
     */
    const char* debug = getenv("ANDROID_EMULATOR_DEBUG");

    if (debug != NULL && *debug && *debug != '0')
        android_verbose = 1;

    /* Parse command-line and look for an avd name
     * Either in the form or '-avd <name>' or '@<name>'
     */
    int  nn;
    for (nn = 1; nn < argc; nn++) {
        const char* opt = argv[nn];

        if (!strcmp(opt,"-qemu"))
            break;

        if (!strcmp(opt,"-avd") && nn+1 < argc) {
            avdName = argv[nn+1];
            break;
        }
        else if (opt[0] == '@' && opt[1] != '\0') {
            avdName = opt+1;
            break;
        }
    }

    /* If there is an AVD name, we're going to extract its target architecture
     * by looking at its config.ini
     */
    if (avdName != NULL) {
        D("Found AVD name '%s'\n", avdName);
        avdArch = path_getAvdTargetArch(avdName);
        D("Found AVD target architecture: %s\n", avdArch);
    } else {
        /* Otherwise, using the ANDROID_PRODUCT_OUT directory */
        const char* androidOut = getenv("ANDROID_PRODUCT_OUT");

        if (androidOut != NULL && *androidOut != '\0') {
            D("Found ANDROID_PRODUCT_OUT: %s\n", androidOut);
            avdArch = path_getBuildTargetArch(androidOut);
            D("Found build target architecture: %s\n", avdArch);
        }
    }

    if (avdArch == NULL) {
        avdArch = "arm";
        D("Can't determine target AVD architecture: defaulting to %s\n", avdArch);
    }

    /* Find the architecture-specific program in the same directory */
    emulatorPath = getTargetEmulatorPath(argv[0], avdArch);
    D("Found target-specific emulator binary: %s\n", emulatorPath);

    /* Replace it in our command-line */
    argv[0] = emulatorPath;

    /* Launch it with the same set of options ! */
    /* execv() should be available on Windows with mingw32 */
    execv(emulatorPath, argv);

    /* We could not launch the program ! */
    fprintf(stderr, "Could not launch '%s': %s\n", emulatorPath, strerror(errno));
    return errno;
}


/* Find the target-specific emulator binary. This will be something
 * like  <programDir>/emulator-<targetArch>, where <programDir> is
 * the directory of the current program.
 */
static char*
getTargetEmulatorPath(const char* progName, const char* avdArch)
{
    char*  progDir;
    char   temp[PATH_MAX], *p=temp, *end=p+sizeof(temp);
#ifdef _WIN32
    const char* exeExt = ".exe";
#else
    const char* exeExt = "";
#endif

    /* Get program's directory name in progDir */
    path_split(progName, &progDir, NULL);

    p = bufprint(temp, end, "%s/emulator-%s%s", progDir, avdArch, exeExt);
    free(progDir);
    if (p >= end) {
        APANIC("Path too long: %s\n", progName);
    }

    if (path_exists(temp)) {
        return strdup(temp);
    }

    /* Mmm, the file doesn't exist, If there is no slash / backslash
     * in our path, we're going to try to search it in our path.
     */
#ifdef _WIN32
    if (strchr(progName, '/') == NULL && strchr(progName, '\\') == NULL) {
#else
    if (strchr(progName, '/') == NULL) {
#endif
        p = bufprint(temp, end, "emulator-%s%s", avdArch, exeExt);
        if (p < end) {
            char*  resolved = path_search_exec(temp);
            if (resolved != NULL)
                return resolved;
        }
    }

    /* Otherwise, the program is missing */
    APANIC("Missing arch-specific emulator program: %s\n", temp);
    return NULL;
}
