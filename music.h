#include <dirent.h>
#include <stddef.h>
#include <time.h>
#include <unistd.h>

#define MUSIC_LIST_CAP (1 << 10)

typedef int (*sorter)(const void*, const void*);
int sort_last_modified_time(const void*, const void*);
int sort_duration(const void*, const void*);
int sort_category(const void*, const void*);
int sort_title(const void*, const void*);

typedef struct {
    int duration;
    __time_t file_last_modified;
    char name[NAME_MAX + 1];
    char category[NAME_MAX + 1];
} music;

typedef struct {
    sorter sorter;
    char* exclude;
    int duration_range[2];
    size_t count;
    char path[NAME_MAX];
    music musics[MUSIC_LIST_CAP];
} music_list;

void load_music_from(DIR*, music_list*);

const char* get_time_str(long seconds);
int get_music_duration(const char* filepath);

const char* music_name_trim_suffix(const char*);
const char* music_name_get_hash(const char*);

typedef music* (*selector)(music_list*, size_t);
music* selector_random(music_list*, size_t seed);
music* selector_linear(music_list*, size_t seed);
music* selector_linear_reverse(music_list*, size_t seed);
