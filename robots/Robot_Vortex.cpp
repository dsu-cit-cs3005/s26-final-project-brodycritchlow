#include "../RobotBase.h"
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
    int m_target_row;
    int m_target_col;
    bool m_should_shoot;
    int m_last_enemy_direction;
    bool m_enemy_found_last_scan;
    bool m_taking_fire;
    int m_urgent_scan_index;
    int m_last_health;
    int m_last_seen_row;
    int m_last_seen_col;
    int m_same_pos_count;
    bool m_hunting;
    int m_hunt_row;
    int m_hunt_col;
    int m_circle_step;
    int m_spiral_radius;
    
    static constexpr int SCAN_ORDER[] = {7, 3, 1, 5, 8, 6, 2, 4};
    
    bool is_hazard(int row, int col, char type) {
        for (const auto& h : m_hazards) {
            if (h.row == row && h.col == col && h.type == type) {
                return true;
            }
        }
        return false;
    }
    
    bool is_any_hazard(int row, int col) {
        for (const auto& h : m_hazards) {
            if (h.row == row && h.col == col) {
                return true;
            }
        }
        return false;
    }
    
    void remember_hazard(char type, int row, int col) {
        if (type == 'M' || type == 'P' || type == 'F') {
            if (!is_any_hazard(row, col)) {
                m_hazards.push_back({row, col, type});
            }
        }
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
        int dr = target_row - current_row;
        int dc = target_col - current_col;
        
        if (dr == 0 && dc == 0) return 0;
        
        int sign_r = (dr > 0) ? 1 : (dr < 0) ? -1 : 0;
        int sign_c = (dc > 0) ? 1 : (dc < 0) ? -1 : 0;
        
        int best = 0;
        for (int d = 1; d <= 8; ++d) {
            if (directions[d].first == sign_r && directions[d].second == sign_c) {
                best = d;
                break;
            }
        }
        
        if (best == 0) return 1;
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
    Robot_Vortex() : RobotBase(2, 5, hammer) {
        m_name = "Vortex";
        m_character = 'V';
        m_scan_index = 0;
        m_target_row = -1;
        m_target_col = -1;
        m_should_shoot = false;
        m_last_enemy_direction = -1;
        m_enemy_found_last_scan = false;
        m_taking_fire = false;
        m_urgent_scan_index = 0;
        m_last_health = 100;
        m_last_seen_row = -1;
        m_last_seen_col = -1;
        m_same_pos_count = 0;
        m_hunting = false;
        m_hunt_row = -1;
        m_hunt_col = -1;
        m_circle_step = 0;
        m_spiral_radius = 1;
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
        m_scan_index = (m_scan_index + 1) % 8;
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
        
        for (const auto& obj : radar_results) {
            remember_hazard(obj.m_type, obj.m_row, obj.m_col);
            
            if (obj.m_type == 'R') {
                m_enemy_found_last_scan = true;
                m_taking_fire = false;
                
                int dr = obj.m_row - current_row;
                int dc = obj.m_col - current_col;
                int sign_r = (dr > 0) ? 1 : (dr < 0) ? -1 : 0;
                int sign_c = (dc > 0) ? 1 : (dc < 0) ? -1 : 0;
                
                for (int d = 1; d <= 8; ++d) {
                    if (directions[d].first == sign_r && directions[d].second == sign_c) {
                        m_last_enemy_direction = d;
                        break;
                    }
                }
                
                double dist = std::abs(obj.m_row - current_row) + 
                              std::abs(obj.m_col - current_col);
                
                if (dist < closest_dist) {
                    closest_dist = dist;
                    m_target_row = obj.m_row;
                    m_target_col = obj.m_col;
                    m_should_shoot = true;
                }
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
        } else if (!m_hunting) {
            m_same_pos_count = 0;
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override {
        if (m_should_shoot) {
            shot_row = m_target_row;
            shot_col = m_target_col;
            return true;
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
        int dist_from_center = std::abs(current_row - center_row) + std::abs(current_col - center_col);
        bool near_center = dist_from_center <= 3;

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

        if (!near_center) {
            int dir = safe_direction_toward(current_row, current_col, center_row, center_col);
            if (dir > 0) {
                move_direction = dir;
                move_distance = speed;
                return;
            }
        }

        int dr = current_row - center_row;
        int dc = current_col - center_col;

        int tangent_r = -dc;
        int tangent_c = dr;

        int sign_r = (tangent_r > 0) ? 1 : (tangent_r < 0) ? -1 : 0;
        int sign_c = (tangent_c > 0) ? 1 : (tangent_c < 0) ? -1 : 0;
        
        int best_dir = 1;
        for (int d = 1; d <= 8; ++d) {
            if (directions[d].first == sign_r && directions[d].second == sign_c) {
                best_dir = d;
                break;
            }
        }
        
        if (is_move_safe(current_row, current_col, best_dir)) {
            move_direction = best_dir;
            move_distance = 1;
        } else {
            for (int offset = 1; offset <= 3; ++offset) {
                int alt = best_dir + offset;
                if (alt > 8) alt -= 8;
                if (is_move_safe(current_row, current_col, alt)) {
                    move_direction = alt;
                    move_distance = 1;
                    return;
                }
                alt = best_dir - offset;
                if (alt < 1) alt += 8;
                if (is_move_safe(current_row, current_col, alt)) {
                    move_direction = alt;
                    move_distance = 1;
                    return;
                }
            }
            move_direction = best_dir;
            move_distance = 1;
        }
    }
};

extern "C" RobotBase* create_robot() {
    return new Robot_Vortex();
}

extern "C" const char* robot_summary() {
    return "Feature 8: True vortex tangential motion";
}
