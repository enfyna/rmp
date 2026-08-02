#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "rmp.h"

#define clear_line "\033[2K"

#define BUF_SIZE 1024

typedef char* (*fn_get_music_name)(char*, size_t, char*);

char* get_music_name_mock(char* dest, size_t dest_size, char* music_hash)
{
    snprintf(dest, dest_size, "%s\n", music_hash);
    return dest;
}

char* get_music_name(char* dest, size_t dest_size, char* music_hash)
{
    char cmd[BUF_SIZE] = { 0 };
    sprintf(cmd, CMD_MUSIC_NAME, music_hash);
    return run_shell_command(dest, dest_size, cmd);
}

int rmp_build(int argc, char** argv)
{
    // TODO: Extract isnt implemented!
    // Extract will be used to create the .rmp file from the given directory.
    if (argc <= 0) {
        wprintf(
            L"                      ____        _ _     _              \n"
            " _ __ _ __ ___  _ __ | __ ) _   _(_) | __| |              \n"
            "| '__| '_ ` _ \\| '_ \\|  _ \\| | | | | |/ _` |           \n"
            "| |  | | | | | | |_) | |_) | |_| | | | (_| |              \n"
            "|_|  |_| |_| |_| .__/|____/ \\__,_|_|_|\\__,_|            \n"
            "               |_|            random music player™ Builder (WIP)\n"
            "                                                          \n"
            "Usage:\n"
            "\trmp build <path> [build_flags]...\n"
            "\n"
            "Build Flags:\n"
            "\tUse build file:          [-f|--file] <string> (default: '.rmp')\n"
            "\tUse mock functions:      [-m|--mock]             \n"
            "\tSimulate:                [-s|--simulate]         \n"
            "\tExtract:(not implemented)[-e|--extract]          \n");
        return 0;
    }

    time_t start = time(NULL);

    srand(start);

    char* path = argv[0];

    int res = chdir(path);
    if (res != 0) {
        fwprintf(stderr, L"Couldnt change to music directory!\n");
        return 1;
    }

    char buf[BUF_SIZE] = { 0 };

    if (strcmp(getcwd(buf, BUF_SIZE), path) == 0) {
        fwprintf(stderr, L"The music directory couldnt be determined!\n");
        fwprintf(stderr, L"'%s' != '%s'\n", buf, path);
        return 2;
    }

    bool mock = false;
    // bool extract = false;
    bool simulate = false;
    char build_file_name[NAME_MAX + 1] = { 0 };

    for (int i = 1; i < argc; i++) {
        char* cmd = argv[i];

        if (arg(cmd, "-f", "--file")) {
            strcpy(build_file_name, argv[++i]);
        } else if (arg(cmd, "-s", "--simulate")) {
            simulate = true;
        } else if (arg(cmd, "-m", "--mock")) {
            mock = true;
        } else if (arg(cmd, "-e", "--extract")) {
            // extract = true;
            fwprintf(stderr, L"Warning: Skipping --extract (not implemented)\n");
        } else {
            fwprintf(stderr, L"Invalid argument: %s\n", cmd);
            return 1;
        }
    }

    if (strlen(build_file_name) == 0) {
        strcpy(build_file_name, ".rmp");
    }

    char buf2[BUF_SIZE] = { 0 };
    char buf3[BUF_SIZE] = { 0 };

    char simbuf[BUF_SIZE] = { 0 };
    int written = 0;

    FILE* f = fopen(build_file_name, "r");
    if (f == NULL) {
        fwprintf(stderr, L"Couldnt find build file: %s\n", build_file_name);
        return 1;
    }

    fn_get_music_name get_name = mock ? get_music_name_mock : get_music_name;

    if (simulate) {
        wprintf(L"Simulating...\n");
    }

    size_t line_count = 0;
    while (NULL != (fgets(buf, BUF_SIZE, f))) {
        wprintf(L"line: %zu\r", ++line_count);
        fflush(stdout);

        char* dir = strtok(buf, ":");
        char* hash = strtok(NULL, " |\n\0");

        if (dir == NULL) {
            fwprintf(stderr, L"Couldnt find entry dir!");
            continue;
        }

        if (hash == NULL) {
            fwprintf(stderr, L"Couldnt find entry hash!");
            continue;
        }

        if (!simulate) {
            sprintf(buf2, CMD_MKDIR, dir);
            if (system(buf2) != 0) {
                fwprintf(stderr, L"Error while creating folder: %s\n", dir);
                break;
            }
        }

        char* name = get_name(buf2, BUF_SIZE, hash);

        if (simulate) {
            written += snprintf(simbuf + written, BUF_SIZE - written, "%s: %s", dir, name);
        } else {
            sprintf(buf3, CMD_DL_MUSIC, dir, hash, hash);
            system(buf3);
        }
    };

    fclose(f);

    wprintf(clear_line L"Complete!\n");

    if (simulate) {
        wprintf(L"%s", simbuf);
    }

    return 0;
}
