#ifndef S26_FINAL_PROJECT_BRODYCRITCHLOW_ARENA_H
#define S26_FINAL_PROJECT_BRODYCRITCHLOW_ARENA_H

#include "RobotBase.h"
#include "RadarObj.h"
#include <vector>
#include <string>
#include <map>

struct Cell {
    char obstacle;
    RobotBase* robot;
    bool has_dead_robot;

    Cell() : obstacle('.'), robot(nullptr), has_dead_robot(false) {}
};

class Arena {
private:
    int m_height;
    int m_width;
    int m_max_rounds;
    double m_sleep_interval;
    bool m_game_state_live;
    int m_num_flamethrowers;
    int m_num_pits;
    int m_num_mounds;

    std::vector<std::vector<Cell>> m_grid;
    std::vector<RobotBase*> m_robots;
    std::map<RobotBase*, char> m_robot_chars;
    std::vector<void*> m_robot_handles;

    int m_current_round;

    void load_config(const std::string& config_file);
    void place_obstacles();
    void load_robots();
    RobotBase* load_single_robot(const std::string& robot_file, void*& handle);
    void place_robots();
    bool is_cell_empty(int row, int col) const;
    void print_arena() const;
    bool check_winner(RobotBase*& winner);
    std::vector<RadarObj> perform_radar_scan(RobotBase* robot, int direction);
    void handle_shot(RobotBase* shooter, int shot_row, int shot_col);
    void handle_movement(RobotBase* robot);
    void apply_damage(RobotBase* target, int damage);
    int calculate_damage(WeaponType weapon);
    void cleanup();

public:
    Arena(const std::string& config_file);
    ~Arena();
    void run();
};

#endif