#!/bin/bash

results_file=$(mktemp)

for i in {1..30}; do
  output=$(./RobotWarz config_fast.txt 2>&1)
  winner=$(echo "$output" | /usr/bin/grep "Final Stats:" -A 1 | tail -1 | awk '{print $1}' | tr -d ':')
  
  if [ -z "$winner" ]; then
    winner="Draw"
  fi
  
  echo "Game $i: $winner"
  echo "$winner" >> "$results_file"
done

echo ""
echo "================================"
echo "         WIN STATISTICS         "
echo "================================"
printf "%-20s | %5s | %6s\n" "Bot Name" "Wins" "Win %"
echo "--------------------------------"

sort "$results_file" | uniq -c | while read count bot; do
  percent=$(awk "BEGIN {printf \"%.1f\", ($count/30)*100}")
  printf "%-20s | %5d | %5s%%\n" "$bot" "$count" "$percent"
done | sort -t'|' -k2 -rn

echo "================================"

rm -f "$results_file"
