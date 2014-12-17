/* Copyright (C) 2007-2008 The Android Open Source Project
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
#include "android/utils/debug.h"
#include "android/utils/timezone.h"
#include "android/utils/bufprint.h"
#include "android/android.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "qemu-common.h"

#define  DEBUG  1

#if 1
#  define  D_ACTIVE   VERBOSE_CHECK(timezone)
#else
#  define  D_ACTIVE   DEBUG
#endif

#if DEBUG
#  define  D(...)  do { if (D_ACTIVE) fprintf(stderr, __VA_ARGS__); } while (0)
#else
#  define  D(...)  ((void)0)
#endif



static const char* get_zoneinfo_timezone( void );  /* forward */

static char         android_timezone0[256];
static const char*  android_timezone;
static int          android_timezone_init;

static int
check_timezone_is_zoneinfo(const char*  tz)
{
    const char*  slash1 = NULL, *slash2 = NULL;

    if (tz == NULL)
        return 0;

    /* the name must be of the form Area/Location or Area/Location/SubLocation */
    slash1 = strchr( tz, '/' );
    if (slash1 == NULL || slash1[1] == 0)
        return 0;

    slash2 = strchr( slash1+1, '/');
    if (slash2 != NULL) {
        if (slash2[1] == 0 || strchr(slash2+1, '/') != NULL)
            return 0;
    }

    return 1;
}

int
timezone_set( const char*  tzname )
{
    int   len;

    if ( !check_timezone_is_zoneinfo(tzname) )
        return -1;

    len = strlen(tzname);
    if (len > sizeof(android_timezone0)-1)
        return -1;

    strcpy( android_timezone0, tzname );
    android_timezone      = android_timezone0;
    android_timezone_init = 1;

    return 0;
}


char*
bufprint_zoneinfo_timezone( char*  p, char*  end )
{
    const char*  tz = get_zoneinfo_timezone();

    if (tz == NULL || !check_timezone_is_zoneinfo(tz))
        return bufprint(p, end, "Unknown/Unknown");
    else
        return bufprint(p, end, "%s", tz);
}

/* on OS X, the timezone directory is always /usr/share/zoneinfo
 * this makes things easy.
 */
#if defined(__APPLE__)

#include <unistd.h>
#include <limits.h>
#define  LOCALTIME_FILE  "/etc/localtime"
#define  ZONEINFO_DIR    "/usr/share/zoneinfo/"
static const char*
get_zoneinfo_timezone( void )
{
    if (!android_timezone_init) {
        const char*  tz = getenv("TZ");
        char         buff[PATH_MAX+1];

        android_timezone_init = 1;
        if (tz == NULL) {
            int   len = readlink(LOCALTIME_FILE, buff, sizeof(buff));
            if (len < 0) {
                dprint( "### WARNING: Could not read %s, something is very wrong on your system",
                        LOCALTIME_FILE);
                return NULL;
            }

            buff[len] = 0;
            D("%s: %s points to %s\n", __FUNCTION__, LOCALTIME_FILE, buff);
            if ( memcmp(buff, ZONEINFO_DIR, sizeof(ZONEINFO_DIR)-1) ) {
                dprint( "### WARNING: %s does not point to %s, can't determine zoneinfo timezone name",
                        LOCALTIME_FILE, ZONEINFO_DIR );
                return NULL;
            }
            tz = buff + sizeof(ZONEINFO_DIR)-1;
            if ( !check_timezone_is_zoneinfo(tz) ) {
                dprint( "### WARNING: %s does not point to zoneinfo-compatible timezone name\n", LOCALTIME_FILE );
                return NULL;
            }
        }
        snprintf(android_timezone0, sizeof(android_timezone0), "%s", tz );
        android_timezone = android_timezone0;
    }
    D( "found timezone %s", android_timezone );
    return android_timezone;
}

#endif  /* __APPLE__ */

/* on Linux, with glibc2, the zoneinfo directory can be changed with TZDIR environment variable
 * but should be /usr/share/zoneinfo by default. /etc/localtime is not guaranteed to exist on
 * all platforms, so if it doesn't, try $TZDIR/localtime, then /usr/share/zoneinfo/locatime
 * ugly, isn't it ?
 *
 * besides, modern Linux distribution don't make /etc/localtime a symlink but a straight copy of
 * the original timezone file. the only way to know which zoneinfo name to retrieve is to compare
 * it with all files in $TZDIR (at least those that match Area/Location or Area/Location/SubLocation
 */
#if defined(__linux__) || defined (__FreeBSD__)

#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#define  ZONEINFO_DIR  "/usr/share/zoneinfo/"
#define  LOCALTIME_FILE1  "/etc/localtime"

typedef struct {
    const char*   localtime;
    struct stat   localtime_st;
    char*         path_end;
    char*         path_root;
    char          path[ PATH_MAX ];
} ScanDataRec;

static int
compare_timezone_to_localtime( ScanDataRec*  scan,
                               const char*   path )
{
    struct  stat  st;
    int           fd1, fd2, result = 0;

    D( "%s: comparing %s:", __FUNCTION__, path );

    if ( stat( path, &st ) < 0 ) {
        D( " can't stat: %s\n", strerror(errno) );
        return 0;
    }

    if ( st.st_size != scan->localtime_st.st_size ) {
        D( " size mistmatch (%zd != %zd)\n", (size_t)st.st_size, (size_t)scan->localtime_st.st_size );
        return 0;
    }

    fd1 = open( scan->localtime, O_RDONLY );
    if (fd1 < 0) {
        D(" can't open %s: %s\n", scan->localtime, strerror(errno) );
        return 0;
    }
    fd2 = open( path, O_RDONLY );
    if (fd2 < 0) {
        D(" can't open %s: %s\n", path, strerror(errno) );
        close(fd1);
        return 0;
    }
    do {
        off_t  nn;

        for (nn = 0; nn < st.st_size; nn++) {
            char  temp[2];
            int   ret;

            do { ret = read(fd1, &temp[0], 1); } while (ret < 0 && errno == EINTR);
            if (ret < 0) break;

            do { ret = read(fd2, &temp[1], 1); } while (ret < 0 && errno == EINTR);
            if (ret < 0) break;

            if (temp[0] != temp[1])
                break;
        }

        result = (nn == st.st_size);

    } while (0);

    D( result ? " MATCH\n" : "no match\n" );

    close(fd2);
    close(fd1);

    return result;
}

static const char*
scan_timezone_dir( ScanDataRec*  scan,
                   char*         top,
                   int           depth )
{
    DIR*         d = opendir( scan->path );
    const char*  result = NULL;

    D( "%s: entering '%s\n", __FUNCTION__, scan->path );
    if (d != NULL) {
        struct  dirent*  ent;
        while ((ent = readdir(d)) != NULL) {
            struct stat   ent_st;
            char*         p = top;

            if  (ent->d_name[0] == '.')  /* avoid hidden and special files */
                continue;

            p = bufprint( p, scan->path_end, "/%s", ent->d_name );
            if (p >= scan->path_end)
                continue;

            //D( "%s: scanning '%s'\n", __FUNCTION__, scan->path );

            if ( stat( scan->path, &ent_st ) < 0 )
                continue;

            if ( S_ISDIR(ent_st.st_mode) && depth < 2 )
            {
                //D( "%s: directory '%s'\n", __FUNCTION__, scan->path );
                result = scan_timezone_dir( scan, p, depth + 1 );
                if (result != NULL)
                    break;
            }
            else if ( S_ISREG(ent_st.st_mode) && (depth >= 1 && depth <= 2) )
            {
                char*   name = scan->path_root + 1;

                if ( check_timezone_is_zoneinfo( name ) )
                {
                    if (compare_timezone_to_localtime( scan, scan->path ))
                    {
                        result = strdup( name );
                        D( "%s: found '%s'\n", __FUNCTION__, result );
                        break;
                    }
                }
                else
                {
                    //D( "%s: ignoring '%s'\n", __FUNCTION__, scan->path );
                }
            }
        }
        closedir(d);
    }
    return  result;
}

static const char*
get_zoneinfo_timezone( void )
{
    if (!android_timezone_init)
    {
        const char*  tz = getenv( "TZ" );

        android_timezone_init = 1;

        if ( tz != NULL && !check_timezone_is_zoneinfo(tz) ) {
            D( "%s: ignoring non zoneinfo formatted TZ environment variable: '%s'\n",
               __FUNCTION__, tz );
            tz = NULL;
        }

        if (tz == NULL) {
            char*        tzdir     = NULL;
            int          tzdirlen  = 0;
            char*        localtime = NULL;
            int          len;
            char         temp[ PATH_MAX ];

            /* determine the correct timezone directory */
            {
                const char*  env = getenv("TZDIR");
                const char*  zoneinfo_dir = ZONEINFO_DIR;

                if (env == NULL)
                    env = zoneinfo_dir;

                if ( access( env, R_OK ) != 0 ) {
                    if ( env == zoneinfo_dir ) {
                        fprintf( stderr,
                                 "### WARNING: could not find %s directory. unable to determine host timezone\n", env );
                    } else {
                        D( "%s: TZDIR does not point to valid directory, using %s instead\n",
                           __FUNCTION__, zoneinfo_dir );
                        env = zoneinfo_dir;
                    }
                    return NULL;
                }
                tzdir = strdup(env);
            }

            /* remove trailing slash, if any */
            len = strlen(tzdir);
            if (len > 0 && tzdir[len-1] == '/') {
                tzdir[len-1] = 0;
                len         -= 1;
            }
            tzdirlen = len;
            D( "%s: found timezone dir as %s\n", __FUNCTION__, tzdir );

            /* try to find the localtime file */
            localtime = LOCALTIME_FILE1;
            if ( access( localtime, R_OK ) != 0 ) {
                char  *p = temp, *end = p + sizeof(temp);

                p = bufprint( p, end, "%s/%s", tzdir, "localtime" );
                if (p >= end || access( temp, R_OK ) != 0 ) {
                    fprintf( stderr, "### WARNING: could not find %s or %s. unable to determine host timezone\n",
                                     LOCALTIME_FILE1, temp );
                    goto Exit;
                }
                localtime = temp;
            }
            localtime = strdup(localtime);
            D( "%s: found localtime file as %s\n", __FUNCTION__, localtime );

#if 1
            /* if the localtime file is a link, make a quick check */
            len = readlink( localtime, temp, sizeof(temp)-1 );
            if (len >= 0 && len > tzdirlen + 2) {
                temp[len] = 0;

                /* verify that the link points to tzdir/<something> where <something> is a valid zoneinfo name */
                if ( !memcmp( temp, tzdir, tzdirlen ) && temp[tzdirlen] == '/' ) {
                    if ( check_timezone_is_zoneinfo( temp + tzdirlen + 1 ) ) {
                        /* we have it ! */
                        tz = temp + tzdirlen + 1;
                        D( "%s: found zoneinfo timezone %s from %s symlink\n", __FUNCTION__, tz, localtime );
                        goto Exit;
                    }
                    D( "%s: %s link points to non-zoneinfo filename %s, comparing contents\n",
                       __FUNCTION__, localtime, temp );
                }
            }
#endif

            /* otherwise, parse all files under tzdir and see if we have something that looks like it */
            {
                ScanDataRec  scan[1];

                if ( stat( localtime, &scan->localtime_st ) < 0 ) {
                    fprintf( stderr, "### WARNING: can't access '%s', unable to determine host timezone\n",
                             localtime );
                    goto Exit;
                }

                scan->localtime = localtime;
                scan->path_end  = scan->path + sizeof(scan->path);
                scan->path_root = bufprint( scan->path, scan->path_end, "%s", tzdir );

                tz = scan_timezone_dir( scan, scan->path_root, 0 );
            }

        Exit:
            if (tzdir)
                free(tzdir);
            if (localtime)
                free(localtime);

            if (tz == NULL)
                return NULL;

            snprintf(android_timezone0, sizeof(android_timezone0), "%s", tz);
            android_timezone = android_timezone0;
        }
        D( "found timezone %s\n", android_timezone );
    }
    return android_timezone;
}

#endif /* __linux__ */


/* on Windows, we need to translate the Windows timezone into a ZoneInfo one */
#ifdef _WIN32
#include <time.h>

typedef struct {
    const char*  win_name;
    const char*  zoneinfo_name;
} Win32Timezone;

/* table generated from http://unicode.org/cldr/data/diff/supplemental/windows_tzid.html */
static const Win32Timezone  _win32_timezones[] = {
    { "AUS Central Standard Time"             , "Australia/Darwin" },
    { "AUS Eastern Standard Time"             , "Australia/Sydney" },
    { "Acre Standard Time"                    , "America/Rio_Branco" },
    { "Afghanistan Standard Time"             , "Asia/Kabul" },
    { "Africa_Central Standard Time"          , "Africa/Kigali" },
    { "Africa_Eastern Standard Time"          , "Africa/Kampala" },
    { "Africa_FarWestern Standard Time"       , "Africa/El_Aaiun" },
    { "Africa_Southern Standard Time"         , "Africa/Johannesburg" },
    { "Africa_Western Standard Time"          , "Africa/Niamey" },
    { "Aktyubinsk Standard Time"              , "Asia/Aqtobe" },
    { "Alaska Standard Time"                  , "America/Juneau" },
    { "Alaska_Hawaii Standard Time"           , "America/Anchorage" },
    { "Alaskan Standard Time"                 , "America/Anchorage" },
    { "Almaty Standard Time"                  , "Asia/Almaty" },
    { "Amazon Standard Time"                  , "America/Manaus" },
    { "America_Central Standard Time"         , "America/Winnipeg" },
    { "America_Eastern Standard Time"         , "America/Panama" },
    { "America_Mountain Standard Time"        , "America/Edmonton" },
    { "America_Pacific Standard Time"         , "America/Vancouver" },
    { "Anadyr Standard Time"                  , "Asia/Anadyr" },
    { "Aqtau Standard Time"                   , "Asia/Aqtau" },
    { "Aqtobe Standard Time"                  , "Asia/Aqtobe" },
    { "Arab Standard Time"                    , "Asia/Riyadh" },
    { "Arabian Standard Time"                 , "Asia/Bahrain" },
    { "Arabic Standard Time"                  , "Asia/Baghdad" },
    { "Argentina Standard Time"               , "America/Buenos_Aires" },
    { "Argentina_Western Standard Time"       , "America/Mendoza" },
    { "Armenia Standard Time"                 , "Asia/Yerevan" },
    { "Ashkhabad Standard Time"               , "Asia/Ashgabat" },
    { "Atlantic Standard Time"                , "America/Curacao" },
    { "Australia_Central Standard Time"       , "Australia/Adelaide" },
    { "Australia_CentralWestern Standard Time", "Australia/Eucla" },
    { "Australia_Eastern Standard Time"       , "Australia/Sydney" },
    { "Australia_Western Standard Time"       , "Australia/Perth" },
    { "Azerbaijan Standard Time"              , "Asia/Baku" },
    { "Azores Standard Time"                  , "Atlantic/Azores" },
    { "Baku Standard Time"                    , "Asia/Baku" },
    { "Bangladesh Standard Time"              , "Asia/Dhaka" },
    { "Bering Standard Time"                  , "America/Adak" },
    { "Bhutan Standard Time"                  , "Asia/Thimphu" },
    { "Bolivia Standard Time"                 , "America/La_Paz" },
    { "Borneo Standard Time"                  , "Asia/Kuching" },
    { "Brasilia Standard Time"                , "America/Sao_Paulo" },
    { "British Standard Time"                 , "Europe/London" },
    { "Brunei Standard Time"                  , "Asia/Brunei" },
    { "Canada Central Standard Time"          , "America/Regina" },
    { "Cape Verde Standard Time"              , "Atlantic/Cape_Verde" },
    { "Cape_Verde Standard Time"              , "Atlantic/Cape_Verde" },
    { "Caucasus Standard Time"                , "Asia/Yerevan" },
    { "Cen. Australia Standard Time"          , "Australia/Adelaide" },
    { "Central Standard Time"                 , "America/Chicago" },
    { "Central America Standard Time"         , "America/Guatemala" },
    { "Central Asia Standard Time"            , "Asia/Dhaka" },
    { "Central Brazilian Standard Time"       , "America/Manaus" },
    { "Central Europe Standard Time"          , "Europe/Prague" },
    { "Central European Standard Time"        , "Europe/Warsaw" },
    { "Central Pacific Standard Time"         , "Pacific/Guadalcanal" },
    { "Central Standard Time (Mexico)"        , "America/Mexico_City" },
    { "Chamorro Standard Time"                , "Pacific/Guam" },
    { "Changbai Standard Time"                , "Asia/Harbin" },
    { "Chatham Standard Time"                 , "Pacific/Chatham" },
    { "Chile Standard Time"                   , "America/Santiago" },
    { "China Standard Time"                   , "Asia/Taipei" },
    { "Choibalsan Standard Time"              , "Asia/Choibalsan" },
    { "Christmas Standard Time"               , "Indian/Christmas" },
    { "Cocos Standard Time"                   , "Indian/Cocos" },
    { "Colombia Standard Time"                , "America/Bogota" },
    { "Cook Standard Time"                    , "Pacific/Rarotonga" },
    { "Cuba Standard Time"                    , "America/Havana" },
    { "Dacca Standard Time"                   , "Asia/Dhaka" },
    { "Dateline Standard Time"                , "Pacific/Kwajalein" },
    { "Davis Standard Time"                   , "Antarctica/Davis" },
    { "Dominican Standard Time"               , "America/Santo_Domingo" },
    { "DumontDUrville Standard Time"          , "Antarctica/DumontDUrville" },
    { "Dushanbe Standard Time"                , "Asia/Dushanbe" },
    { "Dutch_Guiana Standard Time"            , "America/Paramaribo" },
    { "E. Africa Standard Time"               , "Africa/Nairobi" },
    { "E. Australia Standard Time"            , "Australia/Brisbane" },
    { "E. Europe Standard Time"               , "Europe/Minsk" },
    { "E. South America Standard Time"        , "America/Sao_Paulo" },
    { "East_Timor Standard Time"              , "Asia/Dili" },
    { "Easter Standard Time"                  , "Pacific/Easter" },
    { "Eastern Standard Time"                 , "America/New_York" },
    { "Ecuador Standard Time"                 , "America/Guayaquil" },
    { "Egypt Standard Time"                   , "Africa/Cairo" },
    { "Ekaterinburg Standard Time"            , "Asia/Yekaterinburg" },
    { "Europe_Central Standard Time"          , "Europe/Oslo" },
    { "Europe_Eastern Standard Time"          , "Europe/Vilnius" },
    { "Europe_Western Standard Time"          , "Atlantic/Canary" },
    { "FLE Standard Time"                     , "Europe/Helsinki" },
    { "Falkland Standard Time"                , "Atlantic/Stanley" },
    { "Fiji Standard Time"                    , "Pacific/Fiji" },
    { "French_Guiana Standard Time"           , "America/Cayenne" },
    { "French_Southern Standard Time"         , "Indian/Kerguelen" },
    { "Frunze Standard Time"                  , "Asia/Bishkek" },
    { "GMT Standard Time"                     , "Europe/Dublin" },
    { "GTB Standard Time"                     , "Europe/Istanbul" },
    { "Galapagos Standard Time"               , "Pacific/Galapagos" },
    { "Gambier Standard Time"                 , "Pacific/Gambier" },
    { "Georgia Standard Time"                 , "Asia/Tbilisi" },
    { "Georgian Standard Time"                , "Asia/Tbilisi" },
    { "Gilbert_Islands Standard Time"         , "Pacific/Tarawa" },
    { "Goose_Bay Standard Time"               , "America/Goose_Bay" },
    { "Greenland Standard Time"               , "America/Godthab" },
    { "Greenland_Central Standard Time"       , "America/Scoresbysund" },
    { "Greenland_Eastern Standard Time"       , "America/Scoresbysund" },
    { "Greenland_Western Standard Time"       , "America/Godthab" },
    { "Greenwich Standard Time"               , "Africa/Casablanca" },
    { "Guam Standard Time"                    , "Pacific/Guam" },
    { "Gulf Standard Time"                    , "Asia/Muscat" },
    { "Guyana Standard Time"                  , "America/Guyana" },
    { "Hawaii_Aleutian Standard Time"         , "Pacific/Honolulu" },
    { "Hawaiian Standard Time"                , "Pacific/Honolulu" },
    { "Hong_Kong Standard Time"               , "Asia/Hong_Kong" },
    { "Hovd Standard Time"                    , "Asia/Hovd" },
    { "India Standard Time"                   , "Asia/Calcutta" },
    { "Indian_Ocean Standard Time"            , "Indian/Chagos" },
    { "Indochina Standard Time"               , "Asia/Vientiane" },
    { "Indonesia_Central Standard Time"       , "Asia/Makassar" },
    { "Indonesia_Eastern Standard Time"       , "Asia/Jayapura" },
    { "Indonesia_Western Standard Time"       , "Asia/Jakarta" },
    { "Iran Standard Time"                    , "Asia/Tehran" },
    { "Irish Standard Time"                   , "Europe/Dublin" },
    { "Irkutsk Standard Time"                 , "Asia/Irkutsk" },
    { "Israel Standard Time"                  , "Asia/Jerusalem" },
    { "Japan Standard Time"                   , "Asia/Tokyo" },
    { "Jordan Standard Time"                  , "Asia/Amman" },
    { "Kamchatka Standard Time"               , "Asia/Kamchatka" },
    { "Karachi Standard Time"                 , "Asia/Karachi" },
    { "Kashgar Standard Time"                 , "Asia/Kashgar" },
    { "Kazakhstan_Eastern Standard Time"      , "Asia/Almaty" },
    { "Kazakhstan_Western Standard Time"      , "Asia/Aqtobe" },
    { "Kizilorda Standard Time"               , "Asia/Qyzylorda" },
    { "Korea Standard Time"                   , "Asia/Seoul" },
    { "Kosrae Standard Time"                  , "Pacific/Kosrae" },
    { "Krasnoyarsk Standard Time"             , "Asia/Krasnoyarsk" },
    { "Kuybyshev Standard Time"               , "Europe/Samara" },
    { "Kwajalein Standard Time"               , "Pacific/Kwajalein" },
    { "Kyrgystan Standard Time"               , "Asia/Bishkek" },
    { "Lanka Standard Time"                   , "Asia/Colombo" },
    { "Liberia Standard Time"                 , "Africa/Monrovia" },
    { "Line_Islands Standard Time"            , "Pacific/Kiritimati" },
    { "Long_Shu Standard Time"                , "Asia/Chongqing" },
    { "Lord_Howe Standard Time"               , "Australia/Lord_Howe" },
    { "Macau Standard Time"                   , "Asia/Macau" },
    { "Magadan Standard Time"                 , "Asia/Magadan" },
    { "Malaya Standard Time"                  , "Asia/Kuala_Lumpur" },
    { "Malaysia Standard Time"                , "Asia/Kuching" },
    { "Maldives Standard Time"                , "Indian/Maldives" },
    { "Marquesas Standard Time"               , "Pacific/Marquesas" },
    { "Marshall_Islands Standard Time"        , "Pacific/Majuro" },
    { "Mauritius Standard Time"               , "Indian/Mauritius" },
    { "Mawson Standard Time"                  , "Antarctica/Mawson" },
    { "Mexico Standard Time"                  , "America/Mexico_City" },
    { "Mexico Standard Time 2 Standard Time"  , "America/Chihuahua" },
    { "Mid-Atlantic Standard Time"            , "America/Noronha" },
    { "Middle East Standard Time"             , "Asia/Beirut" },
    { "Mongolia Standard Time"                , "Asia/Ulaanbaatar" },
    { "Montevideo Standard Time"              , "America/Montevideo" },
    { "Moscow Standard Time"                  , "Europe/Moscow" },
    { "Mountain Standard Time"                , "America/Denver" },
    { "Mountain Standard Time (Mexico)"       , "America/Chihuahua" },
    { "Myanmar Standard Time"                 , "Asia/Rangoon" },
    { "N. Central Asia Standard Time"         , "Asia/Novosibirsk" },
    { "Namibia Standard Time"                 , "Africa/Windhoek" },
    { "Nauru Standard Time"                   , "Pacific/Nauru" },
    { "Nepal Standard Time"                   , "Asia/Katmandu" },
    { "New Zealand Standard Time"             , "Pacific/Auckland" },
    { "New_Caledonia Standard Time"           , "Pacific/Noumea" },
    { "New_Zealand Standard Time"             , "Pacific/Auckland" },
    { "Newfoundland Standard Time"            , "America/St_Johns" },
    { "Niue Standard Time"                    , "Pacific/Niue" },
    { "Norfolk Standard Time"                 , "Pacific/Norfolk" },
    { "Noronha Standard Time"                 , "America/Noronha" },
    { "North Asia Standard Time"              , "Asia/Krasnoyarsk" },
    { "North Asia East Standard Time"         , "Asia/Ulaanbaatar" },
    { "North_Mariana Standard Time"           , "Pacific/Saipan" },
    { "Novosibirsk Standard Time"             , "Asia/Novosibirsk" },
    { "Omsk Standard Time"                    , "Asia/Omsk" },
    { "Oral Standard Time"                    , "Asia/Oral" },
    { "Pacific Standard Time"                 , "America/Los_Angeles" },
    { "Pacific SA Standard Time"              , "America/Santiago" },
    { "Pacific Standard Time (Mexico)"        , "America/Tijuana" },
    { "Pakistan Standard Time"                , "Asia/Karachi" },
    { "Palau Standard Time"                   , "Pacific/Palau" },
    { "Papua_New_Guinea Standard Time"        , "Pacific/Port_Moresby" },
    { "Paraguay Standard Time"                , "America/Asuncion" },
    { "Peru Standard Time"                    , "America/Lima" },
    { "Philippines Standard Time"             , "Asia/Manila" },
    { "Phoenix_Islands Standard Time"         , "Pacific/Enderbury" },
    { "Pierre_Miquelon Standard Time"         , "America/Miquelon" },
    { "Pitcairn Standard Time"                , "Pacific/Pitcairn" },
    { "Ponape Standard Time"                  , "Pacific/Ponape" },
    { "Qyzylorda Standard Time"               , "Asia/Qyzylorda" },
    { "Reunion Standard Time"                 , "Indian/Reunion" },
    { "Romance Standard Time"                 , "Europe/Paris" },
    { "Rothera Standard Time"                 , "Antarctica/Rothera" },
    { "Russian Standard Time"                 , "Europe/Moscow" },
    { "SA Eastern Standard Time"              , "America/Buenos_Aires" },
    { "SA Pacific Standard Time"              , "America/Bogota" },
    { "SA Western Standard Time"              , "America/Caracas" },
    { "SE Asia Standard Time"                 , "Asia/Bangkok" },
    { "Sakhalin Standard Time"                , "Asia/Sakhalin" },
    { "Samara Standard Time"                  , "Europe/Samara" },
    { "Samarkand Standard Time"               , "Asia/Samarkand" },
    { "Samoa Standard Time"                   , "Pacific/Apia" },
    { "Seychelles Standard Time"              , "Indian/Mahe" },
    { "Shevchenko Standard Time"              , "Asia/Aqtau" },
    { "Singapore Standard Time"               , "Asia/Singapore" },
    { "Solomon Standard Time"                 , "Pacific/Guadalcanal" },
    { "South Africa Standard Time"            , "Africa/Johannesburg" },
    { "South_Georgia Standard Time"           , "Atlantic/South_Georgia" },
    { "Sri Lanka Standard Time"               , "Asia/Colombo" },
    { "Suriname Standard Time"                , "America/Paramaribo" },
    { "Sverdlovsk Standard Time"              , "Asia/Yekaterinburg" },
    { "Syowa Standard Time"                   , "Antarctica/Syowa" },
    { "Tahiti Standard Time"                  , "Pacific/Tahiti" },
    { "Taipei Standard Time"                  , "Asia/Taipei" },
    { "Tajikistan Standard Time"              , "Asia/Dushanbe" },
    { "Tashkent Standard Time"                , "Asia/Tashkent" },
    { "Tasmania Standard Time"                , "Australia/Hobart" },
    { "Tbilisi Standard Time"                 , "Asia/Tbilisi" },
    { "Tokelau Standard Time"                 , "Pacific/Fakaofo" },
    { "Tokyo Standard Time"                   , "Asia/Tokyo" },
    { "Tonga Standard Time"                   , "Pacific/Tongatapu" },
    { "Truk Standard Time"                    , "Pacific/Truk" },
    { "Turkey Standard Time"                  , "Europe/Istanbul" },
    { "Turkmenistan Standard Time"            , "Asia/Ashgabat" },
    { "Tuvalu Standard Time"                  , "Pacific/Funafuti" },
    { "US Eastern Standard Time"              , "America/Indianapolis" },
    { "US Mountain Standard Time"             , "America/Phoenix" },
    { "Uralsk Standard Time"                  , "Asia/Oral" },
    { "Uruguay Standard Time"                 , "America/Montevideo" },
    { "Urumqi Standard Time"                  , "Asia/Urumqi" },
    { "Uzbekistan Standard Time"              , "Asia/Tashkent" },
    { "Vanuatu Standard Time"                 , "Pacific/Efate" },
    { "Venezuela Standard Time"               , "America/Caracas" },
    { "Vladivostok Standard Time"             , "Asia/Vladivostok" },
    { "Volgograd Standard Time"               , "Europe/Volgograd" },
    { "Vostok Standard Time"                  , "Antarctica/Vostok" },
    { "W. Australia Standard Time"            , "Australia/Perth" },
    { "W. Central Africa Standard Time"       , "Africa/Lagos" },
    { "W. Europe Standard Time"               , "Europe/Berlin" },
    { "Wake Standard Time"                    , "Pacific/Wake" },
    { "Wallis Standard Time"                  , "Pacific/Wallis" },
    { "West Asia Standard Time"               , "Asia/Karachi" },
    { "West Pacific Standard Time"            , "Pacific/Guam" },
    { "Yakutsk Standard Time"                 , "Asia/Yakutsk" },
    { "Yekaterinburg Standard Time"           , "Asia/Yekaterinburg" },
    { "Yerevan Standard Time"                 , "Asia/Yerevan" },
    { "Yukon Standard Time"                   , "America/Yakutat" },
    { NULL, NULL }
};

static const char*
get_zoneinfo_timezone( void )
{
    if (!android_timezone_init)
    {
        char		          tzname[128];
        time_t		          t = time(NULL);
        struct tm*            tm = localtime(&t);
        const Win32Timezone*  win32tz = _win32_timezones;

        android_timezone_init = 1;

        if (!tm) {
            D("%s: could not determine current date/time\n", __FUNCTION__);
            return NULL;
        }

        memset(tzname, 0, sizeof(tzname));
        strftime(tzname, sizeof(tzname) - 1, "%Z", tm);

        for (win32tz = _win32_timezones; win32tz->win_name != NULL; win32tz++)
            if ( !strcmp(win32tz->win_name, tzname) ) {
                android_timezone = win32tz->zoneinfo_name;
                goto Exit;
            }

#if 0  /* TODO */
    /* we didn't find it, this may come from localized versions of Windows. we're going to explore the registry,
    * as the code in Postgresql does...
    */
#endif
        D( "%s: could not determine current timezone\n", __FUNCTION__ );
        return NULL;
    }
Exit:
    D( "emulator: found timezone %s\n", android_timezone );
    return android_timezone;
}

#endif /* _WIN32 */

