#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "rmp.h"

#define CMD_DURATION "ffmpeg -i \"%s\" 2>&1 | grep Duration | sed -E 's/.*Duration: ([0-9]{2}:[0-9]{2}:[0-9]{2}).*/\\1/'"
#define CMD_CVLC "cvlc \"%s%s\" --no-volume-save --play-and-exit 2>/dev/null" // --gain 0.3 --volume-step 0.1 --norm-max-level 0.01
#define DELETED_FILE_NAME "%s.removed.%s.bak"

#define MUSIC_LIST_CAP 128

#define CURSOR_UP "\x1b[A"
#define CLEAR_LINE "\033[2K"

static wchar_t print_buffer[1024] = { 0 };
#define printt(end, fmt, ...)                                       \
    do {                                                            \
        struct winsize win;                                         \
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);                     \
        swprintf(print_buffer, win.ws_col - 1, fmt, ##__VA_ARGS__); \
        wprintf(L"%ls" end, print_buffer);                          \
    } while (0)

static int seed = 0;

typedef int (*sorter)(const void*, const void*);

typedef struct {
    int duration;
    __time_t file_last_modified;
    char name[NAME_MAX + 1];
    char category[NAME_MAX + 1];
} music;

typedef struct {
    music musics[MUSIC_LIST_CAP];
    sorter sorter;
    size_t count;
    char* path;
    char* exclude;
} music_list;

typedef music* (*selector)(music_list*, size_t);

int get_music_duration(char* music_name)
{
    char cmd[BUF_SIZE] = { 0 };
    snprintf(cmd, BUF_SIZE, CMD_DURATION, music_name);

    char res[BUF_SIZE] = { 0 };
    run_shell_command(res, BUF_SIZE, cmd);

    int total_time = 0;
    int time_mul = 1;

    for (size_t i = strlen(res) - 2; i > 0; i--) {
        if (res[i] == ':') {
            time_mul *= 6;
            time_mul /= 10;
        } else {
            total_time += (res[i] - '0') * time_mul;
            time_mul *= 10;
        }
    }

    return total_time;
}

void get_time_str(wchar_t* buf, size_t size, long elapsed_seconds)
{
    assert(size > 9);
    memset(buf, 0, size);

    int hours = elapsed_seconds / 3600;
    int minutes = (elapsed_seconds % 3600) / 60;
    int seconds = elapsed_seconds % 60;

    swprintf(buf, size, L"%02d:%02d:%02d", hours, minutes, seconds);
}

void load_music_from(DIR* dir, music_list* list, char* category)
{
    if (dir == NULL) {
        fwprintf(stderr, L"The music directory couldnt be opened!\n");
        return;
    }

    char buf[BUF_SIZE * 2] = { 0 };
    char buf2[BUF_SIZE] = { 0 };

    bool is_this_global_and_excluded = false;

    if (category == NULL && list->exclude != NULL) {
        snprintf(buf2, BUF_SIZE, "%s", list->exclude);
        for (char* tok = strtok(buf2, " "); !is_this_global_and_excluded && tok != NULL; tok = strtok(NULL, " ")) {
            is_this_global_and_excluded = strcmp(tok, "g") == 0 || strcmp(tok, "global") == 0;
        }
    }

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.')
            continue;

        if (ent->d_type == DT_DIR) {
            bool isExcluded = false;
            if (list->exclude != NULL) {
                sprintf(buf2, "%s", list->exclude);
                for (char* tok = strtok(buf2, " "); !isExcluded && tok != NULL; tok = strtok(NULL, " ")) {
                    isExcluded = strcmp(tok, ent->d_name) == 0;
                }
            }
            if (isExcluded)
                continue;

            DIR* sub;
            if (category == NULL)
                sprintf(buf2, "%s", ent->d_name);
            else
                sprintf(buf2, "%s/%s", category, ent->d_name);

            snprintf(buf, BUF_SIZE * 2, "%s/%s", list->path, buf2);

            sub = opendir(buf);
            load_music_from(sub, list, buf2);
            if (sub == NULL) {
                wprintf(L"Couldnt open path: %ls\n", buf);
                wprintf(L"Errno[%d]: %ls\n", errno, strerror(errno));
            } else {
                closedir(sub);
            }
        }
        if (ent->d_type == DT_REG) {
            if (is_this_global_and_excluded)
                continue;

            char file_name[NAME_MAX];
            strcpy(file_name, ent->d_name);

            char* ext = NULL;
            for (char* tok = strtok(file_name, "."); tok != NULL; tok = strtok(NULL, ".")) {
                ext = tok;
            }

            if (ext == NULL)
                continue;

            for (size_t i = 0; i < strlen(ext); i++) {
                ext[i] = tolower(ext[i]);
            }

            if (strcmp(ext, "mp3"))
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

            printt(L"\r", CLEAR_LINE L"Loaded %zu musics. [%s]\r", list->count, buf);
            fflush(stdout);

            if (list->count == MUSIC_LIST_CAP) {
                fwprintf(stderr, L"Cant read all musics! Increase 'MUSIC_LIST_CAP'!\n");
                break;
            }
        }
    }

    if (category == NULL) {
        qsort(list, list->count, sizeof(music), list->sorter);
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

void quit(int a)
{
    wprintf(L"\nQuitting! (Seed was %d)\n", seed);
    exit(a);
}

int rmp_play(int argc, char** argv)
{
    if (chdir(argv[0]) != 0) {
        fwprintf(stderr, L"Error: Couldnt change to music directory: %s\n", argv[1]);
        return 1;
    }

    signal(SIGINT, quit);

    time_t start = time(NULL);

    music_list list = { 0 };
    list.path = alloca(sizeof(char) * NAME_MAX);
    list.sorter = sort_last_modified_time;
    list.exclude = NULL;

    selector get_music = selector_random;
    bool switch_list = false;
    size_t max_wait = 10 * 60; // seconds
    size_t min_wait = 0; // seconds
    bool skip = false;
    seed = start % 1000;
    int run = 0;

    for (int i = 1; i < argc; i++) {
        char* cmd = argv[i];

        if (arg(cmd, "-li", "--list")) {
            switch_list = true;
        } else if (arg(cmd, "-", "--")) {
            // do nothing
        } else if (arg(cmd, "-n", "--new")) {
            list.sorter = sort_last_modified_time;
            get_music = selector_linear;
        } else if (arg(cmd, "-o", "--old")) {
            list.sorter = sort_last_modified_time;
            get_music = selector_linear_reverse;
        } else if (arg(cmd, "-t", "--title")) {
            list.sorter = sort_title;
            get_music = selector_linear;
        } else if (arg(cmd, "-tr", "--title-reverse")) {
            list.sorter = sort_title;
            get_music = selector_linear_reverse;
        } else if (arg(cmd, "-c", "--category")) {
            list.sorter = sort_category;
            get_music = selector_linear;
        } else if (arg(cmd, "-cr", "--category-reverse")) {
            list.sorter = sort_category;
            get_music = selector_linear_reverse;
        } else if (arg(cmd, "-s", "--shortest")) {
            list.sorter = sort_duration;
            get_music = selector_linear;
        } else if (arg(cmd, "-l", "--longest")) {
            list.sorter = sort_duration;
            get_music = selector_linear_reverse;
        }

        else if (arg(cmd, "-st", "--start"))
            run = strtol(argv[++i], NULL, 10) - 1; // convert index to subscript
        else if (arg(cmd, "-sd", "--seed"))
            seed = strtol(argv[++i], NULL, 10);
        else if (arg(cmd, "-max", "--max-wait"))
            max_wait = strtoul(argv[++i], NULL, 10);
        else if (arg(cmd, "-min", "--min-wait"))
            min_wait = strtoul(argv[++i], NULL, 10);
        else if (arg(cmd, "-ni", "--no-instant"))
            skip = true;
        else if (arg(cmd, "-e", "--exclude"))
            list.exclude = argv[++i];
        else {
            fwprintf(stderr, L"Warning: Unknown argument: %s\n", cmd);
            exit(4);
        }
    }

    srand(seed);

    wchar_t wbuf[BUF_SIZE] = { 0 };
    char buf[BUF_SIZE] = { 0 };

    if (strcmp(getcwd(list.path, BUF_SIZE), list.path)) {
        fwprintf(stderr, L"Error: The music directory couldnt be determined!\n");
        fwprintf(stderr, L"Reason: list.path is '%s' but ", list.path);
        fwprintf(stderr, L"getcwd(list.path, BUF_SIZE) is '%s'.\n", getcwd(list.path, BUF_SIZE));
        exit(3);
    }

    wprintf(L"Loading musics from: '%s'\r", list.path);
    fflush(stdout);

    DIR* dir = opendir(list.path);
    list.count = 0;
    load_music_from(dir, &list, NULL);
    closedir(dir);

    if (list.count == 0) {
        fwprintf(stderr, L"The music directory has no music!\n");
        exit(6);
    }

    if (switch_list) {
        int total_duration = 0;

        printt(L"\n", CLEAR_LINE L"Listing: %s", list.path);
        for (size_t i = 0; i < list.count; i++) {
            music* m = get_music(&list, run + i);
            total_duration += m->duration;
            get_time_str(wbuf, BUF_SIZE, m->duration);
            printt(L"\n", L"-|%3zu|%ls/%s%s", i + 1, wbuf, m->category, m->name);
        }

        get_time_str(wbuf, BUF_SIZE, total_duration);
        printt(L"\n", L"Found %zu musics with %ls of duration.", list.count, wbuf);
        if (list.exclude != NULL)
            printt(L"\n", L"Excluded categories: %s", list.exclude);
        if (get_music == selector_random)
            fwprintf(stderr, L"Listed using random selector with seed = %d\n", seed);

        return 0;
    }

    // Source - https://stackoverflow.com/questions/5616092/non-blocking-call-for-reading-descriptor
    // Posted by Judge Maygarden
    // Retrieved 11/5/2025, License - CC-BY-SA 4.0
    int flags = fcntl(stdin->_fileno, F_GETFL, 0);
    fcntl(stdin->_fileno, F_SETFL, flags | O_NONBLOCK);

    bool now = false;
    bool repeat = false;
    bool pause = false;
    bool delete = false;

    unsigned int session_listen_count = 0;

    music* current_music = NULL;

    while (true) {
        now = false;
        delete = false;
        pause = false;

        if (!repeat || current_music == NULL)
            current_music = get_music(&list, run);

        printt(L"\r", CLEAR_LINE L"==   =>> %s", current_music->name);
        fflush(stdout);

        if (skip == false) {
            memset(buf, 0, BUF_SIZE);
            snprintf(buf, BUF_SIZE, CMD_CVLC, current_music->category, current_music->name);
            if (system(buf) == 0) {
                session_listen_count += 1;
            } else {
                wprintf(L"%s", CURSOR_UP CLEAR_LINE);
            }
        } else {
            skip = false;
        }

        int res = 0;
        int wait = (rand() % max_wait) + min_wait;

        for (int i = 0; i < wait; i++) {
            memset(wbuf, 0, BUF_SIZE * sizeof(wchar_t));
            while ((res = read(stdin->_fileno, wbuf, BUF_SIZE)) > 0) {
                for (int j = 0; j < res; j++) {
                    char o = wbuf[j];
                    if (o == 'q') {
                        quit(0);
                    } else if (o == 'l') {
                        printt(L"\n", CURSOR_UP CLEAR_LINE L"   Reloading List...");
                        DIR* dir = opendir(list.path);
                        list.count = 0;
                        load_music_from(dir, &list, NULL);
                        closedir(dir);
                    } else if (o == 'n')
                        now = true;
                    else if (o == 'd')
                        delete = true;
                    else if (o == 'D')
                        delete = false;
                    else if (o == 'p')
                        pause = true;
                    else if (o == 'P')
                        pause = false;
                    else if (o == 'r')
                        repeat = true;
                    else if (o == 'R')
                        repeat = false;
                }

                wprintf(L"%s", CURSOR_UP CLEAR_LINE);
            }

            if (now)
                break;

            wchar_t status_buf[256] = { 0 };
            int status_w = 0;

            time_t current = time(NULL);
            time_t elapsed = current - start;

            wchar_t tbuf[16] = { 0 };

            get_time_str(tbuf, 16, wait - i);
            swprintf(wbuf, 1024, L"Next music will play in %ls ! ", tbuf);
            const int message_length = wcslen(wbuf);

            int diff = elapsed % message_length;

            wprintf(L"%s", CLEAR_LINE);
            fflush(stdout);

            status_w += swprintf(status_buf + status_w, 256, L"=>> ");
            if (delete || repeat || pause) {
                if (repeat) {
                    status_w += swprintf(status_buf + status_w, 256, L" ");
                }
                if (pause) {
                    i--;
                    status_w += swprintf(status_buf + status_w, 256, L"󰏤 ");
                }
                if (delete) {
                    int music_name_len = strlen(current_music->name);
                    int ddiff = (elapsed * 3) % music_name_len;
                    wchar_t nbuf[16] = { 0 };
                    swprintf(nbuf, 16, L"%.*s%.*s", music_name_len - ddiff, current_music->name + ddiff, ddiff, current_music->name);
                    status_w += swprintf(status_buf + status_w, 256, L" (%ls) ", nbuf);
                }
            }

            get_time_str(tbuf, 16, elapsed);

            status_w += swprintf(status_buf + status_w, 256,
                L"| %.*ls%.*ls   | %d   | %zu 󱍚  | %ls  ",
                message_length - diff, wbuf + diff, diff, wbuf,
                session_listen_count,
                list.count,
                tbuf);

            printt(L"\r", L"%ls", status_buf);
            fflush(stdout);

            sleep(1);
        }

        if (repeat)
            continue;

        run += 1;

        if (delete) {
            snprintf(buf, BUF_SIZE, DELETED_FILE_NAME, current_music->category, current_music->name);
            char buf2[BUF_SIZE] = { 0 };
            snprintf(buf2, BUF_SIZE, "%s%s", current_music->category, current_music->name);
            if (rename(buf2, buf)) {
                fprintf(stderr, "Error renaming: '%s' to '%s'\n", buf2, buf);
                fprintf(stderr, "Path: '%s'\n", list.path);
                exit(7);
            }
            DIR* dir = opendir(list.path);
            list.count = 0;
            load_music_from(dir, &list, NULL);
            closedir(dir);
            run -= 1;
        }
    }

    return 0;
}
