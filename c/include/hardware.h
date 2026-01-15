/*
 * Hardware abstraction layer header.
 * MCC 134 board interface functions.
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include <stdint.h>
#include <stddef.h>
#include <daqhats/daqhats.h>

/* Return codes */
#define THERMO_SUCCESS 0
#define THERMO_ERROR -1
#define THERMO_INVALID_PARAM -2
#define THERMO_NOT_FOUND -3
#define THERMO_IO_ERROR -4

/* Thermocouple type constants */
#define TC_TYPE_J 0
#define TC_TYPE_K 1
#define TC_TYPE_T 2
#define TC_TYPE_E 3
#define TC_TYPE_R 4
#define TC_TYPE_S 5
#define TC_TYPE_B 6
#define TC_TYPE_N 7
#define TC_DISABLED 8

/* Special temperature values from MCC 134 */
#define OPEN_TC_VALUE (-9999.0)
#define OVERRANGE_TC_VALUE (-8888.0)
#define COMMON_MODE_TC_VALUE (-7777.0)

/* Calibration info structure */
typedef struct {
    double slope;
    double offset;
} CalibrationInfo;

/* Board lifecycle - caller manages open/close for efficient batching */
int thermo_open(uint8_t address);
int thermo_close(uint8_t address);
int thermo_is_open(uint8_t address);

/* Hardware abstraction functions (board must be open unless noted) */
int thermo_list_boards(struct HatInfo **boards, int *count);  /* no open required */
int thermo_get_serial(uint8_t address, char *buffer, size_t len);
int thermo_get_calibration_date(uint8_t address, char *buffer, size_t len);
int thermo_get_calibration_coeffs(uint8_t address, uint8_t channel, CalibrationInfo *cal);
int thermo_set_calibration_coeffs(uint8_t address, uint8_t channel, double slope, double offset);
int thermo_get_update_interval(uint8_t address, uint8_t *interval);
int thermo_set_update_interval(uint8_t address, uint8_t interval);
int thermo_set_tc_type(uint8_t address, uint8_t channel, const char *tc_type_str);
uint8_t thermo_tc_type_from_string(const char *tc_type_str);

/* Reading functions (board must be open, tc_type must be set for temp/adc) */
int thermo_read_temp(uint8_t address, uint8_t channel, double *value);
int thermo_read_adc(uint8_t address, uint8_t channel, double *value);
int thermo_read_cjc(uint8_t address, uint8_t channel, double *value);
void thermo_wait_for_readings(void);

#endif /* HARDWARE_H */
