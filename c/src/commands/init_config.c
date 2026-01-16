/*
 * Init-config command implementation.
 * Generates example configuration files.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#include "commands/init_config.h"
#include "common.h"
#include "hardware.h"

/* Command: init-config - Generate example configuration file */
int cmd_init_config(int argc, char **argv) {
    char output_path[256] = "thermo_config.yaml";
    
    static struct option long_options[] = {
        {"output", required_argument, 0, 'o'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "o:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'o':
                strncpy(output_path, optarg, sizeof(output_path) - 1);
                break;
            default:
                fprintf(stderr, "Usage: thermo-cli init-config [--output FILE]\n");
                return 1;
        }
    }
    
    if (config_create_example(output_path) == THERMO_SUCCESS) {
        printf("Created example config: %s\n", output_path);
        return 0;
    } else {
        fprintf(stderr, "Error creating config file\n");
        return 1;
    }
}
