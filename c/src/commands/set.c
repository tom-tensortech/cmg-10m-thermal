/*
 * Set command implementation.
 * Configures MCC 134 channel parameters.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

#include "commands/set.h"
#include "hardware.h"

/* Command: set - Configure channel parameters */
int cmd_set(int argc, char **argv) {
    int address = 0;
    int channel = 0;
    double cal_slope = 0;
    double cal_offset = 0;
    int has_slope = 0;
    int has_offset = 0;
    int update_interval = 0;
    int has_interval = 0;
    
    static struct option long_options[] = {
        {"address", required_argument, 0, 'a'},
        {"channel", required_argument, 0, 'c'},
        {"cali-slope", required_argument, 0, 'S'},
        {"cali-offset", required_argument, 0, 'O'},
        {"update-interval", required_argument, 0, 'i'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "a:c:S:O:i:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'a': address = atoi(optarg); break;
            case 'c': channel = atoi(optarg); break;
            case 'S': cal_slope = atof(optarg); has_slope = 1; break;
            case 'O': cal_offset = atof(optarg); has_offset = 1; break;
            case 'i': update_interval = atoi(optarg); has_interval = 1; break;
            default:
                fprintf(stderr, "Usage: thermo-cli set [OPTIONS]\n");
                return 1;
        }
    }
    
    /* Set calibration coefficients */
    if (has_slope || has_offset) {
        if (!has_slope || !has_offset) {
            fprintf(stderr, "Error: Both --cali-slope and --cali-offset must be provided\n");
            return 1;
        }
        
        if (thermo_open(address) != THERMO_SUCCESS) {
            fprintf(stderr, "Error opening board at address %d\n", address);
            return 1;
        }
        
        int result = thermo_set_calibration_coeffs(address, channel, cal_slope, cal_offset);
        thermo_close(address);
        
        if (result == THERMO_SUCCESS) {
            printf("Calibration Coefficients (Addr %d Ch %d) set to:\n", address, channel);
            printf("  Slope:  %.6f\n", cal_slope);
            printf("  Offset: %.6f\n", cal_offset);
        } else {
            fprintf(stderr, "Error setting calibration coefficients\n");
            return 1;
        }
    }
    
    /* Set update interval */
    if (has_interval) {
        if (thermo_open(address) != THERMO_SUCCESS) {
            fprintf(stderr, "Error opening board at address %d\n", address);
            return 1;
        }
        
        int result = thermo_set_update_interval(address, update_interval);
        thermo_close(address);
        
        if (result == THERMO_SUCCESS) {
            printf("Update Interval (Addr %d) set to: %d seconds\n", address, update_interval);
        } else {
            fprintf(stderr, "Error setting update interval\n");
            return 1;
        }
    }
    
    return 0;
}
