/*
 * Configuration file handling header.
 * YAML/JSON config parsing and generation.
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

/* Thermal source configuration */
typedef struct {
    char key[64];
    uint8_t address;
    uint8_t channel;
    char tc_type[8];
} ThermalSource;

/* Configuration structure */
typedef struct {
    ThermalSource *sources;
    int source_count;
} Config;

/* Configuration functions */
int config_load(const char *path, Config *config);
void config_free(Config *config);
int config_create_example(const char *output_path);

#endif /* CONFIG_H */
