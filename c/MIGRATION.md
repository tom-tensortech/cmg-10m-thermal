# Complete Migration Guide: ThermoData → ChannelReading/BoardInfo

This document details the complete migration from the legacy `ThermoData` API to the new `ChannelReading`/`BoardInfo` API.

## Table of Contents

1. [Overview](#overview)
2. [Data Structure Comparison](#data-structure-comparison)
3. [Current State Analysis](#current-state-analysis)
4. [Migration Steps](#migration-steps)
5. [Code Changes Required](#code-changes-required)
6. [Files to Modify](#files-to-modify)
7. [Testing Checklist](#testing-checklist)

---

## Overview

### Why Migrate?

The legacy `ThermoData` structure combines static board information with dynamic readings in a single flat structure. This causes:

1. **Data duplication**: Serial number, calibration date are fetched repeatedly in streaming mode
2. **Unclear ownership**: Same struct holds both per-board and per-channel data
3. **Inefficient memory**: All fields allocated even when not needed

### New Architecture

```
┌─────────────────┐     ┌──────────────────┐
│   BoardInfo     │     │  ChannelReading  │
├─────────────────┤     ├──────────────────┤
│ address         │     │ address          │
│ serial[16]      │     │ channel          │
│ update_interval │     │ temperature      │
│ channels[4]:    │     │ adc_voltage      │
│   ├─ cal_date   │     │ cjc_temp         │
│   ├─ cal_coeffs │     │ has_temp : 1     │
│   └─ tc_type    │     │ has_adc : 1      │
└─────────────────┘     │ has_cjc : 1      │
     (static)           └──────────────────┘
                             (dynamic)
```

**Benefits:**
- Static data fetched once, cached
- Dynamic readings are lightweight (no string fields)
- Per-channel calibration properly modeled
- Cleaner separation of concerns

---

## Data Structure Comparison

### Legacy: ThermoData

```c
typedef struct {
    int address;
    int channel;
    
    /* Flags */
    int has_serial;
    int has_cal_date;
    int has_cal_coeffs;
    int has_temp;
    int has_adc;
    int has_cjc;
    int has_interval;
    
    /* Data */
    char serial[16];
    char cal_date[16];
    CalibrationInfo cal_coeffs;
    double temperature;
    double adc_voltage;
    double cjc_temp;
    uint8_t update_interval;
} ThermoData;
```

**Size:** ~88 bytes per reading (including unused fields)

### New: ChannelReading + BoardInfo

```c
typedef struct {
    char cal_date[16];
    CalibrationInfo cal_coeffs;  /* slope + offset = 16 bytes */
    uint8_t tc_type;
} ChannelConfig;  /* ~33 bytes */

typedef struct {
    uint8_t address;
    char serial[16];
    uint8_t update_interval;
    ChannelConfig channels[4];
} BoardInfo;  /* ~150 bytes per board, shared across channels */

typedef struct {
    uint8_t address;
    uint8_t channel;
    double temperature;
    double adc_voltage;
    double cjc_temp;
    unsigned has_temp : 1;
    unsigned has_adc : 1;
    unsigned has_cjc : 1;
} ChannelReading;  /* ~28 bytes per reading */
```

**Memory savings in streaming:** 
- Legacy: 88 bytes × N readings
- New: 150 bytes (once) + 28 bytes × N readings

---

## Current State Analysis

### Functions Using Legacy API

| Function | File | Usage | Migration Complexity |
|----------|------|-------|---------------------|
| `thermo_data_init()` | get.c:24 | Initialize ThermoData | Replace with `channel_reading_init()` |
| `thermo_data_collect()` | get.c:151 | Collect all data types | Split into `channel_reading_collect()` + `board_info_collect()` |
| `thermo_data_output_json()` | get.c:217 | JSON output | Replace with json_utils functions |
| `thermo_data_output_table()` | get.c:223 | Table output | Create new table output functions |
| `thermo_data_split()` | get.c:240 | Split static/dynamic | No longer needed (already separate) |
| `collect_channels()` | get.c:265 | Batch collection | Rewrite to use new structs |
| `output_channels_json()` | get.c:307 | Multi-channel JSON | Use `thermo_data_array_to_json()` or new equivalent |
| `output_channels_table()` | get.c:315 | Multi-channel table | Rewrite for new structs |
| `stream_channels()` | get.c:366 | Streaming loop | Major rewrite |

### Call Graph

```
cmd_get()
├── collect_channels()          # Single reading mode
│   ├── board_manager_init()
│   ├── board_manager_configure()
│   ├── thermo_data_init()      ← MIGRATE
│   ├── thermo_data_collect()   ← MIGRATE
│   └── output_channels_json() / output_channels_table()
│       ├── thermo_data_output_json()  ← MIGRATE
│       └── thermo_data_output_table() ← MIGRATE
│
└── stream_channels()           # Streaming mode
    ├── board_manager_init()
    ├── board_manager_configure()
    ├── (static data collection once)
    │   ├── thermo_data_init()      ← MIGRATE
    │   └── thermo_data_collect()   ← MIGRATE
    │
    └── (loop: dynamic data)
        ├── thermo_data_init()      ← MIGRATE
        ├── thermo_data_collect()   ← MIGRATE
        └── output functions        ← MIGRATE
```

---

## Migration Steps

### Step 1: Create New Output Functions

**File:** `src/commands/get.c` (or new `src/output.c`)

```c
/* Output ChannelReading in table format */
void channel_reading_output_table(const ChannelReading *reading, int show_header);

/* Output BoardInfo in table format */  
void board_info_output_table(const BoardInfo *info, uint8_t channel, int show_header);

/* Output combined reading + board info in table format */
void output_reading_with_info_table(const ChannelReading *reading, 
                                    const BoardInfo *info,
                                    const char *key,
                                    int show_header);
```

### Step 2: Update json_utils.h/c

Add functions for combined output:

```c
/* Convert ChannelReading + BoardInfo to combined JSON */
cJSON* reading_with_info_to_json(const ChannelReading *reading,
                                  const BoardInfo *info,
                                  const char *key);

/* Convert arrays to JSON */
cJSON* readings_to_json_array(const ChannelReading *readings,
                               const BoardInfo *infos,  /* One per unique board */
                               const ThermalSource *sources,
                               int count);
```

### Step 3: Rewrite collect_channels()

**Before:**
```c
static int collect_channels(ThermalSource *sources, int source_count, 
                           ThermoData **data_out, ..., BoardManager *mgr_out) {
    ThermoData *data_array = calloc(source_count, sizeof(ThermoData));
    
    for (int i = 0; i < source_count; i++) {
        thermo_data_init(&data_array[i], ...);
        thermo_data_collect(&data_array[i], ...);
    }
    
    *data_out = data_array;
}
```

**After:**
```c
typedef struct {
    ChannelReading *readings;
    BoardInfo *board_infos;      /* Array indexed by board address */
    int reading_count;
    int board_count;
} CollectedData;

static int collect_channels_new(ThermalSource *sources, int source_count,
                                CollectedData *out,
                                int get_serial, int get_cal_date, int get_cal_coeffs,
                                int get_temp, int get_adc, int get_cjc, int get_interval,
                                BoardManager *mgr_out) {
    /* Allocate readings (one per source) */
    out->readings = calloc(source_count, sizeof(ChannelReading));
    out->reading_count = source_count;
    
    /* Allocate board infos (max 8 boards) */
    out->board_infos = calloc(MAX_BOARDS, sizeof(BoardInfo));
    out->board_count = 0;
    
    /* Initialize BoardManager */
    if (board_manager_init(mgr_out, sources, source_count) != THERMO_SUCCESS) {
        free(out->readings);
        free(out->board_infos);
        return THERMO_ERROR;
    }
    board_manager_configure(mgr_out);
    
    /* Collect board info once per unique board */
    uint8_t board_collected[MAX_BOARDS] = {0};
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
    }
    
    return THERMO_SUCCESS;
}

static void collected_data_free(CollectedData *data) {
    free(data->readings);
    free(data->board_infos);
    data->readings = NULL;
    data->board_infos = NULL;
}
```

### Step 4: Rewrite stream_channels()

**Key insight:** In streaming mode, static data (serial, calibration) is fetched ONCE before the loop, then only dynamic readings are collected in the loop.

```c
static int stream_channels_new(ThermalSource *sources, int source_count,
                               int get_serial, int get_cal_date, int get_cal_coeffs,
                               int get_temp, int get_adc, int get_cjc, int get_interval,
                               int stream_hz, int json_output, int clean_mode) {
    BoardManager mgr;
    BoardInfo board_infos[MAX_BOARDS] = {0};
    uint8_t board_collected[MAX_BOARDS] = {0};
    
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
            /* Output board info JSON once */
        } else {
            /* Output board info table once */
        }
    }
    
    /* Print streaming info */
    if (!json_output && !clean_mode) {
        printf("Streaming at %d Hz (Ctrl+C to stop)\n", stream_hz);
        printf("----------------------------------------\n");
    }
    
    signals_install_handlers();
    
    /* Streaming loop - only dynamic readings */
    while (g_running) {
        ChannelReading *readings = calloc(source_count, sizeof(ChannelReading));
        if (!readings) {
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
            output_readings_json(readings, source_count, sources);
        } else {
            output_readings_table(readings, source_count, sources, clean_mode);
        }
        
        free(readings);
        nanosleep(&sleep_time, NULL);
    }
    
    board_manager_close(&mgr);
    return 0;
}
```

### Step 5: Update Output Functions

**New output_readings_json():**
```c
static void output_readings_json(ChannelReading *readings, int count, 
                                 ThermalSource *sources) {
    if (count == 1) {
        cJSON *root = reading_to_json(&readings[0]);
        if (sources[0].key[0] != '\0') {
            /* Insert KEY at beginning */
            cJSON *key = cJSON_CreateString(sources[0].key);
            cJSON_AddItemToObjectCS(root, "KEY", key);
        }
        json_print_and_free(root, 0);
    } else {
        cJSON *arr = cJSON_CreateArray();
        for (int i = 0; i < count; i++) {
            cJSON *item = reading_to_json(&readings[i]);
            if (sources[i].key[0] != '\0') {
                cJSON_AddStringToObject(item, "KEY", sources[i].key);
            }
            cJSON_AddItemToArray(arr, item);
        }
        json_print_and_free(arr, 0);
    }
}
```

**New output_readings_table():**
```c
static void output_readings_table(ChannelReading *readings, int count,
                                  ThermalSource *sources, int clean_mode) {
    for (int i = 0; i < count; i++) {
        if (count > 1 || sources[i].key[0] != '\0') {
            if (sources[i].key[0] != '\0') {
                printf("%s (Address: %d, Channel: %d):\n",
                       sources[i].key, readings[i].address, readings[i].channel);
            } else {
                printf("Address: %d, Channel: %d:\n",
                       readings[i].address, readings[i].channel);
            }
        }
        
        if (readings[i].has_temp) {
            printf("  Temperature: %.6f degC\n", readings[i].temperature);
        }
        if (readings[i].has_adc) {
            printf("  ADC: %.6f V\n", readings[i].adc_voltage);
        }
        if (readings[i].has_cjc) {
            printf("  CJC: %.6f degC\n", readings[i].cjc_temp);
        }
    }
    
    if (!clean_mode) {
        printf("----------------------------------------\n");
    }
}
```

### Step 6: Update cmd_get()

```c
int cmd_get(int argc, char **argv) {
    /* ... argument parsing unchanged ... */
    
    if (stream_hz > 0) {
        result = stream_channels_new(sources, source_count, ...);
    } else {
        CollectedData data;
        BoardManager mgr;
        
        if (collect_channels_new(sources, source_count, &data, ..., &mgr) == THERMO_SUCCESS) {
            if (json_output) {
                output_collected_json(&data, sources);
            } else {
                output_collected_table(&data, sources, clean_mode);
            }
            collected_data_free(&data);
        }
        
        board_manager_close(&mgr);
    }
    
    /* ... cleanup ... */
}
```

### Step 7: Remove Legacy Code

After migration is complete and tested:

**Remove from get.c:**
- `thermo_data_init()`
- `thermo_data_collect()`
- `thermo_data_output_json()`
- `thermo_data_output_table()`
- `thermo_data_split()`
- Old `collect_channels()` 
- Old `stream_channels()`
- `output_single_json_with_key()` (already removed)
- Old `output_channels_json()`
- Old `output_channels_table()`

**Remove from get.h:**
- Declarations for above functions

**Remove from common.h:**
- `ThermoData` typedef (or keep for bridge.c compatibility)

**Remove from common.c:**
- `thermo_data_to_reading()` (if no longer needed)
- `reading_to_thermo_data()` (if no longer needed)
- `thermo_data_to_board_info()` (never used)

**Remove from json_utils.c:**
- `thermo_data_add_to_json()`
- `thermo_data_to_json()`
- `thermo_data_to_json_with_key()`
- `thermo_data_array_to_json()`

---

## Files to Modify

| File | Changes |
|------|---------|
| `src/commands/get.c` | Major rewrite: ~400 lines affected |
| `include/commands/get.h` | Update function declarations |
| `src/json_utils.c` | Add new functions, remove legacy |
| `include/json_utils.h` | Update declarations |
| `src/common.c` | Remove unused adapters |
| `include/common.h` | Remove ThermoData (optional) |
| `src/bridge.c` | May need updates if using ThermoData |

---

## Testing Checklist

### Single Reading Mode
- [ ] `thermo-cli get -a 0 -c 0 --temp`
- [ ] `thermo-cli get -a 0 -c 0 --temp --adc --cjc`
- [ ] `thermo-cli get -a 0 -c 0 --temp --json`
- [ ] `thermo-cli get -a 0 -c 0 --serial --cali-coeffs`
- [ ] `thermo-cli get -a 0 -c 0 --serial --cali-coeffs --json`
- [ ] `thermo-cli get -a 0 -c 0 --temp --serial --cali-coeffs --cali-date --update-interval`

### Multi-Channel Mode (with config)
- [ ] `thermo-cli get -C config.yaml --temp`
- [ ] `thermo-cli get -C config.yaml --temp --json`
- [ ] `thermo-cli get -C config.yaml --temp --serial`

### Streaming Mode
- [ ] `thermo-cli get -a 0 -c 0 --temp --stream 1`
- [ ] `thermo-cli get -a 0 -c 0 --temp --stream 1 --json`
- [ ] `thermo-cli get -C config.yaml --temp --stream 1`
- [ ] `thermo-cli get -C config.yaml --temp --stream 1 --json`
- [ ] `thermo-cli get -a 0 -c 0 --temp --serial --stream 1` (static header + dynamic loop)
- [ ] Ctrl+C graceful shutdown

### Edge Cases
- [ ] Invalid address/channel rejected
- [ ] Missing required args show help
- [ ] Multiple channels from same board (shared BoardInfo)
- [ ] Channels from different boards

### Output Format Verification
- [ ] JSON output is valid JSON
- [ ] Table output aligned correctly
- [ ] Clean mode removes separators
- [ ] KEY field appears when config has keys

---

## Estimated Effort

| Task | Time |
|------|------|
| Step 1: New output functions | 1-2 hours |
| Step 2: Update json_utils | 1 hour |
| Step 3: Rewrite collect_channels | 2-3 hours |
| Step 4: Rewrite stream_channels | 2-3 hours |
| Step 5: Update output functions | 1-2 hours |
| Step 6: Update cmd_get | 1 hour |
| Step 7: Remove legacy code | 1 hour |
| Testing & debugging | 2-3 hours |
| **Total** | **11-16 hours** |

---

## Alternative: Minimal Cleanup (Option A)

If full migration is too risky/time-consuming, remove only the dead code:

1. Remove from `get.c`:
   - `channel_reading_collect()` (lines 35-59)
   - `board_info_collect()` (lines 61-89)
   - `channel_reading_output_json()` (lines 91-116)
   - `board_info_output_json()` (lines 118-148)

2. Remove from `get.h`:
   - Corresponding declarations

3. Remove from `common.c`:
   - `thermo_data_to_board_info()` (never called)

4. Remove from `json_utils.c`:
   - `reading_to_json()`
   - `reading_add_to_json()`
   - `board_info_to_json()`
   - `board_info_add_to_json()`

5. Keep:
   - `ChannelReading`, `BoardInfo`, `ChannelConfig` structures (for future use)
   - `channel_reading_init()`, `board_info_init()` (for future use)
   - All ThermoData-related code (still in use)

**Estimated effort:** 1-2 hours
