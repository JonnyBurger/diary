#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <ncurses.h>

#define YEAR_RANGE 1
#define CAL_WIDTH 21
#define ASIDE_WIDTH 4
#define MAX_MONTH_HEIGHT 6

int cy, cx;
time_t raw_time;
struct tm today;
struct tm curs_date;
struct tm cal_start;
struct tm cal_end;

static const char* WEEKDAYS[] = {"Mo","Tu","We","Th","Fr","Sa","Su", NULL};

void fpath(char* dir, size_t dir_size, struct tm* date, char* rpath, size_t rpath_size);

void get_date_str(struct tm* date, char* date_str, size_t date_str_size) {
    strftime(date_str, date_str_size, "%Y-%m-%d", date);
}

void draw_wdays(WINDOW* head) {
    char** wd;
    for (wd = (char**)WEEKDAYS; *wd; wd++) {
        waddstr(head, *wd);
        waddch(head, ' ');
    }
    wrefresh(head);
}

bool date_has_entry(char* dir, size_t dir_size, struct tm* i) {
    char epath[100];

    // get entry path and check for existence
    fpath(dir, dir_size, i, epath, sizeof epath);

    if (epath == NULL) {
        fprintf(stderr, "Error while retrieving file path for checking entry existence");
        return false;
    }

    return (access(epath, F_OK) != -1);
}

void draw_calendar(WINDOW* number_pad, WINDOW* month_pad, char* diary_dir, size_t diary_dir_size) {
    struct tm i = cal_start;
    char month[10];
    char epath[100];
    bool has_entry;

    while (mktime(&i) <= mktime(&cal_end)) {
        has_entry = date_has_entry(diary_dir, diary_dir_size, &i);

        if (has_entry)
            wattron(number_pad, A_BOLD);

        wprintw(number_pad, "%2i", i.tm_mday);

        if (has_entry)
            wattroff(number_pad, A_BOLD);

        waddch(number_pad, ' ');

        // print month in sidebar
        if (i.tm_mday == 1) {
            strftime(month, sizeof month, "%b", &i);
            getyx(number_pad, cy, cx);
            mvwprintw(month_pad, cy, 0, "%s ", month);
        }

        i.tm_mday++;
        mktime(&i);
    }
}

void update_date(WINDOW* dh) {
    char dstr[16];
    mktime(&curs_date);
    get_date_str(&curs_date, dstr, sizeof dstr);
    mvwaddstr(dh, 0, 0, dstr);
    wrefresh(dh);
}

void fpath(char* dir, size_t dir_size, struct tm* date, char* rpath, size_t rpath_size) {
    // check size of result path
    if (dir_size + 1 > rpath_size) {
        fprintf(stderr, "Directory path too long");
        rpath == NULL;
        return;
    }

    // add path of the diary dir to result path
    strcpy(rpath, dir);

    // check for terminating '/' in path
    if (dir[dir_size - 1] != '/') {
        // check size again to accomodate '/'
        if (dir_size + 1 > rpath_size) {
            fprintf(stderr, "Directory path too long");
            rpath == NULL;
            return;
        }
        strcat(rpath, "/");
    }

    char dstr[16];
    get_date_str(date, dstr, sizeof dstr);

    // append date to the result path
    if (strlen(rpath) + strlen(dstr) > rpath_size) {
        fprintf(stderr, "File path too long");
        rpath == NULL;
        return;
    }
    strcat(rpath, dstr);
}

void edit_cmd(char* dir, size_t dir_size, struct tm* date, char* rcmd, size_t rcmd_size) {
    // get editor from environment
    char* editor = getenv("EDITOR");
    if (editor == NULL) {
        fprintf(stderr, "'EDITOR' environment variable not set");
        rcmd = NULL;
        return;
    }

    // set the edit command to env editor
    if (strlen(editor) + 2 > rcmd_size) {
        fprintf(stderr, "Binary path of default editor too long");
        rcmd = NULL;
        return;
    }
    strcpy(rcmd, editor);
    strcat(rcmd, " ");

    // get path of entry
    char path[100];
    fpath(dir, dir_size, date, path, sizeof path);

    if (path == NULL) {
        fprintf(stderr, "Error while retrieving file path for editing");
        rcmd = NULL;
        return;
    }

    // concatenate editor command with entry path
    if (strlen(rcmd) + strlen(path) + 1 > rcmd_size) {
        fprintf(stderr, "Edit command too long");
        return;
    }
    strcat(rcmd, path);
}

bool is_leap(int year) {
    // normally leap is every 4 years,
    // but is skipped every 100 years,
    // unless it is divisible by 400
    return (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
}

/* Returns WINDOW* to preview window if diary reading was successful, NULL otherwise */
void read_diary(char* dir, size_t dir_size, struct tm* date, WINDOW* prev, int width) {
    wclear(prev);
    char buff[width];
    char path[100];
    int i = 0;

    // get entry path
    fpath(dir, dir_size, date, path, sizeof path);
    if (path == NULL) {
        fprintf(stderr, "Error while retrieving file path for diary reading");
        prev = NULL;
        return;
    }

    FILE* fp =  fopen(path, "r");
    if (fp != NULL) {
        while(fgets(buff, width, fp) != NULL) {
            mvwprintw(prev, i, 0, buff);
            i++;
        }
        fclose(fp);
    }

    wrefresh(prev);
}

bool go_to(WINDOW* calendar, WINDOW* month_sidebar, time_t date, int* cur_pad_pos) {
    if (date < mktime(&cal_start) || date > mktime(&cal_end))
        return false;

    int diff_seconds = date - mktime(&cal_start);
    int diff_days = diff_seconds / 60 / 60 / 24;
    int diff_weeks = diff_days / 7;
    int diff_wdays = diff_days % 7;

    localtime_r(&date, &curs_date);

    getyx(calendar, cy, cx);

    // remove the STANDOUT attribute from the day we are leaving
    chtype current_attrs = mvwinch(calendar, cy, cx) & A_ATTRIBUTES;
    // leave every attr as is, but turn off STANDOUT
    current_attrs &= ~A_STANDOUT;
    mvwchgat(calendar, cy, cx, 2, current_attrs, 0, NULL);

    // add the STANDOUT attribute to the day we are entering
    chtype new_attrs =  mvwinch(calendar, diff_weeks, diff_wdays * 3) & A_ATTRIBUTES;
    new_attrs |= A_STANDOUT;
    mvwchgat(calendar, diff_weeks, diff_wdays * 3, 2, new_attrs, 0, NULL);

    if (diff_weeks < *cur_pad_pos)
        *cur_pad_pos = diff_weeks;
    if (diff_weeks > *cur_pad_pos + LINES - 2)
        *cur_pad_pos = diff_weeks - LINES + 2;
    prefresh(month_sidebar, *cur_pad_pos, 0, 1, 0, LINES - 1, ASIDE_WIDTH);
    prefresh(calendar, *cur_pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    return true;
}

void setup_cal_timeframe() {
    raw_time = time(NULL);
    localtime_r(&raw_time, &today);
    curs_date = today;

    cal_start = today;
    cal_start.tm_year -= YEAR_RANGE;
    cal_start.tm_mon = 0;
    cal_start.tm_mday = 1;
    mktime(&cal_start);

    if (cal_start.tm_wday != 1) {
        // adjust start date to first Mon before 01.01
        cal_start.tm_year--;
        cal_start.tm_mon = 11;
        cal_start.tm_mday = 31 - cal_start.tm_wday + 2;
        mktime(&cal_start);
    }

    cal_end = today;
    cal_end.tm_year += YEAR_RANGE;
    cal_end.tm_mon = 11;
    cal_end.tm_mday = 31;
    mktime(&cal_end);
}

int main(int argc, char** argv) {
    char diary_dir[80];
    char* env_var;
    chtype atrs;

    // get the diary directory via environment variable or argument
    // if both are given, the argument takes precedence
    if (argc < 2) {
        // the diary directory is not specified via command line argument
        // use the environment variable if available
        env_var = getenv("DIARY_DIR");
        if (env_var == NULL) {
            fprintf(stderr, "The diary directory must be given as command line "
                            "argument or in the DIARY_DIR environment variable\n");
            return 1;
        }

        if (strlen(env_var) + 1 > sizeof diary_dir) {
            fprintf(stderr, "Diary directory path too long\n");
            return 1;
        }
        strcpy(diary_dir, env_var);
    } else {
        if (strlen(argv[1]) + 1 > sizeof diary_dir) {
            fprintf(stderr, "Diary directory path too long\n");
            return 1;
        }
        strcpy(diary_dir, argv[1]);
    }

    // check if that directory exists
    DIR* diary_dir_ptr = opendir(diary_dir);
    if (diary_dir_ptr) {
        // directory exists, continue
        closedir(diary_dir_ptr);
    } else if (errno == ENOENT) {
        fprintf(stderr, "The directory '%s' does not exist\n", diary_dir);
        return 2;
    } else {
        fprintf(stderr, "The directory '%s' could not be opened\n", diary_dir);
        return 1;
    }

    setup_cal_timeframe();

    initscr();
    raw();
    curs_set(0);

    WINDOW* date_header = newwin(1, 10, 0, ASIDE_WIDTH + CAL_WIDTH);
    wattron(date_header, A_BOLD);
    update_date(date_header);
    WINDOW* wdays_header = newwin(1, 3 * 7, 0, ASIDE_WIDTH);
    draw_wdays(wdays_header);

    WINDOW* aside = newpad((YEAR_RANGE * 2 + 1) * 12 * MAX_MONTH_HEIGHT, ASIDE_WIDTH);
    WINDOW* cal = newpad((YEAR_RANGE * 2 + 1) * 12 * MAX_MONTH_HEIGHT, CAL_WIDTH);
    keypad(cal, TRUE);
    draw_calendar(cal, aside, diary_dir, strlen(diary_dir));

    int ch, pad_pos = 0;
    struct tm new_date;
    int prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
    int prev_height = LINES - 1;

    bool mv_valid = go_to(cal, aside, raw_time, &pad_pos);
    // mark current day
    atrs = winch(cal) & A_ATTRIBUTES;
    wchgat(cal, 2, atrs | A_UNDERLINE, 0, NULL);
    prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    WINDOW* prev = newwin(prev_height, prev_width, 1, ASIDE_WIDTH + CAL_WIDTH);
    read_diary(diary_dir, strlen(diary_dir), &today, prev, prev_width);

    do {
        ch = wgetch(cal);
        // new_date represents the desired date the user wants to go_to(),
        // which may not be a faisable date at all
        new_date = curs_date;
        char ecmd[150];
        char pth[100];
        edit_cmd(diary_dir, strlen(diary_dir), &new_date, ecmd, sizeof ecmd);

        switch(ch) {
            // basic movements
            case 'j':
            case KEY_DOWN:
                new_date.tm_mday += 7;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'k':
            case KEY_UP:
                new_date.tm_mday -= 7;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'l':
            case KEY_RIGHT:
                new_date.tm_mday++;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'h':
            case KEY_LEFT:
                new_date.tm_mday--;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;

            // jump to top/bottom of page
            case 'g':
                mv_valid = go_to(cal, aside, mktime(&cal_start), &pad_pos);
                break;
            case 'G':
                mv_valid = go_to(cal, aside, mktime(&cal_end), &pad_pos);
                break;

            // jump backward/forward by a month
            case 'K':
                if (new_date.tm_mday == 1)
                    new_date.tm_mon--;
                new_date.tm_mday = 1;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;
            case 'J':
                new_date.tm_mon++;
                new_date.tm_mday = 1;
                mv_valid = go_to(cal, aside, mktime(&new_date), &pad_pos);
                break;

            // today shortcut
            case 't':
                new_date = today;
                mv_valid = go_to(cal, aside, raw_time, &pad_pos);
                break;
            // delete entry
            case 'd':
            case 'x':
                if (date_has_entry(diary_dir, strlen(diary_dir), &curs_date)) {
                    // get file path of entry and delete entry
                    fpath(diary_dir, strlen(diary_dir), &curs_date, pth, sizeof pth);
                    if (unlink(pth) != -1) {
                        // file successfully delete, remove entry highlight
                        atrs = winch(cal) & A_ATTRIBUTES;
                        wchgat(cal, 2, atrs & ~A_BOLD, 0, NULL);
                        prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                    }
                }
                break;
            // edit/create a diary entry
            case 'e':
            case '\n':
                if (ecmd) {
                    curs_set(1);
                    system(ecmd);
                    curs_set(0);
                    keypad(cal, TRUE);

                    // mark newly created entry
                    if (date_has_entry(diary_dir, strlen(diary_dir), &curs_date)) {
                        atrs = winch(cal) & A_ATTRIBUTES;
                        wchgat(cal, 2, atrs | A_BOLD, 0, NULL);

                        // refresh the calendar to add highlighting
                        prefresh(cal, pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                    }
                }
                break;
        }

        if (mv_valid) {
            update_date(date_header);

            if (prev != NULL) {
                // adjust prev width (if terminal was resized in the mean time)
                // and read the diary
                prev_width = COLS - ASIDE_WIDTH - CAL_WIDTH;
                wresize(prev, prev_height, prev_width);
                read_diary(diary_dir, strlen(diary_dir), &curs_date, prev, prev_width);
            }
        }
    } while (ch != 'q');

    endwin();
    system("clear");
    return 0;
}
