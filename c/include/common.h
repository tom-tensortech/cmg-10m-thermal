/*
 * Common header for shared data structures and configuration.
 * Includes ThermoData structure and configuration management.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include "hardware.h"

/* Thermal source configuration */
typedef struct {
    char key[64];
    uint8_t address;
    uint8_t channel;
    char tc_type[8];
} ThermalSource;

/* Configuration structure */
typedef struct {
    ThermalSource *sources;
    int source_count;
} Config;

/* Data structure for holding all readings */
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

/* Configuration functions */
int config_load(const char *path, Config *config);
void config_free(Config *config);
int config_create_example(const char *output_path);

#endif /* COMMON_H */
