#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <taglib/tag_c.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "rmp.h"

#define CMD_CVLC "cvlc \"%s%s\" --no-volume-save --play-and-exit 2>/dev/null" // --gain 0.3 --volume-step 0.1 --norm-max-level 0.01
#define DELETED_FILE_NAME "%s.removed.%s.bak"

#define MUSIC_LIST_CAP 128

#define CURSOR_UP "\x1b[A"
#define CLEAR_SCREEN "\033[2J\033[H"

static wchar_t print_buffer[1024] = { 0 };
#define printt(end, fmt, ...)                                   \
    do {                                                        \
        struct winsize win;                                     \
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &win);                 \
        swprintf(print_buffer, win.ws_col, fmt, ##__VA_ARGS__); \
        wprintf(L"%ls" end, print_buffer);                      \
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

            printt(L"\r", CLEAR_LINE L"Loaded %zu musics. [%s]\r", list->count, buf);
            fflush(stdout);
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
    endwin();
    wprintf(CLEAR_LINE L"Quitting! (Seed was %d)\n", seed);
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
    bool list_musics = false;
    bool list_categories = false;
    size_t max_wait = 10 * 60; // seconds
    size_t min_wait = 0; // seconds
    bool skip = false;
    seed = start % 1000;
    int run = 0;

    for (int i = 1; i < argc; i++) {
        char* cmd = argv[i];

        if (arg(cmd, "-li", "--list")) {
            list_musics = true;
        } else if (arg(cmd, "-lc", "--list-categories")) {
            list_categories = true;
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
            fwprintf(stderr, L"Error: Unknown argument: %s\n", cmd);
            exit(4);
        }
    }

    if (max_wait == 0)
        max_wait = 1;

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

    if (list_categories) {
        if (list.sorter != sort_category) {
            qsort(&list, list.count, sizeof(music), sort_category);
        }
        printt(L"\n", CLEAR_LINE L"Listing Categories: %s", list.path);
        printt(L"\n", L"--> .");
        char prev_category[NAME_MAX + 1] = { 0 };
        for (size_t i = 0; i < list.count; i++) {
            music* m = selector_linear(&list, run + i);
            if (strcmp(prev_category, m->category) == 0) {
                continue;
            } else {
                strcpy(prev_category, m->category);
            }
            printt(L"\n", L"--> %.*s", strlen(m->category) - 1, m->category);
        }
        if (list.count == MUSIC_LIST_CAP)
            fwprintf(stderr, L"!! Music list capacity is full !!\nIncrease 'MUSIC_LIST_CAP' to be able to add new musics.\n");
        return 0;
    }

    if (list_musics) {
        int total_duration = 0;

        printt(L"\n", CLEAR_LINE L"Listing Musics: %s", list.path);
        for (size_t i = 0; i < list.count; i++) {
            music* m = get_music(&list, run + i);
            total_duration += m->duration;
            printt(L"\n", L"-|%3zu|%s/%s%s", i + 1, get_time_str(m->duration), m->category, music_name_trim_suffix(m->name));
        }

        printt(L"\n", L"Found %zu musics with %s of duration.", list.count, get_time_str(total_duration));
        if (list.exclude != NULL)
            printt(L"\n", L"Excluded categories: %s", list.exclude);
        if (get_music == selector_random)
            fwprintf(stderr, L"Listed using random selector with seed = %d\n", seed);
        if (list.count == MUSIC_LIST_CAP)
            fwprintf(stderr, L"!! Music list capacity is full !!\nIncrease 'MUSIC_LIST_CAP' to be able to add new musics.\n");
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

    initscr();
    cbreak();
    noecho();
    curs_set(0);

    while (true) {
        now = false;
        delete = false;
        pause = false;
        int reloaded = 0;

        clear();

        if (COLS < 35) {
            move(0, 0);
            printw("Screen is too small!");
            refresh();
            sleep(1);
            continue;
        }

        if (!repeat || current_music == NULL)
            current_music = get_music(&list, run);

        {
            int nh = (LINES - 1) / 2;
            int category_len = strlen(current_music->category);
            if (category_len > 0) {
                move(LINES - 1, ((COLS - category_len - 3) / 2));
                printw("<|%.*s|>", category_len - 1, current_music->category);
            }
            move(nh + 2, ((COLS - 8) / 2));
            printw("%s", get_time_str(current_music->duration));
            move(nh - 2, ((COLS - 6) / 2));
            printw("   ");
            const char* trimmed_name = music_name_trim_suffix(current_music->name);
            int name_len = strlen(trimmed_name);
            if (name_len + 4 < COLS) {
                move(nh, (COLS - name_len) / 2);
                printw("%s", trimmed_name);
            } else {
                move(nh, 2);
                printw("%.*s", COLS - 4, trimmed_name);
            }
        }

        refresh();

        if (skip == false) {
            snprintf(buf, BUF_SIZE, CMD_CVLC, current_music->category, current_music->name);
            if (system(buf) == 0) {
                session_listen_count += 1;
            }
        } else {
            skip = false;
        }

        clear();

        int wait = (rand() % max_wait) + min_wait;

        for (int i = 0; i < wait + 3; i++) {
            int res = 0;
            while ((res = read(stdin->_fileno, wbuf, BUF_SIZE)) > 0) {
                for (int j = 0; j < res; j++) {
                    char o = wbuf[j];
                    if (o == 'q') {
                        quit(0);
                    } else if (o == 'l') {
                        DIR* dir = opendir(list.path);
                        list.count = 0;
                        load_music_from(dir, &list, NULL);
                        closedir(dir);
                        reloaded = 3;
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
            }

            if (now)
                break;

            wchar_t status_buf[256] = { 0 };
            int status_w = 0;

            time_t current = time(NULL);
            time_t elapsed = current - start;

            swprintf(wbuf, 1024, L"Next music will play in %s ! ", get_time_str(wait - i));
            const int message_length = wcslen(wbuf);

            int diff = elapsed % message_length;

            if (delete || repeat || pause) {
                status_w += swprintf(status_buf + status_w, 256, L"=>> ");
                if (repeat) {
                    status_w += swprintf(status_buf + status_w, 256, L" ");
                }
                if (pause) {
                    i--;
                    status_w += swprintf(status_buf + status_w, 256, L"󰏤 ");
                }
                if (delete) {
                    const char* music_name = music_name_trim_suffix(current_music->name);
                    int music_name_len = strlen(music_name);
                    int ddiff = (elapsed * 1) % music_name_len;
                    wchar_t nbuf[32] = { 0 };
                    swprintf(nbuf, 32, L"%.*s%.*s", music_name_len - ddiff, music_name + ddiff, ddiff, music_name);
                    status_w += swprintf(status_buf + status_w, 256, L" //   %ls // ", nbuf);
                }
            }

            clear();

            if (COLS < message_length) {
                move(0, 0);
                printw("Screen is too small!");
            } else {
                move(0, 0);
                printw("%ls", status_buf);

                if (reloaded > 0) {
                    move(LINES - 2, 0);
                    printw("=(%d)=> 󱍚  reloaded! ", reloaded);
                    reloaded -= 1;
                }

                move(LINES - 1, 0);
                printw(" %zu 󱍚  | %d 󰐑  |", list.count, session_listen_count);

                move(LINES - 1, (COLS - 14));
                printw("|   %s", get_time_str(elapsed));

                move(3, (COLS / 2));
                printw(" ");

                int center_x = (COLS - message_length) / 2;
                move(4, center_x);
                printw("%.*ls%.*ls ", message_length - diff, wbuf + diff, diff, wbuf);

                if (COLS > 60 && LINES - 6 >= 9) {
                    int h = (LINES - 6) / 2;
                    move(h + 2, ((COLS - 30) / 2));
                    printw("r/R: Repeat       | d/D: Delete");
                    move(h + 3, ((COLS - 30) / 2));
                    printw("p/P: Pause timer  | n: Play now");
                    move(h + 4, ((COLS - 30) / 2));
                    printw("l: Reload musics  | q: Quit    ");
                }
            }

            refresh();
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
                fwprintf(stderr, L"Error renaming: '%s' to '%s'\n", buf2, buf);
                fwprintf(stderr, L"Path: '%s'\n", list.path);
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
