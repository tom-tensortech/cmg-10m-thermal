/*
 * JSON utility functions for thermo-cli.
 * Consolidates JSON building and output operations.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "json_utils.h"

/* ============================================================================
 * ChannelReading JSON functions
 * ============================================================================ */

void reading_add_to_json(cJSON *obj, const ChannelReading *reading) {
    if (reading->has_temp) {
        cJSON_AddNumberToObject(obj, "TEMPERATURE", reading->temperature);
    }
    if (reading->has_adc) {
        cJSON_AddNumberToObject(obj, "ADC", reading->adc_voltage);
    }
    if (reading->has_cjc) {
        cJSON_AddNumberToObject(obj, "CJC", reading->cjc_temp);
    }
}

cJSON* reading_to_json(const ChannelReading *reading) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "ADDRESS", reading->address);
    cJSON_AddNumberToObject(obj, "CHANNEL", reading->channel);
    reading_add_to_json(obj, reading);
    return obj;
}

/* ============================================================================
 * BoardInfo JSON functions
 * ============================================================================ */

void board_info_add_to_json(cJSON *obj, const BoardInfo *info, int channel) {
    /* BoardInfo always has serial if populated */
    if (info->serial[0] != '\0') {
        cJSON_AddStringToObject(obj, "SERIAL", info->serial);
    }
    
    /* Add per-channel calibration data */
    if (channel >= 0 && channel < MCC134_NUM_CHANNELS) {
        const ChannelConfig *ch = &info->channels[channel];
        
        /* Check if calibration data exists */
        int has_cal_date = (ch->cal_date[0] != '\0');
        int has_cal_coeffs = (ch->cal_coeffs.slope != 0.0 || ch->cal_coeffs.offset != 0.0);
        
        if (has_cal_date || has_cal_coeffs) {
            cJSON *cal = cJSON_AddObjectToObject(obj, "CALIBRATION");
            if (has_cal_date) {
                cJSON_AddStringToObject(cal, "DATE", ch->cal_date);
            }
            if (has_cal_coeffs) {
                cJSON_AddNumberToObject(cal, "SLOPE", ch->cal_coeffs.slope);
                cJSON_AddNumberToObject(cal, "OFFSET", ch->cal_coeffs.offset);
            }
        }
    }
    
    /* Add update interval if non-zero */
    if (info->update_interval > 0) {
        cJSON_AddNumberToObject(obj, "UPDATE_INTERVAL", info->update_interval);
    }
}

cJSON* board_info_to_json(const BoardInfo *info, int channel) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "ADDRESS", info->address);
    if (channel >= 0) {
        cJSON_AddNumberToObject(obj, "CHANNEL", channel);
    }
    board_info_add_to_json(obj, info, channel);
    return obj;
}

/* ============================================================================
 * ThermoData JSON functions (legacy compatibility)
 * ============================================================================ */

void thermo_data_add_to_json(cJSON *obj, const ThermoData *data) {
    if (data->has_serial) {
        cJSON_AddStringToObject(obj, "SERIAL", data->serial);
    }
    
    if (data->has_cal_date || data->has_cal_coeffs) {
        cJSON *cal = cJSON_AddObjectToObject(obj, "CALIBRATION");
        if (data->has_cal_date) {
            cJSON_AddStringToObject(cal, "DATE", data->cal_date);
        }
        if (data->has_cal_coeffs) {
            cJSON_AddNumberToObject(cal, "SLOPE", data->cal_coeffs.slope);
            cJSON_AddNumberToObject(cal, "OFFSET", data->cal_coeffs.offset);
        }
    }
    
    if (data->has_interval) {
        cJSON_AddNumberToObject(obj, "UPDATE_INTERVAL", data->update_interval);
    }
    
    if (data->has_temp) {
        cJSON_AddNumberToObject(obj, "TEMPERATURE", data->temperature);
    }
    
    if (data->has_adc) {
        cJSON_AddNumberToObject(obj, "ADC", data->adc_voltage);
    }
    
    if (data->has_cjc) {
        cJSON_AddNumberToObject(obj, "CJC", data->cjc_temp);
    }
}

cJSON* thermo_data_to_json(const ThermoData *data, int include_address) {
    cJSON *obj = cJSON_CreateObject();
    
    if (include_address) {
        cJSON_AddNumberToObject(obj, "ADDRESS", data->address);
        cJSON_AddNumberToObject(obj, "CHANNEL", data->channel);
    }
    
    thermo_data_add_to_json(obj, data);
    return obj;
}

cJSON* thermo_data_to_json_with_key(const ThermoData *data, const char *key) {
    cJSON *obj = cJSON_CreateObject();
    
    if (key && key[0] != '\0') {
        cJSON_AddStringToObject(obj, "KEY", key);
    }
    
    cJSON_AddNumberToObject(obj, "ADDRESS", data->address);
    cJSON_AddNumberToObject(obj, "CHANNEL", data->channel);
    thermo_data_add_to_json(obj, data);
    
    return obj;
}

/* ============================================================================
 * Array/batch JSON functions
 * ============================================================================ */

cJSON* thermo_data_array_to_json(const ThermoData *data_array, int count,
                                  const ThermalSource *sources) {
    if (count == 1) {
        /* Single channel - output flat object */
        const char *key = (sources && sources[0].key[0] != '\0') ? sources[0].key : NULL;
        return thermo_data_to_json_with_key(&data_array[0], key);
    }
    
    /* Multiple channels - output array */
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        const char *key = (sources && sources[i].key[0] != '\0') ? sources[i].key : NULL;
        cJSON *item = thermo_data_to_json_with_key(&data_array[i], key);
        cJSON_AddItemToArray(arr, item);
    }
    return arr;
}

/* ============================================================================
 * Output utilities
 * ============================================================================ */

void json_print(cJSON *json, int formatted) {
    char *str = formatted ? cJSON_Print(json) : cJSON_PrintUnformatted(json);
    printf("%s\n", str);
    fflush(stdout);
    free(str);
}

void json_print_and_free(cJSON *json, int formatted) {
    json_print(json, formatted);
    cJSON_Delete(json);
}
