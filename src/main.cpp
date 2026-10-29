#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <unistd.h>
#include <filesystem>
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while (true){
    const char* path = std::getenv("PATH");
    std::cout << "$ ";
    std:: string input;
    std::getline(std::cin, input);
    std::unordered_set<std::string> commands = {"echo", "exit", "type"};
    std::string command = input.substr(0, input.find(' '));

    if (commands.contains(command)){
      if (command == "exit") {
        break;
      }
      else if (command == "echo") {
        std::cout << input.substr(5) << std::endl;
      }
      else{
        std::string target = input.size() > 5 ? input.substr(5) : "";
        std::boolean find = false;
        // Step 1: Check built-in commands
        if (commands.contains(target)) {
            std::cout << target << " is a shell builtin\n";
        }
        else{
          const char* path = std::getenv("PATH");
          std::string pathString = path != nullptr ? path : "";
          std::stringstream ss(pathString);
          std::string directory;
          while (std::getline(ss, directory, ':')) {
            std::filesystem::path fullPath = std::filesystem:: path(directory) / target;
            if (std::filesystem::is_regular_file(fullPath) && access(fullPath.c_str(), X_OK) == 0){
              std::cout << target << " is " << fullPath << std::endl;
              find = true
            }

          }
          if (find == false){
            std:: cout << target << ": not found" << std::endl;
          }
        }
      }
    }
    else {
      std::cout << input << ": command not found"<< std::endl;
    }
  }


}
