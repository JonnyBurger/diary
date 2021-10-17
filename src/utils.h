#ifndef DIARY_UTILS_H
#define DIARY_UTILS_H

#include <regex.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <wordexp.h>
#include <stdbool.h>
#include <ncurses.h>

#define GOOGLE_OAUTH_TOKEN_FILE "~/.diary-token"
#ifndef GOOGLE_OAUTH_CLIENT_ID
    #define GOOGLE_OAUTH_CLIENT_ID ""
#endif
#ifndef GOOGLE_OAUTH_CLIENT_SECRET
    #define GOOGLE_OAUTH_CLIENT_SECRET ""
#endif

#define CAL_WIDTH 21
#define ASIDE_WIDTH 4
#define MAX_MONTH_HEIGHT 6

void update_date(WINDOW* header, struct tm* curs_date);
char* extract_json_value(const char* json, char* key, bool quoted);
char* fold(const char* str);
char* unfold(const char* str);
char* extract_ical_field(const char* ical, char* key, long* start_pos, bool multline);
char* expand_path(const char* str);
char* strrstr(char *haystack, char *needle);
void fpath(const char* dir, size_t dir_size, const struct tm* date, char** rpath, size_t rpath_size);
bool go_to(WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos, struct tm* curs_date, struct tm* cal_start, struct tm* cal_end);
void* show_progress(void* vargp);

typedef struct
{
    // Path that holds the journal text files
    char* dir;
    // Number of years to show before/after todays date
    int range;
    // 7 = Sunday, 1 = Monday, ..., 6 = Saturday
    int weekday;
    // 2020-12-31
    char* fmt;
    // Editor to open journal files with
    char* editor;
    // File for Google OAuth access token
    char* google_tokenfile;
    // Google client id
    char* google_clientid;
    // Google secret id
    char* google_secretid;
    // Google calendar to synchronize
    char* google_calendar;
} config;

extern config CONFIG;

#endif
