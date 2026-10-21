#include <alloca.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wchar.h>

#include "music.h"
#include "rmp.h"

#define PAIR_GREY 1
#define COLOR_GREY (COLORS - PAIR_GREY)
#define PAIR_LIGHTGREY 2
#define COLOR_LIGHTGREY (COLORS - PAIR_LIGHTGREY)
#define PAIR_E1 3
#define PAIR_E2 4
#define PAIR_E3 5
#define PAIR_E4 6
#define PAIR_E5 7 // delete
#define PAIR_E6 8 // repeat
#define PAIR_E7 9 // wait
#define PAIR_E8 10 // skip

#define TEXT_COLOR(pair, code)     \
    do {                           \
        attron(COLOR_PAIR(pair));  \
        code;                      \
        attroff(COLOR_PAIR(pair)); \
    } while (0)

static const char* xs[] = { " ", "󰩹 ", "󰗨 ", " ", "󱟃 ", "󱟁 ", "󰐓 ", " ", " ", "󰖭 ", "󱎘 ", "󰛉 ", "󰅚 ", "󰅝 ", "󰅗 ", "󰅙 " };
static const char* ns[] = { "󰽶 ", "󰎇 ", " ", "󰽴 ", "󰎉 ", " ", "󰽬 ", "󰽫 ", "󰽺 ", "󰽻 ", "󰽷 ", "󰽸 ", "󰽰 ", " ", "󰽭 ", "󰽩 " };

static int seed = 0;

void sigchld_handler(int signum)
{
    (void)signum;
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

void quit(int a)
{
    endwin();
    wprintf(CLEAR_LINE L"Quitting! (Seed was %d)\n", seed);
    exit(a);
}

void draw_vertical_line(int y)
{
    for (int i = 0; i < COLS; i++) {
        move(y, i);
        printw("░");
    }
}

void draw_music_screen(music* current_music, bool music_has_hash, bool delete, bool repeat, bool wait_music_end_to_quit)
{
    int nh = (LINES - 1) / 2;

    // const char* f = "█";
    // const char* d = "▓";
    // const char* m = "▒";
    // const char* l = "░";

#define BAR_PAIR delete      ? PAIR_E5 \
    : wait_music_end_to_quit ? PAIR_E7 \
    : repeat                 ? PAIR_E6 \
                             : PAIR_E1

    for (int i = 0; i < nh - 3; i++) {
        move(i, ((COLS - 11) / 2));
        for (int j = 0; j < 4; j++) {
            TEXT_COLOR(BAR_PAIR + j, {
                printw("░░ ");
            });
        }
    }

    move(nh - 2, ((COLS - 11) / 2));
    for (int i = 0; i < 4; i++) {
        TEXT_COLOR(BAR_PAIR + i, {
            printw("%s ", (delete ? xs : ns)[rand() % 16]);
        });
    }

    const char* trimmed_name = music_name_trim_suffix(current_music->name);
    int name_len = strlen(trimmed_name);
    bool found_paren_start = false;
    int paren_start = 0;
    int dash_position = 0;
    for (int i = name_len - 1; i > 0; i--) {
        char c = trimmed_name[i];
        if (!found_paren_start) {
            if (c == '(' || c == '[') {
                paren_start += 1;
                if (paren_start == 0) {
                    paren_start = i;
                    found_paren_start = true;
                }
            }
            if (c == ')' || c == ']') {
                paren_start -= 1;
            }
        }
        if (c == '-'
            && i < name_len - 2 && trimmed_name[i + 1] == ' '
            && trimmed_name[i - 1] == ' '
            && (i < paren_start || !found_paren_start)) {
            dash_position = i;
        }
    }

    if (name_len + 4 < COLS) {
        if (dash_position == 0 && !found_paren_start) {
            move(nh, (COLS - name_len) / 2);
            printw("%s", trimmed_name);
        } else {
            char artist[NAME_MAX] = { 0 };
            char name[NAME_MAX] = { 0 };
            char extra[NAME_MAX] = { 0 };
            if (dash_position > 0) {
                snprintf(artist, NAME_MAX, "%.*s", dash_position - 1, trimmed_name);
                dash_position += 2;
            }
            if (found_paren_start) {
                snprintf(extra, NAME_MAX, "%.*s", name_len - paren_start, trimmed_name + paren_start);
                paren_start -= 1;
            }
            snprintf(name, NAME_MAX, "%.*s", paren_start - dash_position, trimmed_name + dash_position);
            char* texts[] = { artist, name, extra };

            int line = 0;
            for (int i = 0; i < 3; i++) {
                char* t = texts[i];
                int len = strlen(t);
                if (len == 0)
                    continue;
                move(nh + line, (COLS - len) / 2);
                if (i == 0)
                    attron(COLOR_PAIR(PAIR_LIGHTGREY));
                if (i == 2)
                    attron(COLOR_PAIR(PAIR_GREY));
                printw("%s", t);
                if (i == 0)
                    attroff(COLOR_PAIR(PAIR_LIGHTGREY));
                if (i == 2)
                    attroff(COLOR_PAIR(PAIR_GREY));
                line += 1;
            }
        }
    } else {
        move(nh, 2);
        printw("%.*s", COLS - 4, trimmed_name);
    }

    TEXT_COLOR(PAIR_GREY, {
        const char* time_str = get_time_str(current_music->duration);
        move(LINES - 3, ((COLS - strlen(time_str)) / 2));
        printw("%s", time_str);
    });

    TEXT_COLOR(PAIR_LIGHTGREY, {
        int category_len = strlen(current_music->category) - 1;
        if (category_len > 0) {
            move(LINES - 1, (COLS - category_len - 2) / 2);
            printw("[%.*s]", category_len, current_music->category);
        }

        move(LINES - 3, 1);
        printw("%s:w/W", wait_music_end_to_quit ? "󱫦 " : "󱫧 ");

        move(LINES - 2, 1);
        printw(music_has_hash ? "󰖟 :i" : "󰕑 ");

        move(LINES - 1, 1);
        printw("󰩈 :q");

        move(LINES - 3, COLS - 8);
        printw("r/R: %s", repeat ? " " : " ");

        move(LINES - 2, COLS - 8);
        printw("d/D: %s", delete ? "󰗨 " : " ");

        move(LINES - 1, COLS - 6);
        printw("s: 󰒭");
    });
}

void draw_waiting_screen(int i, music* current_music, time_t start, int wait, bool pause, bool repeat, bool delete, int reloaded, int session_listen_count, music_list* list)
{
    time_t current = time(NULL);
    time_t elapsed = current - start;

    wchar_t time_buf[BUF_SIZE] = { 0 };
    swprintf(time_buf, BUF_SIZE, L"Next music will play in %s | ", get_time_str(wait - i));
    const int time_buf_len = wcslen(time_buf);

    if (COLS < time_buf_len || LINES < 8) {
        move(0, 0);
        printw("Screen is too small!");
        return;
    }

    TEXT_COLOR(PAIR_E5, {
        if (delete) {
            const char* music_name = music_name_trim_suffix(current_music->name);
            int music_name_len = strlen(music_name);
            int ddiff = (elapsed * 1) % music_name_len;
            wchar_t nbuf[32] = { 0 };
            wchar_t status_buf[256] = { 0 };
            swprintf(nbuf, 32, L"%.*s ░ %.*s", music_name_len - ddiff, music_name + ddiff, ddiff, music_name);
            swprintf(status_buf, COLS - 10, L" %s %ls %s ", xs[rand() % 16], nbuf, xs[rand() % 16]);
            draw_vertical_line(0);
            move(0, (COLS - wcslen(status_buf)) / 2);
            printw("%ls", status_buf);
        }
    });

    TEXT_COLOR(PAIR_E8, {
        if (reloaded > 0) {
            move(LINES - 2, 0);
            printw("░░░░ %d ░░ 󱍚  list reloaded!", reloaded);
        }
    });

    int bar_end = (COLS - time_buf_len - 8) / 2;

    TEXT_COLOR(pause ? PAIR_E7
            : repeat ? PAIR_E8
                     : PAIR_E6,
        {
            const char* info
                = pause  ? "󰏤 "
                : repeat ? " "
                         : " ";

            draw_vertical_line(4);
            move(4, bar_end);
            int diff = elapsed % time_buf_len;
            printw(" %s %.*ls%.*ls %s ", info, time_buf_len - diff, time_buf + diff, diff, time_buf, info);
        });

    if (COLS > 60 && LINES - 6 >= 9) {
        int h = (LINES - 6) / 2;
        TEXT_COLOR(PAIR_E8, {
            for (int i = 0; i < COLS; i++) {
                if (i < bar_end || i > bar_end + time_buf_len + 7) {
                    move(h + 1, i);
                    printw("░");
                }
            }
            move(h + 1, bar_end);
            printw(" %s r/R: Repeat      | n: Play now    ", repeat ? " " : " ");
        });
        TEXT_COLOR(PAIR_E7, {
            for (int i = 0; i < COLS; i++) {
                if (i < bar_end || i > bar_end + time_buf_len + 7) {
                    move(h + 3, i);
                    printw("░");
                }
            }
            move(h + 3, bar_end);
            printw(" %s p/P: Pause timer | l: Reload musics", pause ? "󰏤 " : " ");
        });
        TEXT_COLOR(PAIR_E5, {
            for (int i = 0; i < COLS; i++) {
                if (i < bar_end || i > bar_end + time_buf_len + 7) {
                    move(h + 5, i);
                    printw("░");
                }
            }
            move(h + 5, bar_end);
            printw("   d/D: Delete      | q: Quit       󰩈 ");
        });
    }

    TEXT_COLOR(PAIR_LIGHTGREY, {
        draw_vertical_line(LINES - 1);
        move(LINES - 1, 0);
        printw(" %zu 󱍚  ░ %d 󰐑  ░", list->count, session_listen_count);

        const char* elapsed_str = get_time_str(elapsed);
        move(LINES - 1, (COLS - (6 + strlen(elapsed_str))));
        printw("   %s  ", elapsed_str);
    });

    TEXT_COLOR(PAIR_E6, {
        move(LINES - 1, (COLS - 8) / 2);
        printw("  rmp™  ");
    });
}

int rmp_play(int argc, char** argv, char** envp)
{
    // TODO: Use vlc as a library directly
    // #include <vlc/vlc.h>
    // const char* name = libvlc_get_version();
    // wprintf(L"%s\n", name);
    // exit(0);

    signal(SIGINT, quit);
    signal(SIGCHLD, sigchld_handler);

    time_t start = time(NULL);

    music_list list = { .sorter = sort_last_modified_time };

    selector get_music = selector_random;
    bool list_musics = false;
    bool list_categories = false;
    size_t max_wait = 10 * 60; // seconds
    size_t min_wait = 0; // seconds
    bool skip = false;
    seed = start % 1000;
    int run = 0;

    for (int i = 0; i < argc; i++) {
        char* cmd = argv[i];

        if (arg(cmd, "-li", "--list")) {
            list_musics = true;
        } else if (arg(cmd, "-lc", "--list-categories")) {
            list_categories = true;
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
        else if (arg(cmd, "-mad", "--max-duration"))
            list.duration_range[1] = strtoul(argv[++i], NULL, 10);
        else if (arg(cmd, "-mid", "--min-duration"))
            list.duration_range[0] = strtoul(argv[++i], NULL, 10);
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

    if (strcmp(getcwd(list.path, BUF_SIZE), list.path) != 0) {
        fwprintf(stderr, L"Error: The music directory couldnt be determined!\n");
        fwprintf(stderr, L"Reason: list.path is '%s' but ", list.path);
        fwprintf(stderr, L"getcwd(list.path, BUF_SIZE) is '%s'.\n", getcwd(list.path, BUF_SIZE));
        exit(3);
    }

    wprintf(L"Loading musics from: '%s'\r", list.path);
    fflush(stdout);

    DIR* dir = opendir(list.path);
    load_music_from(dir, &list);
    closedir(dir);

    if (list.count == 0) {
        fwprintf(stderr, L"The music directory has no music!\n");
        exit(6);
    }

    if (list_categories) {
        if (list.sorter != sort_category) {
            qsort(&list.musics, list.count, sizeof(music), sort_category);
        }
        wprint_line("\n", CLEAR_LINE L"Listing Categories: %s", list.path);
        wprint_line("\n", L"--> .");
        char prev_category[NAME_MAX + 1] = { 0 };
        for (size_t i = 0; i < list.count; i++) {
            music* m = selector_linear(&list, run + i);
            if (strcmp(prev_category, m->category) == 0) {
                continue;
            }
            strcpy(prev_category, m->category);
            wprint_line("\n", L"--> %.*s", strlen(m->category) - 1, m->category);
        }
        if (list.count == MUSIC_LIST_CAP)
            fwprintf(stderr, L"!! Music list capacity is full !!\nIncrease 'MUSIC_LIST_CAP' to be able to add new musics.\n");
        return 0;
    }

    if (list_musics) {
        int total_duration = 0;

        wprint_line("\n", CLEAR_LINE L"Listing Musics: %s", list.path);
        for (size_t i = 0; i < list.count; i++) {
            music* m = get_music(&list, run + i);
            total_duration += m->duration;

            struct winsize w;
            ioctl(0, TIOCGWINSZ, &w);
            const char* m_name = music_name_trim_suffix(m->name);
            int c_len = strlen(m->category);
            int m_len = strlen(m_name);
            wprintf(
                L"-|" ANSI_RED "%3zu" ANSI_RESET
                "|" ANSI_CYAN "%s" ANSI_RESET
                "|" ANSI_YELLOW "%.*s" ANSI_RESET
                "%s %.*s\n",
                i + 1,
                get_time_str(m->duration),
                c_len - 1, m->category,
                c_len > 0 ? "|" : "", w.ws_col - 1 > (c_len + m_len + 14) ? m_len : w.ws_col - c_len - 14, m_name);
        }

        wprint_line("\n", L"Found %zu musics with %s of duration.", list.count, get_time_str(total_duration));
        if (list.exclude != NULL)
            wprint_line("\n", L"Excluded categories: %s", list.exclude);
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
    bool once = false;

    unsigned int session_listen_count = 0;

    music* current_music = NULL;

    initscr();
    cbreak();
    noecho();
    curs_set(0);
    if (has_colors() == FALSE) {
        endwin();
        wprintf(L"Your terminal does not support color\n");
        exit(1);
    }
    start_color();
    init_color(COLOR_WHITE, 1000, 1000, 1000);
    init_color(COLOR_LIGHTGREY, 810, 810, 810);
    init_color(COLOR_GREY, 540, 540, 540);
    init_pair(PAIR_GREY, COLOR_GREY, COLOR_BLACK);
    init_pair(PAIR_LIGHTGREY, COLOR_LIGHTGREY, COLOR_BLACK);
    init_pair(PAIR_E1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_E2, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_E3, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_E4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_E5, COLOR_RED, COLOR_BLACK);
    init_pair(PAIR_E6, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_E7, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_E8, COLOR_GREEN, COLOR_BLACK);

    __pid_t cvlc_pid = -1;

    while (true) {
        now = false;
        delete = false;
        pause = false;

        int reloaded = 0;

        if (current_music == NULL || (!once && !repeat))
            current_music = get_music(&list, run);

        if (skip == false || once) {
            once = false;
            cvlc_pid = fork();
            if (cvlc_pid < 0) {
                // error forking
                quit(0);
            } else if (cvlc_pid == 0) {
                // child
                int fd = open("/dev/null", O_WRONLY);
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
                snprintf(buf, BUF_SIZE, "%s/%s%s", list.path, current_music->category, current_music->name);
                char* const args[] = {
                    "/bin/cvlc",
                    buf,
                    "--no-repeat",
                    "--audio-filter=compressor",
                    // "--audio-filter=volnorm",
                    // "--norm-max-level=1.0",
                    "--no-volume-save",
                    "--play-and-exit",
                    NULL,
                }; // --gain 0.3 --volume-step 0.1
                return execve("/bin/cvlc", args, envp);
            } else if (cvlc_pid > 0) {
                // parent
                session_listen_count += 1;
            }
        } else {
            skip = false;
        }

        bool wait_music_end_to_quit = false;
        bool music_has_hash = music_name_get_hash(current_music->name) != NULL;
        int status = -1;
        while (0 == waitpid(cvlc_pid, &status, WNOHANG)) {
            if (WIFEXITED(status) > 0)
                break;
            clear();
            if (COLS < 35 || LINES < 8) {
                move(0, 0);
                printw("Screen is too small!");
                refresh();
                sleep(1);
                continue;
            }
            int res = 0;
            while ((res = read(stdin->_fileno, wbuf, BUF_SIZE)) > 0) {
                for (int j = 0; j < res; j++) {
                    char o = wbuf[j];
                    if (o == 's') {
                        kill(cvlc_pid, SIGTERM);
                        goto end;
                    } else if (o == 'q') {
                        kill(cvlc_pid, SIGTERM);
                        quit(0);
                    } else if (o == 'w' || o == 'W') {
                        wait_music_end_to_quit = o == 'w';
                    } else if (o == 'r' || o == 'R') {
                        repeat = o == 'r';
                    } else if (o == 'd' || o == 'D') {
                        delete = o == 'd';
                    } else if (o == 'i' && music_has_hash) {
                        char cmd[BUF_SIZE] = { 0 };
                        snprintf(cmd, BUF_SIZE, "xdg-open " URL_YOUTUBE "%s", music_name_get_hash(current_music->name));
                        system(cmd);
                    }
                }
            }

            draw_music_screen(current_music, music_has_hash, delete, repeat, wait_music_end_to_quit);
            refresh();
            sleep(1);
        }

    end:
        NULL;

        if (wait_music_end_to_quit)
            quit(0);

        int wait = (rand() % max_wait) + min_wait;

        for (int i = 0; i < wait; i++) {
            int res = 0;
            while ((res = read(stdin->_fileno, wbuf, BUF_SIZE)) > 0) {
                for (int j = 0; j < res; j++) {
                    char o = wbuf[j];
                    if (o == 'q') {
                        quit(0);
                    } else if (o == 'l') {
                        DIR* dir = opendir(list.path);
                        load_music_from(dir, &list);
                        closedir(dir);
                        reloaded = 3;
                    } else if (o == 'n')
                        now = true;
                    else if (o == 'd' || o == 'D')
                        delete = o == 'd';
                    else if (o == 'p' || o == 'P')
                        pause = o == 'p';
                    else if (o == 'r' || o == 'R')
                        repeat = o == 'r';
                    else if (o == 'o')
                        once = true;
                }
            }

            if (pause)
                i--;

            if (now || once)
                break;

            clear();
            draw_waiting_screen(i, current_music, start, wait, pause, repeat, delete, reloaded, session_listen_count, &list);
            if (reloaded > 0)
                reloaded -= 1;

            refresh();
            sleep(1);
        }

        if (repeat)
            continue;

        run += 1;

        if (delete) {
            snprintf(buf, BUF_SIZE, "%s.removed.%s", current_music->category, current_music->name);
            char buf2[BUF_SIZE] = { 0 };
            snprintf(buf2, BUF_SIZE, "%s%s", current_music->category, current_music->name);
            if (rename(buf2, buf)) {
                fwprintf(stderr, L"Error renaming: '%s' to '%s'\n", buf2, buf);
                fwprintf(stderr, L"Path: '%s'\n", list.path);
                exit(7);
            }
            DIR* dir = opendir(list.path);
            load_music_from(dir, &list);
            closedir(dir);
            run -= 1;
        }
    }

    return 0;
}
