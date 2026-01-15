/*
 * Hardware abstraction for MCC 134 thermocouple DAQ boards.
 *
 * This module provides a clean interface to the daqhats library,
 * handling board initialization, configuration, and data acquisition.
 *
 * Board lifecycle: caller must use thermo_open() before any operations
 * and thermo_close() when done. This allows efficient batching of operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <daqhats/daqhats.h>

#include "hardware.h"

/* Convert string to TC type enum */
uint8_t thermo_tc_type_from_string(const char *tc_type_str) {
    if (strcmp(tc_type_str, "K") == 0) return TC_TYPE_K;
    if (strcmp(tc_type_str, "J") == 0) return TC_TYPE_J;
    if (strcmp(tc_type_str, "T") == 0) return TC_TYPE_T;
    if (strcmp(tc_type_str, "E") == 0) return TC_TYPE_E;
    if (strcmp(tc_type_str, "R") == 0) return TC_TYPE_R;
    if (strcmp(tc_type_str, "S") == 0) return TC_TYPE_S;
    if (strcmp(tc_type_str, "B") == 0) return TC_TYPE_B;
    if (strcmp(tc_type_str, "N") == 0) return TC_TYPE_N;
    return TC_DISABLED;
}

/* Open a board for operations */
int thermo_open(uint8_t address) {
    int result = mcc134_open(address);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Close a board */
int thermo_close(uint8_t address) {
    int result = mcc134_close(address);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Check if a board is open */
int thermo_is_open(uint8_t address) {
    return mcc134_is_open(address);
}

/* List all connected MCC 134 boards (no open required) */
int thermo_list_boards(struct HatInfo **boards, int *count) {
    int num_boards = hat_list(HAT_ID_MCC_134, NULL);
    if (num_boards <= 0) {
        *boards = NULL;
        *count = 0;
        return THERMO_SUCCESS;
    }

    *boards = (struct HatInfo*)malloc(num_boards * sizeof(struct HatInfo));
    if (*boards == NULL) {
        return THERMO_ERROR;
    }

    hat_list(HAT_ID_MCC_134, *boards);
    *count = num_boards;
    return THERMO_SUCCESS;
}

/* Get board serial number (board must be open) */
int thermo_get_serial(uint8_t address, char *buffer, size_t len) {
    if (buffer == NULL || len < 9) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_serial(address, buffer);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Get calibration date (board must be open) */
int thermo_get_calibration_date(uint8_t address, char *buffer, size_t len) {
    if (buffer == NULL || len < 11) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_calibration_date(address, buffer);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Get calibration coefficients (board must be open) */
int thermo_get_calibration_coeffs(uint8_t address, uint8_t channel, CalibrationInfo *cal) {
    if (cal == NULL || channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_calibration_coefficient_read(address, channel, &cal->slope, &cal->offset);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Set calibration coefficients (board must be open) */
int thermo_set_calibration_coeffs(uint8_t address, uint8_t channel, double slope, double offset) {
    if (channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_calibration_coefficient_write(address, channel, slope, offset);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Get update interval (board must be open) */
int thermo_get_update_interval(uint8_t address, uint8_t *interval) {
    if (interval == NULL) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_update_interval_read(address, interval);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Set update interval (board must be open) */
int thermo_set_update_interval(uint8_t address, uint8_t interval) {
    if (interval < 1 || interval > 255) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_update_interval_write(address, interval);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Set thermocouple type for a channel (board must be open) */
int thermo_set_tc_type(uint8_t address, uint8_t channel, const char *tc_type_str) {
    if (tc_type_str == NULL || channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    uint8_t tc_type = thermo_tc_type_from_string(tc_type_str);
    if (tc_type == TC_DISABLED && strcmp(tc_type_str, "DISABLED") != 0) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_tc_type_write(address, channel, tc_type);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Read temperature from channel (board must be open, tc_type must be set) */
int thermo_read_temp(uint8_t address, uint8_t channel, double *value) {
    if (value == NULL || channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_t_in_read(address, channel, value);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Read ADC voltage from channel (board must be open, tc_type must be set) */
int thermo_read_adc(uint8_t address, uint8_t channel, double *value) {
    if (value == NULL || channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_a_in_read(address, channel, OPTS_DEFAULT, value);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Read CJC temperature from channel (board must be open) */
int thermo_read_cjc(uint8_t address, uint8_t channel, double *value) {
    if (value == NULL || channel > 3) {
        return THERMO_INVALID_PARAM;
    }

    int result = mcc134_cjc_read(address, channel, value);
    return (result == RESULT_SUCCESS) ? THERMO_SUCCESS : THERMO_ERROR;
}

/* Wait for readings to stabilize after setting TC type */
void thermo_wait_for_readings(void) {
    // TODO: Check if this is necessary
    // usleep(1100000); /* 1.1 seconds - MCC134 updates every 1 second */
}
