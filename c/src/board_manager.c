/*
 * Board Manager implementation.
 * Centralizes board lifecycle management to reduce code duplication.
 */

#include <stdio.h>
#include <string.h>

#include "board_manager.h"
#include "utils.h"

/* Initialize manager and open all required boards */
int board_manager_init(BoardManager *mgr, ThermalSource *sources, int source_count) {
    memset(mgr->opened, 0, sizeof(mgr->opened));
    mgr->sources = sources;
    mgr->source_count = source_count;
    
    /* Open all unique boards and apply update intervals */
    for (int i = 0; i < source_count; i++) {
        uint8_t addr = sources[i].address;
        
        if (!mgr->opened[addr]) {
            DEBUG_PRINT("Opening board at address %d", addr);
            
            if (thermo_open(addr) != THERMO_SUCCESS) {
                fprintf(stderr, "Error: Failed to open board at address %d\n", addr);
                board_manager_close(mgr);
                return THERMO_ERROR;
            }
            mgr->opened[addr] = 1;
            
            /* Apply update interval if it differs from default */
            if (sources[i].update_interval > 0 && 
                sources[i].update_interval != DEFAULT_UPDATE_INTERVAL) {
                DEBUG_PRINT("Setting update interval for address %d to %d", 
                           addr, sources[i].update_interval);
                if (thermo_set_update_interval(addr, (uint8_t)sources[i].update_interval) != THERMO_SUCCESS) {
                    fprintf(stderr, "Warning: Failed to set update interval for address %d\n", addr);
                }
            }
        }
    }
    
    return THERMO_SUCCESS;
}

/* Apply calibration coefficients and TC type settings for all sources */
int board_manager_configure(BoardManager *mgr) {
    for (int i = 0; i < mgr->source_count; i++) {
        ThermalSource *src = &mgr->sources[i];
        
        /* Apply calibration coefficients if non-default */
        if (src->cal_coeffs.slope != DEFAULT_CALIBRATION_SLOPE ||
            src->cal_coeffs.offset != DEFAULT_CALIBRATION_OFFSET) {
            DEBUG_PRINT("Setting calibration for addr %d ch %d: slope=%.6f, offset=%.6f",
                       src->address, src->channel, 
                       src->cal_coeffs.slope, src->cal_coeffs.offset);
            if (thermo_set_calibration_coeffs(src->address, src->channel,
                                             src->cal_coeffs.slope,
                                             src->cal_coeffs.offset) != THERMO_SUCCESS) {
                fprintf(stderr, "Warning: Failed to set calibration coefficients for address %d, channel %d\n",
                        src->address, src->channel);
            }
        }
        
        /* Set TC type */
        if (thermo_set_tc_type(src->address, src->channel, src->tc_type) != THERMO_SUCCESS) {
            fprintf(stderr, "Warning: Failed to set TC type for address %d, channel %d\n",
                    src->address, src->channel);
        }
    }
    
    return THERMO_SUCCESS;
}

/* Apply only TC type settings */
int board_manager_set_tc_types(BoardManager *mgr) {
    for (int i = 0; i < mgr->source_count; i++) {
        ThermalSource *src = &mgr->sources[i];
        
        if (thermo_set_tc_type(src->address, src->channel, src->tc_type) != THERMO_SUCCESS) {
            fprintf(stderr, "Warning: Failed to set TC type for address %d, channel %d\n",
                    src->address, src->channel);
        }
    }
    
    return THERMO_SUCCESS;
}

/* Close all open boards */
void board_manager_close(BoardManager *mgr) {
    for (int i = 0; i < MAX_BOARDS; i++) {
        if (mgr->opened[i]) {
            DEBUG_PRINT("Closing board at address %d", i);
            thermo_close(i);
            mgr->opened[i] = 0;
        }
    }
    mgr->sources = NULL;
    mgr->source_count = 0;
}

/* Check if a specific board is open */
int board_manager_is_open(BoardManager *mgr, uint8_t address) {
    if (address >= MAX_BOARDS) return 0;
    return mgr->opened[address];
}

/* Get count of open boards */
int board_manager_open_count(BoardManager *mgr) {
    int count = 0;
    for (int i = 0; i < MAX_BOARDS; i++) {
        if (mgr->opened[i]) count++;
    }
    return count;
}
