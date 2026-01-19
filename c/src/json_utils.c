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

void board_info_add_to_json(cJSON *obj, const BoardInfo *info, int channel,
                           int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval) {
    /* BoardInfo serial if populated and requested */
    if (show_serial && info->serial[0] != '\0') {
        cJSON_AddStringToObject(obj, "SERIAL", info->serial);
    }
    
    /* Add per-channel calibration data if requested */
    if ((show_cal_date || show_cal_coeffs) && channel >= 0 && channel < MCC134_NUM_CHANNELS) {
        const ChannelConfig *ch = &info->channels[channel];
        
        /* Check if calibration data exists */
        int has_cal_date = show_cal_date && (ch->cal_date[0] != '\0');
        int has_cal_coeffs = show_cal_coeffs && (ch->cal_coeffs.slope != 0.0 || ch->cal_coeffs.offset != 0.0);
        
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
    
    /* Add update interval if non-zero and requested */
    if (show_interval && info->update_interval > 0) {
        cJSON_AddNumberToObject(obj, "UPDATE_INTERVAL", info->update_interval);
    }
}

cJSON* board_info_to_json(const BoardInfo *info, int channel,
                         int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval) {
    cJSON *obj = cJSON_CreateObject();
    cJSON_AddNumberToObject(obj, "ADDRESS", info->address);
    if (channel >= 0) {
        cJSON_AddNumberToObject(obj, "CHANNEL", channel);
    }
    board_info_add_to_json(obj, info, channel, show_serial, show_cal_date, show_cal_coeffs, show_interval);
    return obj;
}

/* ============================================================================
 * Combined JSON functions (ChannelReading + BoardInfo)
 * ============================================================================ */

cJSON* reading_with_info_to_json(const ChannelReading *reading,
                                  const BoardInfo *info,
                                  const char *key,
                                  int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval) {
    cJSON *obj = cJSON_CreateObject();
    
    if (key && key[0] != '\0') {
        cJSON_AddStringToObject(obj, "KEY", key);
    }
    
    cJSON_AddNumberToObject(obj, "ADDRESS", reading->address);
    cJSON_AddNumberToObject(obj, "CHANNEL", reading->channel);
    
    /* Add board info fields if requested */
    if (info) {
        board_info_add_to_json(obj, info, reading->channel, show_serial, show_cal_date, show_cal_coeffs, show_interval);
    }
    
    /* Add reading fields */
    reading_add_to_json(obj, reading);
    
    return obj;
}

cJSON* readings_to_json_array(const ChannelReading *readings,
                               const BoardInfo *infos,
                               const ThermalSource *sources,
                               int count,
                               int show_serial, int show_cal_date, int show_cal_coeffs, int show_interval) {
    if (count == 1) {
        /* Single channel - output flat object */
        const char *key = (sources && sources[0].key[0] != '\0') ? sources[0].key : NULL;
        const BoardInfo *info = infos ? &infos[sources[0].address] : NULL;
        return reading_with_info_to_json(&readings[0], info, key, show_serial, show_cal_date, show_cal_coeffs, show_interval);
    }
    
    /* Multiple channels - output array */
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        const char *key = (sources && sources[i].key[0] != '\0') ? sources[i].key : NULL;
        const BoardInfo *info = infos ? &infos[sources[i].address] : NULL;
        cJSON *item = reading_with_info_to_json(&readings[i], info, key, show_serial, show_cal_date, show_cal_coeffs, show_interval);
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
