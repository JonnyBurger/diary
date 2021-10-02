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

    long search_pos = 0;
    char* desc = extract_ical_field(ics, "BEGIN", &search_pos, false);
    if (desc != NULL) {
        fprintf(stderr, "Import DESCRIPTION: %s\n", desc);
        fprintf(stderr, "Search pos: %li\n", search_pos);
        free(desc);
    }
    free(ics);
}