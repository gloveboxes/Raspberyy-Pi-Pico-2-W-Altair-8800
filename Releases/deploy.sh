#!/bin/bash

# Script to deploy firmware to Pico board
# This script lists available .uf2 files and deploys the selected one

# Color codes for output
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Get the directory where the script is located
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}  Pico Altair 8800 Firmware Deployment Tool${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
echo ""

# Check if picotool is available
if ! command -v picotool &> /dev/null; then
    echo -e "${RED}Error: picotool not found.${NC}"
    echo ""
    echo "Please install picotool first:"
    echo "  macOS:   brew install picotool"
    echo "  Linux:   sudo apt install picotool  (or build from source)"
    echo "  Source:  https://github.com/raspberrypi/picotool"
    echo ""
    exit 1
fi

# Find all .uf2 files in the Releases directory
cd "$SCRIPT_DIR"

# Build array of .uf2 files in alphabetical order (compatible with bash 3.x and zsh)
UF2_FILES=()
while IFS= read -r file; do
    UF2_FILES+=("$file")
done < <(ls *.uf2 2>/dev/null | sort)

# Check if any .uf2 files were found
if [ ${#UF2_FILES[@]} -eq 0 ]; then
    echo -e "${RED}Error: No .uf2 files found in the Releases directory${NC}"
    exit 1
fi

# Main deployment loop
while true; do
    # Display available firmware files
    echo -e "${YELLOW}Available firmware files:${NC}"
    echo ""
    for i in "${!UF2_FILES[@]}"; do
        printf "  ${GREEN}%2d)${NC} %s\n" $((i+1)) "${UF2_FILES[$i]}"
    done
    echo ""

    # Prompt user for selection
    while true; do
        read -p "Select firmware to deploy (1-${#UF2_FILES[@]}) or 'q' to quit: " choice
        
        if [ "$choice" = "q" ] || [ "$choice" = "Q" ]; then
            echo "Exiting deployment tool."
            exit 0
        fi
        
        # Check if input is a valid number
        if [[ "$choice" =~ ^[0-9]+$ ]] && [ "$choice" -ge 1 ] && [ "$choice" -le "${#UF2_FILES[@]}" ]; then
            SELECTED_FILE="${UF2_FILES[$((choice-1))]}"
            break
        else
            echo -e "${RED}Invalid selection. Please enter a number between 1 and ${#UF2_FILES[@]}${NC}"
        fi
    done

    echo ""
    echo -e "${YELLOW}Selected: ${GREEN}${SELECTED_FILE}${NC}"
    echo ""

    # Deploy the firmware
    echo -e "${BLUE}🔄 Rebooting Pico into BOOTSEL mode...${NC}"
    if ! picotool reboot -f -u 2>/dev/null; then
        echo -e "${YELLOW}Note: Could not reboot device (it may already be in BOOTSEL mode or not connected)${NC}"
    fi

    sleep 2

    echo -e "${BLUE}📤 Uploading firmware: ${SELECTED_FILE}${NC}"
    if picotool load -x "${SELECTED_FILE}" -f; then
        echo ""
        echo -e "${GREEN}✅ Firmware deployed and running successfully!${NC}"
        echo ""
        echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
        echo ""
    else
        echo ""
        echo -e "${RED}❌ Failed to deploy firmware. Please check:${NC}"
        echo -e "${RED}   - Pico board is connected${NC}"
        echo -e "${RED}   - Board is in BOOTSEL mode (hold BOOTSEL button while connecting)${NC}"
        echo ""
        echo -e "${BLUE}═══════════════════════════════════════════════════════${NC}"
        echo ""
    fi
done
