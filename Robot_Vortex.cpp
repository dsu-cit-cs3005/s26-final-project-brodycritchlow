#include "RobotBase.h"
#include <vector>
#include <cmath>
#include <algorithm>

class Robot_Vortex : public RobotBase
{
private:
    struct HazardInfo {
        int row, col;
        char type;
    };

    std::vector<HazardInfo> m_hazards;

    int m_scan_index;
    int m_last_enemy_direction;
    bool m_enemy_found_last_scan;
    bool m_taking_fire;
    int m_urgent_scan_index;

    int m_target_row;
    int m_target_col;
    bool m_should_shoot;

    int m_last_health;
    int m_patrol_step;
    bool m_reached_center;

    int m_last_seen_row;
    int m_last_seen_col;
    int m_same_pos_count;
    bool m_hunting;
    int m_hunt_row;
    int m_hunt_col;
    int m_turns_without_damage;
    int m_last_enemy_health;

    static constexpr int SCAN_ORDER[] = {7, 3, 1, 5, 8, 6, 2, 4};

    bool is_hazard(int row, int col, char type) {
        return std::any_of(m_hazards.begin(), m_hazards.end(),
            [row, col, type](const HazardInfo& h) {
                return h.row == row && h.col == col && h.type == type;
            });
    }

    bool is_any_hazard(int row, int col) {
        return std::any_of(m_hazards.begin(), m_hazards.end(),
            [row, col](const HazardInfo& h) {
                return h.row == row && h.col == col;
            });
    }

    void remember_hazard(char type, int row, int col) {
        if (type == 'M' || type == 'P' || type == 'F') {
            if (!is_any_hazard(row, col)) {
                m_hazards.push_back({row, col, type});
            }
        }
    }

    int direction_toward(int from_row, int from_col, int to_row, int to_col) {
        int dr = to_row - from_row;
        int dc = to_col - from_col;

        if (dr == 0 && dc == 0) return 0;

        int sign_r = (dr > 0) ? 1 : (dr < 0) ? -1 : 0;
        int sign_c = (dc > 0) ? 1 : (dc < 0) ? -1 : 0;

        for (int d = 1; d <= 8; ++d) {
            if (directions[d].first == sign_r && directions[d].second == sign_c) {
                return d;
            }
        }
        return 1;
    }

    bool is_move_safe(int current_row, int current_col, int direction) {
        if (direction < 1 || direction > 8) return false;

        int dr = directions[direction].first;
        int dc = directions[direction].second;
        int nr = current_row + dr;
        int nc = current_col + dc;

        if (nr < 0 || nr >= m_board_row_max || nc < 0 || nc >= m_board_col_max) {
            return false;
        }

        return !is_hazard(nr, nc, 'P') && !is_hazard(nr, nc, 'F');
    }

    int safe_direction_toward(int current_row, int current_col, int target_row, int target_col) {
        int best = direction_toward(current_row, current_col, target_row, target_col);
        if (best == 0) return 0;

        if (is_move_safe(current_row, current_col, best)) return best;

        int try_offsets[] = {1, -1, 2, -2, 3, -3};
        for (int offset : try_offsets) {
            int alt = best + offset;
            if (alt < 1) alt += 8;
            if (alt > 8) alt -= 8;
            if (is_move_safe(current_row, current_col, alt)) return alt;
        }

        return best;
    }

public:
    Robot_Vortex() : RobotBase(2, 5, grenade) {
        m_name = "Vortex";
        m_character = 'V';

        m_scan_index = 0;
        m_last_enemy_direction = -1;
        m_enemy_found_last_scan = false;
        m_taking_fire = false;
        m_urgent_scan_index = 0;

        m_target_row = -1;
        m_target_col = -1;
        m_should_shoot = false;

        m_last_health = 100;
        m_patrol_step = 0;
        m_reached_center = false;

        m_last_seen_row = -1;
        m_last_seen_col = -1;
        m_same_pos_count = 0;
        m_hunting = false;
        m_hunt_row = -1;
        m_hunt_col = -1;
        m_turns_without_damage = 0;
        m_last_enemy_health = -1;
    }

    void get_radar_direction(int& radar_direction) override {
        if (m_enemy_found_last_scan && m_last_enemy_direction >= 1) {
            radar_direction = m_last_enemy_direction;
            return;
        }

        if (m_taking_fire) {
            radar_direction = SCAN_ORDER[m_urgent_scan_index];
            m_urgent_scan_index++;
            if (m_urgent_scan_index >= 8) {
                m_urgent_scan_index = 0;
                m_taking_fire = false;
            }
            return;
        }

        radar_direction = SCAN_ORDER[m_scan_index];
        m_scan_index++;
        if (m_scan_index >= 8) {
            m_scan_index = 0;
        }
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override {
        m_should_shoot = false;
        m_enemy_found_last_scan = false;

        if (get_health() < m_last_health) {
            if (!m_taking_fire) {
                m_taking_fire = true;
                m_urgent_scan_index = 0;
            }
        }
        m_last_health = get_health();

        int current_row, current_col;
        get_current_location(current_row, current_col);

        double closest_dist = 99999;
        bool found_enemy = false;

        for (const auto& obj : radar_results) {
            remember_hazard(obj.m_type, obj.m_row, obj.m_col);

            if (obj.m_type == 'R') {
                m_enemy_found_last_scan = true;
                m_taking_fire = false;
                m_last_enemy_direction = direction_toward(current_row, current_col,
                                                          obj.m_row, obj.m_col);

                double dist = std::abs(obj.m_row - current_row) +
                              std::abs(obj.m_col - current_col);

                if (dist < closest_dist) {
                    closest_dist = dist;
                    m_target_row = obj.m_row;
                    m_target_col = obj.m_col;
                    m_should_shoot = true;
                }
                found_enemy = true;
            }
        }

        if (m_enemy_found_last_scan) {
            m_scan_index = 0;

            if (m_target_row == m_last_seen_row && m_target_col == m_last_seen_col) {
                m_same_pos_count++;
            } else {
                m_same_pos_count = 1;
            }
            m_last_seen_row = m_target_row;
            m_last_seen_col = m_target_col;

            if (m_same_pos_count >= 3) {
                m_hunting = true;
                m_hunt_row = m_target_row;
                m_hunt_col = m_target_col;
            }

            // Track if we're doing damage
            if (found_enemy) {
                m_turns_without_damage++;
                if (m_turns_without_damage >= 5) {
                    m_hunting = true;
                    m_hunt_row = m_target_row;
                    m_hunt_col = m_target_col;
                }
            }
        } else if (!m_hunting) {
            m_same_pos_count = 0;
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        if (m_should_shoot) {
            int current_row, current_col;
            get_current_location(current_row, current_col);
            
            // Hammer requires adjacency (≤1 cell in both row and col)
            int row_dist = std::abs(m_target_row - current_row);
            int col_dist = std::abs(m_target_col - current_col);
            
            if (row_dist <= 1 && col_dist <= 1) {
                shot_row = m_target_row;
                shot_col = m_target_col;
                m_turns_without_damage = 0;  // Reset counter when we shoot
                return true;
            }
        }
        return false;
    }

    void get_move_direction(int& move_direction, int& move_distance) override {
        int current_row, current_col;
        get_current_location(current_row, current_col);
        int speed = get_move_speed();

        if (speed == 0) {
            move_direction = 1;
            move_distance = 0;
            return;
        }

        int center_row = m_board_row_max / 2;
        int center_col = m_board_col_max / 2;
        bool near_center = std::abs(current_row - center_row) <= 2 &&
                           std::abs(current_col - center_col) <= 2;

        if (!near_center && !m_reached_center) {
            int dir = safe_direction_toward(current_row, current_col, center_row, center_col);
            if (dir > 0) {
                move_direction = dir;
                move_distance = speed;
                return;
            }
        }

        m_reached_center = true;

        if (m_hunting && m_hunt_row >= 0) {
            if (std::abs(current_row - m_hunt_row) <= 1 &&
                std::abs(current_col - m_hunt_col) <= 1) {
                m_hunting = false;
            } else {
                int dir = safe_direction_toward(current_row, current_col, m_hunt_row, m_hunt_col);
                if (dir > 0) {
                    move_direction = dir;
                    move_distance = speed;
                    return;
                }
            }
        }

        static constexpr int PATROL[] = {3, 5, 7, 1};
        int dir = PATROL[m_patrol_step % 4];

        if (is_move_safe(current_row, current_col, dir)) {
            move_direction = dir;
            move_distance = 1;
        } else {
            m_patrol_step++;
            dir = PATROL[m_patrol_step % 4];
            move_direction = dir;
            move_distance = 1;
        }
        m_patrol_step++;
    }
};

extern "C" RobotBase* create_robot() {
    return new Robot_Vortex();
}

extern "C" const char* robot_summary() {
    return "Center-camping hammer tank, max armor.";
}