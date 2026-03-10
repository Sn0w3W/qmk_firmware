#!/usr/bin/env bash
# Post-build safety check for SN32F248BF (8KB RAM, 64KB flash)
# Usage: ./util/check_firmware_safety.sh .build/keychron_k3_rgb_v2_optical_ansi_via

set -uo pipefail

ELF="${1:?Usage: $0 <path-to-elf-without-extension>}.elf"
SU_FILES=(.build/*.su)

if [ ! -f "$ELF" ]; then
    echo "ERROR: $ELF not found" >&2
    exit 1
fi

RAM_TOTAL=8192
FLASH_TOTAL=65536
FLASH_FLAG=4          # .flag section at 0xFFFC
FLASH_USABLE=$((FLASH_TOTAL - FLASH_FLAG))

MAIN_STACK_WARN=600   # main() stack frame warning threshold
PSTACK_SIZE=1536
MSTACK_SIZE=768
HEAP_MIN_WARN=100     # warn if heap < this
FUNC_STACK_WARN=150   # warn for any function > this

RED='\033[0;31m'
YEL='\033[0;33m'
GRN='\033[0;32m'
RST='\033[0m'

errors=0
warnings=0

warn()  { echo -e "${YEL}  WARNING: $*${RST}"; ((warnings++)); }
fail()  { echo -e "${RED}  FAIL:    $*${RST}"; ((errors++)); }
ok()    { echo -e "${GRN}  OK:      $*${RST}"; }

echo ""
echo "=== Firmware Safety Check ==="
echo ""

# --- Flash size ---
FLASH_SIZE=$(arm-none-eabi-size -B "$ELF" | tail -1 | awk '{print $1 + $2}')
FLASH_PCT=$((FLASH_SIZE * 100 / FLASH_USABLE))
if [ "$FLASH_SIZE" -gt "$FLASH_USABLE" ]; then
    fail "Flash overflow: ${FLASH_SIZE} / ${FLASH_USABLE} bytes (${FLASH_PCT}%)"
elif [ "$FLASH_PCT" -gt 90 ]; then
    warn "Flash ${FLASH_PCT}% full: ${FLASH_SIZE} / ${FLASH_USABLE} bytes"
else
    ok   "Flash ${FLASH_PCT}% full: ${FLASH_SIZE} / ${FLASH_USABLE} bytes"
fi

# --- RAM sections ---
eval "$(arm-none-eabi-size -A "$ELF" | awk '
    /^\.mstack/ {print "SEC_MSTACK=" $2}
    /^\.pstack/ {print "SEC_PSTACK=" $2}
    /^\.data /  {print "SEC_DATA="   $2}
    /^\.bss /   {print "SEC_BSS="    $2}
    /^\.heap/   {print "SEC_HEAP="   $2}
')"

RAM_STATIC=$((SEC_MSTACK + SEC_PSTACK + SEC_DATA + SEC_BSS))
RAM_FREE=$SEC_HEAP
RAM_STATIC_PCT=$((RAM_STATIC * 100 / RAM_TOTAL))

echo ""
echo "  RAM layout (${RAM_TOTAL} bytes total):"
echo "    .mstack (ISR stack) : ${SEC_MSTACK}"
echo "    .pstack (main stack): ${SEC_PSTACK}"
echo "    .data               : ${SEC_DATA}"
echo "    .bss                : ${SEC_BSS}"
echo "    .heap (free)        : ${SEC_HEAP}"
echo ""

if [ "$RAM_FREE" -eq 0 ]; then
    fail "RAM 100% static — zero heap, any change may crash"
elif [ "$RAM_FREE" -lt "$HEAP_MIN_WARN" ]; then
    warn "Heap only ${RAM_FREE} bytes — static ${RAM_STATIC_PCT}% of RAM"
else
    ok   "Heap ${RAM_FREE} bytes free — static ${RAM_STATIC_PCT}% of RAM"
fi

# --- Stack usage analysis (requires -fstack-usage build) ---
if [ ${#SU_FILES[@]} -gt 0 ] && [ -f "${SU_FILES[0]}" ]; then
    echo ""
    echo "  Stack usage analysis (.su files found):"

    # Check main() stack frame
    MAIN_STACK=$(cat .build/*.su 2>/dev/null | awk -F'\t' '/:main\t/ {print $2}' | sort -rn | head -1)
    if [ -n "$MAIN_STACK" ] && [ "$MAIN_STACK" -gt 0 ]; then
        MAIN_PCT=$((MAIN_STACK * 100 / PSTACK_SIZE))
        if [ "$MAIN_STACK" -gt "$MAIN_STACK_WARN" ]; then
            fail "main() uses ${MAIN_STACK} bytes (${MAIN_PCT}% of pstack=${PSTACK_SIZE})"
        else
            ok   "main() uses ${MAIN_STACK} bytes (${MAIN_PCT}% of pstack=${PSTACK_SIZE})"
        fi
    fi

    # Find dangerously large functions
    LARGE_FUNCS=$(cat .build/*.su 2>/dev/null | awk -F'\t' -v limit="$FUNC_STACK_WARN" '$2+0 > limit {print $2, $1}' | sort -rn || true)
    if [ -n "$LARGE_FUNCS" ]; then
        warn "Functions with stack > ${FUNC_STACK_WARN} bytes:"
        while read -r size name; do
            echo -e "${YEL}           ${size}B  ${name}${RST}"
        done <<< "$LARGE_FUNCS"
    fi

    # ISR stack check — rgb_callback is the heaviest ISR
    ISR_STACK=$(cat .build/*.su 2>/dev/null | awk -F'\t' '/rgb_callback/ {print $2}' | sort -rn | head -1)
    if [ -n "$ISR_STACK" ] && [ "$ISR_STACK" -gt 0 ]; then
        ISR_PCT=$((ISR_STACK * 100 / MSTACK_SIZE))
        if [ "$ISR_PCT" -gt 50 ]; then
            warn "rgb_callback ISR uses ${ISR_STACK} bytes (${ISR_PCT}% of mstack=${MSTACK_SIZE})"
        else
            ok   "rgb_callback ISR uses ${ISR_STACK} bytes (${ISR_PCT}% of mstack=${MSTACK_SIZE})"
        fi
    fi
else
    echo ""
    echo "  (no .su files — rebuild with EXTRAFLAGS=\"-fstack-usage\" for stack analysis)"
fi

# --- Summary ---
echo ""
if [ "$errors" -gt 0 ]; then
    echo -e "${RED}=== UNSAFE: ${errors} error(s), ${warnings} warning(s) — DO NOT FLASH ===${RST}"
    exit 1
elif [ "$warnings" -gt 0 ]; then
    echo -e "${YEL}=== RISKY: ${warnings} warning(s) — flash at your own risk ===${RST}"
    exit 0
else
    echo -e "${GRN}=== SAFE: all checks passed ===${RST}"
    exit 0
fi
