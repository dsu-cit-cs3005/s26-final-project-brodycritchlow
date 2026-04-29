#include "Arena.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <chrono>
#include <thread>
#include <dlfcn.h>
#include <cstring>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

Arena::Arena(const std::string& config_file) : m_current_round(0) {
    load_config(config_file);
    m_grid.resize(m_height, std::vector<Cell>(m_width));
    place_obstacles();
    load_robots();
    place_robots();
}

Arena::~Arena() {
    cleanup();
}

void Arena::load_config(const std::string& config_file) {
    std::ifstream file(config_file);
    if (!file.is_open()) {
        std::cerr << "Failed to open config file: " << config_file << std::endl;
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string key;
        if (std::getline(iss, key, ':')) {
            std::string value;
            std::getline(iss, value);

            if (key == "Arena_Size") {
                std::istringstream val_stream(value);
                val_stream >> m_height >> m_width;
            } else if (key == "Max_Rounds") {
                m_max_rounds = std::stoi(value);
            } else if (key == "Sleep_interval") {
                m_sleep_interval = std::stod(value);
            } else if (key == "Game_State_Live") {
                m_game_state_live = (value.find("true") != std::string::npos);
            } else if (key == "Flamethrowers") {
                m_num_flamethrowers = std::stoi(value);
            } else if (key == "Pits") {
                m_num_pits = std::stoi(value);
            } else if (key == "Mounds") {
                m_num_mounds = std::stoi(value);
            }
        }
    }

    std::cout << "Arena Configuration Loaded:\n";
    std::cout << "  Size: " << m_height << "x" << m_width << "\n";
    std::cout << "  Max Rounds: " << m_max_rounds << "\n";
    std::cout << "  Sleep Interval: " << m_sleep_interval << "s\n";
    std::cout << "  Live Display: " << (m_game_state_live ? "true" : "false") << "\n";
    std::cout << "  Obstacles: " << m_num_mounds << " Mounds, "
              << m_num_pits << " Pits, " << m_num_flamethrowers << " Flamethrowers\n\n";
}

void Arena::place_obstacles() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> row_dist(0, m_height - 1);
    std::uniform_int_distribution<> col_dist(0, m_width - 1);

    auto place_obstacle = [&](char type, int count) {
        for (int i = 0; i < count; ++i) {
            int row, col;
            do {
                row = row_dist(gen);
                col = col_dist(gen);
            } while (m_grid[row][col].obstacle != '.');

            m_grid[row][col].obstacle = type;
        }
    };

    place_obstacle('M', m_num_mounds);
    place_obstacle('P', m_num_pits);
    place_obstacle('F', m_num_flamethrowers);
}

bool is_valid_robot_filename(const std::string& filename) {
    if (filename.size() <= 4 || filename.substr(filename.size() - 4) != ".cpp") {
        return false;
    }
    for (const unsigned char ch : filename) {
        if (!(std::isalnum(ch) || ch == '_' || ch == '-' || ch == '.')) {
            return false;
        }
    }
    return true;
}

RobotBase* Arena::load_single_robot(const std::string& robot_file, void*& handle) {
    handle = nullptr;

    if (!is_valid_robot_filename(robot_file)) {
        std::cerr << "Invalid robot filename: " << robot_file << std::endl;
        return nullptr;
    }

    std::string shared_lib = "lib" + robot_file.substr(0, robot_file.find(".cpp")) + ".so";
    std::string compile_cmd = "g++ -shared -fPIC -o " + shared_lib + " " +
                             robot_file + " RobotBase.o -I. -std=c++20 2>&1";

    std::cout << "Compiling " << robot_file << " to " << shared_lib << "...\n";

    int compile_result = std::system(compile_cmd.c_str());
    if (compile_result != 0) {
        std::cerr << "Failed to compile " << robot_file << std::endl;
        return nullptr;
    }

    handle = dlopen(shared_lib.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load " << shared_lib << ": " << dlerror() << std::endl;
        return nullptr;
    }

    RobotFactory create_robot = (RobotFactory)dlsym(handle, "create_robot");
    if (!create_robot) {
        std::cerr << "Failed to find create_robot in " << shared_lib << ": " << dlerror() << std::endl;
        dlclose(handle);
        return nullptr;
    }

    RobotBase* robot = create_robot();
    if (!robot) {
        std::cerr << "Failed to create robot instance from " << shared_lib << std::endl;
        dlclose(handle);
        return nullptr;
    }

    robot->set_boundaries(m_height, m_width);
    return robot;
}

void Arena::load_robots() {
    std::vector<std::string> robot_files;

    if (fs::exists("robots") && fs::is_directory("robots")) {
        for (const auto& entry : fs::directory_iterator("robots")) {
            std::string filename = entry.path().filename().string();
            if (filename.find("Robot_") == 0 && filename.substr(filename.size() - 4) == ".cpp") {
                robot_files.push_back("robots/" + filename);
            }
        }
    }

    for (const auto& entry : fs::directory_iterator(".")) {
        std::string filename = entry.path().filename().string();
        if (filename.find("Robot_") == 0 && filename.substr(filename.size() - 4) == ".cpp") {
            robot_files.push_back(filename);
        }
    }

    std::cout << "\nLoading robots...\n";
    for (const auto& robot_file : robot_files) {
        void* handle;
        RobotBase* robot = load_single_robot(robot_file, handle);
        if (robot) {
            m_robots.push_back(robot);
            m_robot_handles.push_back(handle);
            std::cout << "  Loaded: " << robot->m_name << std::endl;
        }
    }

    std::cout << "\nTotal robots loaded: " << m_robots.size() << "\n\n";
}

bool Arena::is_cell_empty(int row, int col) const {
    if (row < 0 || row >= m_height || col < 0 || col >= m_width) {
        return false;
    }
    return m_grid[row][col].obstacle == '.' &&
           m_grid[row][col].robot == nullptr &&
           !m_grid[row][col].has_dead_robot;
}

void Arena::place_robots() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> row_dist(0, m_height - 1);
    std::uniform_int_distribution<> col_dist(0, m_width - 1);

    const char robot_chars[] = {'@', '$', '#', '&', '!', '%', '^', '*', '+', '='};
    int char_idx = 0;

    for (auto* robot : m_robots) {
        int row, col;
        do {
            row = row_dist(gen);
            col = col_dist(gen);
        } while (!is_cell_empty(row, col));

        robot->move_to(row, col);
        m_grid[row][col].robot = robot;

        char robot_char = robot_chars[char_idx % 10];
        robot->m_character = robot_char;
        m_robot_chars[robot] = robot_char;
        char_idx++;
    }
}

void Arena::print_arena() const {
    std::cout << "\n     ";
    for (int col = 0; col < m_width; ++col) {
        std::cout << std::setw(3) << col;
    }
    std::cout << "\n";

    for (int row = 0; row < m_height; ++row) {
        std::cout << std::setw(2) << row << "  ";
        for (int col = 0; col < m_width; ++col) {
            const Cell& cell = m_grid[row][col];

            if (cell.robot != nullptr) {
                if (cell.robot->get_health() > 0) {
                    std::cout << " R" << m_robot_chars.at(cell.robot);
                } else {
                    std::cout << " X" << m_robot_chars.at(cell.robot);
                }
            } else if (cell.has_dead_robot) {
                std::cout << " X ";
            } else if (cell.obstacle != '.') {
                std::cout << "  " << cell.obstacle;
            } else {
                std::cout << "  .";
            }
        }
        std::cout << "\n";
    }
    std::cout << "\n";
}

bool Arena::check_winner(RobotBase*& winner) {
    std::vector<RobotBase*> alive_robots;
    for (auto* robot : m_robots) {
        if (robot->get_health() > 0) {
            alive_robots.push_back(robot);
        }
    }

    if (alive_robots.size() == 1) {
        winner = alive_robots[0];
        return true;
    }
    return false;
}

std::vector<RadarObj> Arena::perform_radar_scan(RobotBase* robot, int direction) {
    std::vector<RadarObj> results;
    int robot_row, robot_col;
    robot->get_current_location(robot_row, robot_col);

    if (direction == 0) {
        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                if (dr == 0 && dc == 0) continue;

                int scan_row = robot_row + dr;
                int scan_col = robot_col + dc;

                if (scan_row >= 0 && scan_row < m_height &&
                    scan_col >= 0 && scan_col < m_width) {
                    const Cell& cell = m_grid[scan_row][scan_col];

                    if (cell.robot != nullptr && cell.robot != robot) {
                        char type = (cell.robot->get_health() > 0) ? 'R' : 'X';
                        results.emplace_back(type, scan_row, scan_col);
                    } else if (cell.has_dead_robot) {
                        results.emplace_back('X', scan_row, scan_col);
                    } else if (cell.obstacle != '.') {
                        results.emplace_back(cell.obstacle, scan_row, scan_col);
                    }
                }
            }
        }
    } else {
        int dr = directions[direction].first;
        int dc = directions[direction].second;

        for (int distance = 1; distance < std::max(m_height, m_width); ++distance) {
            for (int offset = -1; offset <= 1; ++offset) {
                int scan_row, scan_col;

                if (dr == 0) {
                    scan_row = robot_row + offset;
                    scan_col = robot_col + dc * distance;
                } else if (dc == 0) {
                    scan_row = robot_row + dr * distance;
                    scan_col = robot_col + offset;
                } else {
                    scan_row = robot_row + dr * distance + (offset * (dc == 0 ? 1 : 0));
                    scan_col = robot_col + dc * distance + (offset * (dr == 0 ? 1 : 0));
                }

                if (scan_row >= 0 && scan_row < m_height &&
                    scan_col >= 0 && scan_col < m_width) {
                    const Cell& cell = m_grid[scan_row][scan_col];

                    if (cell.robot != nullptr && cell.robot != robot) {
                        char type = (cell.robot->get_health() > 0) ? 'R' : 'X';
                        results.emplace_back(type, scan_row, scan_col);
                    } else if (cell.has_dead_robot) {
                        results.emplace_back('X', scan_row, scan_col);
                    } else if (cell.obstacle != '.') {
                        results.emplace_back(cell.obstacle, scan_row, scan_col);
                    }
                }
            }
        }
    }

    return results;
}

int Arena::calculate_damage(WeaponType weapon) {
    std::random_device rd;
    std::mt19937 gen(rd());

    switch (weapon) {
        case railgun: {
            std::uniform_int_distribution<> dist(10, 20);
            return dist(gen);
        }
        case hammer: {
            std::uniform_int_distribution<> dist(50, 60);
            return dist(gen);
        }
        case grenade: {
            std::uniform_int_distribution<> dist(10, 40);
            return dist(gen);
        }
        case flamethrower: {
            std::uniform_int_distribution<> dist(30, 50);
            return dist(gen);
        }
        default:
            return 0;
    }
}

void Arena::apply_damage(RobotBase* target, int damage) {
    if (target->get_health() <= 0) return;

    int armor = target->get_armor();
    double reduction = armor * 0.1;
    int actual_damage = static_cast<int>(damage * (1.0 - reduction));

    target->take_damage(actual_damage);
    target->reduce_armor(1);

    std::cout << "    " << target->m_name << " takes " << actual_damage
              << " damage (reduced from " << damage << " by armor)\n";

    if (target->get_health() <= 0) {
        std::cout << "    " << target->m_name << " is destroyed!\n";

        int row, col;
        target->get_current_location(row, col);
        m_grid[row][col].has_dead_robot = true;
    }
}

void Arena::handle_shot(RobotBase* shooter, int shot_row, int shot_col) {
    int shooter_row, shooter_col;
    shooter->get_current_location(shooter_row, shooter_col);
    WeaponType weapon = shooter->get_weapon();

    std::cout << "  " << shooter->m_name << " (" << m_robot_chars[shooter]
              << ") fires " << weapon << " at (" << shot_row << "," << shot_col << ")\n";

    if (weapon == grenade) {
        shooter->decrement_grenades();

        for (int dr = -1; dr <= 1; ++dr) {
            for (int dc = -1; dc <= 1; ++dc) {
                int target_row = shot_row + dr;
                int target_col = shot_col + dc;

                if (target_row >= 0 && target_row < m_height &&
                    target_col >= 0 && target_col < m_width) {
                    RobotBase* target = m_grid[target_row][target_col].robot;
                    if (target != nullptr && target != shooter && target->get_health() > 0) {
                        int damage = calculate_damage(weapon);
                        apply_damage(target, damage);
                    }
                }
            }
        }
    } else if (weapon == flamethrower) {
        int dr = (shot_row > shooter_row) ? 1 : (shot_row < shooter_row) ? -1 : 0;
        int dc = (shot_col > shooter_col) ? 1 : (shot_col < shooter_col) ? -1 : 0;

        for (int distance = 1; distance <= 4; ++distance) {
            for (int offset = -1; offset <= 1; ++offset) {
                int target_row, target_col;

                if (dr == 0) {
                    target_row = shooter_row + offset;
                    target_col = shooter_col + dc * distance;
                } else if (dc == 0) {
                    target_row = shooter_row + dr * distance;
                    target_col = shooter_col + offset;
                } else {
                    target_row = shooter_row + dr * distance;
                    target_col = shooter_col + dc * distance;
                }

                if (target_row >= 0 && target_row < m_height &&
                    target_col >= 0 && target_col < m_width) {
                    RobotBase* target = m_grid[target_row][target_col].robot;
                    if (target != nullptr && target != shooter && target->get_health() > 0) {
                        int damage = calculate_damage(weapon);
                        apply_damage(target, damage);
                    }
                }
            }
        }
    } else if (weapon == railgun) {
        int dr = (shot_row > shooter_row) ? 1 : (shot_row < shooter_row) ? -1 : 0;
        int dc = (shot_col > shooter_col) ? 1 : (shot_col < shooter_col) ? -1 : 0;

        int current_row = shooter_row + dr;
        int current_col = shooter_col + dc;

        while (current_row >= 0 && current_row < m_height &&
               current_col >= 0 && current_col < m_width) {
            RobotBase* target = m_grid[current_row][current_col].robot;
            if (target != nullptr && target != shooter && target->get_health() > 0) {
                int damage = calculate_damage(weapon);
                apply_damage(target, damage);
            }

            current_row += dr;
            current_col += dc;
        }
    } else if (weapon == hammer) {
        if (shot_row >= 0 && shot_row < m_height &&
            shot_col >= 0 && shot_col < m_width) {
            RobotBase* target = m_grid[shot_row][shot_col].robot;
            if (target != nullptr && target != shooter && target->get_health() > 0) {
                int damage = calculate_damage(weapon);
                apply_damage(target, damage);
            }
        }
    }
}

void Arena::handle_movement(RobotBase* robot) {
    int move_direction, move_distance;
    robot->get_move_direction(move_direction, move_distance);

    if (move_direction < 1 || move_direction > 8 || move_distance <= 0) {
        std::cout << "  " << robot->m_name << " does not move\n";
        return;
    }

    int max_speed = robot->get_move_speed();
    if (move_distance > max_speed) {
        move_distance = max_speed;
    }

    int current_row, current_col;
    robot->get_current_location(current_row, current_col);

    int dr = directions[move_direction].first;
    int dc = directions[move_direction].second;

    int steps_moved = 0;
    for (int step = 1; step <= move_distance; ++step) {
        int new_row = current_row + dr * step;
        int new_col = current_col + dc * step;

        if (new_row < 0 || new_row >= m_height || new_col < 0 || new_col >= m_width) {
            break;
        }

        Cell& target_cell = m_grid[new_row][new_col];

        if (target_cell.obstacle == 'M' || target_cell.robot != nullptr || target_cell.has_dead_robot) {
            break;
        }

        if (target_cell.obstacle == 'P') {
            m_grid[current_row][current_col].robot = nullptr;
            m_grid[new_row][new_col].robot = robot;
            robot->move_to(new_row, new_col);
            robot->disable_movement();
            std::cout << "  " << robot->m_name << " fell into a PIT at (" << new_row << "," << new_col << ")!\n";
            return;
        }

        if (target_cell.obstacle == 'F') {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dist(30, 50);
            int damage = dist(gen);

            int armor = robot->get_armor();
            double reduction = armor * 0.1;
            int actual_damage = static_cast<int>(damage * (1.0 - reduction));

            robot->take_damage(actual_damage);
            robot->reduce_armor(1);

            std::cout << "  " << robot->m_name << " moves through FLAMETHROWER and takes "
                      << actual_damage << " damage\n";

            if (robot->get_health() <= 0) {
                m_grid[current_row][current_col].robot = nullptr;
                m_grid[new_row][new_col].robot = robot;
                m_grid[new_row][new_col].obstacle = '.';
                m_grid[new_row][new_col].has_dead_robot = true;
                robot->move_to(new_row, new_col);
                std::cout << "  " << robot->m_name << " is destroyed by the flamethrower!\n";
                return;
            }
        }

        steps_moved = step;
    }

    if (steps_moved > 0) {
        int final_row = current_row + dr * steps_moved;
        int final_col = current_col + dc * steps_moved;

        m_grid[current_row][current_col].robot = nullptr;
        m_grid[final_row][final_col].robot = robot;
        robot->move_to(final_row, final_col);

        std::cout << "  " << robot->m_name << " moves to (" << final_row << "," << final_col << ")\n";
    } else {
        std::cout << "  " << robot->m_name << " movement blocked\n";
    }
}

void Arena::run() {
    std::cout << "\n========================================\n";
    std::cout << "     ROBOTWARZ - BATTLE BEGINS!\n";
    std::cout << "========================================\n\n";

    for (m_current_round = 1; m_current_round <= m_max_rounds; ++m_current_round) {
        std::cout << "\n=========== ROUND " << m_current_round << " ===========\n";

        if (m_game_state_live) {
            print_arena();
        }

        RobotBase* winner = nullptr;
        if (check_winner(winner)) {
            std::cout << "\n\n╔════════════════════════════════════════╗\n";
            std::cout << "║         WINNER: " << std::left << std::setw(20) << winner->m_name << "  ║\n";
            std::cout << "╚════════════════════════════════════════╝\n\n";
            print_arena();
            std::cout << "Final Stats:\n" << winner->print_stats() << "\n";
            std::cout << "Victory achieved in round " << m_current_round << "!\n";
            return;
        }

        for (auto* robot : m_robots) {
            if (robot->get_health() <= 0) {
                std::cout << "\n" << robot->m_name << " (" << m_robot_chars[robot] << ") is out\n";
                continue;
            }

            std::cout << "\n" << robot->m_name << " (" << m_robot_chars[robot] << ") ";
            int row, col;
            robot->get_current_location(row, col);
            std::cout << "at (" << row << "," << col << ") " << robot->print_stats() << "\n";

            int radar_direction;
            robot->get_radar_direction(radar_direction);
            std::vector<RadarObj> radar_results = perform_radar_scan(robot, radar_direction);

            std::cout << "  Radar scan (direction " << radar_direction << "): ";
            if (radar_results.empty()) {
                std::cout << "nothing detected\n";
            } else {
                std::cout << radar_results.size() << " objects detected\n";
            }

            robot->process_radar_results(radar_results);

            int shot_row, shot_col;
            if (robot->get_shot_location(shot_row, shot_col)) {
                handle_shot(robot, shot_row, shot_col);
            } else {
                std::cout << "  Not firing\n";
                handle_movement(robot);
            }
        }

        if (m_game_state_live && m_sleep_interval > 0) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(static_cast<int>(m_sleep_interval * 1000))
            );
        }
    }

    std::cout << "\n\nMax rounds reached! Finding survivors...\n";
    print_arena();

    for (auto* robot : m_robots) {
        if (robot->get_health() > 0) {
            std::cout << "Survivor: " << robot->print_stats() << "\n";
        }
    }
}

void Arena::cleanup() {
    for (auto* robot : m_robots) {
        delete robot;
    }
    for (auto* handle : m_robot_handles) {
        if (handle) {
            dlclose(handle);
        }
    }
    m_robots.clear();
    m_robot_handles.clear();
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>\n";
        return 1;
    }

    try {
        Arena arena(argv[1]);
        arena.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}