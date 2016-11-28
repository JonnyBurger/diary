#ifndef DIARY
#define DIARY

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <dirent.h>
#include <ncurses.h>

#define YEAR_RANGE 1
#define CAL_WIDTH 21
#define ASIDE_WIDTH 4
#define MAX_MONTH_HEIGHT 6

static const char* WEEKDAYS[] = {"Mo","Tu","We","Th","Fr","Sa","Su", NULL};

struct app_state;

void setup_cal_timeframe(struct app_state*);
void draw_wdays(WINDOW* head);
void draw_calendar(struct app_state*, WINDOW* number_pad, WINDOW* month_pad, char* diary_dir, size_t diary_dir_size);
void update_date(struct app_state*, WINDOW* header);

bool go_to(struct app_state*, WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos);
void display_entry(const char* dir, size_t dir_size, const struct tm* date, WINDOW* win, int width);
void edit_cmd(const char* dir, size_t dir_size, const struct tm* date, char* rcmd, size_t rcmd_size);

bool date_has_entry(const char* dir, size_t dir_size, const struct tm* i);
void get_date_str(const struct tm* date, char* date_str, size_t date_str_size);
void fpath(const char* dir, size_t dir_size, const struct tm* date, char* rpath, size_t rpath_size);

#endif
