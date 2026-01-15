/*
 * Utility functions for JSON output, formatting, and display.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>

#include "utils.h"
#include "hardware.h"

#include "cJSON.h"

/* ANSI color codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[1;31m"
#define COLOR_GREEN   "\033[1;32m"
#define COLOR_YELLOW  "\033[1;33m"
#define COLOR_BLUE    "\033[1;34m"
#define COLOR_MAGENTA "\033[1;35m"
#define COLOR_CYAN    "\033[1;36m"

/* Format temperature value for display */
char* format_temperature(double temp) {
    static char buffer[64];
    
    if (temp == OPEN_TC_VALUE) {
        snprintf(buffer, sizeof(buffer), "OPEN");
    } else if (temp == OVERRANGE_TC_VALUE) {
        snprintf(buffer, sizeof(buffer), "OVERRANGE");
    } else if (temp == COMMON_MODE_TC_VALUE) {
        snprintf(buffer, sizeof(buffer), "COMMON_MODE_ERROR");
    } else if (isnan(temp)) {
        snprintf(buffer, sizeof(buffer), "NaN");
    } else {
        snprintf(buffer, sizeof(buffer), "%.2f", temp);
    }
    
    return buffer;
}

/* Print colored text to console */
void print_colored(const char *color, const char *text) {
    if (color == NULL) {
        printf("%s", text);
    } else {
        printf("%s%s%s", color, text, COLOR_RESET);
    }
}

/* Print with specific color name */
void print_with_color(const char *color_name, const char *format, ...) {
    const char *color_code = NULL;
    
    if (strcmp(color_name, "red") == 0) color_code = COLOR_RED;
    else if (strcmp(color_name, "green") == 0) color_code = COLOR_GREEN;
    else if (strcmp(color_name, "yellow") == 0) color_code = COLOR_YELLOW;
    else if (strcmp(color_name, "blue") == 0) color_code = COLOR_BLUE;
    else if (strcmp(color_name, "magenta") == 0) color_code = COLOR_MAGENTA;
    else if (strcmp(color_name, "cyan") == 0) color_code = COLOR_CYAN;
    
    if (color_code) {
        printf("%s", color_code);
    }
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    if (color_code) {
        printf("%s", COLOR_RESET);
    }
    printf("\n");
}

/* Table implementation */

Table* table_create(int num_cols) {
    Table *table = (Table*)calloc(1, sizeof(Table));
    table->num_cols = num_cols;
    table->num_rows = 0;
    table->headers = (char**)calloc(num_cols, sizeof(char*));
    table->col_widths = (int*)calloc(num_cols, sizeof(int));
    table->rows = NULL;
    return table;
}

void table_set_header(Table *table, int col, const char *header) {
    if (col >= table->num_cols) return;
    
    if (table->headers[col]) free(table->headers[col]);
    table->headers[col] = strdup(header);
    
    int len = strlen(header);
    if (len > table->col_widths[col]) {
        table->col_widths[col] = len;
    }
}

void table_add_row(Table *table, char **row_data) {
    table->rows = (char***)realloc(table->rows, (table->num_rows + 1) * sizeof(char**));
    table->rows[table->num_rows] = (char**)calloc(table->num_cols, sizeof(char*));
    
    for (int i = 0; i < table->num_cols; i++) {
        table->rows[table->num_rows][i] = strdup(row_data[i]);
        int len = strlen(row_data[i]);
        if (len > table->col_widths[i]) {
            table->col_widths[i] = len;
        }
    }
    
    table->num_rows++;
}

void table_print(Table *table, const char *title) {
    if (title) {
        printf("\n%s\n", title);
    }
    
    /* Print top border */
    for (int i = 0; i < table->num_cols; i++) {
        printf("+");
        for (int j = 0; j < table->col_widths[i] + 2; j++) {
            printf("-");
        }
    }
    printf("+\n");
    
    /* Print headers */
    for (int i = 0; i < table->num_cols; i++) {
        printf("| %-*s ", table->col_widths[i], table->headers[i]);
    }
    printf("|\n");
    
    /* Print header separator */
    for (int i = 0; i < table->num_cols; i++) {
        printf("+");
        for (int j = 0; j < table->col_widths[i] + 2; j++) {
            printf("=");
        }
    }
    printf("+\n");
    
    /* Print rows */
    for (int r = 0; r < table->num_rows; r++) {
        for (int c = 0; c < table->num_cols; c++) {
            printf("| %-*s ", table->col_widths[c], table->rows[r][c]);
        }
        printf("|\n");
    }
    
    /* Print bottom border */
    for (int i = 0; i < table->num_cols; i++) {
        printf("+");
        for (int j = 0; j < table->col_widths[i] + 2; j++) {
            printf("-");
        }
    }
    printf("+\n\n");
}

void table_free(Table *table) {
    if (!table) return;
    
    for (int i = 0; i < table->num_cols; i++) {
        if (table->headers[i]) free(table->headers[i]);
    }
    free(table->headers);
    
    for (int r = 0; r < table->num_rows; r++) {
        for (int c = 0; c < table->num_cols; c++) {
            if (table->rows[r][c]) free(table->rows[r][c]);
        }
        free(table->rows[r]);
    }
    free(table->rows);
    free(table->col_widths);
    free(table);
}
