/*
 * Utility functions header.
 * Formatting, tables, and display utilities.
 */

#ifndef UTILS_H
#define UTILS_H

/* Table structure for formatted output */
typedef struct {
    char **headers;
    char ***rows;
    int num_cols;
    int num_rows;
    int *col_widths;
} Table;

/* Table functions */
Table* table_create(int num_cols);
void table_set_header(Table *table, int col, const char *header);
void table_add_row(Table *table, char **row_data);
void table_print(Table *table, const char *title);
void table_free(Table *table);

/* Formatting functions */
char* format_temperature(double temp);
void print_colored(const char *color, const char *text);
void print_with_color(const char *color_name, const char *format, ...);

#endif /* UTILS_H */
