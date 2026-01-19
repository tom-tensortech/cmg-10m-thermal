/*
 * Utility functions header.
 * Formatting, tables, and display utilities.
 */

#ifndef UTILS_H
#define UTILS_H

#include "common.h"

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

/* Debug printing macro */
#ifdef DEBUG
#define DEBUG_PRINT(fmt, ...) \
    fprintf(stderr, "[DEBUG] %s:%d:%s(): " fmt "\n", \
            __FILE__, __LINE__, __func__, ##__VA_ARGS__)
#else
#define DEBUG_PRINT(fmt, ...) do {} while(0)
#endif

/* Scope profiler for performance measurement */
#ifdef DEBUG
#include <time.h>

typedef struct {
    struct timespec start;
    const char *scope_name;
    const char *file;
    int line;
} ScopeTimer;

void __scope_timer_cleanup(ScopeTimer *timer);

/* PROFILE_SCOPE(name) - Profile execution time of a scope
 * Usage:
 *   void my_function() {
 *       PROFILE_SCOPE("my_function");
 *       // ... code to profile ...
 *   }  // Automatically prints elapsed time when scope exits
 *
 * Only active when compiled with DEBUG=1
 */
#define PROFILE_SCOPE(name) \
    ScopeTimer __scope_timer_##__LINE__ \
    __attribute__((cleanup(__scope_timer_cleanup))) = { \
        .scope_name = name, \
        .file = __FILE__, \
        .line = __LINE__ \
    }; \
    clock_gettime(CLOCK_MONOTONIC, &__scope_timer_##__LINE__.start)

#else
#define PROFILE_SCOPE(name) do {} while(0)
#endif

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
void data_format_print_value(const char *label, double value, const char *unit, int indent, int key_width, int value_width, int unit_width);

/* Legacy ThermoData formatting (for bridge.c compatibility) */
void data_format_calculate_max_width(const ThermoData *data_array, int count, int *max_key_len, int *max_value_len, int *max_unit_len);
void data_format_output(const ThermoData *data, int indent, int key_width, int value_width, int unit_width);

/* New ChannelReading/BoardInfo formatting functions */
void reading_format_calculate_max_width(const ChannelReading *readings, const BoardInfo *board_infos, 
                                        const ThermalSource *sources, int count,
                                        int *max_key_len, int *max_value_width, int *max_unit_len);
void reading_format_output(const ChannelReading *reading, const BoardInfo *info, 
                          const ThermalSource *source, int indent,
                          int key_width, int value_width, int unit_width,
                          int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval);

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

/* Input validation helpers */
static inline int validate_address(int address) {
    return (address >= 0 && address <= 7);
}

static inline int validate_channel(int channel) {
    return (channel >= 0 && channel <= 3);
}

#endif /* UTILS_H */
