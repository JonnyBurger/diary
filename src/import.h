#ifndef DIARY_IMPORT_H
#define DIARY_IMPORT_H

#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <regex.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include "utils.h"

void ics_import(const char* ics_input, WINDOW* header, WINDOW* cal, WINDOW* aside, int* pad_pos, struct tm* curs_date, struct tm* cal_start, struct tm* cal_end);

#endif