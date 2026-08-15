#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "rmp.h"

bool arg(const char* cmd, const char* short_name, const char* long_name)
{
    return !strcmp(cmd, short_name) || !strcmp(cmd, long_name);
}

char* run_shell_command(char* dest, size_t dest_size, const char* cmd)
{
    FILE* f = popen(cmd, "r");
    if (f == NULL) {
        fwprintf(stderr, L"Failed to run: %s\n", cmd);
        return NULL;
    }

    size_t read = fread(dest, 1, dest_size, f);
    pclose(f);

    if (read == 0) {
        fwprintf(stderr, L"Failed to read any output: %s\n", cmd);
        return NULL;
    }

    if (read == dest_size) {
        fwprintf(stderr, L"Output length is equal or greater than the buffer size: %s\n", cmd);
        return NULL;
    }

    return dest;
}

int main(int argc, char** argv)
{
    setlocale(LC_ALL, "");

    if (argc < 2) {
        wprintf(
            L"  _ __ _ __ ___  _ __      \n"
            " | '__| '_ ` _ \\| '_ \\    \n"
            " | |  | | | | | | |_) |     \n"
            " |_|  |_| |_| |_| .__/      \n"
            "                |_|         random music player™\n"
            "                            \n"
            "Usage:\n"
            "\trmp <path> [selector] [play_flag]...\n"
            "\n"
            "Play Flags:\n"
            "\tMin. wait between musics:  [-min|--min-wait] <uint> (default:   0 seconds)\n"
            "\tMax. wait between musics:  [-max|--max-wait] <uint> (default: 600 seconds)\n"
            "\tExclude category:          [-e|--exclude] <string>\n"
            "\tSet random seed:           [-sd|--seed] <int>\n"
            "\tStart from index:          [-st|--start] <int>\n"
            "\tDont play instantly:       [-ni|--no-instant]\n"
            "\tPrint category list:                         [-lc|--list-categories]\n"
            "\tPrint music list with selected selector:     [-li|--list]\n"
            "Selectors:\n"
            "\tBy recency:                [-n | --newest]   [-o | --oldest]\n"
            "\tBy duration:               [-s | --shortest] [-l | --longest]\n"
            "\tBy title:                  [-t | --title]    [-tr| --title-reverse]\n"
            "\tBy category:               [-c | --category] [-cr| --category-reverse]\n"
            "\tDefaults to randomized selection.\n"
            "\n"
            "Note: If you want to exclude more than one category you should specify all of them in 1 string. e.g: -e \". fast/pop fast/rock slow\"\n"
            "See also: There are 'rmp add' and 'rmp build' subcommands available.\n");
        return 0;
    }

    if (strcmp(argv[1], "add") == 0) {
        return rmp_add(argc - 2, argv + 2);
    }

    if (strcmp(argv[1], "build") == 0) {
        return rmp_build(argc - 2, argv + 2);
    }

    return rmp_play(argc - 1, argv + 1);
}
