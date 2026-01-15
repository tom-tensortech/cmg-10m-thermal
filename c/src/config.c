/*
 * Configuration file loading and parsing.
 * Supports both YAML (libyaml) and JSON (cJSON) config files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yaml.h>

#include "config.h"
#include "hardware.h"

#include "cJSON.h"

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
    fread(content, 1, fsize, fp);
    fclose(fp);
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
    ThermalSource current_source = {0};
    char current_key[64] = {0};

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
                if (!in_source_item && strcmp((char*)event.data.scalar.value, "sources") == 0) {
                    in_sources = 1;
                } else if (in_sources && !in_source_item) {
                    strncpy(current_key, (char*)event.data.scalar.value, sizeof(current_key) - 1);
                } else if (in_source_item) {
                    if (strcmp(current_key, "key") == 0) {
                        strncpy(current_source.key, (char*)event.data.scalar.value, sizeof(current_source.key) - 1);
                    } else if (strcmp(current_key, "address") == 0) {
                        current_source.address = (uint8_t)atoi((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "channel") == 0) {
                        current_source.channel = (uint8_t)atoi((char*)event.data.scalar.value);
                    } else if (strcmp(current_key, "tc_type") == 0) {
                        strncpy(current_source.tc_type, (char*)event.data.scalar.value, sizeof(current_source.tc_type) - 1);
                    }
                    current_key[0] = '\0';
                }
                break;

            case YAML_MAPPING_START_EVENT:
                if (in_sources) {
                    in_source_item = 1;
                    memset(&current_source, 0, sizeof(current_source));
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
        fprintf(fp, "      \"tc_type\": \"K\"\n");
        fprintf(fp, "    },\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"key\": \"MOTOR_TEMP\",\n");
        fprintf(fp, "      \"address\": 0,\n");
        fprintf(fp, "      \"channel\": 1,\n");
        fprintf(fp, "      \"tc_type\": \"K\"\n");
        fprintf(fp, "    },\n");
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"key\": \"AMBIENT_TEMP\",\n");
        fprintf(fp, "      \"address\": 0,\n");
        fprintf(fp, "      \"channel\": 2,\n");
        fprintf(fp, "      \"tc_type\": \"K\"\n");
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
        fprintf(fp, "- key: MOTOR_TEMP\n");
        fprintf(fp, "  address: 0\n");
        fprintf(fp, "  channel: 1\n");
        fprintf(fp, "  tc_type: K\n");
        fprintf(fp, "- key: AMBIENT_TEMP\n");
        fprintf(fp, "  address: 0\n");
        fprintf(fp, "  channel: 2\n");
        fprintf(fp, "  tc_type: K\n");
    }

    fclose(fp);
    return THERMO_SUCCESS;
}
