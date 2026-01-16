/*
 * Board Manager - Centralized board lifecycle management.
 * Handles opening, configuring, and closing MCC 134 boards.
 */

#ifndef BOARD_MANAGER_H
#define BOARD_MANAGER_H

#include "common.h"
#include "hardware.h"

#define MAX_BOARDS 8

typedef struct {
    uint8_t opened[MAX_BOARDS];   /* Track which boards are open */
    ThermalSource *sources;       /* Reference to sources (not owned) */
    int source_count;
} BoardManager;

/* Initialize manager and open all required boards for the given sources */
int board_manager_init(BoardManager *mgr, ThermalSource *sources, int source_count);

/* Apply calibration coefficients and TC type settings for all sources */
int board_manager_configure(BoardManager *mgr);

/* Apply only TC type settings (useful when calibration already set) */
int board_manager_set_tc_types(BoardManager *mgr);

/* Close all open boards */
void board_manager_close(BoardManager *mgr);

/* Check if a specific board is open */
int board_manager_is_open(BoardManager *mgr, uint8_t address);

/* Get count of open boards */
int board_manager_open_count(BoardManager *mgr);

#endif /* BOARD_MANAGER_H */
