#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#include "music.h"
#include "rmp.h"

char* get_playlist_from_youtube(char* dest, size_t dest_size, const char* playlist_hash)
{
    char cmd[BUF_SIZE] = { 0 };
    sprintf(cmd, CMD_GET_PLAYLIST, playlist_hash);
    return run_shell_command(dest, dest_size, cmd);
}

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

int does_hash_exist(music_list* list, const char* hash)
{
    for (size_t i = 0; i < list->count; i++) {
        const char* h = music_name_get_hash(list->musics[i].name);
        if (h != NULL && strcmp(h, hash) == 0)
            return i;
    }
    return -1;
}

static char failed_hashes[BUF_SIZE] = { 0 };
static int total_added = 0;
void rmp_download_music(music_list* list, const char* category, const char* hash)
{
    char buf[BUF_SIZE] = { 0 };

    int i = does_hash_exist(list, hash);
    if (i >= 0) {
        music m = list->musics[i];
        strcat(failed_hashes, "Exists   : ");
        strcat(failed_hashes, hash);
        strcat(failed_hashes, " -> ");
        strcat(failed_hashes, m.category);
        strcat(failed_hashes, m.name);
        strcat(failed_hashes, "\n");
        fwprintf(stderr, L"Didnt download: %s -> %s%s\n", hash, m.category, m.name);
        return;
    }

    char* name = get_music_name_from_youtube(buf, BUF_SIZE, hash);
    if (name == NULL) {
        strcat(failed_hashes, "Not found: ");
        strcat(failed_hashes, hash);
        strcat(failed_hashes, "\n");
        fwprintf(stderr, L"Couldnt find: %s\n", hash);
        return;
    }
    int res = download_music_from_youtube(category, hash);
    if (res != 0) {
        strcat(failed_hashes, "Failed   : ");
        strcat(failed_hashes, hash);
        strcat(failed_hashes, "\n");
        fwprintf(stderr, L"Couldnt download: %s\n", hash);
        return;
    }
    wprintf(L"Download successfull: %s\n", hash);
    wprintf(L"%s: %s | %s", category, hash, name);
    total_added += 1;

    DIR* dir = opendir(list->path);
    load_music_from(dir, list);
    closedir(dir);
}

int rmp_add(int argc, char** argv)
{
    if (argc < 2) {
        wprintf(
            L"                        _       _     _                \n"
            " _ __ _ __ ___  _ __   / \\   __| | __| |               \n"
            "| '__| '_ ` _ \\| '_ \\ / _ \\ / _` |/ _` |             \n"
            "| |  | | | | | | |_) / ___ \\ (_| | (_| |               \n"
            "|_|  |_| |_| |_| .__/_/   \\_\\__,_|\\__,_|             \n"
            "               |_|            random music player™ Adder\n"
            "Usage: \n"
            "\trmp add <category> [music]... [-p <playlist>...]\n"
            "\n"
            "Category: <string>\n"
            "\tSubfolder where the musics will be placed.\n"
            "\t=> rmp add <path> fast/pop [music]...\n"
            "\tWill place the musics in <path>/fast/pop/.\n"
            "\tYou can use '.' or '-' to place them in <path>.\n"
            "Music   : <string>\n"
            "\tYoutube hash id of the music.\n"
            "\t=> https://www.youtube.com/watch?v=<music>\n"
            "Playlist: <string>\n"
            "\tYoutube hash id of the playlist.\n"
            "\t=> https://www.youtube.com/playlist?list=<playlist>\n");
        return 0;
    }

    char* category = argv[0];

    char buf[BUF_SIZE] = { 0 };

    if (strcmp(getcwd(buf, BUF_SIZE), buf) != 0) {
        fwprintf(stderr, L"The music directory couldnt be determined!\n");
        return 2;
    }

    music_list music_list = { .sorter = sort_last_modified_time };
    getcwd(music_list.path, NAME_MAX);

    DIR* dir = opendir(music_list.path);
    load_music_from(dir, &music_list);
    closedir(dir);

    fwprintf(stderr, L"path = '%s', category = '%s'\n", buf, category);
    wprintf(L"Continue?", category);
    scanf("0");

    if (strcmp("-", category) == 0) {
        category = ".";
    }

    if (strcmp(".", category) != 0) {
        snprintf(buf, BUF_SIZE, CMD_MKDIR, category);
        if (system(buf) != 0) {
            fwprintf(stderr, L"Error while creating folder: %s\n", category);
            return 3;
        }
    }

    bool playlist_mode = false;

    for (int i = 1; i < argc; i++) {
        const char* cmd = argv[i];
        if (!playlist_mode && strcmp(cmd, "-p") == 0) {
            playlist_mode = true;
            continue;
        }
        if (playlist_mode) {
            const char* playlist = argv[i];
            char* list = get_playlist_from_youtube(buf, BUF_SIZE, playlist);
            if (list == NULL) {
                fwprintf(stderr, L"Couldnt find playlist: %s\n", playlist);
                continue;
            }
            wprintf(L"Found entries for '%s'", playlist);
            for (char* music = strtok(buf, "\n"); music != NULL; music = strtok(NULL, "\n")) {
                rmp_download_music(&music_list, category, music);
            }
        } else {
            const char* hash = argv[i];
            if (strlen(hash) != 11) {
                fwprintf(stderr, L"Hash length is not 11: %s\n", hash);
                continue;
            }
            rmp_download_music(&music_list, category, hash);
        }
    }

    wprintf(L"Successfully added %d new musics to '%s'\n", total_added, category);

    if (strlen(failed_hashes) > 0) {
        wprintf(L"%s", failed_hashes);
    }

    return 0;
}
