#!/bin/bash
# Simple test script for thermo-cli
# Run from the project root: ./scripts/test.sh

set -e

CLI="thermo-cli"
PASS=0
FAIL=0

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

pass() {
    echo -e "${GREEN}✓ PASS${NC}: $1"
    ((PASS++)) || true
}

fail() {
    echo -e "${RED}✗ FAIL${NC}: $1"
    ((FAIL++)) || true
}

skip() {
    echo -e "${YELLOW}○ SKIP${NC}: $1"
}

echo "========================================"
echo "thermo-cli Test Suite"
echo "========================================"
echo

# Check binary exists
# if [ ! -x "$CLI" ]; then
#     echo "Error: $CLI not found or not executable"
#     echo "Run 'make' first"
#     exit 1
# fi

# Test 1: Help command
echo "--- Basic Commands ---"
if $CLI --help >/dev/null 2>&1; then
    pass "Help command"
else
    fail "Help command"
fi

# Test 2: List command (may fail without hardware)
if $CLI list >/dev/null 2>&1; then
    pass "List command"
    HAS_HARDWARE=1
else
    skip "List command (no hardware detected)"
    HAS_HARDWARE=0
fi

# Test 3: Invalid command
if ! $CLI invalid_command >/dev/null 2>&1; then
    pass "Invalid command rejected"
else
    fail "Invalid command should be rejected"
fi

# Test 4: Get without arguments (shows help)
if $CLI get >/dev/null 2>&1; then
    pass "Get without args shows help"
else
    fail "Get without args should show help"
fi

# Hardware-dependent tests
if [ "$HAS_HARDWARE" -eq 1 ]; then
    echo
    echo "--- Hardware Tests ---"
    
    # Test 5: Get temperature from address 0, channel 0
    if $CLI get -a 0 -c 0 --temp 2>/dev/null; then
        pass "Get temperature (addr=0, ch=0)"
    else
        fail "Get temperature (addr=0, ch=0)"
    fi
    
    # Test 6: Get with JSON output
    if $CLI get -a 0 -c 0 --temp --json 2>/dev/null | grep -q "TEMPERATURE"; then
        pass "JSON output contains TEMPERATURE"
    else
        fail "JSON output should contain TEMPERATURE"
    fi
    
    # Test 7: Get serial number
    if $CLI get -a 0 -c 0 --serial 2>/dev/null | grep -qi "serial"; then
        pass "Get serial number"
    else
        fail "Get serial number"
    fi
    
    # Test 8: Get calibration coefficients
    if $CLI get -a 0 -c 0 --cali-coeffs 2>/dev/null; then
        pass "Get calibration coefficients"
    else
        fail "Get calibration coefficients"
    fi
    
    # Test 9: Get ADC voltage
    if $CLI get -a 0 -c 0 --adc 2>/dev/null; then
        pass "Get ADC voltage"
    else
        fail "Get ADC voltage"
    fi
    
    # Test 10: Get CJC temperature
    if $CLI get -a 0 -c 0 --cjc 2>/dev/null; then
        pass "Get CJC temperature"
    else
        fail "Get CJC temperature"
    fi
    
    # Test 11: Get all data
    if $CLI get -a 0 -c 0 --temp --adc --cjc --serial 2>/dev/null; then
        pass "Get all data combined"
    else
        fail "Get all data combined"
    fi
    
    # Test 12: Different TC type
    if $CLI get -a 0 -c 0 --temp --tc-type T 2>/dev/null; then
        pass "Get with TC type T"
    else
        fail "Get with TC type T"
    fi
else
    echo
    echo "--- Hardware Tests ---"
    skip "Hardware tests (no MCC 134 detected)"
fi

# Summary
echo
echo "========================================"
echo -e "Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC}"
echo "========================================"

exit $FAIL
