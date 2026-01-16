/*
 * Common header for shared data structures and configuration.
 * Includes ThermoData structure and configuration management.
 */

#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include "hardware.h"


#define DEFAULT_CALIBRATION_SLOPE 0.999560
#define DEFAULT_CALIBRATION_OFFSET -38.955465
#define DEFAULT_UPDATE_INTERVAL 1  /* seconds */

#define MCC134_NUM_CHANNELS 4

/* ============================================================================
 * NEW DATA STRUCTURES (Phase 3 refactoring)
 * These provide cleaner separation between static board info and dynamic readings.
 * ============================================================================ */

/* Per-channel calibration and configuration */
typedef struct {
    char cal_date[16];           /* Calibration date for this channel */
    CalibrationInfo cal_coeffs;  /* Slope and offset */
    uint8_t tc_type;             /* Thermocouple type enum value */
} ChannelConfig;

/* Per-board static information */
typedef struct {
    uint8_t address;
    char serial[16];
    uint8_t update_interval;
    ChannelConfig channels[MCC134_NUM_CHANNELS];  /* Per-channel data */
} BoardInfo;

/* Dynamic reading from a single channel */
typedef struct {
    uint8_t address;
    uint8_t channel;
    double temperature;
    double adc_voltage;
    double cjc_temp;
    
    /* Availability flags (bitfields for compact storage) */
    unsigned has_temp : 1;
    unsigned has_adc : 1;
    unsigned has_cjc : 1;
} ChannelReading;

/* ============================================================================
 * EXISTING DATA STRUCTURES (maintained for backward compatibility)
 * Will be gradually migrated to use new structures internally.
 * ============================================================================ */

/* Thermal source configuration (from CLI args or config file) */
typedef struct {
    char key[64];
    uint8_t address;
    uint8_t channel;
    char tc_type[8];
    CalibrationInfo cal_coeffs;
    int update_interval;
} ThermalSource;

/* Configuration structure */
typedef struct {
    ThermalSource *sources;
    int source_count;
} Config;

/* Data structure for holding all readings (legacy, still widely used) */
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

/* ============================================================================
 * ADAPTER FUNCTIONS (for gradual migration)
 * ============================================================================ */

/* Initialize a ChannelReading structure */
void channel_reading_init(ChannelReading *reading, uint8_t address, uint8_t channel);

/* Initialize a BoardInfo structure */
void board_info_init(BoardInfo *info, uint8_t address);

/* Convert ThermoData to ChannelReading (extracts dynamic data only) */
void thermo_data_to_reading(const ThermoData *data, ChannelReading *reading);

/* Convert ChannelReading back to ThermoData (for functions that still need ThermoData) */
void reading_to_thermo_data(const ChannelReading *reading, ThermoData *data);

/* Extract BoardInfo from ThermoData (for a single channel) */
void thermo_data_to_board_info(const ThermoData *data, BoardInfo *info, int channel);

/* ============================================================================
 * CONFIGURATION FUNCTIONS
 * ============================================================================ */

int config_load(const char *path, Config *config);
void config_free(Config *config);
int config_create_example(const char *output_path);

#endif /* COMMON_H */
