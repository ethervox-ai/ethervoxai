#!/usr/bin/env bash
# Download phonemizer dictionaries for English, Chinese, and German
# Part of EthervoxAI custom phonemizer (GPL-free TTS)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
DATA_DIR="${PROJECT_ROOT}/src/tts/phonemizer/data"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}================================================${NC}"
echo -e "${BLUE}   EthervoxAI Phonemizer Dictionary Downloader${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Create data directory if it doesn't exist
mkdir -p "${DATA_DIR}"

# Function to download file with progress
download_file() {
    local url="$1"
    local output="$2"
    local description="$3"
    
    echo -e "${YELLOW}Downloading: ${description}${NC}"
    echo -e "  URL: ${url}"
    echo -e "  Output: ${output}"
    
    if [ -f "${output}" ]; then
        echo -e "${GREEN}  ✓ Already exists, skipping${NC}"
        return 0
    fi
    
    if curl -L --progress-bar -o "${output}" "${url}"; then
        echo -e "${GREEN}  ✓ Download complete${NC}"
        return 0
    else
        echo -e "${RED}  ✗ Download failed${NC}"
        return 1
    fi
}

# Function to verify file size
verify_file() {
    local file="$1"
    local min_size="$2"
    local description="$3"
    
    if [ ! -f "${file}" ]; then
        echo -e "${RED}  ✗ File not found: ${file}${NC}"
        return 1
    fi
    
    local size=$(stat -f%z "${file}" 2>/dev/null || stat -c%s "${file}" 2>/dev/null)
    if [ "${size}" -lt "${min_size}" ]; then
        echo -e "${RED}  ✗ File too small (${size} bytes, expected >${min_size}): ${description}${NC}"
        rm -f "${file}"
        return 1
    fi
    
    echo -e "${GREEN}  ✓ Verified: ${description} (${size} bytes)${NC}"
    return 0
}

echo -e "${BLUE}[1/3] CMU Pronouncing Dictionary (English)${NC}"
echo -e "  License: Public Domain"
echo -e "  Entries: ~135,000 English words"
echo ""

CMU_URL="https://raw.githubusercontent.com/cmusphinx/cmudict/master/cmudict.dict"
CMU_FILE="${DATA_DIR}/cmudict-0.7b.txt"

download_file "${CMU_URL}" "${CMU_FILE}" "CMU Pronouncing Dictionary"
verify_file "${CMU_FILE}" 3000000 "cmudict-0.7b.txt"
echo ""

echo -e "${BLUE}[2/3] Unicode Unihan Database (Chinese)${NC}"
echo -e "  License: Unicode License v3 (permissive, like MIT)"
echo -e "  Entries: ~44,000 Chinese characters with Pinyin"
echo ""

UNIHAN_URL="https://www.unicode.org/Public/UCD/latest/ucd/Unihan.zip"
UNIHAN_ZIP="${DATA_DIR}/Unihan.zip"
UNIHAN_FILE="${DATA_DIR}/Unihan_Readings.txt"

if [ -f "${UNIHAN_FILE}" ]; then
    echo -e "${GREEN}  ✓ Unihan database already exists, skipping${NC}"
else
    download_file "${UNIHAN_URL}" "${UNIHAN_ZIP}" "Unihan database (compressed)"
    
    if [ -f "${UNIHAN_ZIP}" ]; then
        echo -e "${YELLOW}  Extracting Unihan_Readings.txt...${NC}"
        unzip -o -j "${UNIHAN_ZIP}" "Unihan_Readings.txt" -d "${DATA_DIR}"
        echo -e "${GREEN}  ✓ Extraction complete${NC}"
        rm -f "${UNIHAN_ZIP}"
    fi
fi

verify_file "${UNIHAN_FILE}" 8000000 "Unihan_Readings.txt"
echo ""

echo -e "${BLUE}[3/3] German Pronunciation${NC}"
echo -e "  Method: Rule-based (no dictionary required)"
echo -e "  Status: Complete"
echo ""
echo -e "${GREEN}  ✓ German uses algorithmic G2P rules - no download needed${NC}"
echo ""

echo -e "${BLUE}================================================${NC}"
echo -e "${GREEN}✓ Dictionary downloads complete!${NC}"
echo -e "${BLUE}================================================${NC}"
echo ""

# Print summary
echo -e "${BLUE}Summary:${NC}"
if [ -f "${CMU_FILE}" ]; then
    CMU_SIZE=$(stat -f%z "${CMU_FILE}" 2>/dev/null || stat -c%s "${CMU_FILE}" 2>/dev/null)
    echo -e "  ${GREEN}✓${NC} English: ${CMU_FILE} ($(numfmt --to=iec-i --suffix=B ${CMU_SIZE} 2>/dev/null || echo "${CMU_SIZE} bytes"))"
else
    echo -e "  ${RED}✗${NC} English: Failed to download"
fi

if [ -f "${UNIHAN_FILE}" ]; then
    UNIHAN_SIZE=$(stat -f%z "${UNIHAN_FILE}" 2>/dev/null || stat -c%s "${UNIHAN_FILE}" 2>/dev/null)
    echo -e "  ${GREEN}✓${NC} Chinese: ${UNIHAN_FILE} ($(numfmt --to=iec-i --suffix=B ${UNIHAN_SIZE} 2>/dev/null || echo "${UNIHAN_SIZE} bytes"))"
else
    echo -e "  ${RED}✗${NC} Chinese: Failed to download"
fi

echo -e "  ${GREEN}✓${NC} German: Rule-based (no dictionary needed)"
echo ""
echo -e "${BLUE}Total dictionary data: ~12 MB${NC}"
echo ""
echo -e "${GREEN}Phonemizer is ready to use!${NC}"
echo -e "Build the project with: ${YELLOW}make clean && make${NC}"
echo ""
