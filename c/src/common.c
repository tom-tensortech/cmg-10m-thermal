/*
 * Common functions for configuration and shared structures.
 * Supports both YAML (libyaml) and JSON (cJSON) config files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "common.h"
#include "hardware.h"

#include "cJSON.h"

/* ============================================================================
 * ADAPTER FUNCTIONS (for gradual migration)
 * ============================================================================ */

/* Initialize a ChannelReading structure */
void channel_reading_init(ChannelReading *reading, uint8_t address, uint8_t channel) {
    reading->address = address;
    reading->channel = channel;
    reading->temperature = 0.0;
    reading->adc_voltage = 0.0;
    reading->cjc_temp = 0.0;
    reading->has_temp = 0;
    reading->has_adc = 0;
    reading->has_cjc = 0;
}

/* Initialize a BoardInfo structure */
void board_info_init(BoardInfo *info, uint8_t address) {
    info->address = address;
    info->serial[0] = '\0';
    info->update_interval = DEFAULT_UPDATE_INTERVAL;
    for (int i = 0; i < MCC134_NUM_CHANNELS; i++) {
        info->channels[i].cal_date[0] = '\0';
        info->channels[i].cal_coeffs.slope = DEFAULT_CALIBRATION_SLOPE;
        info->channels[i].cal_coeffs.offset = DEFAULT_CALIBRATION_OFFSET;
        info->channels[i].tc_type = TC_TYPE_K;
    }
}

/* Convert ThermoData to ChannelReading (extracts dynamic data only) */
void thermo_data_to_reading(const ThermoData *data, ChannelReading *reading) {
    reading->address = (uint8_t)data->address;
    reading->channel = (uint8_t)data->channel;
    reading->temperature = data->temperature;
    reading->adc_voltage = data->adc_voltage;
    reading->cjc_temp = data->cjc_temp;
    reading->has_temp = data->has_temp ? 1 : 0;
    reading->has_adc = data->has_adc ? 1 : 0;
    reading->has_cjc = data->has_cjc ? 1 : 0;
}

/* Convert ChannelReading back to ThermoData (for functions that still need ThermoData) */
void reading_to_thermo_data(const ChannelReading *reading, ThermoData *data) {
    data->address = reading->address;
    data->channel = reading->channel;
    data->temperature = reading->temperature;
    data->adc_voltage = reading->adc_voltage;
    data->cjc_temp = reading->cjc_temp;
    data->has_temp = reading->has_temp;
    data->has_adc = reading->has_adc;
    data->has_cjc = reading->has_cjc;
    /* Static fields are not set - caller should handle separately */
    data->has_serial = 0;
    data->has_cal_date = 0;
    data->has_cal_coeffs = 0;
    data->has_interval = 0;
}

/* Extract BoardInfo from ThermoData (for a single channel) */
void thermo_data_to_board_info(const ThermoData *data, BoardInfo *info, int channel) {
    info->address = (uint8_t)data->address;
    if (data->has_serial) {
        strncpy(info->serial, data->serial, sizeof(info->serial) - 1);
        info->serial[sizeof(info->serial) - 1] = '\0';
    }
    if (data->has_interval) {
        info->update_interval = data->update_interval;
    }
    if (channel >= 0 && channel < MCC134_NUM_CHANNELS) {
        if (data->has_cal_date) {
            strncpy(info->channels[channel].cal_date, data->cal_date, 
                    sizeof(info->channels[channel].cal_date) - 1);
            info->channels[channel].cal_date[sizeof(info->channels[channel].cal_date) - 1] = '\0';
        }
        if (data->has_cal_coeffs) {
            info->channels[channel].cal_coeffs = data->cal_coeffs;
        }
    }
}

/* ============================================================================
 * CONFIGURATION FUNCTIONS
 * ============================================================================ */

/* Load JSON config file */
static int load_json_config(const char *path, Config *config) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open config file: %s\n", path);
        return THERMO_NOT_FOUND;
    }

    /* Read entire file */
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *content = (char*)malloc(fsize + 1);
    size_t bytes_read = fread(content, 1, fsize, fp);
    fclose(fp);
    if (bytes_read != (size_t)fsize) {
        fprintf(stderr, "Error: Could not read entire config file\n");
        free(content);
        return THERMO_IO_ERROR;
    }
    content[fsize] = '\0';

    /* Parse JSON */
    cJSON *root = cJSON_Parse(content);
    free(content);

    if (!root) {
        fprintf(stderr, "Error: Invalid JSON in config file\n");
        return THERMO_ERROR;
    }

    cJSON *sources_array = cJSON_GetObjectItem(root, "sources");
    if (!sources_array || !cJSON_IsArray(sources_array)) {
        fprintf(stderr, "Error: Config must contain 'sources' array\n");
        cJSON_Delete(root);
        return THERMO_ERROR;
    }

    int num_sources = cJSON_GetArraySize(sources_array);
    config->sources = (ThermalSource*)calloc(num_sources, sizeof(ThermalSource));
    config->source_count = 0;

    for (int i = 0; i < num_sources; i++) {
        cJSON *src = cJSON_GetArrayItem(sources_array, i);
        if (!cJSON_IsObject(src)) continue;

        cJSON *key_item = cJSON_GetObjectItem(src, "key");
        cJSON *addr_item = cJSON_GetObjectItem(src, "address");
        cJSON *chan_item = cJSON_GetObjectItem(src, "channel");
        cJSON *tc_type_item = cJSON_GetObjectItem(src, "tc_type");
        cJSON *cal_slope_item = cJSON_GetObjectItem(src, "cal_slope");
        cJSON *cal_offset_item = cJSON_GetObjectItem(src, "cal_offset");
        cJSON *update_interval_item = cJSON_GetObjectItem(src, "update_interval");

        if (!addr_item || !chan_item) {
            fprintf(stderr, "Warning: Source %d missing required fields (address/channel), skipping\n", i);
            continue;
        }

        ThermalSource *ts = &config->sources[config->source_count];
        
        if (key_item && cJSON_IsString(key_item)) {
            strncpy(ts->key, key_item->valuestring, sizeof(ts->key) - 1);
        } else {
            snprintf(ts->key, sizeof(ts->key), "TEMP_%d_%d", 
                     addr_item->valueint, chan_item->valueint);
        }

        ts->address = (uint8_t)addr_item->valueint;
        ts->channel = (uint8_t)chan_item->valueint;
        
        /* Default to type K thermocouple if not specified */
        if (tc_type_item && cJSON_IsString(tc_type_item)) {
            strncpy(ts->tc_type, tc_type_item->valuestring, sizeof(ts->tc_type) - 1);
        } else {
            strncpy(ts->tc_type, "K", sizeof(ts->tc_type) - 1);
        }
        
        /* Parse calibration coefficients with defaults */
        if (cal_slope_item && cJSON_IsNumber(cal_slope_item)) {
            ts->cal_coeffs.slope = cal_slope_item->valuedouble;
        } else {
            ts->cal_coeffs.slope = DEFAULT_CALIBRATION_SLOPE;
        }
        
        if (cal_offset_item && cJSON_IsNumber(cal_offset_item)) {
            ts->cal_coeffs.offset = cal_offset_item->valuedouble;
        } else {
            ts->cal_coeffs.offset = DEFAULT_CALIBRATION_OFFSET;
        }
        
        /* Parse update interval with default */
        if (update_interval_item && cJSON_IsNumber(update_interval_item)) {
            ts->update_interval = update_interval_item->valueint;
        } else {
            ts->update_interval = DEFAULT_UPDATE_INTERVAL;
        }

        config->source_count++;
    }

    cJSON_Delete(root);
    return THERMO_SUCCESS;
}

/* Load YAML config file */
static int load_yaml_config(const char *path, Config *config) {
    FILE *fp = fopen(path, "r");
    if (!fp) {
        fprintf(stderr, "Error: Could not open config file: %s\n", path);
        return THERMO_NOT_FOUND;
    }

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        fclose(fp);
        fprintf(stderr, "Error: Failed to initialize YAML parser\n");
        return THERMO_ERROR;
    }

    yaml_parser_set_input_file(&parser, fp);

    /* Allocate initial space for sources */
    int max_sources = 10;
    config->sources = (ThermalSource*)calloc(max_sources, sizeof(ThermalSource));
    config->source_count = 0;

    int in_sources = 0;
    int in_source_item = 0;
    int in_mapping = 0;
    ThermalSource current_source = {0};
    char current_key[64] = {0};
    int expecting_value = 0;
    
    /* Initialize defaults for current source */
    current_source.cal_coeffs.slope = DEFAULT_CALIBRATION_SLOPE;
    current_source.cal_coeffs.offset = DEFAULT_CALIBRATION_OFFSET;
    current_source.update_interval = DEFAULT_UPDATE_INTERVAL;

    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            fprintf(stderr, "Error: YAML parse error\n");
            yaml_parser_delete(&parser);
            fclose(fp);
            return THERMO_ERROR;
        }

        switch (event.type) {
            case YAML_SCALAR_EVENT:
                if (!in_sources && strcmp((char*)event.data.scalar.value, "sources") == 0) {
                    in_sources = 1;
                } else if (in_source_item && !expecting_value) {
                    /* This is a key within the source mapping */
                    strncpy(current_key, (char*)event.data.scalar.value, sizeof(current_key) - 1);
                    expecting_value = 1;
                } else if (in_source_item && expecting_value) {
                    /* This is a value for the current key */
                    if (strcmp(current_key, "key") == 0) {
                        strncpy(current_source.key, (char*)event.data.scalar.value, sizeof(current_source.key) - 1);
                    } else if (strcmp(current_key, "address") == 0) {
                        current_source.address = (uint8_t)atoi((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "channel") == 0) {
                        current_source.channel = (uint8_t)atoi((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "tc_type") == 0) {
                        strncpy(current_source.tc_type, (char*)event.data.scalar.value, sizeof(current_source.tc_type) - 1);
                    } else if (strcmp(current_key, "cal_slope") == 0) {
                        current_source.cal_coeffs.slope = atof((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "cal_offset") == 0) {
                        current_source.cal_coeffs.offset = atof((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "update_interval") == 0) {
                        current_source.update_interval = atoi((char*)event.data.scalar.value);
                    }
                    current_key[0] = '\0';
                    expecting_value = 0;
                }
                break;

            case YAML_SEQUENCE_START_EVENT:
                if (in_sources) {
                    /* Start of the sources array */
                }
                break;

            case YAML_MAPPING_START_EVENT:
                if (in_sources) {
                    in_source_item = 1;
                    memset(&current_source, 0, sizeof(current_source));
                    /* Initialize defaults for new source */
                    current_source.cal_coeffs.slope = DEFAULT_CALIBRATION_SLOPE;
                    current_source.cal_coeffs.offset = DEFAULT_CALIBRATION_OFFSET;
                    current_source.update_interval = DEFAULT_UPDATE_INTERVAL;
                    expecting_value = 0;
                }
                break;

            case YAML_MAPPING_END_EVENT:
                if (in_source_item) {
                    /* Add completed source */
                    if (config->source_count >= max_sources) {
                        max_sources *= 2;
                        config->sources = (ThermalSource*)realloc(config->sources, max_sources * sizeof(ThermalSource));
                    }
                    
                    /* Generate default key if not provided */
                    if (current_source.key[0] == '\0') {
                        snprintf(current_source.key, sizeof(current_source.key), 
                                "TEMP_%d_%d", current_source.address, current_source.channel);
                    }
                    
                    /* Default to type K thermocouple if not specified */
                    if (current_source.tc_type[0] == '\0') {
                        strncpy(current_source.tc_type, "K", sizeof(current_source.tc_type) - 1);
                    }
                    
                    config->sources[config->source_count++] = current_source;
                    in_source_item = 0;
                }
                break;

            case YAML_STREAM_END_EVENT:
                done = 1;
                break;

            default:
                break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fp);

    return THERMO_SUCCESS;
}

/* Load configuration file (auto-detect format) */
int config_load(const char *path, Config *config) {
    if (path == NULL || config == NULL) {
        return THERMO_INVALID_PARAM;
    }

    /* Detect file format by extension */
    const char *ext = strrchr(path, '.');
    if (ext && (strcmp(ext, ".json") == 0)) {
        return load_json_config(path, config);
    } else {
        /* Default to YAML */
        return load_yaml_config(path, config);
    }
}

/* Free config structure */
void config_free(Config *config) {
    if (config && config->sources) {
        free(config->sources);
        config->sources = NULL;
        config->source_count = 0;
    }
}

/* Create example configuration file */
int config_create_example(const char *output_path) {
    if (output_path == NULL) {
        return THERMO_INVALID_PARAM;
    }

    /* Detect format by extension */
    const char *ext = strrchr(output_path, '.');
    int is_json = (ext && strcmp(ext, ".json") == 0);

    FILE *fp = fopen(output_path, "w");
    if (!fp) {
        fprintf(stderr, "Error: Could not create config file: %s\n", output_path);
        return THERMO_IO_ERROR;
    }

    if (is_json) {
        /* Write JSON example */
        fprintf(fp, "{\n");
        fprintf(fp, "  \"sources\": [\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"key\": \"BATTERY_TEMP\",\n");
        fprintf(fp, "      \"address\": 0,\n");
        fprintf(fp, "      \"channel\": 0,\n");
        fprintf(fp, "      \"tc_type\": \"K\",\n");
        fprintf(fp, "      \"cal_slope\": 1.0,\n");
        fprintf(fp, "      \"cal_offset\": 0.0,\n");
        fprintf(fp, "      \"update_interval\": 1\n");
        fprintf(fp, "    },\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"key\": \"MOTOR_TEMP\",\n");
        fprintf(fp, "      \"address\": 0,\n");
        fprintf(fp, "      \"channel\": 1,\n");
        fprintf(fp, "      \"tc_type\": \"K\",\n");
        fprintf(fp, "      \"cal_slope\": 1.0,\n");
        fprintf(fp, "      \"cal_offset\": 0.0,\n");
        fprintf(fp, "      \"update_interval\": 1\n");
        fprintf(fp, "    },\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"key\": \"AMBIENT_TEMP\",\n");
        fprintf(fp, "      \"address\": 0,\n");
        fprintf(fp, "      \"channel\": 2,\n");
        fprintf(fp, "      \"tc_type\": \"K\",\n");
        fprintf(fp, "      \"cal_slope\": 1.0,\n");
        fprintf(fp, "      \"cal_offset\": 0.0,\n");
        fprintf(fp, "      \"update_interval\": 1\n");
        fprintf(fp, "    }\n");
        fprintf(fp, "  ]\n");
        fprintf(fp, "}\n");
    } else {
        /* Write YAML example */
        fprintf(fp, "sources:\n");
        fprintf(fp, "- key: BATTERY_TEMP\n");
        fprintf(fp, "  address: 0\n");
        fprintf(fp, "  channel: 0\n");
        fprintf(fp, "  tc_type: K\n");
        fprintf(fp, "  cal_slope: 1.0\n");
        fprintf(fp, "  cal_offset: 0.0\n");
        fprintf(fp, "  update_interval: 1\n");
        fprintf(fp, "- key: MOTOR_TEMP\n");
        fprintf(fp, "  address: 0\n");
        fprintf(fp, "  channel: 1\n");
        fprintf(fp, "  tc_type: K\n");
        fprintf(fp, "  cal_slope: 1.0\n");
        fprintf(fp, "  cal_offset: 0.0\n");
        fprintf(fp, "  update_interval: 1\n");
        fprintf(fp, "- key: AMBIENT_TEMP\n");
        fprintf(fp, "  address: 0\n");
        fprintf(fp, "  channel: 2\n");
        fprintf(fp, "  tc_type: K\n");
        fprintf(fp, "  cal_slope: 1.0\n");
        fprintf(fp, "  cal_offset: 0.0\n");
        fprintf(fp, "  update_interval: 1\n");
    }

    fclose(fp);
    return THERMO_SUCCESS;
}
