/*
 * Common header for command implementations.
 * Shared structures used across multiple commands.
 */

#ifndef COMMANDS_COMMON_H
#define COMMANDS_COMMON_H

#include <stdint.h>
#include "../hardware.h"
#include "../utils.h"

/* Data structure for holding all readings (used by get command) */
typedef struct {
    int address;
    int channel;
    
    /* Flags for what data is available */
    int has_serial;
    int has_cal_date;
    int has_cal_coeffs;
    int has_temp;
    int has_adc;
    int has_cjc;
    int has_interval;
    
    /* Data values */
    char serial[16];
    char cal_date[16];
    CalibrationInfo cal_coeffs;
    double temperature;
    double adc_voltage;
    double cjc_temp;
    uint8_t update_interval;
} ThermoData;

/* ThermoData helper functions (implemented in get.c) */
void thermo_data_init(ThermoData *data, int address, int channel);
int thermo_data_collect(ThermoData *data, int get_serial, int get_cal_date, 
                        int get_cal_coeffs, int get_temp, int get_adc, 
                        int get_cjc, int get_interval, const char *tc_type);
void thermo_data_output_json(const ThermoData *data, int include_address_channel);
void thermo_data_output_table(const ThermoData *data, int show_header, int clean_mode);

#endif /* COMMANDS_COMMON_H */
