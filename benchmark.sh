#!/usr/bin/env bash
# benchmark.sh - run RobotWarz N times and tally winners.
#
# Usage:
#   ./benchmark.sh                  # 50 runs with bench_config.txt
#   ./benchmark.sh 200              # 200 runs
#   ./benchmark.sh 200 my_cfg.txt   # 200 runs with custom config
#   ./benchmark.sh 200 my_cfg.txt 1000  # deterministic seeds 1000..1199
#
# Requires: ./RobotWarz built (`make`) and Robot_*.cpp present in the root.

set -euo pipefail

RUNS="${1:-50}"
CONFIG="${2:-bench_config.txt}"
SEED_START="${3:-}"

if [[ ! -x ./RobotWarz ]]; then
    echo "RobotWarz not built. Run 'make' first." >&2
    exit 1
fi

# Generate a benchmark config if one wasn't supplied. We want fast headless
# runs (Game_State_Live:false, Sleep_interval:0) and a round cap that's high
# enough to almost always produce a real winner.
if [[ "$CONFIG" == "bench_config.txt" && ! -f "$CONFIG" ]]; then
    cat > "$CONFIG" <<'EOF'
Arena_Size:20 20
Max_Rounds:2000
Sleep_interval:0
Game_State_Live:false
Flamethrowers:5
Pits:5
Mounds:5
EOF
    echo "Wrote default $CONFIG"
fi

if [[ -n "$SEED_START" ]]; then
    echo "Running $RUNS battles using $CONFIG (seed range $SEED_START..$((SEED_START + RUNS - 1))) ..."
else
    echo "Running $RUNS battles using $CONFIG ..."
fi
echo

# Tally counters - using a temp file because we run inside a subshell pipeline.
TALLY_FILE="$(mktemp)"
trap 'rm -f "$TALLY_FILE"' EXIT

for ((i = 1; i <= RUNS; i++)); do
    RUN_CONFIG="$CONFIG"
    if [[ -n "$SEED_START" ]]; then
        SEED=$((SEED_START + i - 1))
        RUN_CONFIG="$(mktemp)"
        cp "$CONFIG" "$RUN_CONFIG"
        printf '\nRandom_Seed:%d\n' "$SEED" >> "$RUN_CONFIG"
    fi

    # Each run: capture only the final-summary lines so we can tell winner from
    # mutual destruction from a draw.
    OUTPUT="$(./RobotWarz "$RUN_CONFIG" 2>&1 | tail -20)"
    if [[ -n "$SEED_START" ]]; then
        rm -f "$RUN_CONFIG"
    fi

    if WINNER_LINE="$(printf '%s\n' "$OUTPUT" | grep -E '^WINNER: ' || true)"; then
        :
    fi

    if [[ -n "${WINNER_LINE:-}" ]]; then
        # "WINNER: Robot_Tyler" -> "Robot_Tyler"
        WINNER="${WINNER_LINE#WINNER: }"
        echo "$WINNER" >> "$TALLY_FILE"
    elif printf '%s\n' "$OUTPUT" | grep -q 'Mutual destruction'; then
        echo "__mutual__" >> "$TALLY_FILE"
    else
        echo "__draw__" >> "$TALLY_FILE"
    fi

    # Tiny progress indicator every 10 runs.
    if (( i % 10 == 0 )); then
        printf '  ... %d / %d\n' "$i" "$RUNS"
    fi
done

echo
echo "===== RESULTS ($RUNS runs) ====="
sort "$TALLY_FILE" | uniq -c | sort -rn | while read -r count name; do
    pct=$(awk -v c="$count" -v r="$RUNS" 'BEGIN{ printf "%.1f", (c/r)*100 }')
    printf '  %5d  %5s%%  %s\n' "$count" "$pct" "$name"
done
