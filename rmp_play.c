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
        printf("Your terminal does not support color\n");
        exit(1);
    }
    start_color();
#define PAIR_GREY 1
#define COLOR_GREY (COLORS - PAIR_GREY)
#define PAIR_LIGHTGREY 2
#define COLOR_LIGHTGREY (COLORS - PAIR_LIGHTGREY)
#define PAIR_E1 3
#define PAIR_E2 4
#define PAIR_E3 5
#define PAIR_E4 6
#define PAIR_E5 7
    init_color(COLOR_WHITE, 1000, 1000, 1000);
    init_color(COLOR_LIGHTGREY, 945, 945, 945);
    init_color(COLOR_GREY, 540, 540, 540);
    init_pair(PAIR_GREY, COLOR_GREY, COLOR_BLACK);
    init_pair(PAIR_LIGHTGREY, COLOR_LIGHTGREY, COLOR_BLACK);
    init_pair(PAIR_E1, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_E2, COLOR_GREEN, COLOR_BLACK);
    init_pair(PAIR_E3, COLOR_CYAN, COLOR_BLACK);
    init_pair(PAIR_E4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(PAIR_E5, COLOR_RED, COLOR_BLACK);

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
                    } else if (o == 'w') {
                        wait_music_end_to_quit = true;
                    } else if (o == 'W') {
                        wait_music_end_to_quit = false;
                    } else if (o == 'r') {
                        repeat = true;
                    } else if (o == 'R') {
                        repeat = false;
                    } else if (o == 'd') {
                        delete = true;
                    } else if (o == 'D') {
                        delete = false;
                    } else if (o == 'i') {
                        if (music_has_hash) {
                            char cmd[BUF_SIZE] = { 0 };
                            snprintf(cmd, BUF_SIZE, "xdg-open " URL_YOUTUBE "%s", music_name_get_hash(current_music->name));
                            system(cmd);
                        }
                    }
                }
            }
            int nh = (LINES - 1) / 2;

            // const char* f = "█";
            // const char* d = "▓";
            // const char* m = "▒";
            // const char* l = "░";

            for (int i = 0; i < nh - 3; i++) {
                move(i, ((COLS - 11) / 2));
                for (int j = 0; j < 4; j++) {
                    attron(COLOR_PAIR(delete ? PAIR_E5 : PAIR_E1 + j));
                    printw("░░ ");
                    attroff(COLOR_PAIR(delete ? PAIR_E5 : PAIR_E1 + j));
                }
            }

            move(nh - 2, ((COLS - 11) / 2));
            const char* xs[] = { " ", "󰩹 ", "󰗨 ", " ", "󱟃 ", "󱟁 ", "󰐓 ", " ", " ", "󰖭 ", "󱎘 ", "󰛉 ", "󰅚 ", "󰅝 ", "󰅗 ", "󰅙 " };
            const char* ns[] = { "󰽶 ", "󰎇 ", " ", "󰽴 ", "󰎉 ", " ", "󰽬 ", "󰽫 ", "󰽺 ", "󰽻 ", "󰽷 ", "󰽸 ", "󰽰 ", " ", "󰽭 ", "󰽩 " };
            for (int i = 0; i < 4; i++) {
                attron(COLOR_PAIR(delete ? PAIR_E5 : PAIR_E1 + i));
                printw("%s ", (delete ? xs : ns)[rand() % 16]);
                attroff(COLOR_PAIR(delete ? PAIR_E5 : PAIR_E1 + i));
            }

            const char* trimmed_name = music_name_trim_suffix(current_music->name);
            int name_len = strlen(trimmed_name);
            if (name_len + 4 < COLS) {
                move(nh, (COLS - name_len) / 2);
                printw("%s", trimmed_name);
            } else {
                move(nh, 2);
                printw("%.*s", COLS - 4, trimmed_name);
            }

            attron(COLOR_PAIR(PAIR_GREY));
            const char* time_str = get_time_str(current_music->duration);
            move(nh + 2, ((COLS - strlen(time_str)) / 2));
            printw("%s", time_str);
            attroff(COLOR_PAIR(PAIR_GREY));

            attron(COLOR_PAIR(PAIR_LIGHTGREY));
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
            attroff(COLOR_PAIR(PAIR_LIGHTGREY));

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
                    else if (o == 'o')
                        once = true;
                }
            }

            if (pause)
                i--;

            if (now || once)
                break;

            time_t current = time(NULL);
            time_t elapsed = current - start;

            swprintf(wbuf, 1024, L"Next music will play in %s ! ", get_time_str(wait - i));
            const int message_length = wcslen(wbuf);

            int diff = elapsed % message_length;

            clear();

            attron(COLOR_PAIR(PAIR_LIGHTGREY));
            if (COLS < message_length || LINES < 8) {
                move(0, 0);
                printw("Screen is too small!");
            } else {

                if (delete) {
                    const char* music_name = music_name_trim_suffix(current_music->name);
                    int music_name_len = strlen(music_name);
                    int ddiff = (elapsed * 1) % music_name_len;
                    wchar_t nbuf[32] = { 0 };
                    wchar_t status_buf[256] = { 0 };
                    swprintf(nbuf, 32, L"%.*s%.*s", music_name_len - ddiff, music_name + ddiff, ddiff, music_name);
                    swprintf(status_buf, COLS - 10, L"//   %ls //", nbuf);
                    move(0, (COLS - wcslen(status_buf)) / 2);
                    printw("%ls", status_buf);
                }

                if (reloaded > 0) {
                    move(LINES - 2, 0);
                    printw("=(%d)=> 󱍚  reloaded! ", reloaded);
                    reloaded -= 1;
                }

                move(LINES - 1, 0);
                printw(" %zu 󱍚  | %d 󰐑  |", list.count, session_listen_count);

                const char* elapsed_str = get_time_str(elapsed);
                move(LINES - 1, (COLS - (6 + strlen(elapsed_str))));
                printw("|   %s", elapsed_str);

                if (pause) {
                    move(3, (COLS - 1) / 2);
                    printw("󰏤 ");
                } else {
                    move(3, (COLS - 1) / 2);
                    printw(" ");
                }

                int center_x = (COLS - message_length) / 2;
                move(4, center_x);
                printw("%.*ls%.*ls ", message_length - diff, wbuf + diff, diff, wbuf);

                if (COLS > 60 && LINES - 6 >= 9) {
                    int h = (LINES - 6) / 2;
                    move(h + 2, ((COLS - 30) / 2));
                    printw("r/R: Repeat  %s   | d/D: Delete", repeat ? " " : " ");
                    move(h + 3, ((COLS - 30) / 2));
                    printw("p/P: Pause timer  | n: Play now");
                    move(h + 4, ((COLS - 30) / 2));
                    printw("l: Reload musics  | q: Quit  󰩈 ");
                }
            }
            attroff(COLOR_PAIR(PAIR_LIGHTGREY));

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
