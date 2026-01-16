/*
 * Utility functions header.
 * Formatting, tables, and display utilities.
 */

#ifndef UTILS_H
#define UTILS_H

#include "common.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Data formatting structure */
typedef struct {
    const char *key;
    const char *unit;
} DataFormat;

/* Data format indices */
#define TEMP_FORMAT 0
#define ADC_FORMAT 1
#define CJC_FORMAT 2
#define CALI_SLOPE_FORMAT 3
#define CALI_OFFSET_FORMAT 4

/* Data format definitions */
extern const DataFormat DATA_FORMATS[5];

/* Data formatting functions */
int count_digits_before_decimal(double value);
void data_format_calculate_max_width(const ThermoData *data_array, int count, int *max_key_len, int *max_value_len, int *max_unit_len);
void data_format_print_value(const char *label, double value, const char *unit, int indent, int key_width, int value_width, int unit_width);
void data_format_output(const ThermoData *data, int indent, int key_width, int value_width, int unit_width);

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
