#include "RobotBase.h"

class Robot_Tyler : public RobotBase
{
private:
    int m_radar_dir = 1;
    int m_last_scan_dir = 1;
    int m_locked_dir = 0;
    int m_misses = 0;
    static constexpr int kMaxLockMisses = 3;

    int m_target_row = -1;
    int m_target_col = -1;
    int m_last_seen_row = -1;
    int m_last_seen_col = -1;
    int m_chase_turns_left = 0;
    int m_no_contact_streak = 0;
    bool m_moving_down = true;
    static constexpr int kChaseTurns = 3;

    static int sign_of(int v)
    {
        return (v > 0) - (v < 0);
    }

    static int direction_from_delta(int dr, int dc)
    {
        if (dr < 0 && dc == 0) return 1;
        if (dr < 0 && dc > 0) return 2;
        if (dr == 0 && dc > 0) return 3;
        if (dr > 0 && dc > 0) return 4;
        if (dr > 0 && dc == 0) return 5;
        if (dr > 0 && dc < 0) return 6;
        if (dr == 0 && dc < 0) return 7;
        if (dr < 0 && dc < 0) return 8;
        return 0;
    }
public:
    Robot_Tyler() : RobotBase(3, 4, railgun)
    {
        m_name = "Robot_Tyler";
        m_character = 'T';
    }

    void get_radar_direction(int& radar_direction) override
    {
        if (m_locked_dir != 0) {
            radar_direction = (m_misses == 2)
                            ? ((m_locked_dir == 1) ? 8 : (m_locked_dir - 1))
                            : m_locked_dir;
            m_last_scan_dir = radar_direction;
            return;
        }

        // After losing lock, spend a short window scanning toward last seen
        // location before returning to broad sweep.
        if (m_no_contact_streak > 0 && m_no_contact_streak <= 10 && m_last_seen_row >= 0) {
            int my_row = 0;
            int my_col = 0;
            get_current_location(my_row, my_col);
            const int toward_last = direction_from_delta(sign_of(m_last_seen_row - my_row),
                                                         sign_of(m_last_seen_col - my_col));
            if (toward_last != 0) {
                radar_direction = toward_last;
                m_last_scan_dir = radar_direction;
                return;
            }
        }

        if (m_no_contact_streak >= 4 && (m_no_contact_streak % 4 == 0)) {
            radar_direction = 0;
            m_last_scan_dir = radar_direction;
            return;
        }

        radar_direction = m_radar_dir;
        m_radar_dir = (m_radar_dir % 8) + 1;
        m_last_scan_dir = radar_direction;
    }

    void process_radar_results(const std::vector<RadarObj>& radar_results) override
    {
        m_target_row = -1;
        m_target_col = -1;
        for (const auto& obj : radar_results) {
            if (obj.m_type == 'R') {
                m_target_row = obj.m_row;
                m_target_col = obj.m_col;
                break;
            }
        }

        if (m_target_row >= 0) {
            m_locked_dir = m_last_scan_dir;
            m_misses = 0;
            m_last_seen_row = m_target_row;
            m_last_seen_col = m_target_col;
            m_chase_turns_left = kChaseTurns;
            m_no_contact_streak = 0;
        } else if (m_locked_dir != 0) {
            m_misses++;
            if (m_misses >= kMaxLockMisses) {
                m_locked_dir = 0;
                m_misses = 0;
            }
            m_no_contact_streak++;
        } else {
            m_no_contact_streak++;
        }
    }

    bool get_shot_location(int& shot_row, int& shot_col) override
    {
        if (m_target_row < 0) return false;
        shot_row = m_target_row;
        shot_col = m_target_col;
        return true;
    }

    void get_move_direction(int& direction, int& distance) override
    {
        if (m_target_row >= 0) {
            direction = 0;
            distance = 0;
            return;
        }

        if (m_chase_turns_left > 0 && m_last_seen_row >= 0) {
            int my_row = 0;
            int my_col = 0;
            get_current_location(my_row, my_col);
            const int chase_dir = direction_from_delta(sign_of(m_last_seen_row - my_row),
                                                       sign_of(m_last_seen_col - my_col));
            if (chase_dir != 0) {
                direction = chase_dir;
                distance = 1;
                m_chase_turns_left--;
                return;
            }
            m_chase_turns_left = 0;
        }

        int row = 0;
        int col = 0;
        get_current_location(row, col);
        const int speed = get_move_speed();

        if (col > 0) {
            direction = 7; // left
            distance = (col < speed) ? col : speed;
            return;
        }

        if (m_moving_down) {
            if (row + speed < m_board_row_max) {
                direction = 5; // down
                const int room = m_board_row_max - row - 1;
                distance = (room < speed) ? room : speed;
            } else {
                m_moving_down = false;
                direction = 1; // up
                distance = 1;
            }
        } else {
            if (row - speed >= 0) {
                direction = 1; // up
                distance = (row < speed) ? row : speed;
            } else {
                m_moving_down = true;
                direction = 5; // down
                distance = 1;
            }
        }
    }
};

extern "C" RobotBase* create_robot()
{
    return new Robot_Tyler();
}

extern "C" const char* robot_summary()
{
    return "Lock+reacquire with wall-anchor fallback. v2.2.";
}