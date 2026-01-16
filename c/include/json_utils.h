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
void board_info_add_to_json(cJSON *obj, const BoardInfo *info, int channel);

/* Convert BoardInfo to cJSON object for a specific channel */
cJSON* board_info_to_json(const BoardInfo *info, int channel);

/* ============================================================================
 * ThermoData JSON functions (legacy compatibility)
 * ============================================================================ */

/* Add ThermoData fields to existing cJSON object */
void thermo_data_add_to_json(cJSON *obj, const ThermoData *data);

/* Convert ThermoData to cJSON object */
cJSON* thermo_data_to_json(const ThermoData *data, int include_address);

/* Convert ThermoData with key to cJSON object */
cJSON* thermo_data_to_json_with_key(const ThermoData *data, const char *key);

/* ============================================================================
 * Array/batch JSON functions
 * ============================================================================ */

/* Convert array of ThermoData to cJSON array */
cJSON* thermo_data_array_to_json(const ThermoData *data_array, int count,
                                  const ThermalSource *sources);

/* ============================================================================
 * Output utilities
 * ============================================================================ */

/* Output cJSON to stdout and cleanup (formatted or compact) */
void json_print_and_free(cJSON *json, int formatted);

/* Output cJSON to stdout without cleanup */
void json_print(cJSON *json, int formatted);

#endif /* JSON_UTILS_H */
