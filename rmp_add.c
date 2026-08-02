#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wchar.h>

#include "rmp.h"

char* get_music_name_from_youtube(char* dest, size_t dest_size, const char* music_hash)
{
    char cmd[BUF_SIZE] = { 0 };
    sprintf(cmd, CMD_MUSIC_NAME, music_hash);
    return run_shell_command(dest, dest_size, cmd);
}

int download_music_from_youtube(const char* category, const char* music_hash)
{
    char buf[BUF_SIZE] = { 0 };
    sprintf(buf, CMD_DL_MUSIC, category, music_hash, music_hash);
    return system(buf);
}

int rmp_add(int argc, char** argv)
{
    if (argc < 3) {
        wprintf(
            L"                        _       _     _                \n"
            " _ __ _ __ ___  _ __   / \\   __| | __| |               \n"
            "| '__| '_ ` _ \\| '_ \\ / _ \\ / _` |/ _` |             \n"
            "| |  | | | | | | |_) / ___ \\ (_| | (_| |               \n"
            "|_|  |_| |_| |_| .__/_/   \\_\\__,_|\\__,_|             \n"
            "               |_|            random music player™ Adder\n"
            "Usage: \n"
            "\trmp add <path> <category> <music>...\n"
            "\n"
            "Path    : <string>\n"
            "Category: <string>\n"
            "\tSubfolder where the musics will be placed.\n"
            "\t=> rmp add <path> fast/pop <music>...\n"
            "\tWill place the musics in <path>/fast/pop/.\n"
            "\tYou can use '.' or '-' to place them in <path>.\n"
            "Music   : <string>\n"
            "\tYoutube hash id of the music.\n"
            "\t=> https://www.youtube.com/watch?v=<music>\n");
        return 0;
    }

    char* path = argv[0];
    char* category = argv[1];

    fwprintf(stderr, L"path = '%s', category = '%s'\n", path, category);

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

    if (strcmp("-", category) != 0) {
        category = ".";
    }

    if (strcmp(".", category) != 0) {
        snprintf(buf, BUF_SIZE, CMD_MKDIR, category);
        if (system(buf) != 0) {
            fwprintf(stderr, L"Error while creating folder: %s\n", category);
            return 3;
        }
    }

    FILE* f = fopen(".rmp", "a");

    for (int i = 2; i < argc; i++) {
        const char* hash = argv[i];
        wprintf(L"Searching: %s\r", hash);
        char* name = get_music_name_from_youtube(buf, BUF_SIZE, hash);
        if (name == NULL) {
            fwprintf(stderr, L"Couldnt find: %s\n", hash);
            continue;
        }
        wprintf(L"%s: %s | %s", category, hash, name);
        int res = download_music_from_youtube(category, hash);
        if (res != 0) {
            fwprintf(stderr, L"Couldnt download: %s\n", hash);
            continue;
        }
        fprintf(f, "%s: %s | %s", category, hash, name);
    }

    fclose(f);

    return 0;
}
