# Thermo CLI (C Implementation)

A C implementation of the MCC 134 thermocouple interface and data fusion tool.

## Features

- **List MCC 134 boards** - Detect and display connected thermocouple DAQ boards
- **Read temperature data** - Get temperature, ADC voltage, and CJC readings
- **Stream mode** - Continuous readings at specified frequency (Hz)
- **Configure channels** - Set thermocouple types, calibration coefficients, update intervals
- **Data fusion bridge** - Inject thermal data into `cmg-cli` output streams
- **JSON output** - Machine-readable output for integration
- **Clean mode** - Simple output without formatting for scripting
- **Config files** - Support YAML and JSON configuration files

## Dependencies

### Required
- **libdaqhats** - MCC DAQ HAT library ([installation guide](https://github.com/mccdaq/daqhats))
- **libyaml-dev** - YAML parsing library
- **gcc** - GNU C compiler
- **make** - Build system

### Included (vendored)
- **cJSON** - JSON parsing/generation (single-file library)

## Installation

### 1. Install Dependencies

On Raspberry Pi OS (Debian-based):
```bash
./install_deps.sh
```

Or manually:
```bash
sudo apt update
sudo apt install -y libyaml-dev

# Install daqhats if not already installed
git clone https://github.com/mccdaq/daqhats.git
cd daqhats
sudo ./install.sh
```

### 2. Build

```bash
make
```

### 3. Install System-wide (optional)

```bash
sudo make install
```

This installs `thermo-cli` to `/usr/local/bin/`.

## Usage

### List Connected Boards

```bash
thermo-cli list
thermo-cli list --json
```

### Read Temperature

```bash
# Read temperature from address 0, channel 0 (default)
thermo-cli get --temp

# Specify address and channel
thermo-cli get --address 0 --channel 1 --tc-type K --temp

# Get multiple readings
thermo-cli get -a 0 -c 0 --temp --adc --cjc

# JSON output
thermo-cli get -a 0 -c 1 --temp --json

# Stream temperature readings at 2 Hz
thermo-cli get --temp --stream 2

# Stream with JSON output at 5 Hz
thermo-cli get -T -A --stream 5 --json

# Clean output mode (no alignment/formatting)
thermo-cli get --temp --clean
```

### Get Board Information

```bash
# Serial number
thermo-cli get --serial

# Calibration date
thermo-cli get --cali-date

# Calibration coefficients
thermo-cli get --channel 0 --cali-coeffs

# Update interval
thermo-cli get --update-interval
```

### Configure Channel

```bash
# Set calibration coefficients
thermo-cli set --address 0 --channel 0 --cali-slope 1.0 --cali-offset 0.0

# Set update interval
thermo-cli set --address 0 --update-interval 2
```

### Data Fusion (Inject into cmg-cli)

The `fuse` command spawns `cmg-cli` as a subprocess and injects thermal readings into the JSON output with timestamps.

#### Single source mode:
```bash
thermo-cli fuse --address 0 --channel 1 --key MOTOR_TEMP -- --power
```

#### Config file mode:
```bash
# Create example config
thermo-cli init-config --output my_config.yaml

# Use config for fusion
thermo-cli fuse --config my_config.yaml -- --actuator --stream 5
```

#### Custom timestamp format:
```bash
# Default ISO 8601 with microseconds
thermo-cli fuse -a 0 -c 0 -- --power
# Output: {"...", "TIMESTAMP": "2026-01-15T14:30:45.123456", "THERMOCOUPLE": {"TEMP": 25.5}}

# Custom format (time only)
thermo-cli fuse -a 0 -c 0 -T '%H:%M:%S.%f' -- --power
# Output: {"...", "TIMESTAMP": "14:30:45.123456", "THERMOCOUPLE": {"TEMP": 25.5}}
```

### Configuration Files

Generate example config:
```bash
thermo-cli init-config --output thermo_config.yaml
```

Example YAML config:
```yaml
sources:
- key: BATTERY_TEMP
  address: 0
  channel: 0
  tc_type: K
- key: MOTOR_TEMP
  address: 0
  channel: 1
  tc_type: K
```

Example JSON config:
```bash
thermo-cli init-config --output thermo_config.json
```

## Command Reference

### Global Options
- `--help, -h` - Show help
- `--version, -v` - Show version

### Commands

#### `list`
List all connected MCC 134 boards.

**Options:**
- `-j, --json` - Output as JSON

#### `get`
Read data from a specific channel.

**Options:**
- `-a, --address NUM` - Board address (0-7) [default: 0]
- `-c, --channel NUM` - Channel index (0-3) [default: 0]
- `-t, --tc-type TYPE` - Thermocouple type (K,J,T,E,R,S,B,N) [default: K]
- `-s, --serial` - Get serial number
- `-D, --cali-date` - Get calibration date
- `-C, --cali-coeffs` - Get calibration coefficients
- `-T, --temp` - Get temperature (default)
- `-A, --adc` - Get raw ADC voltage
- `-J, --cjc` - Get CJC temperature
- `-i, --update-interval` - Get update interval
- `-S, --stream HZ` - Stream readings at specified frequency (Hz)
- `-l, --clean` - Simple output without alignment/formatting
- `-j, --json` - Output as JSON

**Stream Mode:**
- Streams dynamic data (temperature, ADC, CJC) continuously at the specified Hz rate
- Static data (serial, calibration, update interval) is displayed once at the beginning
- Use Ctrl+C to stop streaming
- In clean mode, no separator lines or alignment are used

**Examples:**
```bash
# Single reading with formatted output
thermo-cli get -T -A

# Stream at 2 Hz
thermo-cli get -T --stream 2

# Stream with static info displayed once
thermo-cli get -s -T -A --stream 5

# Clean mode (simple output)
thermo-cli get -T --clean
```

#### `set`
Configure channel parameters.

**Options:**
- `-a, --address NUM` - Board address (0-7) [default: 0]
- `-c, --channel NUM` - Channel index (0-3) [default: 0]
- `-S, --cali-slope VALUE` - Set calibration slope
- `-O, --cali-offset VALUE` - Set calibration offset
- `-i, --update-interval NUM` - Set update interval in seconds

#### `fuse`
Fuse thermal data into cmg-cli JSON output.

**Options:**
- `-C, --config FILE` - Path to YAML/JSON config file
- `-a, --address NUM` - Single mode: Board address
- `-c, --channel NUM` - Single mode: Channel index
- `-k, --key NAME` - Single mode: JSON key for temperature [default: TEMP_FUSED]
- `-t, --tc-type TYPE` - Single mode: Thermocouple type [default: K]
- `-T, --time-format FMT` - Timestamp format [default: %Y-%m-%dT%H:%M:%S.%f]
  - Use `%f` for 6-digit microseconds

**Note:** 
- Use `--` to separate thermo-cli options from cmg-cli arguments.
- Data fusion only works with JSON output. The `--json` flag is automatically added if not specified.
- Thermal data is injected as a `THERMOCOUPLE` object with a `TIMESTAMP` field.

#### `init-config`
Generate an example configuration file.

**Options:**
- `-o, --output FILE` - Output file path [default: thermo_config.yaml]

## Building from Source

### Project Structure
```
cmg-10m-thermal-c/
├── src/                   # Source files
│   ├── main.c             # Entry point & argp parsing
│   ├── hardware.c         # MCC 134 hardware abstraction
│   ├── config.c           # YAML/JSON config parsing
│   ├── bridge.c           # Data fusion bridge
│   ├── utils.c            # Utilities & formatting
│   └── commands/          # Command implementations
│       ├── list.c         # List command
│       ├── get.c          # Get command
│       ├── set.c          # Set command
│       ├── init_config.c  # Init-config command
│       └── fuse.c         # Fuse command (in bridge.c)
├── include/               # Header files
│   ├── hardware.h         # Hardware abstraction API
│   ├── config.h           # Configuration API
│   ├── bridge.h           # Data fusion bridge API
│   ├── utils.h            # Utility functions API
│   └── commands/          # Command headers
│       ├── common.h       # Shared command structures
│       ├── list.h         # List command API
│       ├── get.h          # Get command API
│       ├── set.h          # Set command API
│       ├── init_config.h  # Init-config command API
│       └── fuse.h         # Fuse command API
├── vendor/                # Third-party libraries
│   ├── cJSON.c
│   └── cJSON.h
├── Makefile               # Build system
└── install_deps.sh        # Dependency installer
```

### Build Commands

```bash
# Build project
make

# Clean build artifacts
make clean

# Install to /usr/local/bin
sudo make install

# Uninstall
sudo make uninstall

# Test compilation only
make test-compile

# Show help
make help
```

## Troubleshooting

### Library not found errors

If you see `error while loading shared libraries: libdaqhats.so`:
```bash
sudo ldconfig
```

### YAML parsing errors

Make sure `libyaml-dev` is installed:
```bash
sudo apt install libyaml-dev
```

### Permission denied

For hardware access, you may need to run with sudo or add your user to the `gpio` group:
```bash
sudo usermod -a -G gpio $USER
```

Then log out and back in.

## Support

For issues specific to:
- **This C implementation** - Open an issue in this repository
- **MCC 134 hardware** - See [MCC DAQ HAT documentation](https://github.com/mccdaq/daqhats)
- **DAQ HAT library** - See [daqhats repository](https://github.com/mccdaq/daqhats)
