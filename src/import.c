#include "import.h"

/* Import journal entries from an ics file */
void ics_import(const char* ics_input, WINDOW* header, WINDOW* cal, WINDOW* aside, int* pad_pos, struct tm* curs_date, struct tm* cal_start, struct tm* cal_end) {
    pthread_t progress_tid;

    FILE* pfile = fopen(ics_input, "r");
    if (pfile == NULL) {
        perror("Error opening file");
        return;
    }

    fseek(pfile, 0, SEEK_END);
    long ics_bytes = ftell(pfile);
    rewind(pfile);

    char* ics = malloc(ics_bytes + 1);
    fread(ics, 1, ics_bytes, pfile);
    fclose(pfile);

    ics[ics_bytes] = 0;
    // fprintf(stderr, "Import ICS file: %s\n", ics);

    int conf_ch = 0;
    char dstr[16];
    struct tm date = {};

    long search_pos = 0;
    char* vevent;
    char* vevent_date;
    char* vevent_desc;

    // find all VEVENTs and write to files
    char *i = ics;
    char* j;
    while (i < ics + ics_bytes) {
        vevent = extract_ical_field(i, "BEGIN:VEVENT", &search_pos, false);
        vevent_date = extract_ical_field(i, "DTSTART", &search_pos, false);
        vevent_desc = extract_ical_field(i, "DESCRIPTION", &search_pos, true);
        if (vevent == NULL || vevent_desc == NULL) {
            free(vevent);
            free(vevent_date);
            free(vevent_desc);
            break;
        }

        i += search_pos;

        // fprintf(stderr, "VEVENT DESCRIPTION: \n\n%s\n\n", vevent_desc);

        // parse date
        strptime(vevent_date, "%Y%m%d", &date);
        strftime(dstr, sizeof dstr, CONFIG.fmt, &date);

        // get path of entry
        char path[100];
        char* ppath = path;
        fpath(CONFIG.dir, strlen(CONFIG.dir), &date, &ppath, sizeof path);
        fprintf(stderr, "Import date file path: %s\n", path);

        if (conf_ch == 'c') {
            free(vevent);
            free(vevent_date);
            free(vevent_desc);
            break;
        }

        if (conf_ch != 'a') {
            // prepare header for confirmation dialogue
            curs_set(2);
            noecho();
            wclear(header);

            // ask for confirmation
            mvwprintw(header, 0, 0, "Import entry '%s' and overwrite local file? [(Y)es/(a)ll/(n)o/(c)ancel] ", dstr);
            conf_ch = wgetch(header);
        }

        if (conf_ch == 'y' || conf_ch == 'Y' || conf_ch == 'a' || conf_ch == '\n') {
            pthread_create(&progress_tid, NULL, show_progress, (void*)header);
            pthread_detach(progress_tid);

            // persist VEVENT to local file
            FILE* cursordate_file = fopen(path, "wb");
            if (cursordate_file == NULL) {
                perror("Failed to open import date file");
            } else {
                for (j = vevent_desc; *j != '\0'; j++) {
                    if (vevent_desc[j-vevent_desc] == 0x5C) { // backslash
                        switch (*(j+1)) {
                            case 'n':
                                fputc('\n', cursordate_file);
                                j++;
                                break;
                            case 0x5c: // preserve real backslash
                                fputc(0x5c, cursordate_file);
                                j++;
                                break;
                        }
                    } else {
                        fputc(*j, cursordate_file);
                    }
                }
                fclose(cursordate_file);

                bool mv_valid = go_to(cal, aside, mktime(&date), pad_pos, curs_date, cal_start, cal_end);
                if (mv_valid) {
                    // add new entry highlight
                    chtype atrs = winch(cal) & A_ATTRIBUTES;
                    wchgat(cal, 2, atrs | A_BOLD, 0, NULL);
                    prefresh(cal, *pad_pos, 0, 1, ASIDE_WIDTH, LINES - 1, ASIDE_WIDTH + CAL_WIDTH);
                }
            }
            pthread_cancel(progress_tid);
        }

        // fprintf(stderr, "Import DTSTART: %s\n", desc);
        // fprintf(stderr, "Import DESCRIPTION: %s\n", desc);
        fprintf(stderr, "* * * * * * * * * * * * * \n");
        free(vevent);
        free(vevent_date);
        free(vevent_desc);
    }
    free(ics);
}