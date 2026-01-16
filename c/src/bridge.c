/*
 * Data fusion bridge for fusing thermal data into cmg-cli output.
 * Spawns cmg-cli as subprocess and injects thermal readings.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <getopt.h>
#include <daqhats/daqhats.h>

#include "bridge.h"
#include "hardware.h"
#include "common.h"

#include "cJSON.h"

struct FuseBridge {
    ThermalSource *sources;
    int source_count;
    char **args;
    int arg_count;
    int boards_initialized;
    char time_format[64];
};

/* Create a new bridge instance */
FuseBridge* bridge_create(ThermalSource *sources, int source_count, char **args, int arg_count, const char *time_format) {
    FuseBridge *bridge = (FuseBridge*)malloc(sizeof(FuseBridge));
    
    bridge->sources = (ThermalSource*)malloc(source_count * sizeof(ThermalSource));
    memcpy(bridge->sources, sources, source_count * sizeof(ThermalSource));
    bridge->source_count = source_count;
    
    bridge->args = (char**)malloc(arg_count * sizeof(char*));
    for (int i = 0; i < arg_count; i++) {
        bridge->args[i] = strdup(args[i]);
    }
    bridge->arg_count = arg_count;
    bridge->boards_initialized = 0;
    strncpy(bridge->time_format, time_format, sizeof(bridge->time_format) - 1);
    bridge->time_format[sizeof(bridge->time_format) - 1] = '\0';
    
    return bridge;
}

/* Free bridge resources */
void bridge_free(FuseBridge *bridge) {
    if (!bridge) return;
    
    /* Close any open boards */
    if (bridge->boards_initialized) {
        for (int i = 0; i < bridge->source_count; i++) {
            mcc134_close(bridge->sources[i].address);
        }
    }
    
    if (bridge->sources) free(bridge->sources);
    
    for (int i = 0; i < bridge->arg_count; i++) {
        free(bridge->args[i]);
    }
    free(bridge->args);
    free(bridge);
}

/* Initialize boards for continuous reading */
static int bridge_init_boards(FuseBridge *bridge) {
    /* Track which addresses we've already opened */
    uint8_t opened[8] = {0};
    
    for (int i = 0; i < bridge->source_count; i++) {
        ThermalSource *src = &bridge->sources[i];
        
        if (!opened[src->address]) {
            /* Open the board */
            int result = mcc134_open(src->address);
            if (result != RESULT_SUCCESS) {
                fprintf(stderr, "Error: Failed to open board at address %d\n", src->address);
                return -1;
            }
            opened[src->address] = 1;
            
            /* Apply update interval if it differs from default */
            if (src->update_interval > 0 && src->update_interval != DAFAULT_UPDATE_INTERVAL) {
                if (thermo_set_update_interval(src->address, (uint8_t)src->update_interval) != THERMO_SUCCESS) {
                    fprintf(stderr, "Warning: Failed to set update interval for address %d\n", src->address);
                }
            }
        }
        
        /* Apply calibration coefficients if they differ from defaults */
        if (src->cal_coeffs.slope != DAFAULT_CALIBRATION_SLOPE || 
            src->cal_coeffs.offset != DAFAULT_CALIBRATION_OFFSET) {
            if (thermo_set_calibration_coeffs(src->address, src->channel, 
                                             src->cal_coeffs.slope, 
                                             src->cal_coeffs.offset) != THERMO_SUCCESS) {
                fprintf(stderr, "Warning: Failed to set calibration coefficients for address %d, channel %d\n",
                        src->address, src->channel);
            }
        }
        
        /* Set thermocouple type for this channel */
        uint8_t tc_type = thermo_tc_type_from_string(src->tc_type);
        int result = mcc134_tc_type_write(src->address, src->channel, tc_type);
        if (result != RESULT_SUCCESS) {
            fprintf(stderr, "Error: Failed to set TC type for addr %d ch %d\n", 
                    src->address, src->channel);
            return -1;
        }
    }
    
    bridge->boards_initialized = 1;
    
    thermo_wait_for_readings();
    
    return 0;
}

/* Get thermal data from all configured sources (boards must be initialized) */
static cJSON* get_thermal_data(FuseBridge *bridge) {
    cJSON *data = cJSON_CreateObject();
    
    for (int i = 0; i < bridge->source_count; i++) {
        ThermalSource *src = &bridge->sources[i];
        double temp, adc, cjc;
        
        /* Create a sub-object for each source */
        cJSON *source_data = cJSON_CreateObject();
        
        /* Read temperature (board is already open with TC type set) */
        int temp_result = mcc134_t_in_read(src->address, src->channel, &temp);
        if (temp_result == RESULT_SUCCESS) {
            cJSON_AddNumberToObject(source_data, "TEMP", temp);
        } else {
            cJSON_AddNumberToObject(source_data, "TEMP", 0.0/0.0);
        }
        
        /* Read ADC voltage */
        int adc_result = mcc134_a_in_read(src->address, src->channel, OPTS_DEFAULT, &adc);
        if (adc_result == RESULT_SUCCESS) {
            cJSON_AddNumberToObject(source_data, "ADC", adc);
        } else {
            cJSON_AddNumberToObject(source_data, "ADC", 0.0/0.0);
        }
        
        /* Read CJC temperature */
        int cjc_result = mcc134_cjc_read(src->address, src->channel, &cjc);
        if (cjc_result == RESULT_SUCCESS) {
            cJSON_AddNumberToObject(source_data, "CJC", cjc);
        } else {
            cJSON_AddNumberToObject(source_data, "CJC", 0.0/0.0);
        }
        
        /* Add the source data object to the main data object */
        cJSON_AddItemToObject(data, src->key, source_data);
    }
    
    return data;
}

/*
 * Format timestamp with microsecond support.
 * Use %f in format string for 6-digit microseconds.
 */
static void format_timestamp(char *buf, size_t buf_size, const struct timeval *tv, const char *format) {
    struct tm *tm_info = localtime(&tv->tv_sec);
    
    /* First pass: replace %f with microseconds placeholder */
    char temp_format[128];
    char *dst = temp_format;
    const char *src = format;
    char usec_str[8];
    snprintf(usec_str, sizeof(usec_str), "%06ld", (long)tv->tv_usec);
    
    while (*src && (dst - temp_format) < (int)sizeof(temp_format) - 7) {
        if (*src == '%' && *(src + 1) == 'f') {
            /* Replace %f with microseconds */
            strcpy(dst, usec_str);
            dst += 6;
            src += 2;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    
    strftime(buf, buf_size, temp_format, tm_info);
}

/* Inject thermal data into JSON object */
static void inject_json(cJSON *json_obj, cJSON *thermal_data, const struct timeval *tv, const char *time_format) {
    /* Add timestamp */
    char timestamp[64];
    format_timestamp(timestamp, sizeof(timestamp), tv, time_format);
    cJSON_AddStringToObject(json_obj, "TIMESTAMP", timestamp);
    
    cJSON *sub_obj = cJSON_CreateObject();
    cJSON_AddItemToObject(json_obj, "THERMOCOUPLE", sub_obj);

    cJSON *item = thermal_data->child;
    while (item) {
        cJSON *copy = cJSON_Duplicate(item, 1);
        cJSON_AddItemToObject(sub_obj, item->string, copy);
        item = item->next;
    }
}

/* Run the bridge - spawn cmg-cli and inject thermal data */
int bridge_run(FuseBridge *bridge) {
    /* Initialize boards first (before forking) */
    if (bridge_init_boards(bridge) != 0) {
        fprintf(stderr, "Error: Failed to initialize thermal boards\n");
        return 1;
    }
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }
    
    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }
    
    if (pid == 0) {
        /* Child process - execute cmg-cli */
        close(pipefd[0]); /* Close read end */
        dup2(pipefd[1], STDOUT_FILENO); /* Redirect stdout to pipe */
        close(pipefd[1]);
        
        /* Build command: stdbuf -oL -eL cmg-cli get <args> */
        char **cmd_args = (char**)malloc((bridge->arg_count + 6) * sizeof(char*));
        cmd_args[0] = "stdbuf";
        cmd_args[1] = "-oL";
        cmd_args[2] = "-eL";
        cmd_args[3] = "cmg-cli";
        cmd_args[4] = "get";
        
        for (int i = 0; i < bridge->arg_count; i++) {
            cmd_args[5 + i] = bridge->args[i];
        }
        cmd_args[5 + bridge->arg_count] = NULL;
        
        execvp("stdbuf", cmd_args);
        perror("execvp");
        exit(1);
    } else {
        /* Parent process - read JSON lines and inject thermal data */
        close(pipefd[1]); /* Close write end */
        
        FILE *fp = fdopen(pipefd[0], "r");
        if (!fp) {
            perror("fdopen");
            return 1;
        }
        
        char line[4096];
        
        while (fgets(line, sizeof(line), fp)) {
            /* Remove trailing newline */
            size_t len = strlen(line);
            if (len > 0 && line[len - 1] == '\n') {
                line[len - 1] = '\0';
            }
            
            /* Skip empty lines */
            if (strlen(line) == 0) {
                printf("\n");
                fflush(stdout);
                continue;
            }
            
            /* Capture timestamp when data arrives */
            struct timeval tv;
            gettimeofday(&tv, NULL);
            
            /* Try to parse as JSON */
            cJSON *json_obj = cJSON_Parse(line);
            if (json_obj) {
                /* Get thermal data and inject */
                cJSON *thermal_data = get_thermal_data(bridge);
                inject_json(json_obj, thermal_data, &tv, bridge->time_format);
                
                char *output = cJSON_PrintUnformatted(json_obj);
                printf("%s\n", output);
                fflush(stdout);
                
                free(output);
                cJSON_Delete(json_obj);
                cJSON_Delete(thermal_data);
            } else {
                /* Not JSON - pass through unchanged */
                printf("%s\n", line);
                fflush(stdout);
            }
        }
        
        fclose(fp);
        
        /* Wait for child process */
        int status;
        waitpid(pid, &status, 0);
        
        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
        } else {
            return 1;
        }
    }
    
    return 0;
}

/* Command: fuse - Fuse thermal data into cmg-cli output */
int cmd_fuse(int argc, char **argv) {
    char *config_path = NULL;
    int address = -1;
    int channel = -1;
    char key[64] = "TEMP_FUSED";
    char tc_type[8] = "K";
    char time_format[64] = "%Y-%m-%dT%H:%M:%S.%f";  /* Default with microseconds */
    
    /* Find '--' separator */
    int separator_idx = -1;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            separator_idx = i;
            break;
        }
    }
    
    if (separator_idx == -1) {
        fprintf(stderr, "Error: No '--' separator found\n");
        fprintf(stderr, "Usage: thermo-cli fuse [OPTIONS] -- [cmg-cli arguments...]\n");
        fprintf(stderr, "\nOptions:\n");
        fprintf(stderr, "  --config, -C FILE       Load thermal sources from config file\n");
        fprintf(stderr, "  --address, -a ADDR      Board address (0-7)\n");
        fprintf(stderr, "  --channel, -c CH        Channel number (0-3)\n");
        fprintf(stderr, "  --key, -k KEY           JSON key name (default: TEMP_FUSED)\n");
        fprintf(stderr, "  --tc-type, -t TYPE      Thermocouple type (default: K)\n");
        fprintf(stderr, "  --time-format, -T FMT   Timestamp format (default: %%Y-%%m-%%dT%%H:%%M:%%S.%%f)\n");
        fprintf(stderr, "                          Use %%f for 6-digit microseconds\n");
        fprintf(stderr, "\nNote: Data fusion only works with JSON output from cmg-cli.\n");
        fprintf(stderr, "      The --json flag will be added automatically if not specified.\n");
        fprintf(stderr, "\nExamples:\n");
        fprintf(stderr, "  thermo-cli fuse --address 0 --channel 1 --key MY_TEMP -- --power\n");
        fprintf(stderr, "  thermo-cli fuse --config config.yaml -- --actuator --stream 5\n");
        fprintf(stderr, "  thermo-cli fuse -a 0 -c 0 -T '%%H:%%M:%%S.%%f' -- --power\n");
        return 1;
    }
    
    /* Parse options before '--' */
    optind = 1; /* Reset getopt */
    static struct option long_options[] = {
        {"config", required_argument, 0, 'C'},
        {"address", required_argument, 0, 'a'},
        {"channel", required_argument, 0, 'c'},
        {"key", required_argument, 0, 'k'},
        {"tc-type", required_argument, 0, 't'},
        {"time-format", required_argument, 0, 'T'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while (optind < separator_idx && 
           (opt = getopt_long(separator_idx, argv, "C:a:c:k:t:T:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'C': config_path = optarg; break;
            case 'a': address = atoi(optarg); break;
            case 'c': channel = atoi(optarg); break;
            case 'k': strncpy(key, optarg, sizeof(key) - 1); break;
            case 't': strncpy(tc_type, optarg, sizeof(tc_type) - 1); break;
            case 'T': strncpy(time_format, optarg, sizeof(time_format) - 1); break;
            default:
                fprintf(stderr, "Usage: thermo-cli fuse [OPTIONS] -- [cmg-cli arguments...]\n");
                return 1;
        }
    }
    
    /* Prepare sources */
    Config config = {0};
    ThermalSource single_source = {0};
    ThermalSource *sources = NULL;
    int source_count = 0;
    
    if (config_path) {
        /* Load from config file */
        if (config_load(config_path, &config) != THERMO_SUCCESS) {
            fprintf(stderr, "Error loading config file\n");
            return 1;
        }
        sources = config.sources;
        source_count = config.source_count;
    } else if (address >= 0 && channel >= 0) {
        /* Single source from CLI args */
        strncpy(single_source.key, key, sizeof(single_source.key) - 1);
        single_source.address = address;
        single_source.channel = channel;
        strncpy(single_source.tc_type, tc_type, sizeof(single_source.tc_type) - 1);
        /* Initialize with default calibration and update interval */
        single_source.cal_coeffs.slope = DAFAULT_CALIBRATION_SLOPE;
        single_source.cal_coeffs.offset = DAFAULT_CALIBRATION_OFFSET;
        single_source.update_interval = DAFAULT_UPDATE_INTERVAL;
        sources = &single_source;
        source_count = 1;
    } else {
        fprintf(stderr, "Error: Must specify --config or (--address and --channel)\n");
        return 1;
    }
    
    /* Extract arguments after '--' */
    char **fuse_args = &argv[separator_idx + 1];
    int fuse_arg_count = argc - separator_idx - 1;
    
    if (fuse_arg_count == 0) {
        fprintf(stderr, "Error: No arguments provided after '--'\n");
        return 1;
    }
    
    /* Check if --json or -j is already present */
    int has_json_flag = 0;
    for (int i = 0; i < fuse_arg_count; i++) {
        if (strcmp(fuse_args[i], "--json") == 0 || strcmp(fuse_args[i], "-j") == 0) {
            has_json_flag = 1;
            break;
        }
    }
    
    /* Build final args, adding --json if needed */
    char **final_args = NULL;
    int final_arg_count = fuse_arg_count;
    
    if (!has_json_flag) {
        /* Add --json flag */
        final_arg_count = fuse_arg_count + 1;
        final_args = (char**)malloc(final_arg_count * sizeof(char*));
        for (int i = 0; i < fuse_arg_count; i++) {
            final_args[i] = fuse_args[i];
        }
        final_args[fuse_arg_count] = "--json";
    } else {
        final_args = fuse_args;
    }
    
    /* Create and run bridge */
    FuseBridge *bridge = bridge_create(sources, source_count, final_args, final_arg_count, time_format);
    int exit_code = bridge_run(bridge);
    bridge_free(bridge);
    
    /* Free allocated args array if we created one */
    if (!has_json_flag) {
        free(final_args);
    }
    
    if (config_path) {
        config_free(&config);
    }
    
    return exit_code;
}
