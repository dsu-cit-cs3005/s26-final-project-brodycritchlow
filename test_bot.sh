#!/bin/bash

# Test a robot and return win statistics
bot_name=$1
games=${2:-10}
config=${3:-config_ultrafast.txt}

results_file=$(mktemp)

for i in $(seq 1 $games); do
  output=$(./RobotWarz $config 2>&1)
  winner=$(echo "$output" | /usr/bin/grep "Final Stats:" -A 1 | tail -1 | awk '{print $1}' | tr -d ':')
  
  if [ -z "$winner" ]; then
    winner="Draw"
  fi
  
  echo "$winner" >> "$results_file"
done

# Count wins
target_wins=$(grep -c "$bot_name" "$results_file")
tyler_wins=$(grep -c "Robot_Tyler" "$results_file")
blank_wins=$(grep -c "Blank_Robot" "$results_file")
draws=$(grep -c "Draw" "$results_file")

win_pct=$(awk "BEGIN {printf \"%.1f\", ($target_wins/$games)*100}")

echo "========================================="
echo "Test Results for: $bot_name"
echo "========================================="
echo "$bot_name wins:    $target_wins/$games ($win_pct%)"
echo "Robot_Tyler wins:  $tyler_wins"
echo "Blank_Robot wins:  $blank_wins"
echo "Draws:             $draws"
echo "========================================="

rm -f "$results_file"

# Return win percentage as exit code (scaled by 10, so 35.0% = exit code 35)
exit $(printf "%.0f" "$win_pct")
