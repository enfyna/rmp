#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "music.h"
#include "rmp.h"

typedef char* (*fn_get_music_name)(char*, size_t, char*);

char* get_music_name_mock(char* dest, size_t dest_size, char* music_hash)
{
    snprintf(dest, dest_size, "%s", music_hash);
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
    if (argc <= 0) {
        wprintf(
            L"                      ____        _ _     _              \n"
            " _ __ _ __ ___  _ __ | __ ) _   _(_) | __| |              \n"
            "| '__| '_ ` _ \\| '_ \\|  _ \\| | | | | |/ _` |           \n"
            "| |  | | | | | | |_) | |_) | |_| | | | (_| |              \n"
            "|_|  |_| |_| |_| .__/|____/ \\__,_|_|_|\\__,_|            \n"
            "               |_|            random music player™ Builder\n"
            "                                                          \n"
            "Usage:\n"
            "\trmp build <mode> [build_flags]...\n"
            "\n"
            "Mode:\n"
            "\tExtract:                 [e|extract] \n"
            "\tDownload:                [d|download]\n"
            "\tSimulate:                [s|simulate]\n"
            "Build Flags:\n"
            "\tUse build file:          [-f|--file] <string> (default: '.rmp')\n");
        // "\tSimulate:\n"
        // "\t└Use mock functions:     [-m|--mock] \n"
        // "\tNote: --mock is used to test simulating without calling yt-dlp.\n");
        return 0;
    }

    time_t start = time(NULL);

    srand(start);

    char cwd[BUF_SIZE] = { 0 };

    if (strcmp(getcwd(cwd, BUF_SIZE), cwd) != 0) {
        fwprintf(stderr, L"The music directory couldnt be determined!\n");
        return 2;
    }

    bool mock = false;
    bool extract = false;
    bool download = false;
    bool simulate = false;
    char build_file_name[NAME_MAX + 1] = { 0 };

    for (int i = 0; i < argc; i++) {
        char* cmd = argv[i];

        if (arg(cmd, "-f", "--file")) {
            strcpy(build_file_name, argv[++i]);
        } else if (arg(cmd, "e", "extract")) {
            extract = true;
        } else if (arg(cmd, "d", "download")) {
            download = true;
        } else if (arg(cmd, "s", "simulate")) {
            simulate = true;
        } else if (arg(cmd, "-m", "--mock")) {
            mock = true;
        } else {
            fwprintf(stderr, L"Error: Unknown argument: %s\n", cmd);
            return 1;
        }
    }

    if (download + simulate + extract != 1) {
        fwprintf(stderr, L"Only one of 'simulate', 'download' or 'extract' has to be passed!\n");
        return 1;
    }

    if (strlen(build_file_name) == 0) {
        strcpy(build_file_name, ".rmp");
    }

    char buf[BUF_SIZE] = { 0 };
    char buf2[BUF_SIZE] = { 0 };
    char buf3[BUF_SIZE] = { 0 };

    fn_get_music_name get_name = mock ? get_music_name_mock : get_music_name;

    music_list list = { .sorter = sort_last_modified_time };
    strcpy(list.path, cwd);

    DIR* dir = opendir(list.path);
    load_music_from(dir, &list);
    closedir(dir);

    if (extract) {
        FILE* f = fopen(build_file_name, "w");
        if (f == NULL) {
            fwprintf(stderr, L"Couldnt open build file: %s\n", build_file_name);
            return 1;
        }

        int line = 0;
        for (size_t i = 0; i < list.count; i++) {
            music m = list.musics[i];
            const char* name = music_name_trim_suffix(m.name);
            const char* hash = music_name_get_hash(m.name);
            if (hash == NULL) {
                wprint_line("\n", L"No hash for: %s", name);
                continue;
            }
            int category_len = strlen(m.category);
            if (category_len == 0) {
                fprintf(f, ".: %s | %s\n", hash, name);
            } else {
                fprintf(f, "%.*s: %s | %s\n", category_len - 1, m.category, hash, name);
            }
            line++;
        }

        fclose(f);

        wprintf(L"Successfully extracted %d musics to '%s'!\n", line, build_file_name);

        return 0;
    }

    FILE* f = fopen(build_file_name, "r");
    if (f == NULL) {
        fwprintf(stderr, L"Couldnt find build file: %s\n", build_file_name);
        return 1;
    }

    if (download && list.count > 0) {
        wprintf(L"Requested downloading but folder isnt empty. Continue?\n"
                "(Expected an empty folder)\n");
        scanf("0");
    }

    if (simulate) {
        wprintf(L"Simulating. No musics will be downloaded.\n");
    }

    char* simbuf = calloc(BUF_SIZE, sizeof(char));
    size_t simbuf_count = BUF_SIZE;
    int written = 0;

    size_t download_count = 0;
    size_t line_count = 0;
    while (NULL != (fgets(buf, BUF_SIZE, f))) {
        wprintf(L"Line: %zu\r", ++line_count);
        fflush(stdout);
        char* cat = strtok(buf, ":");
        if (cat == NULL) {
            fwprintf(stderr, L"line %zu: Couldnt find entry category!\n", line_count);
            continue;
        }

        char* hash = strtok(NULL, "|");
        if (hash == NULL) {
            fwprintf(stderr, L"line %zu: Couldnt find entry hash!\n", line_count);
            continue;
        }
        hash += 1; // trim spaces
        hash[11] = 0;

        char* entry_name = strtok(NULL, "\n\0") + 1; // skip first space

        if (simulate) {
            if (simbuf_count - written < 100) {
                simbuf_count *= 2;
                simbuf = realloc(simbuf, simbuf_count);
            }
            char* name = (!mock || entry_name == NULL || strlen(entry_name) == 0)
                ? get_name(buf2, BUF_SIZE, hash)
                : entry_name;
            if (name == NULL) {
                written += snprintf(simbuf + written, simbuf_count - written, "%s:%zu => Not Found: %s (%s)\n", build_file_name, line_count, hash, entry_name);
            } else {
                written += snprintf(simbuf + written, simbuf_count - written, "%s: %s | %s\n", cat, hash, name);
            }
        } else if (download) {
            sprintf(buf2, CMD_MKDIR, cat);
            if (system(buf2) != 0) {
                fwprintf(stderr, L"Error while creating folder: '%s'\n", cat);
                break;
            }
            if (entry_name == NULL || strlen(entry_name) == 0) {
                sprintf(buf3, CMD_DL_MUSIC, cat, hash, hash);
            } else {
                sprintf(buf3, CMD_DL_MUSIC_NAME, cat, entry_name, hash, hash);
            }
            if (system(buf3) == 0) {
                download_count++;
            }
        }
    }

    if (simulate) {
        wprintf(L"\n\n%s", simbuf);
    }

    if (download) {
        wprintf(
            L"\nCompleted! Successfully downloaded %zu musics!\n"
            "You can delete '%s' now.\n"
            "Use 'rmp build -e' to regenerate it later.\n",
            download_count, build_file_name);
    }

    free(simbuf);
    fclose(f);

    return 0;
}
