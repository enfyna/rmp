#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <taglib/tag_c.h>
#include <wchar.h>

#include "music.h"
#include "rmp.h"

#define BUF_SIZE 1024

int get_music_duration(const char* filepath)
{
    TagLib_File* file = taglib_file_new(filepath);
    if (!file || !taglib_file_is_valid(file)) {
        if (file)
            taglib_file_free(file);
        return 0;
    }
    const TagLib_AudioProperties* props = taglib_file_audioproperties(file);
    int length = taglib_audioproperties_length(props);
    taglib_file_free(file);
    return length;
}

char* get_time_str(long elapsed_seconds)
{
    static char buf[16] = { 0 };

    if (elapsed_seconds < 0)
        elapsed_seconds = 0;

    int hours = elapsed_seconds / 3600;
    int minutes = (elapsed_seconds % 3600) / 60;
    int seconds = elapsed_seconds % 60;

    snprintf(buf, 16, "%02d:%02d:%02d", hours, minutes, seconds);

    return buf;
}

const char* music_name_trim_suffix(const char* music_name)
{
    static char buf[NAME_MAX + 1];

    // music_name.hashxxxxxxx.mp3

    int len = strlen(music_name);
    if (len < 3)
        return music_name;

    if (len < 16) {
        snprintf(buf, NAME_MAX + 1, "%.*s", len - 4, music_name);
        return buf;
    }

    if (music_name[len - 16] == '.') {
        snprintf(buf, NAME_MAX + 1, "%.*s", len - 16, music_name);
        return buf;
    }

    if (music_name[len - 4] == '.') {
        snprintf(buf, NAME_MAX + 1, "%.*s", len - 4, music_name);
        return buf;
    }

    return music_name;
}

const char* music_name_get_hash(const char* music_name)
{
    static char buf[NAME_MAX + 1];

    // music_name.hashxxxxxxx.mp3

    int len = strlen(music_name);
    if (len < 16) {
        return NULL;
    }

    if (music_name[len - 16] == '.') {
        snprintf(buf, NAME_MAX + 1, "%.*s", 11, music_name + len - 15);
        return buf;
    }

    return NULL;
}

void load_music_from(DIR* dir, music_list* list, char* category)
{
    if (dir == NULL) {
        fwprintf(stderr, L"The music directory couldnt be opened!\n");
        return;
    }

    char buf[BUF_SIZE * 2] = { 0 };
    char buf2[BUF_SIZE] = { 0 };
    char buf3[BUF_SIZE] = { 0 };

    bool is_this_global_and_excluded = false;

    if (category == NULL && list->exclude != NULL) {
        snprintf(buf2, BUF_SIZE, "%s", list->exclude);
        for (char* tok = strtok(buf2, " "); !is_this_global_and_excluded && tok != NULL; tok = strtok(NULL, " ")) {
            is_this_global_and_excluded = strcmp(tok, ".") == 0 || strcmp(tok, "g") == 0 || strcmp(tok, "global") == 0;
        }
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        if (list->count == MUSIC_LIST_CAP) {
            break;
        }

        if (ent->d_type == DT_DIR) {
            bool isExcluded = false;

            if (category == NULL)
                sprintf(buf3, "%s", ent->d_name);
            else
                sprintf(buf3, "%s/%s", category, ent->d_name);

            if (list->exclude != NULL) {
                sprintf(buf2, "%s", list->exclude);
                for (char* tok = strtok(buf2, " "); !isExcluded && tok != NULL; tok = strtok(NULL, " ")) {
                    isExcluded = strcmp(tok, buf3) == 0 || strcmp(tok, ent->d_name) == 0;
                }
            }
            if (isExcluded)
                continue;

            snprintf(buf, BUF_SIZE * 2, "%s/%s", list->path, buf3);

            DIR* sub = opendir(buf);
            load_music_from(sub, list, buf3);
            if (sub == NULL) {
                wprintf(L"Couldnt open path: %ls\n", buf);
                wprintf(L"Errno[%d]: %ls\n", errno, strerror(errno));
            } else {
                closedir(sub);
            }
        } else if (ent->d_type == DT_REG) {
            if (is_this_global_and_excluded)
                continue;

            char file_name[NAME_MAX];
            strcpy(file_name, ent->d_name);
            size_t file_name_len = strlen(file_name);
            if (file_name_len < 3)
                continue;

            char* ext = &file_name[file_name_len - 3];
            for (size_t i = 0; i < 3; i++) {
                ext[i] = tolower(ext[i]);
            }

            if (strcmp(ext, "mp3") != 0)
                continue;

            music* m = &list->musics[list->count];

            strcpy(m->name, ent->d_name);
            if (category != NULL) {
                sprintf(m->category, "%s/", category);
                sprintf(buf, "./%s/%s", category, ent->d_name);
            } else {
                strcpy(m->category, "");
                sprintf(buf, "./%s", ent->d_name);
            }

            int duration = get_music_duration(buf);
            if (duration < 0)
                continue;

            struct stat s = { 0 };
            if (0 != stat(buf, &s)) {
                fwprintf(stderr, L"stat(): %ls\n", strerror(errno));
                continue;
            };

            m->file_last_modified = s.st_mtime;
            m->duration = duration;

            list->count += 1;

            wprint_line("\r", CLEAR_LINE L"Loaded %zu musics. [%s]\r", list->count, buf);
            fflush(stdout);
        }
    }

    if (category == NULL) {
        qsort(list, list->count, sizeof(music), list->sorter);
        wprint_line("\n", CLEAR_LINE L"Loaded %zu musics.\n", list->count, buf);
        fflush(stdout);
    }
}

music* selector_random(music_list* list, size_t seed)
{
    (void)seed;
    static size_t used[MUSIC_LIST_CAP] = { 0 };

    int start = rand() % list->count;

    for (size_t i = 0; i < list->count; i++) {
        int check = (start + i) % list->count;
        if (used[check] == 0) {
            used[check] = 1;
            return &list->musics[check];
        }
    }

    memset(used, 0, sizeof(size_t) * MUSIC_LIST_CAP);
    used[start] = 1;

    return &list->musics[start];
}

music* selector_linear(music_list* list, size_t seed)
{
    return &list->musics[seed % list->count];
}

music* selector_linear_reverse(music_list* list, size_t seed)
{
    seed %= list->count;
    return &list->musics[list->count - seed - 1];
}

int sort_last_modified_time(const void* a, const void* b)
{
    const music *m1 = a, *m2 = b;
    return m2->file_last_modified - m1->file_last_modified;
}

int sort_duration(const void* a, const void* b)
{
    const music *m1 = a, *m2 = b;
    return m2->duration - m1->duration;
}

int sort_category(const void* a, const void* b)
{
    const music *m1 = a, *m2 = b;

    int diff = strcmp(m1->category, m2->category);
    if (diff != 0)
        return diff;

    return strcmp(m1->name, m2->name);
}

int sort_title(const void* a, const void* b)
{
    const music *m1 = a, *m2 = b;
    return strcmp(m1->name, m2->name);
}
