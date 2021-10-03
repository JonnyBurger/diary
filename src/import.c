#include "import.h"

/* Import journal entries from an ics file */
void ics_import(const char* ics_input) {
    FILE* pfile = fopen(ics_input, "r");

    fseek(pfile, 0, SEEK_END);
    long ics_bytes = ftell(pfile) + 1;
    rewind(pfile);

    char* ics = malloc(ics_bytes);
    fread(ics, 1, ics_bytes, pfile);
    fclose(pfile);

    ics[ics_bytes] = 0;
    // fprintf(stderr, "Import ICS file: %s\n", ics);

    // find all VEVENTs

    long search_pos = 0;
    char *i = ics;
    char* vevent;
    char* date;
    char* desc;

    for (;;) {
        vevent = extract_ical_field(i, "BEGIN:VEVENT", &search_pos, false);
        if (vevent == NULL) {
            break;
        }
        // date = extract_ical_field((ics+search_pos), "DTSTART", &search_pos, false);
        desc = extract_ical_field(i, "DESCRIPTION", &search_pos, true);
        // fprintf(stderr, "Import DTSTART: %s\n", desc);
        // fprintf(stderr, "Import DESCRIPTION: %s\n", desc);
        fprintf(stderr, "* * * * * * * * * * * * * \n");

        free(vevent);
        // free(date);
        free(desc);

        i += search_pos;
    }
    free(ics);
}