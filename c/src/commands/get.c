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
#include "signals.h"
#include "board_manager.h"
#include "json_utils.h"

#include "cJSON.h"

/* ============================================================================
 * CollectedData structure for new API
 * ============================================================================ */

typedef struct {
    ChannelReading *readings;
    BoardInfo *board_infos;      /* Array indexed by board address */
    int reading_count;
    int board_count;
} CollectedData;

static void collected_data_free(CollectedData *data) {
    free(data->readings);
    free(data->board_infos);
    data->readings = NULL;
    data->board_infos = NULL;
}

/* ============================================================================
 * NEW API using ChannelReading and BoardInfo
 * ============================================================================ */

/* Collect dynamic readings from a channel (board must be open, TC type set) */
int channel_reading_collect(ChannelReading *reading, uint8_t address, uint8_t channel,
                           int get_temp, int get_adc, int get_cjc) {
    channel_reading_init(reading, address, channel);
    
    if (get_temp) {
        if (thermo_read_temp(address, channel, &reading->temperature) == THERMO_SUCCESS) {
            reading->has_temp = 1;
        }
    }
    
    if (get_adc) {
        if (thermo_read_adc(address, channel, &reading->adc_voltage) == THERMO_SUCCESS) {
            reading->has_adc = 1;
        }
    }
    
    if (get_cjc) {
        if (thermo_read_cjc(address, channel, &reading->cjc_temp) == THERMO_SUCCESS) {
            reading->has_cjc = 1;
        }
    }
    
    return THERMO_SUCCESS;
}

/* Collect board info (board must be open) */
int board_info_collect(BoardInfo *info, uint8_t address, uint8_t channel,
                      int get_serial, int get_cal_date, int get_cal_coeffs, int get_interval) {
    /* Initialize only if address doesn't match (allows accumulating data for multiple channels) */
    if (info->address != address) {
        board_info_init(info, address);
    }
    
    if (get_serial && info->serial[0] == '\0') {
        thermo_get_serial(address, info->serial, sizeof(info->serial));
    }
    
    if (get_interval) {
        thermo_get_update_interval(address, &info->update_interval);
    }
    
    if (channel < MCC134_NUM_CHANNELS) {
        if (get_cal_date) {
            thermo_get_calibration_date(address, info->channels[channel].cal_date,
                                       sizeof(info->channels[channel].cal_date));
        }
        
        if (get_cal_coeffs) {
            thermo_get_calibration_coeffs(address, channel, &info->channels[channel].cal_coeffs);
        }
    }
    
    return THERMO_SUCCESS;
}

/* ============================================================================
 * NEW COLLECTION API using ChannelReading/BoardInfo
 * ============================================================================ */

/* Collect data from multiple channels using new API */
static int collect_channels(ThermalSource *sources, int source_count, CollectedData *out,
                                int get_serial, int get_cal_date, int get_cal_coeffs,
                                int get_temp, int get_adc, int get_cjc, int get_interval,
                                BoardManager *mgr_out) {
    /* Allocate readings (one per source) */
    out->readings = calloc(source_count, sizeof(ChannelReading));
    if (!out->readings) {
        fprintf(stderr, "Error: Failed to allocate memory for readings\n");
        return THERMO_ERROR;
    }
    out->reading_count = source_count;
    
    /* Allocate board infos (max 8 boards) */
    out->board_infos = calloc(8, sizeof(BoardInfo));
    if (!out->board_infos) {
        fprintf(stderr, "Error: Failed to allocate memory for board infos\n");
        free(out->readings);
        return THERMO_ERROR;
    }
    out->board_count = 0;
    
    /* Initialize BoardManager */
    if (board_manager_init(mgr_out, sources, source_count) != THERMO_SUCCESS) {
        collected_data_free(out);
        return THERMO_ERROR;
    }
    board_manager_configure(mgr_out);
    
    DEBUG_PRINT("Beginning data collection for %d sources", source_count);
    
    /* Collect board info once per unique board */
    uint8_t board_collected[8] = {0};
    for (int i = 0; i < source_count; i++) {
        uint8_t addr = sources[i].address;
        if (!board_collected[addr]) {
            board_info_init(&out->board_infos[addr], addr);
            board_collected[addr] = 1;
            out->board_count++;
        }
        
        /* Collect per-channel board info */
        board_info_collect(&out->board_infos[addr], addr, sources[i].channel,
                          get_serial, get_cal_date, get_cal_coeffs, get_interval);
    }
    
    /* Collect dynamic readings */
    for (int i = 0; i < source_count; i++) {
        channel_reading_collect(&out->readings[i], 
                               sources[i].address, sources[i].channel,
                               get_temp, get_adc, get_cjc);
        DEBUG_PRINT("Reading collected for address %d, channel %d", 
                   sources[i].address, sources[i].channel);
    }
    
    return THERMO_SUCCESS;
}

/* Output collected data in JSON format */
static void output_collected_json(const CollectedData *data, const ThermalSource *sources,
                                  int get_serial, int get_cal_date, int get_cal_coeffs, int get_interval) {
    cJSON *root = readings_to_json_array(data->readings, data->board_infos, sources, data->reading_count,
                                        get_serial, get_cal_date, get_cal_coeffs, get_interval);
    json_print_and_free(root, 0);
}

/* Output collected data in table format */
static void output_collected_table(const CollectedData *data, const ThermalSource *sources, int clean_mode,
                                   int get_serial, int get_cal_date, int get_cal_coeffs, int get_interval) {
    if (data->reading_count == 1) {
        /* Single channel output */
        const ChannelReading *reading = &data->readings[0];
        const BoardInfo *info = &data->board_infos[sources[0].address];
        
        printf("(Address: %d, Channel: %d):\n", reading->address, reading->channel);
        
        /* Calculate formatting widths */
        int max_key_len = 0, max_value_width = 0, max_unit_len = 0;
        reading_format_calculate_max_width(data->readings, data->board_infos, sources, 1,
                                          &max_key_len, &max_value_width, &max_unit_len);
        
        /* Use formatting helper */
        reading_format_output(reading, info, &sources[0], 4, max_key_len, max_value_width, max_unit_len,
                             get_serial, get_cal_date, get_cal_coeffs, get_interval);
    } else {
        /* Multiple channels */
        int max_key_len = 0;
        for (int i = 0; i < data->reading_count; i++) {
            if (sources[i].key[0] != '\0') {
                int len = strlen(sources[i].key);
                if (len > max_key_len) max_key_len = len;
            }
        }
        
        /* Calculate formatting widths */
        int max_value_width = 0, max_unit_len = 0, max_data_key_len = 0;
        reading_format_calculate_max_width(data->readings, data->board_infos, sources,
                                          data->reading_count, &max_data_key_len,
                                          &max_value_width, &max_unit_len);

        if (!clean_mode) {
            printf("----------------------------------------\n");
        }
        
        for (int i = 0; i < data->reading_count; i++) {
            const ChannelReading *reading = &data->readings[i];
            const BoardInfo *info = &data->board_infos[sources[i].address];
            
            if (sources[i].key[0] != '\0') {
                printf("%-*s (Address: %d, Channel: %d):\n",
                       max_key_len, sources[i].key, reading->address, reading->channel);
            } else {
                printf("Address: %d, Channel: %d:\n", reading->address, reading->channel);
            }
            
            /* Use formatting helper */
            reading_format_output(reading, info, &sources[i], 4, max_data_key_len, max_value_width, max_unit_len,
                                 get_serial, get_cal_date, get_cal_coeffs, get_interval);
            if (!clean_mode) {
                printf("----------------------------------------\n");
            }
        }
    }
}

/* ============================================================================
 * NEW STREAMING API using ChannelReading/BoardInfo
 * ============================================================================ */

/* Stream data from multiple channels using new API */
static int stream_channels(ThermalSource *sources, int source_count,
                               int get_serial, int get_cal_date, int get_cal_coeffs,
                               int get_temp, int get_adc, int get_cjc, int get_interval,
                               int stream_hz, int json_output, int clean_mode) {
    BoardManager mgr;
    BoardInfo board_infos[8] = {0};
    uint8_t board_collected[8] = {0};
    
    /* Setup timing */
    long sleep_us = 1000000 / stream_hz;
    struct timespec sleep_time = {
        .tv_sec = sleep_us / 1000000,
        .tv_nsec = (sleep_us % 1000000) * 1000
    };
    
    /* Initialize boards */
    if (board_manager_init(&mgr, sources, source_count) != THERMO_SUCCESS) {
        return 1;
    }
    board_manager_configure(&mgr);
    
    /* Collect static board info ONCE */
    if (get_serial || get_cal_date || get_cal_coeffs || get_interval) {
        for (int i = 0; i < source_count; i++) {
            uint8_t addr = sources[i].address;
            if (!board_collected[addr]) {
                board_info_init(&board_infos[addr], addr);
                board_collected[addr] = 1;
            }
            board_info_collect(&board_infos[addr], addr, sources[i].channel,
                              get_serial, get_cal_date, get_cal_coeffs, get_interval);
        }
        
        /* Output static info header */
        if (json_output) {
            /* Create temporary readings array for static output */
            ChannelReading *static_readings = calloc(source_count, sizeof(ChannelReading));
            if (static_readings) {
                for (int i = 0; i < source_count; i++) {
                    channel_reading_init(&static_readings[i], sources[i].address, sources[i].channel);
                }
                cJSON *root = readings_to_json_array(static_readings, board_infos, sources, source_count,
                                                    get_serial, get_cal_date, get_cal_coeffs, get_interval);
                json_print_and_free(root, 0);
                free(static_readings);
            }
        } else {
            /* Calculate formatting widths */
            int max_data_key_len = 0, max_value_width = 0, max_unit_len = 0;
            ChannelReading *temp_readings = calloc(source_count, sizeof(ChannelReading));
            if (temp_readings) {
                for (int i = 0; i < source_count; i++) {
                    channel_reading_init(&temp_readings[i], sources[i].address, sources[i].channel);
                }
                reading_format_calculate_max_width(temp_readings, board_infos, sources, source_count,
                                                  &max_data_key_len, &max_value_width, &max_unit_len);
                free(temp_readings);
            }

            if (!clean_mode) {
                printf("----------------------------------------\n");
            }
            
            /* Output static board info in table format */
            for (int i = 0; i < source_count; i++) {
                const BoardInfo *info = &board_infos[sources[i].address];
                ChannelReading temp_reading;
                channel_reading_init(&temp_reading, sources[i].address, sources[i].channel);
                
                if (source_count > 1) {
                    if (sources[i].key[0] != '\0') {
                        printf("%s (Address: %d, Channel: %d):\n",
                               sources[i].key, sources[i].address, sources[i].channel);
                    } else {
                        printf("Address: %d, Channel: %d:\n",
                               sources[i].address, sources[i].channel);
                    }
                }
                
                /* Use formatting helper for static info only */
                reading_format_output(&temp_reading, info, &sources[i], 4, max_data_key_len, max_value_width, max_unit_len,
                                     get_serial, get_cal_date, get_cal_coeffs, get_interval);
            }
            
            if (clean_mode) {
                printf("\n");
            } else {
                printf("========================================\n");
            }
        }
    }
    
    /* Print streaming info */
    if (!json_output && !clean_mode) {
        if (source_count == 1) {
            printf("Streaming at %d Hz\n", stream_hz);
            printf("----------------------------------------\n");
        } else {
            printf("Streaming %d source%s at %d Hz\n",
                   source_count, source_count == 1 ? "" : "s", stream_hz);
            printf("========================================\n");
        }
    }
    
    signals_install_handlers();
    
    /* Streaming loop - only dynamic readings */
    while (g_running) {
        ChannelReading *readings = calloc(source_count, sizeof(ChannelReading));
        if (!readings) {
            fprintf(stderr, "Error: Failed to allocate memory\n");
            board_manager_close(&mgr);
            return 1;
        }
        
        /* Collect dynamic data only */
        for (int i = 0; i < source_count; i++) {
            channel_reading_collect(&readings[i],
                                   sources[i].address, sources[i].channel,
                                   get_temp, get_adc, get_cjc);
        }
        
        /* Output */
        if (json_output) {
            cJSON *root = readings_to_json_array(readings, NULL, sources, source_count, 0, 0, 0, 0);
            json_print_and_free(root, 0);
        } else {
            if (source_count == 1) {
                /* Calculate formatting widths for single reading */
                int max_key_len = 0, max_value_width = 0, max_unit_len = 0;
                reading_format_calculate_max_width(readings, NULL, sources, 1,
                                                  &max_key_len, &max_value_width, &max_unit_len);
                
                /* Use formatting helper for dynamic data only */
                reading_format_output(&readings[0], NULL, &sources[0], 4, max_key_len, max_value_width, max_unit_len,
                                     0, 0, 0, 0);
                if (!clean_mode) {
                    printf("----------------------------------------\n");
                }
            } else {
                /* Multi-channel streaming output */
                int max_key_len = 0;
                for (int i = 0; i < source_count; i++) {
                    if (sources[i].key[0] != '\0') {
                        int len = strlen(sources[i].key);
                        if (len > max_key_len) max_key_len = len;
                    }
                }
                
                /* Calculate formatting widths */
                int max_data_key_len = 0, max_value_width = 0, max_unit_len = 0;
                reading_format_calculate_max_width(readings, NULL, sources, source_count,
                                                  &max_data_key_len, &max_value_width, &max_unit_len);
                
                for (int i = 0; i < source_count; i++) {
                    const ChannelReading *reading = &readings[i];
                    
                    if (sources[i].key[0] != '\0') {
                        printf("%-*s (Address: %d, Channel: %d):\n",
                               max_key_len, sources[i].key, reading->address, reading->channel);
                    } else {
                        printf("Address: %d, Channel: %d:\n",
                               reading->address, reading->channel);
                    }
                    
                    /* Use formatting helper for dynamic data only */
                    reading_format_output(reading, NULL, &sources[i], 4, max_data_key_len, max_value_width, max_unit_len,
                                         0, 0, 0, 0);
                }
                
                if (clean_mode) {
                    printf("\n");
                } else {
                    printf("----------------------------------------\n");
                }
            }
        }
        
        free(readings);
        nanosleep(&sleep_time, NULL);
    }
    
    board_manager_close(&mgr);
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
        /* Initialize with default calibration and update interval */
        single_source.cal_coeffs.slope = DEFAULT_CALIBRATION_SLOPE;
        single_source.cal_coeffs.offset = DEFAULT_CALIBRATION_OFFSET;
        single_source.update_interval = DEFAULT_UPDATE_INTERVAL;
        
        sources = &single_source;
        source_count = 1;
    }
    
    DEBUG_PRINT("Setup complete.");
    
    /* Execute unified path for both single and multi-channel */
    int result = 0;
    
    if (stream_hz > 0) {
        /* Stream mode - use new API */
        result = stream_channels(sources, source_count,
                                    get_serial, get_cal_date, get_cal_coeffs,
                                    get_temp, get_adc, get_cjc, get_interval,
                                    stream_hz, json_output, clean_mode);
    } else {
        /* Single reading mode - use new API */
        CollectedData data;
        BoardManager mgr;
        
        if (collect_channels(sources, source_count, &data,
                                get_serial, get_cal_date, get_cal_coeffs,
                                get_temp, get_adc, get_cjc, get_interval,
                                &mgr) == THERMO_SUCCESS) {
            DEBUG_PRINT("Data collection complete.");
            if (json_output) {
                output_collected_json(&data, sources, get_serial, get_cal_date, get_cal_coeffs, get_interval);
            } else {
                output_collected_table(&data, sources, clean_mode, get_serial, get_cal_date, get_cal_coeffs, get_interval);
            }
            collected_data_free(&data);
        } else {
            fprintf(stderr, "Error: Failed to collect data\n");
            result = 1;
        }
        
        board_manager_close(&mgr);
    }
    
    if (config_path) {
        config_free(&config);
    }
    
    return result;
}
