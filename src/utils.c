#include "utils.h"

/* Update the header with the cursor date */
void update_date(WINDOW* header, struct tm* curs_date) {
    // TODO: dstr for strlen(CONFIG.format) > 16 ?
    char dstr[16];
    mktime(curs_date);
    strftime(dstr, sizeof dstr, CONFIG.fmt, curs_date);

    wclear(header);
    mvwaddstr(header, 0, 0, dstr);
    wrefresh(header);
}


char* extract_json_value(const char* json, char* key, bool quoted) {
    // work on a copy of the json
    char* jsoncp = (char*) malloc(strlen(json) * sizeof(char) + 1);
    if (jsoncp == NULL) {
        perror("malloc failed");
        return NULL;
    }
    strcpy(jsoncp, json);

    char* tok = strtok(jsoncp, " ");
    while (tok != NULL) {
        if (strstr(tok, key) != NULL) {
            tok = strtok(NULL, " "); // value
            break;
        }
        // key was not in this tok, advance tok
        tok = strtok(NULL, " ");
    }

    // remove quotes and comma or commma only
    if (quoted) {
        tok = strtok(tok, "\"");
    } else {
        tok = strtok(tok, ",");
    }

    char* res = NULL;
    if (tok != NULL) {
        res = (char*) malloc(strlen(tok) * sizeof(char) + 1);
        if (res == NULL) {
            perror("malloc failed");
            return NULL;
        }
        strcpy(res, tok);
    }

    free(jsoncp);
    return res;
}

char* fold(const char* str) {
    // work on a copy of the str
    char* strcp = (char *) malloc(strlen(str) * sizeof(char) + 1);
    if (strcp == NULL) {
        perror("malloc failed");
        return NULL;
    }
    strcpy(strcp, str);

    // create buffer for escaped result TEXT
    char* buf = malloc(1);
    if (buf == NULL) {
        perror("malloc failed");
        return NULL;
    }
    buf[0] = '\0';

    void* newbuf;
    // bufl is the current buffer size incl. \0
    int bufl = 1;
    // i is the iterator in strcp
    char* i = strcp;
    // escch is the char to be escaped,
    // only written when esc=true
    char escch;
    bool esc = false;

    while(*i != '\0' || esc) {
        fprintf(stderr, "strlen(buf): %i\n", bufl);
        fprintf(stderr, "*i: %c\n", *i);
        fprintf(stderr, "escch: %c\n", escch);
        fprintf(stderr, "esc: %i\n", esc);
        fprintf(stderr, "buffer: %s\n\n", buf);

        newbuf = realloc(buf, ++bufl);
        if (newbuf == NULL) {
            perror("realloc failed");
            free(buf);
            return NULL;
        }
        buf = (char*) newbuf;

        if ((bufl > 1) && ((bufl % 77) == 0)) {
            // break lines after 75 chars
            // split between any two characters by inserting a CRLF
            // immediately followed by a white space character
            buf[bufl-2] = '\n';
            escch = ' ';
            esc = true;
            continue;
        }

        if (esc) {
            // only escape char, do not advance iterator i
            buf[bufl-2] = escch;
            esc = false;
        } else {
            // escape characters
            // https://datatracker.ietf.org/doc/html/rfc5545#section-3.3.11
            switch (*i) {
                case 0x5c: // backslash
                    buf[bufl-2] = 0x5c;
                    escch = 0x5c;
                    esc = true;
                    break;
                case ';':
                    buf[bufl-2] = 0x5c;
                    escch = ';';
                    esc = true;
                    break;
                case ',':
                    buf[bufl-2] = 0x5c;
                    escch = ',';
                    esc = true;
                    break;
                case '\n':
                    buf[bufl-2] = 0x5c;
                    escch = 'n';
                    esc = true;
                    break;
                default:
                    // write regular character from strcp
                    buf[bufl-2] = *i;
                    break;
            }
            i++;
        }

        // terminate the char string in any case (esc or not)
        buf[bufl-1] = '\0';
    }

    fprintf(stderr, "escch: %c\n", escch);
    fprintf(stderr, "end: %c\n", buf[bufl]);

    free(strcp);
    return buf;
}

char* unfold(const char* str) {
    fprintf(stderr, "Before unfolding: %s\n", str);

    // work on a copy of the str
    char* strcp = (char *) malloc(strlen(str) * sizeof(char) + 1);
    if (strcp == NULL) {
        perror("malloc failed");
        return NULL;
    }
    strcpy(strcp, str);

    char* res = strtok(strcp, "\n");

    if (res == NULL) {
        fprintf(stderr, "No more lines in multiline string, stop unfolding.\n");
        free(strcp);
        return NULL;
    }

    char* buf = malloc(strlen(res) + 1);
    if (buf == NULL) {
        perror("malloc failed");
        return NULL;
    }
    strcpy(buf, res);

    char* newbuf;
    regex_t re;
    regmatch_t pm[1];

    if (regcomp(&re, "^[^ \t]", 0) != 0) {
        perror("Failed to compile regex");
        return NULL;
    }

    while (res != NULL) {
        res = strtok(NULL, "\n");

        if (res == NULL) {
            fprintf(stderr, "No more lines in multiline string, stop unfolding.\n");
            break;
        }

        if (regexec(&re, res, 1, pm, 0) == 0) {
            // Stop unfolding if line does not start with white space/tab:
            // https://datatracker.ietf.org/doc/html/rfc2445#section-4.1
            break;
        }

        newbuf = realloc(buf, strlen(buf) + strlen(res) + 1);
        if (newbuf == NULL) {
            perror("realloc failed");
            break;
        } else {
            buf = newbuf;
            strcat(buf, res + 1);
        }
    }

    regfree(&re);
    free(strcp);
    return buf;
}

/* Find ical key in string. The value of 'start_pos' is set to the start position
   of the value (match) after the colon (':').
*/
char* extract_ical_field(const char* ics, char* key, long* start_pos, bool multiline) {
    regex_t re;
    regmatch_t pm[1];
    char key_regex[strlen(key) + 1];
    sprintf(key_regex, "^%s", key);
    fprintf(stderr, "Key regex: %s\n", key_regex);

    if (regcomp(&re, key_regex, 0) != 0) {
        perror("Failed to compile regex");
        return NULL;
    }

    // work on a copy of the ical xml response
    char* icscp = (char *) malloc(strlen(ics) * sizeof(char) + 1);
    if (icscp == NULL) {
        perror("malloc failed");
        return NULL;
    }
    strcpy(icscp, ics);

    // tokenize ical by newlines
    char* res = strtok(icscp, "\n");

    while (res != NULL) {
        if (regexec(&re, res, 1, pm, 0) == 0) {
            res = strstr(res, ":"); // value
            res++; // strip the ":"

            fprintf(stderr, "Extracted ical result value: %s\n", res);
            fprintf(stderr, "Extracted ical result size: %li\n", strlen(res));

            fprintf(stderr, "Sizeof ics: %li\n", strlen(ics));
            fprintf(stderr, "Start pos: %li\n", *start_pos);
            fprintf(stderr, "Res: %s\n", res);
            *start_pos = res - icscp;
            fprintf(stderr, "Start pos: %li\n", *start_pos);
            break;
        }
        // key not in this line, advance line
        res = strtok(NULL, "\n");
    }

    char* buf = NULL;

    if (res != NULL) {
        if (strlen(res) == 0) {
            // empty remote value
            buf = NULL;
        } else if (multiline) {
            buf = unfold(ics + *start_pos);
        } else {
            buf = malloc(strlen(res) + 1);
            if (buf == NULL) {
                perror("malloc failed");
                return NULL;
            }
            strcpy(buf, res);
        }
    }

    regfree(&re);
    free(icscp);

    return buf;
}

// Return expanded file path
char* expand_path(const char* str) {
    char* res;
    wordexp_t str_wordexp;
    if ( wordexp( str, &str_wordexp, 0 ) == 0) {
        res = (char *) calloc(strlen(str_wordexp.we_wordv[0]) + 1, sizeof(char));
        strcpy(res, str_wordexp.we_wordv[0]);
    }
    wordfree(&str_wordexp);
    return res;
}

// Get last occurence of string in string
// https://stackoverflow.com/questions/20213799/finding-last-occurence-of-string
char* strrstr(char *haystack, char *needle) {
    char *i;
    int nlen = strlen(needle);
    for (i = haystack + strlen(haystack) - nlen; i >= haystack; i--) {
        if (strncmp(i, needle, nlen) == 0) {
            return i;
        }
    }
    return NULL;
}

/* Writes file path for 'date' entry to 'rpath'. '*rpath' is NULL on error. */
void fpath(const char* dir, size_t dir_size, const struct tm* date, char** rpath, size_t rpath_size)
{
    // check size of result path
    if (dir_size + 1 > rpath_size) {
        fprintf(stderr, "Directory path too long");
        *rpath = NULL;
        return;
    }

    // add path of the diary dir to result path
    strcpy(*rpath, dir);

    // check for terminating '/' in path
    if (dir[dir_size - 1] != '/') {
        // check size again to accommodate '/'
        if (dir_size + 1 > rpath_size) {
            fprintf(stderr, "Directory path too long");
            *rpath = NULL;
            return;
        }
        strcat(*rpath, "/");
    }

    char dstr[16];
    strftime(dstr, sizeof dstr, CONFIG.fmt, date);

    // append date to the result path
    if (strlen(*rpath) + strlen(dstr) > rpath_size) {
        fprintf(stderr, "File path too long");
        *rpath = NULL;
        return;
    }
    strcat(*rpath, dstr);
}

bool go_to(WINDOW* calendar, WINDOW* aside, time_t date, int* cur_pad_pos, struct tm* curs_date, struct tm* cal_start, struct tm* cal_end) {
    if (date < mktime(cal_start) || date > mktime(cal_end))
        return false;

    int diff_seconds = date - mktime(cal_start);
    int diff_days = diff_seconds / 60 / 60 / 24;
    int diff_weeks = diff_days / 7;
    int diff_wdays = diff_days % 7;

    localtime_r(&date, curs_date);

    int cy, cx;
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
    prefresh(aside, *cur_pad_pos, 0, 1, 0, LINES - 1, ASIDE_WIDTH);
    prefresh(calendar, *cur_pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);

    return true;
}


void* show_progress(void* vargp){
    WINDOW* header = (WINDOW*) vargp;
    mvwprintw(header, 0, COLS - CAL_WIDTH - ASIDE_WIDTH - 11, "   syncing ");
    for(;;) {
        mvwprintw(header, 0, COLS - CAL_WIDTH - ASIDE_WIDTH - 10, "|");
        wrefresh(header);
        usleep(200000);
        mvwprintw(header, 0, COLS - CAL_WIDTH - ASIDE_WIDTH - 10, "/");
        wrefresh(header);
        usleep(200000);
        mvwprintw(header, 0, COLS - CAL_WIDTH - ASIDE_WIDTH - 10, "-");
        wrefresh(header);
        usleep(200000);
        mvwprintw(header, 0, COLS - CAL_WIDTH - ASIDE_WIDTH - 10, "\\");
        wrefresh(header);
        usleep(200000);
    }
}

config CONFIG = {
    .range = 1,
    .weekday = 1,
    .fmt = "%Y-%m-%d",
    .editor = "",
    .google_tokenfile = GOOGLE_OAUTH_TOKEN_FILE,
    .google_clientid = GOOGLE_OAUTH_CLIENT_ID,
    .google_secretid = GOOGLE_OAUTH_CLIENT_SECRET,
    .google_calendar = ""
};
