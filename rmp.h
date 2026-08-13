#include <fcntl.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>

#define BUF_SIZE 1024

#define URL_YOUTUBE "https://www.youtube.com/watch?v="

#define CMD_DL_MUSIC "yt-dlp -x --audio-format mp3 -o \"%s/%%(title)s.%s.%%(ext)s\" " URL_YOUTUBE "%s" // category, hash, url
#define CMD_MUSIC_NAME "yt-dlp --quiet --no-warnings --get-filename --simulate %s"
#define CMD_GET_PLAYLIST "yt-dlp --quiet --no-warnings --get-filename --flat-playlist --simulate -o \"%%(id)s\" %s"
#define CMD_MKDIR "mkdir -p %s"

int rmp_build(int argc, char** argv);
int rmp_play(int argc, char** argv);
int rmp_add(int argc, char** argv);

bool arg(const char* cmd, const char* short_name, const char* long_name);
char* run_shell_command(char* dest, size_t dest_size, const char* cmd);
