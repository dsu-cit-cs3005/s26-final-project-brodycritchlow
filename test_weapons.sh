#!/bin/bash

weapons=("hammer" "railgun" "flamethrower" "grenade")
games_per_weapon=10

echo "Testing Robot_Vortex with different weapons..."
echo ""

for weapon in "${weapons[@]}"; do
  echo "========================================"
  echo "Testing with: $weapon"
  echo "========================================"
  
  # Modify the weapon in Robot_Vortex.cpp
  if [ "$weapon" = "hammer" ]; then
    sed -i.bak 's/Robot_Vortex() : RobotBase(2, 5, [a-z]*)/Robot_Vortex() : RobotBase(2, 5, hammer)/' Robot_Vortex.cpp
  elif [ "$weapon" = "railgun" ]; then
    sed -i.bak 's/Robot_Vortex() : RobotBase(2, 5, [a-z]*)/Robot_Vortex() : RobotBase(2, 5, railgun)/' Robot_Vortex.cpp
  elif [ "$weapon" = "flamethrower" ]; then
    sed -i.bak 's/Robot_Vortex() : RobotBase(2, 5, [a-z]*)/Robot_Vortex() : RobotBase(2, 5, flamethrower)/' Robot_Vortex.cpp
  elif [ "$weapon" = "grenade" ]; then
    sed -i.bak 's/Robot_Vortex() : RobotBase(2, 5, [a-z]*)/Robot_Vortex() : RobotBase(2, 5, grenade)/' Robot_Vortex.cpp
  fi
  
  # Recompile
  g++ -std=c++20 -fPIC -shared -o Robot_Vortex.so Robot_Vortex.cpp RobotBase.o > /dev/null 2>&1
  
  if [ $? -ne 0 ]; then
    echo "Compilation failed for $weapon"
    continue
  fi
  
  results_file=$(mktemp)
  
  for i in $(seq 1 $games_per_weapon); do
    output=$(./RobotWarz config_fast.txt 2>&1)
    winner=$(echo "$output" | /usr/bin/grep "Final Stats:" -A 1 | tail -1 | awk '{print $1}' | tr -d ':')
    
    if [ -z "$winner" ]; then
      winner="Draw"
    fi
    
    echo "$winner" >> "$results_file"
  done
  
  # Count wins for this weapon
  vortex_wins=$(grep -c "Vortex" "$results_file")
  blank_wins=$(grep -c "Blank_Robot" "$results_file")
  draws=$(grep -c "Draw" "$results_file")
  
  vortex_pct=$(awk "BEGIN {printf \"%.1f\", ($vortex_wins/$games_per_weapon)*100}")
  
  echo "Vortex wins: $vortex_wins ($vortex_pct%)"
  echo "Blank_Robot wins: $blank_wins"
  echo "Draws: $draws"
  echo ""
  
  rm -f "$results_file"
done

# Restore original
rm -f Robot_Vortex.cpp.bak

echo "========================================"
echo "Testing complete!"
echo "========================================"
