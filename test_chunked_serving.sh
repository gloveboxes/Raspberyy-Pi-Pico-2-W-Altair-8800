#!/bin/bash

# Test script for chunked HTTP serving
# Tests that the web server consistently returns the same content size

DEVICE_URL="http://192.168.1.119:8088/"
ITERATIONS=100000
TEMP_FILE=$(mktemp)

echo "Testing chunked HTTP serving at ${DEVICE_URL}"
echo "=========================================="
echo ""

# Cleanup on exit
cleanup() {
    rm -f "${TEMP_FILE}"
}
trap cleanup EXIT

# Make initial request to get baseline size
echo "Making initial request to determine baseline size..."
curl -s -o "${TEMP_FILE}" "${DEVICE_URL}"
BASELINE_SIZE=$(wc -c < "${TEMP_FILE}")

if [ $? -ne 0 ]; then
    echo "ERROR: Initial request failed!"
    exit 1
fi

echo "Baseline content size: ${BASELINE_SIZE} bytes"
echo ""
echo "Starting ${ITERATIONS} test iterations..."
echo ""

# Track results
SUCCESS_COUNT=1  # Count the initial baseline request as success
FAILURE_COUNT=0
FAILURES=""

# Run test iterations
for i in $(seq 1 ${ITERATIONS}); do
    # Show progress every 100 iterations
    if [ $((i % 100)) -eq 0 ]; then
        echo "Progress: ${i}/${ITERATIONS} (Success: ${SUCCESS_COUNT}, Failed: ${FAILURE_COUNT})"
    fi
    
    # Fetch content
    curl -s -o "${TEMP_FILE}" "${DEVICE_URL}"
    
    if [ $? -ne 0 ]; then
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
        FAILURES="${FAILURES}Iteration ${i}: curl failed\n"
        continue
    fi
    
    # Check size
    CURRENT_SIZE=$(wc -c < "${TEMP_FILE}")
    
    if [ "${CURRENT_SIZE}" -ne "${BASELINE_SIZE}" ]; then
        FAILURE_COUNT=$((FAILURE_COUNT + 1))
        FAILURES="${FAILURES}Iteration ${i}: Size mismatch (expected ${BASELINE_SIZE}, got ${CURRENT_SIZE})\n"
    else
        SUCCESS_COUNT=$((SUCCESS_COUNT + 1))
    fi
done

echo ""
echo "=========================================="
echo "Test Results:"
echo "=========================================="
echo "Total iterations: ${ITERATIONS}"
echo "Successful:       ${SUCCESS_COUNT}"
echo "Failed:           ${FAILURE_COUNT}"
echo ""

if [ ${FAILURE_COUNT} -gt 0 ]; then
    echo "Failures:"
    echo -e "${FAILURES}"
    exit 1
else
    echo "✓ All tests passed! Server consistently returned ${BASELINE_SIZE} bytes."
    exit 0
fi
