/*
 * JSON utility functions for thermo-cli.
 * Consolidates JSON building and output operations.
 */

#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include "common.h"
#include "cJSON.h"

/* ============================================================================
 * ChannelReading JSON functions
 * ============================================================================ */

/* Add reading fields to existing cJSON object */
void reading_add_to_json(cJSON *obj, const ChannelReading *reading);

/* Convert ChannelReading to cJSON object */
cJSON* reading_to_json(const ChannelReading *reading);

/* ============================================================================
 * BoardInfo JSON functions
 * ============================================================================ */

/* Add board info fields to existing cJSON object */
void board_info_add_to_json(cJSON *obj, const BoardInfo *info, int channel,
                           int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval);

/* Convert BoardInfo to cJSON object for a specific channel */
cJSON* board_info_to_json(const BoardInfo *info, int channel,
                         int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval);

/* ============================================================================
 * Combined JSON functions (ChannelReading + BoardInfo)
 * ============================================================================ */

/* Convert ChannelReading + BoardInfo to combined JSON */
cJSON* reading_with_info_to_json(const ChannelReading *reading,
                                  const BoardInfo *info,
                                  const char *key,
                                  int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval);

/* Convert array of ChannelReadings to JSON array with BoardInfo */
cJSON* readings_to_json_array(const ChannelReading *readings,
                               const BoardInfo *infos,
                               const ThermalSource *sources,
                               int count,
                               int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval);

/* ============================================================================
 * Output utilities
 * ============================================================================ */

/* Output cJSON to stdout and cleanup (formatted or compact) */
void json_print_and_free(cJSON *json, int formatted);

/* Output cJSON to stdout without cleanup */
void json_print(cJSON *json, int formatted);

#endif /* JSON_UTILS_H */
