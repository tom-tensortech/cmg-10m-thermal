/*
 * Utility functions for JSON output, formatting, and display.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>

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

#ifdef DEBUG
/* Scope profiler cleanup function */
void __scope_timer_cleanup(ScopeTimer *timer) {
    struct timespec end;
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    double elapsed_ms = (end.tv_sec - timer->start.tv_sec) * 1000.0 +
                        (end.tv_nsec - timer->start.tv_nsec) / 1000000.0;
    
    fprintf(stderr, "[PROFILE] %s:%d '%s' took %.3f ms\n",
            timer->file, timer->line, timer->scope_name, elapsed_ms);
}
#endif

/* Data format definitions */
const DataFormat DATA_FORMATS[5] = {
    { "Temperature", "degC" },
    { "ADC", "V" },
    { "CJC", "degC" },
    { "Slope", "" },
    { "Offset", "" }
};

/* Calculate number of digits before decimal point */
int count_digits_before_decimal(double value) {
    if (value == 0.0) return 1;
    double abs_val = value < 0 ? -value : value;
    int digits = 0;
    if (abs_val >= 1.0) {
        digits = (int)log10(abs_val) + 1;
    } else {
        digits = 1;
    }
    return digits;
}

/* Calculate maximum width needed for values across all data */
void data_format_calculate_max_width(const ThermoData *data_array, int count, int *max_key_len, int *max_value_width, int *max_unit_len) {
    *max_key_len = 0;
    *max_unit_len = 0;
    
    int max_digits = 1;
    
    for (int i = 0; i < count; i++) {
        if (data_array[i].has_temp) {
            max_digits = MAX(max_digits, count_digits_before_decimal(data_array[i].temperature));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[TEMP_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[TEMP_FORMAT].unit));
        }
        if (data_array[i].has_adc) {
            max_digits = MAX(max_digits, count_digits_before_decimal(data_array[i].adc_voltage));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[ADC_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[ADC_FORMAT].unit));
        }
        if (data_array[i].has_cjc) {
            max_digits = MAX(max_digits, count_digits_before_decimal(data_array[i].cjc_temp));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CJC_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[CJC_FORMAT].unit));
        }
        if (data_array[i].has_cal_coeffs) {
            max_digits = MAX(max_digits, count_digits_before_decimal(data_array[i].cal_coeffs.slope));
            max_digits = MAX(max_digits, count_digits_before_decimal(data_array[i].cal_coeffs.offset));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CALI_SLOPE_FORMAT].key));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CALI_OFFSET_FORMAT].key));
        }
    }
    
    /* Total width: sign(1) + digits + decimal(1) + precision(6) */
    *max_value_width = max_digits + 8;
}

/* Print a formatted value line with indentation and alignment */
void data_format_print_value(const char *label, double value, const char *unit, int indent, int key_width, int value_width, int unit_width) {
    char *indent_str = malloc(indent + 1);
    memset(indent_str, ' ', indent);
    indent_str[indent] = '\0';

    /* Print indentation */
    printf("%s", indent_str);
    free(indent_str);
    
    /* Print label and value with aligned keys and units */
    if (unit && unit[0] != '\0') {
        printf("%-*s: %*.6f %*s\n", key_width, label, value_width, value, unit_width, unit);
    } else {
        printf("%-*s: %*.6f\n", key_width, label, value_width, value);
    }
}

/* Output all dynamic data fields for a ThermoData structure */
void data_format_output(const ThermoData *data, int indent, int key_width, int value_width, int unit_width) {
    char *indent_str = malloc(indent + 1);
    memset(indent_str, ' ', indent);
    indent_str[indent] = '\0';

    if (data->has_serial) {
        printf("%sSerial Number: %s\n", indent_str, data->serial);
    }
    
    if (data->has_cal_date) {
        printf("%sCalibration Date: %s\n", indent_str, data->cal_date);
    }
    
    if (data->has_cal_coeffs) {
        printf("%sCalibration Coefficients:\n", indent_str);
        data_format_print_value(DATA_FORMATS[CALI_SLOPE_FORMAT].key, 
                               data->cal_coeffs.slope,
                               DATA_FORMATS[CALI_SLOPE_FORMAT].unit,
                               indent + 4, key_width, value_width, unit_width);
        data_format_print_value(DATA_FORMATS[CALI_OFFSET_FORMAT].key,
                               data->cal_coeffs.offset,
                               DATA_FORMATS[CALI_OFFSET_FORMAT].unit,
                               indent + 4, key_width, value_width, unit_width);
    }
    
    if (data->has_interval) {
        printf("%sUpdate Interval: %d seconds\n", indent_str, data->update_interval);
    }
    
    if (data->has_temp) {
        data_format_print_value(DATA_FORMATS[TEMP_FORMAT].key,
                               data->temperature,
                               DATA_FORMATS[TEMP_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    
    if (data->has_adc) {
        data_format_print_value(DATA_FORMATS[ADC_FORMAT].key,
                               data->adc_voltage,
                               DATA_FORMATS[ADC_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    
    if (data->has_cjc) {
        data_format_print_value(DATA_FORMATS[CJC_FORMAT].key,
                               data->cjc_temp,
                               DATA_FORMATS[CJC_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    free(indent_str);
}

/* ============================================================================
 * NEW FORMATTING API for ChannelReading/BoardInfo
 * ============================================================================ */

/* Calculate maximum width needed for values across all readings */
void reading_format_calculate_max_width(const ChannelReading *readings, const BoardInfo *board_infos,
                                        const ThermalSource *sources, int count,
                                        int *max_key_len, int *max_value_width, int *max_unit_len) {
    *max_key_len = 0;
    *max_unit_len = 0;
    
    int max_digits = 1;
    
    for (int i = 0; i < count; i++) {
        const ChannelReading *reading = &readings[i];
        const BoardInfo *info = board_infos ? &board_infos[reading->address] : NULL;
        
        if (reading->has_temp) {
            max_digits = MAX(max_digits, count_digits_before_decimal(reading->temperature));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[TEMP_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[TEMP_FORMAT].unit));
        }
        if (reading->has_adc) {
            max_digits = MAX(max_digits, count_digits_before_decimal(reading->adc_voltage));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[ADC_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[ADC_FORMAT].unit));
        }
        if (reading->has_cjc) {
            max_digits = MAX(max_digits, count_digits_before_decimal(reading->cjc_temp));
            *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CJC_FORMAT].key));
            *max_unit_len = MAX(*max_unit_len, (int)strlen(DATA_FORMATS[CJC_FORMAT].unit));
        }
        if (info && reading->channel < MCC134_NUM_CHANNELS) {
            CalibrationInfo *cal = &info->channels[reading->channel].cal_coeffs;
            if (cal->slope != DEFAULT_CALIBRATION_SLOPE || cal->offset != DEFAULT_CALIBRATION_OFFSET) {
                max_digits = MAX(max_digits, count_digits_before_decimal(cal->slope));
                max_digits = MAX(max_digits, count_digits_before_decimal(cal->offset));
                *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CALI_SLOPE_FORMAT].key));
                *max_key_len = MAX(*max_key_len, (int)strlen(DATA_FORMATS[CALI_OFFSET_FORMAT].key));
            }
        }
    }
    
    /* Total width: sign(1) + digits + decimal(1) + precision(6) */
    *max_value_width = max_digits + 8;
}

/* Output all data for a ChannelReading with optional BoardInfo */
void reading_format_output(const ChannelReading *reading, const BoardInfo *info,
                          const ThermalSource *source, int indent,
                          int key_width, int value_width, int unit_width,
                          int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval) {
    char *indent_str = malloc(indent + 1);
    memset(indent_str, ' ', indent);
    indent_str[indent] = '\0';

    /* Output board info fields if available and requested */
    if (info) {
        if (show_serial && info->serial[0] != '\0') {
            printf("%sSerial Number: %s\n", indent_str, info->serial);
        }
        
        if (show_cal_date && reading->channel < MCC134_NUM_CHANNELS && info->channels[reading->channel].cal_date[0] != '\0') {
            printf("%sCalibration Date: %s\n", indent_str, info->channels[reading->channel].cal_date);
        }
        
        if (show_cal_coeffs && reading->channel < MCC134_NUM_CHANNELS) {
            CalibrationInfo *cal = &info->channels[reading->channel].cal_coeffs;
            if (cal->slope != DEFAULT_CALIBRATION_SLOPE || cal->offset != DEFAULT_CALIBRATION_OFFSET) {
                printf("%sCalibration Coefficients:\n", indent_str);
                data_format_print_value(DATA_FORMATS[CALI_SLOPE_FORMAT].key,
                                       cal->slope,
                                       DATA_FORMATS[CALI_SLOPE_FORMAT].unit,
                                       indent + 4, key_width, value_width, unit_width);
                data_format_print_value(DATA_FORMATS[CALI_OFFSET_FORMAT].key,
                                       cal->offset,
                                       DATA_FORMATS[CALI_OFFSET_FORMAT].unit,
                                       indent + 4, key_width, value_width, unit_width);
            }
        }
        
        if (show_interval && info->update_interval > 0 && info->update_interval != DEFAULT_UPDATE_INTERVAL) {
            printf("%sUpdate Interval: %d seconds\n", indent_str, info->update_interval);
        }
    }
    
    /* Output dynamic readings */
    if (reading->has_temp) {
        data_format_print_value(DATA_FORMATS[TEMP_FORMAT].key,
                               reading->temperature,
                               DATA_FORMATS[TEMP_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    
    if (reading->has_adc) {
        data_format_print_value(DATA_FORMATS[ADC_FORMAT].key,
                               reading->adc_voltage,
                               DATA_FORMATS[ADC_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    
    if (reading->has_cjc) {
        data_format_print_value(DATA_FORMATS[CJC_FORMAT].key,
                               reading->cjc_temp,
                               DATA_FORMATS[CJC_FORMAT].unit,
                               indent, key_width, value_width, unit_width);
    }
    
    free(indent_str);
}

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
