/*
 * Get command implementation.
 * Reads data from MCC 134 channels.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>
#include <daqhats/daqhats.h>

#include "commands/get.h"
#include "common.h"
#include "utils.h"

#include "cJSON.h"

/* Initialize ThermoData structure */
void thermo_data_init(ThermoData *data, int address, int channel) {
    memset(data, 0, sizeof(ThermoData));
    data->address = address;
    data->channel = channel;
}

/* Collect data from the board based on requested flags */
int thermo_data_collect(ThermoData *data, int get_serial, int get_cal_date, 
                        int get_cal_coeffs, int get_temp, int get_adc, 
                        int get_cjc, int get_interval, const char *tc_type) {
    int address = data->address;
    int channel = data->channel;
    
    /* Get serial number */
    if (get_serial) {
        if (thermo_get_serial(address, data->serial, sizeof(data->serial)) == THERMO_SUCCESS) {
            data->has_serial = 1;
        }
    }
    
    /* Get calibration date */
    if (get_cal_date) {
        if (thermo_get_calibration_date(address, data->cal_date, sizeof(data->cal_date)) == THERMO_SUCCESS) {
            data->has_cal_date = 1;
        }
    }
    
    /* Get calibration coefficients */
    if (get_cal_coeffs) {
        if (thermo_get_calibration_coeffs(address, channel, &data->cal_coeffs) == THERMO_SUCCESS) {
            data->has_cal_coeffs = 1;
        }
    }
    
    /* Get update interval */
    if (get_interval) {
        if (thermo_get_update_interval(address, &data->update_interval) == THERMO_SUCCESS) {
            data->has_interval = 1;
        }
    }
    
    /* Set TC type if we're reading temp or ADC */
    if (get_temp || get_adc) {
        if (thermo_set_tc_type(address, channel, tc_type) != THERMO_SUCCESS) {
            return THERMO_ERROR;
        }
        /* Wait for readings to stabilize after setting TC type */
        thermo_wait_for_readings();
    }
    
    /* Get temperature */
    if (get_temp) {
        if (thermo_read_temp(address, channel, &data->temperature) == THERMO_SUCCESS) {
            data->has_temp = 1;
        }
    }
    
    /* Get ADC voltage */
    if (get_adc) {
        if (thermo_read_adc(address, channel, &data->adc_voltage) == THERMO_SUCCESS) {
            data->has_adc = 1;
        }
    }
    
    /* Get CJC temperature */
    if (get_cjc) {
        if (thermo_read_cjc(address, channel, &data->cjc_temp) == THERMO_SUCCESS) {
            data->has_cjc = 1;
        }
    }
    
    return THERMO_SUCCESS;
}

/* Output data in JSON format */
void thermo_data_output_json(const ThermoData *data, int include_address_channel) {
    cJSON *root = cJSON_CreateObject();
    
    if (include_address_channel) {
        cJSON_AddNumberToObject(root, "ADDRESS", data->address);
        cJSON_AddNumberToObject(root, "CHANNEL", data->channel);
    }
    
    if (data->has_serial) {
        cJSON_AddStringToObject(root, "SERIAL", data->serial);
    }
    
    if (data->has_cal_date || data->has_cal_coeffs) {
        cJSON *cal = cJSON_AddObjectToObject(root, "CALIBRATION");
        if (data->has_cal_date) {
            cJSON_AddStringToObject(cal, "DATE", data->cal_date);
        }
        if (data->has_cal_coeffs) {
            cJSON_AddNumberToObject(cal, "SLOPE", data->cal_coeffs.slope);
            cJSON_AddNumberToObject(cal, "OFFSET", data->cal_coeffs.offset);
        }
    }
    
    if (data->has_interval) {
        cJSON_AddNumberToObject(root, "UPDATE_INTERVAL", data->update_interval);
    }
    
    if (data->has_temp) {
        cJSON_AddNumberToObject(root, "TEMPERATURE", data->temperature);
    }
    
    if (data->has_adc) {
        cJSON_AddNumberToObject(root, "ADC", data->adc_voltage);
    }
    
    if (data->has_cjc) {
        cJSON_AddNumberToObject(root, "CJC", data->cjc_temp);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);
    fflush(stdout);
    free(json_str);
    cJSON_Delete(root);
}

/* Output data in human-readable format */
void thermo_data_output_table(const ThermoData *data, int show_header, int clean_mode) {
    if (show_header) {
        printf("Address: %d, Channel: %d\n", data->address, data->channel);
        if (!clean_mode) {
            printf("----------------------------------------\n");
        }
    }
    
    /* Calculate value width for alignment */
    int max_key_len, max_value_len, max_unit_len;
    data_format_calculate_max_width(data, 1, &max_key_len, &max_value_len, &max_unit_len);
    
    /* Output all data fields */
    data_format_output(data, 0, max_key_len, max_value_len, max_unit_len);
}

/* Split ThermoData into static and dynamic parts */
void thermo_data_split(const ThermoData *data, ThermoData *static_data, ThermoData *dynamic_data) {
    /* Static data */
    thermo_data_init(static_data, data->address, data->channel);
    static_data->has_serial = data->has_serial;
    if (data->has_serial) {
        strncpy(static_data->serial, data->serial, sizeof(static_data->serial));
    }
    static_data->has_cal_date = data->has_cal_date;
    if (data->has_cal_date) {
        strncpy(static_data->cal_date, data->cal_date, sizeof(static_data->cal_date));
    }
    static_data->has_cal_coeffs = data->has_cal_coeffs;
    if (data->has_cal_coeffs) {
        static_data->cal_coeffs = data->cal_coeffs;
    }
    static_data->has_interval = data->has_interval;
    if (data->has_interval) {
        static_data->update_interval = data->update_interval;
    }
    
    /* Dynamic data */
    thermo_data_init(dynamic_data, data->address, data->channel);
    dynamic_data->has_temp = data->has_temp;
    if (data->has_temp) {
        dynamic_data->temperature = data->temperature;
    }
    dynamic_data->has_adc = data->has_adc;
    if (data->has_adc) {
        dynamic_data->adc_voltage = data->adc_voltage;
    }
    dynamic_data->has_cjc = data->has_cjc;
    if (data->has_cjc) {
        dynamic_data->cjc_temp = data->cjc_temp;
    }
}

/* Collect data from multiple channels */
static int collect_channels(ThermalSource *sources, int source_count, ThermoData **data_out,
                           int get_serial, int get_cal_date, int get_cal_coeffs,
                           int get_temp, int get_adc, int get_cjc, int get_interval) {
    /* Track which boards are already open */
    uint8_t opened[8] = {0};
    
    /* Allocate array for results */
    ThermoData *data_array = (ThermoData*)calloc(source_count, sizeof(ThermoData));
    if (!data_array) {
        fprintf(stderr, "Error: Failed to allocate memory for data array\n");
        return THERMO_ERROR;
    }
    
    /* Open all unique boards */
    for (int i = 0; i < source_count; i++) {
        uint8_t addr = sources[i].address;
        if (!opened[addr]) {
            if (thermo_open(addr) != THERMO_SUCCESS) {
                fprintf(stderr, "Error: Failed to open board at address %d\n", addr);
                /* Close previously opened boards */
                for (int j = 0; j < 8; j++) {
                    if (opened[j]) thermo_close(j);
                }
                free(data_array);
                return THERMO_ERROR;
            }
            opened[addr] = 1;
        }
    }
    
    /* Collect data from each source */
    for (int i = 0; i < source_count; i++) {
        thermo_data_init(&data_array[i], sources[i].address, sources[i].channel);
        
        if (thermo_data_collect(&data_array[i], get_serial, get_cal_date, get_cal_coeffs,
                               get_temp, get_adc, get_cjc, get_interval, 
                               sources[i].tc_type) != THERMO_SUCCESS) {
            fprintf(stderr, "Warning: Failed to collect data from address %d, channel %d\n",
                    sources[i].address, sources[i].channel);
        }
    }
    
    *data_out = data_array;
    return THERMO_SUCCESS;
}

/* Close all boards used by sources */
static void close_boards(ThermalSource *sources, int source_count) {
    uint8_t closed[8] = {0};
    for (int i = 0; i < source_count; i++) {
        uint8_t addr = sources[i].address;
        if (!closed[addr]) {
            thermo_close(addr);
            closed[addr] = 1;
        }
    }
}

/* Output single channel data in JSON format with key */
static void output_single_json_with_key(const ThermoData *data, const char *key) {
    cJSON *root = cJSON_CreateObject();
    
    if (key && key[0] != '\0') {
        cJSON_AddStringToObject(root, "KEY", key);
    }
    cJSON_AddNumberToObject(root, "ADDRESS", data->address);
    cJSON_AddNumberToObject(root, "CHANNEL", data->channel);
    
    if (data->has_serial) {
        cJSON_AddStringToObject(root, "SERIAL", data->serial);
    }
    
    if (data->has_cal_date || data->has_cal_coeffs) {
        cJSON *cal = cJSON_AddObjectToObject(root, "CALIBRATION");
        if (data->has_cal_date) {
            cJSON_AddStringToObject(cal, "DATE", data->cal_date);
        }
        if (data->has_cal_coeffs) {
            cJSON_AddNumberToObject(cal, "SLOPE", data->cal_coeffs.slope);
            cJSON_AddNumberToObject(cal, "OFFSET", data->cal_coeffs.offset);
        }
    }
    
    if (data->has_interval) {
        cJSON_AddNumberToObject(root, "UPDATE_INTERVAL", data->update_interval);
    }
    
    if (data->has_temp) {
        cJSON_AddNumberToObject(root, "TEMPERATURE", data->temperature);
    }
    
    if (data->has_adc) {
        cJSON_AddNumberToObject(root, "ADC", data->adc_voltage);
    }
    
    if (data->has_cjc) {
        cJSON_AddNumberToObject(root, "CJC", data->cjc_temp);
    }
    
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);
    fflush(stdout);
    free(json_str);
    cJSON_Delete(root);
}

/* Output multiple channels in JSON format */
static void output_channels_json(ThermoData *data_array, int count, ThermalSource *sources) {
    if (count == 1) {
        /* Single channel - use existing format */
        thermo_data_output_json(&data_array[0], 1);
    } else {
        /* Multiple channels - output as array */
        cJSON *root = cJSON_CreateArray();
        
        for (int i = 0; i < count; i++) {
            cJSON *item = cJSON_CreateObject();
            
            if (sources[i].key[0] != '\0') {
                cJSON_AddStringToObject(item, "KEY", sources[i].key);
            }
            cJSON_AddNumberToObject(item, "ADDRESS", data_array[i].address);
            cJSON_AddNumberToObject(item, "CHANNEL", data_array[i].channel);
            
            if (data_array[i].has_serial) {
                cJSON_AddStringToObject(item, "SERIAL", data_array[i].serial);
            }
            
            if (data_array[i].has_cal_date || data_array[i].has_cal_coeffs) {
                cJSON *cal = cJSON_AddObjectToObject(item, "CALIBRATION");
                if (data_array[i].has_cal_date) {
                    cJSON_AddStringToObject(cal, "DATE", data_array[i].cal_date);
                }
                if (data_array[i].has_cal_coeffs) {
                    cJSON_AddNumberToObject(cal, "SLOPE", data_array[i].cal_coeffs.slope);
                    cJSON_AddNumberToObject(cal, "OFFSET", data_array[i].cal_coeffs.offset);
                }
            }
            
            if (data_array[i].has_interval) {
                cJSON_AddNumberToObject(item, "UPDATE_INTERVAL", data_array[i].update_interval);
            }
            
            if (data_array[i].has_temp) {
                cJSON_AddNumberToObject(item, "TEMPERATURE", data_array[i].temperature);
            }
            
            if (data_array[i].has_adc) {
                cJSON_AddNumberToObject(item, "ADC", data_array[i].adc_voltage);
            }
            
            if (data_array[i].has_cjc) {
                cJSON_AddNumberToObject(item, "CJC", data_array[i].cjc_temp);
            }
            
            cJSON_AddItemToArray(root, item);
        }
        
        char *json_str = cJSON_PrintUnformatted(root);
        printf("%s\n", json_str);
        fflush(stdout);
        free(json_str);
        cJSON_Delete(root);
    }
}

/* Output multiple channels in table format */
static void output_channels_table(ThermoData *data_array, int count, ThermalSource *sources, int clean_mode) {
    if (count == 1) {
        /* Single channel - use existing format */
        thermo_data_output_table(&data_array[0], 1, clean_mode);
    } else {
        /* Multiple channels */
        if (!clean_mode) {
            printf("Reading from %d source%s...\n", count, count == 1 ? "" : "s");
            printf("========================================\n");
        }
        
        /* Calculate max key length for alignment */
        int max_key_len = 0;
        for (int i = 0; i < count; i++) {
            if (sources[i].key[0] != '\0') {
                int len = strlen(sources[i].key);
                if (len > max_key_len) max_key_len = len;
            }
        }
        
        ThermoData *static_data_array = malloc(sizeof(ThermoData) * count);
        ThermoData *dynamic_data_array = malloc(sizeof(ThermoData) * count);
        for (int i = 0; i < count; i++) {
            thermo_data_split(&data_array[i], &static_data_array[i], &dynamic_data_array[i]);
        }
        
        /* Calculate value width for alignment across all channels */
        // int max_data_key_len, max_value_len, max_unit_len;
        // data_format_calculate_max_width(data_array, count, &max_data_key_len, &max_value_len, &max_unit_len);
        int static_data_key_len, static_value_len, static_unit_len;
        data_format_calculate_max_width(static_data_array, count, &static_data_key_len, &static_value_len, &static_unit_len);
        int dynamic_data_key_len, dynamic_value_len, dynamic_unit_len;
        data_format_calculate_max_width(dynamic_data_array, count, &dynamic_data_key_len, &dynamic_value_len, &dynamic_unit_len);
        
        for (int i = 0; i < count; i++) {
            if (sources[i].key[0] != '\0') {
                printf("%-*s (Address: %d, Channel: %d):\n", 
                       max_key_len, sources[i].key, data_array[i].address, data_array[i].channel);
            } else {
                printf("Address: %d, Channel: %d:\n", 
                       data_array[i].address, data_array[i].channel);
            }
            
            /* Output data with 4-space indentation */
            data_format_output(&static_data_array[i], 4, static_data_key_len, static_value_len, static_unit_len);
            data_format_output(&dynamic_data_array[i], 4, dynamic_data_key_len, dynamic_value_len, dynamic_unit_len);
        }
    }
}

/* Stream data from multiple channels */
static int stream_channels(ThermalSource *sources, int source_count,
                          int get_serial, int get_cal_date, int get_cal_coeffs,
                          int get_temp, int get_adc, int get_cjc, int get_interval,
                          int stream_hz, int json_output, int clean_mode) {
    long sleep_us = 1000000 / stream_hz;
    struct timespec sleep_time;
    sleep_time.tv_sec = sleep_us / 1000000;
    sleep_time.tv_nsec = (sleep_us % 1000000) * 1000;
    
    /* Track which boards are already open */
    uint8_t opened[8] = {0};
    
    /* Open all unique boards */
    for (int i = 0; i < source_count; i++) {
        uint8_t addr = sources[i].address;
        if (!opened[addr]) {
            if (thermo_open(addr) != THERMO_SUCCESS) {
                fprintf(stderr, "Error: Failed to open board at address %d\n", addr);
                /* Close previously opened boards */
                for (int j = 0; j < 8; j++) {
                    if (opened[j]) thermo_close(j);
                }
                return 1;
            }
            opened[addr] = 1;
        }
    }
    
    /* Print header and static data once */
    if (get_serial || get_cal_date || get_cal_coeffs || get_interval) {
        ThermoData *static_data;
        if (collect_channels(sources, source_count, &static_data,
                           get_serial, get_cal_date, get_cal_coeffs,
                           0, 0, 0, get_interval) == THERMO_SUCCESS) {
            if (json_output) {
                output_channels_json(static_data, source_count, sources);
            } else {
                output_channels_table(static_data, source_count, sources, clean_mode);
                if (clean_mode) {
                    printf("\n");  /* Blank line separator in clean mode */
                } else {
                    if (source_count == 1) {
                        printf("----------------------------------------\n");
                    } else {
                        printf("========================================\n");
                    }
                }
            }
            free(static_data);
        }
    }
    
    /* Print streaming info in non-JSON mode */
    if (!json_output && !clean_mode) {
        if (source_count == 1) {
            printf("Streaming at %d Hz (Ctrl+C to stop)\n", stream_hz);
            printf("----------------------------------------\n");
        } else {
            printf("Streaming %d source%s at %d Hz (Ctrl+C to stop)\n",
                   source_count, source_count == 1 ? "" : "s", stream_hz);
            printf("========================================\n");
        }
    }
    
    /* Stream only dynamic data */
    while (1) {
        ThermoData *data_array = (ThermoData*)calloc(source_count, sizeof(ThermoData));
        if (!data_array) {
            fprintf(stderr, "Error: Failed to allocate memory\n");
            close_boards(sources, source_count);
            return 1;
        }
        
        /* Collect data from each source */
        for (int i = 0; i < source_count; i++) {
            thermo_data_init(&data_array[i], sources[i].address, sources[i].channel);
            
            if (thermo_data_collect(&data_array[i], 0, 0, 0,
                                   get_temp, get_adc, get_cjc, 0,
                                   sources[i].tc_type) != THERMO_SUCCESS) {
                fprintf(stderr, "Warning: Failed to collect data from address %d, channel %d\n",
                        sources[i].address, sources[i].channel);
            }
        }
        
        if (json_output) {
            output_channels_json(data_array, source_count, sources);
        } else {
            if (source_count == 1) {
                thermo_data_output_table(&data_array[0], 0, clean_mode);
            } else {
                /* Calculate max key length for alignment */
                int max_key_len = 0;
                for (int i = 0; i < source_count; i++) {
                    if (sources[i].key[0] != '\0') {
                        int len = strlen(sources[i].key);
                        if (len > max_key_len) max_key_len = len;
                    }
                }
                
                /* Calculate value width for alignment across all channels */
                int max_data_key_len, max_value_len, max_unit_len;
                data_format_calculate_max_width(data_array, source_count, &max_data_key_len, &max_value_len, &max_unit_len);
                
                /* Multi-channel streaming output */
                for (int i = 0; i < source_count; i++) {
                    if (sources[i].key[0] != '\0') {
                        printf("%-*s (Address: %d, Channel: %d):\n", 
                               max_key_len, sources[i].key, data_array[i].address, data_array[i].channel);
                    } else {
                        printf("Address: %d, Channel: %d:\n", 
                               data_array[i].address, data_array[i].channel);
                    }
                    
                    /* Output data with 4-space indentation */
                    data_format_output(&data_array[i], 4, max_data_key_len, max_value_len, max_unit_len);
                }
                
                if (clean_mode) {
                    printf("\n");  /* Blank line separator in clean mode */
                } else {
                    printf("----------------------------------------\n");
                }
            }
        }
        
        free(data_array);
        nanosleep(&sleep_time, NULL);
    }
    
    close_boards(sources, source_count);
    return 0;
}

/* Command: get - Read data from a specific channel */
int cmd_get(int argc, char **argv) {
    int address = -1;  /* -1 means not specified */
    int channel = -1;
    char tc_type[8] = "K";
    char *config_path = NULL;
    int json_output = 0;
    int stream_hz = 0;
    int clean_mode = 0;
    
    int get_serial = 0;
    int get_cal_date = 0;
    int get_cal_coeffs = 0;
    int get_temp = 0;
    int get_adc = 0;
    int get_cjc = 0;
    int get_interval = 0;
    
    static struct option long_options[] = {
        {"config", required_argument, 0, 'C'},
        {"address", required_argument, 0, 'a'},
        {"channel", required_argument, 0, 'c'},
        {"tc-type", required_argument, 0, 't'},
        {"serial", no_argument, 0, 's'},
        {"cali-date", no_argument, 0, 'D'},
        {"cali-coeffs", no_argument, 0, 'O'},
        {"temp", no_argument, 0, 'T'},
        {"adc", no_argument, 0, 'A'},
        {"cjc", no_argument, 0, 'J'},
        {"update-interval", no_argument, 0, 'i'},
        {"json", no_argument, 0, 'j'},
        {"stream", required_argument, 0, 'S'},
        {"clean", no_argument, 0, 'l'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "C:a:c:t:sDOTAJijS:l", long_options, NULL)) != -1) {
        switch (opt) {
            case 'C': config_path = optarg; break;
            case 'a': address = atoi(optarg); break;
            case 'c': channel = atoi(optarg); break;
            case 't': strncpy(tc_type, optarg, sizeof(tc_type) - 1); break;
            case 's': get_serial = 1; break;
            case 'D': get_cal_date = 1; break;
            case 'O': get_cal_coeffs = 1; break;
            case 'T': get_temp = 1; break;
            case 'A': get_adc = 1; break;
            case 'J': get_cjc = 1; break;
            case 'i': get_interval = 1; break;
            case 'j': json_output = 1; break;
            case 'S': stream_hz = atoi(optarg); break;
            case 'l': clean_mode = 1; break;
            default:
                fprintf(stderr, "Usage: thermo-cli get [OPTIONS]\n");
                return 1;
        }
    }
    
    /* Default to temperature if nothing specified */
    if (!get_serial && !get_cal_date && !get_cal_coeffs && 
        !get_temp && !get_adc && !get_cjc && !get_interval) {
        get_temp = 1;
    }
    
    /* Validate arguments */
    if (config_path && (address >= 0 || channel >= 0)) {
        fprintf(stderr, "Error: Cannot specify both --config and --address/--channel\n");
        return 1;
    }
    
    if (!config_path && address < 0) {
        address = 0;  /* Default address */
    }
    
    if (!config_path && channel < 0) {
        channel = 0;  /* Default channel */
    }
    
    /* Prepare sources array */
    ThermalSource *sources;
    int source_count;
    Config config = {0};
    ThermalSource single_source = {0};
    
    if (config_path) {
        /* Multi-channel from config file */
        if (config_load(config_path, &config) != THERMO_SUCCESS) {
            fprintf(stderr, "Error: Failed to load config file: %s\n", config_path);
            return 1;
        }
        sources = config.sources;
        source_count = config.source_count;
        
        if (source_count == 0) {
            fprintf(stderr, "Error: No sources defined in config file\n");
            config_free(&config);
            return 1;
        }
    } else {
        /* Single-channel from CLI args */
        snprintf(single_source.key, sizeof(single_source.key), "TEMP_%d_%d", address, channel);
        single_source.address = address;
        single_source.channel = channel;
        strncpy(single_source.tc_type, tc_type, sizeof(single_source.tc_type) - 1);
        
        sources = &single_source;
        source_count = 1;
    }
    
    /* Execute unified path for both single and multi-channel */
    int result = 0;
    
    if (stream_hz > 0) {
        /* Stream mode */
        result = stream_channels(sources, source_count,
                               get_serial, get_cal_date, get_cal_coeffs,
                               get_temp, get_adc, get_cjc, get_interval,
                               stream_hz, json_output, clean_mode);
    } else {
        /* Single reading mode */
        ThermoData *data_array;
        if (collect_channels(sources, source_count, &data_array,
                           get_serial, get_cal_date, get_cal_coeffs,
                           get_temp, get_adc, get_cjc, get_interval) == THERMO_SUCCESS) {
            if (json_output) {
                output_channels_json(data_array, source_count, sources);
            } else {
                output_channels_table(data_array, source_count, sources, clean_mode);
            }
            free(data_array);
        } else {
            fprintf(stderr, "Error: Failed to collect data\n");
            result = 1;
        }
        
        close_boards(sources, source_count);
    }
    
    if (config_path) {
        config_free(&config);
    }
    
    return result;
}
