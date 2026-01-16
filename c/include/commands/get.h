/*
 * Get command header.
 * Reads data from MCC 134 channels.
 */

#ifndef COMMANDS_GET_H
#define COMMANDS_GET_H

#include "common.h"

int cmd_get(int argc, char **argv);

/* ThermoData helper functions */
void thermo_data_init(ThermoData *data, int address, int channel);
int thermo_data_collect(ThermoData *data, int get_serial, int get_cal_date, 
                        int get_cal_coeffs, int get_temp, int get_adc, 
                        int get_cjc, int get_interval, const char *tc_type);
void thermo_data_output_json(const ThermoData *data, int include_address_channel);
void thermo_data_output_table(const ThermoData *data, int show_header, int clean_mode);
void thermo_data_split(const ThermoData *data, ThermoData *static_data, ThermoData *dynamic_data);

#endif /* COMMANDS_GET_H */
