/*
 * Nulib2
 * Copyright (C) 2000 by Andy McFadden, All Rights Reserved.
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * Main entry point and shell command argument processing.
 */
#include "Nulib2.h"
#include <ctype.h>


/*
 * Globals and constants.
 */
const char* gProgName = "Nulib2";


/*
 * Which modifiers are valid with which commands?
 */
typedef struct ValidCombo {
    Command     cmd;
    Boolean     okayForPipe;
    Boolean     filespecRequired;
    const char* modifiers;
} ValidCombo;

static const ValidCombo gValidCombos[] = {
    { kCommandAdd,              false,  true,   "ekcz0jrfu" },
    { kCommandDelete,           false,  true,   "r" },
    { kCommandExtract,          true,   false,  "beslcjrfu" },
    { kCommandExtractToPipe,    true,   false,  "blr" },
    { kCommandListShort,        true,   false,  "br" },
    { kCommandListVerbose,      true,   false,  "br" },
    { kCommandListDebug,        true,   false,  "b" },
    { kCommandTest,             true,   false,  "br" },
    { kCommandHelp,             false,  false,  "" },
};


/*
 * Find an entry in the gValidCombos table matching the specified command.
 *
 * Returns nil if not found.
 */
static const ValidCombo*
FindValidComboEntry(Command cmd)
{
    int i;

    for (i = 0; i < NELEM(gValidCombos); i++) {
        if (gValidCombos[i].cmd == cmd)
            return &gValidCombos[i];
    }

    return nil;
}

/*
 * Determine whether the specified modifier is valid when used with the
 * current command.
 */
static Boolean
IsValidModifier(Command cmd, char modifier)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        if (strchr(pvc->modifiers, modifier) == nil)
            return false;
        else
            return true;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean
IsValidOnPipe(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        return pvc->okayForPipe;
    } else
        return false;
}

/*
 * Determine whether the specified command can be used with stdin as input.
 */
static Boolean
IsFilespecRequired(Command cmd)
{
    const ValidCombo* pvc;

    pvc = FindValidComboEntry(cmd);
    if (pvc != nil) {
        return pvc->filespecRequired;
    } else {
        /* command not found?  warn about it here... */
        fprintf(stderr, "%s: Command %d not found in gValidCombos table\n",
            gProgName, cmd);
        return false;
    }
}


/*
 * Separate the program name out of argv[0].
 */
static const char*
GetProgName(const NulibState* pState, const char* argv0)
{
    const char* result;
    char sep;

    /* use the appropriate system pathname separator */
    sep = NState_GetSystemPathSeparator(pState);

    result = strrchr(argv0, sep);
    if (result == nil)
        result = argv0;
    else
        result++;   /* advance past the separator */
    
    return result;
}


/*
 * Print program usage.
 */
static void
Usage(const NulibState* pState)
{
    long majorVersion, minorVersion, bugVersion;
    const char* nufxLibDate;
    const char* nufxLibFlags;

    (void) NuGetVersion(&majorVersion, &minorVersion, &bugVersion,
            &nufxLibDate, &nufxLibFlags);

    printf("\nNulib2 v%s, linked with NufxLib v%ld.%ld.%ld [%s]\n",
        NState_GetProgramVersion(pState),
        majorVersion, minorVersion, bugVersion, nufxLibFlags);
    printf("This software is distributed under terms of the GNU General Public License.\n");
    printf("Written by Andy McFadden.  See http://www.nulib.com/ for full manual.\n\n");
    printf("Usage: %s -command[modifiers] archive [filename-list]\n\n",
        gProgName);
    printf(
        "  -a  add files, create arc if needed   -x  extract files\n"
        "  -t  list files (short)                -v  list files (verbose)\n"
        "  -p  extract files to pipe, no msgs    -i  test archive integrity\n"
        "  -d  delete files from archive         -h  extended help message\n"
        "\n"
        " modifiers:\n"
        "  -u  update files (add + keep newest)  -f  freshen (update, no add)\n"
        "  -r  recurse into subdirs              -j  junk (don't record) directory names\n"
        "  -0  don't use compression             -c  add one-line comments\n");
    if (NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone)
        printf("  -z  use gzip 'deflate' compression    ");
    else
        printf("  -z  use zlib [not included]           ");
    if (NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone)
        printf("-zz use bzip2 'BWT' compression\n");
    else
        printf("-zz use BWT [not included]\n");
    printf(
        "  -l  auto-convert text files           -ll convert CR/LF on ALL files\n"
        "  -s  stomp existing files w/o asking   -k  store files as disk images\n"
        "  -e  preserve ProDOS file types        -ee preserve types and extend names\n"
        "  -b  force Binary II mode\n"
        );
}


/*
 * Handle the "-h" command.
 */
NuError
DoHelp(const NulibState* pState)
{
    static const struct {
        Command cmd;
        char letter;
        const char* shortDescr;
        const char* longDescr;
    } help[] = {
        { kCommandListVerbose, 'v', "verbose listing of archive contents",
"  List files in the archive, blah blah blah\n"
        },
        { kCommandListShort, 't', "quick dump of table of contents",
"  shortList files in the archive, blah blah blah\n"
        },
        { kCommandAdd, 'a', "add files, creating the archive if necessary",
"  Add files to the archive, blah blah blah\n"
        },
        { kCommandDelete, 'd', "delete files from archive",
"  Delete files from the archive, blah blah blah\n"
        },
        { kCommandExtract, 'x', "extract files from an archive",
"  Extracts files, blah blah blah\n"
        },
        { kCommandExtractToPipe, 'p', "extract files to pipe",
"  Extracts files to stdout, blah blah blah\n"
        },
        { kCommandTest, 'i', "test archive integrity",
"  Tests files, blah blah blah\n"
        },
        { kCommandHelp, 'h', "show extended help",
"  This is the extended help text\n"
"  A full manual is available from http://www.nulib.com/.\n"
        },
    };

    int i;


    printf("%s",
"\n"
"NuLib2 is free software, distributed under terms of the GNU General\n"
"Public License.  NuLib2 uses NufxLib, a complete library of functions\n"
"for accessing NuFX (ShrinkIt) archives.  NufxLib is also free software,\n"
"distributed under terms of the GNU Library General Public License (LGPL).\n"
"Source code for both is available from http://www.nulib.com/, and copies\n"
"of the licenses are included.\n"
"\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"README file for more details.\n"
    );

    for (i = 0; i < NELEM(help); i++) {
        const ValidCombo* pvc;
        int j;

        pvc = FindValidComboEntry(help[i].cmd);
        if (pvc == nil) {
            fprintf(stderr, "%s: internal error: couldn't find vc for %d\n",
                gProgName, help[i].cmd);
            continue;
        }

        printf("\nCommand \"-%c\": %s\n", help[i].letter, help[i].shortDescr);
        printf("  Valid modifiers:");
        for (j = strlen(pvc->modifiers) -1; j >= 0; j--) {
            char ch = pvc->modifiers[j];
            /* print flags, special-casing options that can be doubled */
            if (ch == 'l' || ch == 'e' || ch == 'z')
                printf(" -%c -%c%c", ch, ch, ch);
            else
                printf(" -%c", ch);
        }
        putchar('\n');

        printf("\n%s", help[i].longDescr);
    }
    putchar('\n');

    printf("Compression algorithms supported by this copy of NufxLib:\n");
    printf("  Huffman SQueeze ...... %s\n",
        NuTestFeature(kNuFeatureCompressSQ) == kNuErrNone? "yes" : "no");
    printf("  LZW/1 and LZW/2 ...... %s\n",
        NuTestFeature(kNuFeatureCompressLZW) == kNuErrNone ? "yes" : "no");
    printf("  12- and 16-bit LZC ... %s\n",
        NuTestFeature(kNuFeatureCompressLZC) == kNuErrNone ? "yes" : "no");
    printf("  Deflate .............. %s\n",
        NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone ? "yes" : "no");
    printf("  bzip2 ................ %s\n",
        NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone ? "yes" : "no");


    return kNuErrNone;
}


/*
 * Process the command-line options.  The results are placed into "pState".
 */
static int
ProcessOptions(NulibState* pState, int argc, char* const* argv)
{
    const char* cp;
    int idx;

    /*
     * Must have at least a command letter and an archive filename, unless
     * the command letter is 'h'.  Special-case a solitary "-h" here.
     */
    if (argc == 2 && (tolower(argv[1][0]) == 'h' ||
                     (argv[1][0] == '-' && tolower(argv[1][1] == 'h')) ) )
    {
        DoHelp(nil);
        return -1;
    }

    if (argc < 3) {
        Usage(pState);
        return -1;
    }

    /*
     * Argv[1] and any subsequent entries that have a leading hyphen
     * are options.  Anything after that is a filename.  Parse until we
     * think we've hit the filename.
     *
     * By UNIX convention, however, stdin is specified as a file called "-".
     */
    for (idx = 1; idx < argc; idx++) {
        cp = argv[idx];

        if (idx > 1 && *cp != '-')
            break;

        if (*cp == '-')
            cp++;
        if (*cp == '\0') {
            if (idx == 1) {
                fprintf(stderr,
                    "%s: You must specify a command after the '-'\n",
                    gProgName);
                goto fail;
            } else {
                /* they're using '-' for the filename */
                break;
            }
        }

        if (idx == 1) {
            switch (tolower(*cp)) {
            case 'a': NState_SetCommand(pState, kCommandAdd);           break;
            case 'x': NState_SetCommand(pState, kCommandExtract);       break;
            case 'p': NState_SetCommand(pState, kCommandExtractToPipe); break;
            case 't': NState_SetCommand(pState, kCommandListShort);     break;
            case 'v': NState_SetCommand(pState, kCommandListVerbose);   break;
            case 'g': NState_SetCommand(pState, kCommandListDebug);     break;
            case 'i': NState_SetCommand(pState, kCommandTest);          break;
            case 'd': NState_SetCommand(pState, kCommandDelete);        break;
            case 'h': NState_SetCommand(pState, kCommandHelp);          break;
            default:
                fprintf(stderr, "%s: Unknown command '%c'\n", gProgName, *cp);
                goto fail;
            }

            cp++;
        }

        while (*cp != '\0') {
            switch (tolower(*cp)) {
            case 'u': NState_SetModUpdate(pState, true);                break;
            case 'f': NState_SetModFreshen(pState, true);               break;
            case 'r': NState_SetModRecurse(pState, true);               break;
            case 'j': NState_SetModJunkPaths(pState, true);             break;
            case '0': NState_SetModNoCompression(pState, true);         break;
            case 's': NState_SetModOverwriteExisting(pState, true);     break;
            case 'k': NState_SetModAddAsDisk(pState, true);             break;
            case 'c': NState_SetModComments(pState, true);              break;
            case 'b': NState_SetModBinaryII(pState, true);              break;
            case 'z':
                if (*(cp+1) == 'z') {
                    if (NuTestFeature(kNuFeatureCompressBzip2) == kNuErrNone)
                        NState_SetModCompressBzip2(pState, true);
                    else
                        fprintf(stderr,
                            "%s: WARNING: libbz2 support not compiled in\n",
                            gProgName);
                    cp++;
                } else {
                    if (NuTestFeature(kNuFeatureCompressDeflate) == kNuErrNone)
                        NState_SetModCompressDeflate(pState, true);
                    else
                        fprintf(stderr,
                            "%s: WARNING: zlib support not compiled in\n",
                            gProgName);
                }
                break;
            case 'e':
                if (*(cp-1) == 'e')     /* should never point at invalid */
                    NState_SetModPreserveTypeExtended(pState, true);
                else
                    NState_SetModPreserveType(pState, true);
                break;
            case 'l':
                if (*(cp-1) == 'l')     /* should never point at invalid */
                    NState_SetModConvertAll(pState, true);
                else
                    NState_SetModConvertText(pState, true);
                break;
            default:
                fprintf(stderr, "%s: Unknown modifier '%c'\n", gProgName, *cp);
                goto fail;
            }

            if (!IsValidModifier(NState_GetCommand(pState), (char)tolower(*cp)))
            {
                fprintf(stderr,
                    "%s: The '%c' modifier doesn't make sense here\n",
                    gProgName, tolower(*cp));
                goto fail;
            }

            cp++;
        }
    }

    /*
     * Can't have tea and no tea at the same time.
     */
    if (NState_GetModNoCompression(pState) &&
        NState_GetModCompressDeflate(pState))
    {
        fprintf(stderr, "%s: Can't specify both -0 and -z\n",
            gProgName);
        goto fail;
    }

    /*
     * See if we have an archive name.  If it's "-", see if we allow that.
     */
    Assert(idx < argc);
    NState_SetArchiveFilename(pState, argv[idx]);
    if (IsFilenameStdin(argv[idx])) {
        if (!IsValidOnPipe(NState_GetCommand(pState))) {
            fprintf(stderr, "%s: You can't do that with a pipe\n",
                gProgName);
            goto fail;
        }
    }
    idx++;

    /*
     * See if we have a file specification.  Some of the commands require
     * a filespec; others just perform the requested operation on all of
     * the records in the archive if none is provided.
     */
    if (idx < argc) {
        /* got one or more */
        NState_SetFilespecPointer(pState, &argv[idx]);
        NState_SetFilespecCount(pState, argc - idx);
    } else {
        Assert(idx == argc);
        if (IsFilespecRequired(NState_GetCommand(pState))) {
            fprintf(stderr, "%s: This command requires a list of files\n",
                gProgName);
            goto fail;
        }
        NState_SetFilespecPointer(pState, nil);
        NState_SetFilespecCount(pState, 0);
    }


#ifdef DEBUG_VERBOSE
    NState_DebugDump(pState);
#endif

    return 0;

fail:
    fprintf(stderr,
        "%s: (invoke without arguments to see usage information)\n",
        gProgName);
    return -1;
}


/*
 * We have all of the parsed command line options in "pState".  Now we just
 * have to do something useful with it.
 *
 * Returns 0 on success, 1 on error.
 */
int
DoWork(NulibState* pState)
{
    NuError err;

    switch (NState_GetCommand(pState)) {
    case kCommandAdd:
        err = DoAdd(pState);
        break;
    case kCommandExtract:
        err = DoExtract(pState);
        break;
    case kCommandExtractToPipe:
        err = DoExtractToPipe(pState);
        break;
    case kCommandTest:
        err = DoTest(pState);
        break;
    case kCommandListShort:
        err = DoListShort(pState);
        break;
    case kCommandListVerbose:
        err = DoListVerbose(pState);
        break;
    case kCommandListDebug:
        err = DoListDebug(pState);
        break;
    case kCommandDelete:
        err = DoDelete(pState);
        break;
    case kCommandHelp:
        err = DoHelp(pState);
        break;
    default:
        fprintf(stderr, "ERROR: unexpected command %d\n",
            NState_GetCommand(pState));
        err = kNuErrInternal;
        Assert(0);
        break;
    }

    return (err != kNuErrNone);
}

/*
 * Entry point.
 */
int
main(int argc, char** argv)
{
    NulibState* pState = nil;
    int result = 0;

    #if 0
    extern NuResult ErrorMessageHandler(NuArchive* pArchive,
        void* vErrorMessage);
    NuSetGlobalErrorMessageHandler(ErrorMessageHandler);
    #endif

    if (NState_Init(&pState) != kNuErrNone) {
        fprintf(stderr, "ERROR: unable to initialize globals\n");
        exit(1);
    }

    gProgName = GetProgName(pState, argv[0]);

    if (ProcessOptions(pState, argc, argv) < 0) {
        result = 2;
        goto bail;
    }

    if (NState_ExtraInit(pState) != kNuErrNone) {
        fprintf(stderr, "ERROR: additional initialization failed\n");
        exit(1);
    }

    result = DoWork(pState);
    if (result)
        printf("Failed.\n");

bail:
    NState_Free(pState);
    exit(result);
}

