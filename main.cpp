#include <iostream>
#include <string>
#include <vector>
#include <sstream> // This header provides std::stringstream
#include "filesystem.h"

std::vector<std::string> parse_command(const std::string& command_line) {
    std::vector<std::string> args;
    std::stringstream ss(command_line);
    std::string arg;
    while (ss >> arg) {
        args.push_back(arg);
    }
    return args;
}

int main(int argc, char *argv[]){
    std::string command, arg;
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <filesystem name>" << std::endl;
        return 1;
    }

    filesystem fs(argv[1]);
    directory_item *cur_dir = &fs.root_folder[0];
    while (fs.corrupted) {
        fs.check();
        std::cout << fs.current_file_path(cur_dir) + ">";
        std::getline(std::cin, command);

        if (command.empty()) continue;

        auto args = parse_command(command);
        if (args.empty()) continue;
        std::string cmd = args[0];

        if (cmd == "format") {
            if (args.size() != 2) {
                std::cerr << "Usage: format <size>" << std::endl;
                continue;
            }
            if (fs.format_fs(args[1])){
                break;
            }

            std::cout << "Cannot create file system\n";

        }
    }


    while (true){
        std::cout << fs.current_file_path(cur_dir) + ">";
        std::getline(std::cin, command);

        if (command.empty()) continue;

        auto args = parse_command(command);
        if (args.empty()) continue;

        std::string cmd = args[0];

        if (cmd == "exit") {
            std::cout << "Exiting program." << std::endl;
            break;
        }
        try {
            if (cmd == "format") {
                if (args.size() != 2) {
                    std::cerr << "Usage: format <size>" << std::endl;
                    continue;
                }
                if (fs.format_fs(args[1])){
                    std::cout << "OK\n";
                }
                else{
                    std::cout << "Cannot create file systen\n";
                }
            }
            else if (cmd == "mkdir") {
                if (args.size() != 2) {
                    std::cerr << "Usage: mkdir <directory_name>" << std::endl;
                    continue;
                }
                if (fs.make_directory(cur_dir, args[1])) {
                    std::cout << "Directory created successfully\n";
                } else {
                    std::cout << "Failed to create directory\n";
                }
            }
            else if (cmd == "ls") {
                if (args.size() == 1) {
                    fs.list_directory(cur_dir);
                } else {
                    directory_item* target_dir = fs.find_directory_by_path(cur_dir, args[1]);
                    if (target_dir) {
                        fs.list_directory(target_dir);
                    } else {
                        std::cerr << "Directory not found\n";
                    }
                }
            }
            else if (cmd == "cd") {
                if (args.size() != 2) {
                    std::cerr << "Usage: cd <directory_path>" << std::endl;
                    continue;
                }
                directory_item* new_dir = fs.change_directory(cur_dir, args[1]);
                if (new_dir != cur_dir) {
                    cur_dir = new_dir;
                }
            }
            else if (cmd == "rmdir") {
                if (args.size() != 2) {
                    std::cerr << "Usage: rmdir <directory_path>" << std::endl;
                    continue;
                }
                fs.remove_directory(cur_dir, args[1]);
            }
            else if (cmd == "pwd") {
                std::cout << fs.print_working_directory(cur_dir) << std::endl;
            }
            else if (cmd == "rm") {
                if (args.size() != 2) {
                    std::cerr << "Usage: rm <file_path>" << std::endl;
                    continue;
                }
                fs.remove_file(cur_dir, args[1]);
            }
            else if (cmd == "incp") {
                if (args.size() != 3) {
                    std::cerr << "Usage: incp <source> <destination>" << std::endl;
                    continue;
                }
                fs.copy_file_in(args[1], args[2]);
            }
            else if (cmd == "outcp") {
                if (args.size() != 3) {
                    std::cerr << "Usage: outcp <source> <destination>" << std::endl;
                    continue;
                }
                fs.copy_file_out(args[1], args[2]);
            }
            else if (cmd == "info") {
                if (args.size() != 2) {
                    std::cerr << "Usage: info <path>" << std::endl;
                    continue;
                }
                std::cout << fs.get_file_info(args[1]) << std::endl;
            }
            else if (cmd == "cat") {
                if (args.size() != 2) {
                    std::cerr << "Usage: cat <file>" << std::endl;
                    continue;
                }
                fs.cat_file(args[1]);
            }
            else if (cmd == "cp") {
                if (args.size() != 3) {
                    std::cerr << "Usage: cp <source> <destination>" << std::endl;
                    continue;
                }
                fs.copy_file(args[1], args[2]);
            }
            else if (cmd == "mv") {
                if (args.size() != 3) {
                    std::cerr << "Usage: mv <source> <destination>" << std::endl;
                    continue;
                }
                fs.move_file(args[1], args[2]);
            }
            else if (cmd == "load") {
                if (args.size() != 2) {
                    std::cerr << "Usage: load <file_path>" << std::endl;
                    continue;
                }
                fs.load(cur_dir, args[1]);
            }
            else if (cmd == "bug") {
                if (args.size() != 2) {
                    std::cerr << "Usage: bug <file_path>" << std::endl;
                    continue;
                }
                fs.bug(args[1]);
            }
            else if (cmd == "check") {
                if (args.size() != 1) {
                    std::cerr << "Usage: check" << std::endl;
                    continue;
                }
                fs.check();
            }
            else {
                std::cerr << "Command not found" << std::endl;
            }
        }

        catch (const std::runtime_error& e) {
            std::cerr << e.what() << std::endl;
        }

    }

    return 0;
}
