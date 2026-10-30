#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <unistd.h>
#include <filesystem>
#include <sys/wait.h>
#include <sys/stat.h>
#include <vector>
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while (true){
    std::cout << "$ ";
    std:: string input;
    std::unordered_set<std::string> commands = {"echo", "exit", "type"};

    
    const char* path = std::getenv("PATH");
    std::string pathString = path != nullptr ? path : "";
    std::stringstream ss(pathString);
    std::string directory;

    std::getline(std::cin, input);    
    std::istringstream stream(input);
    std::vector<std::string> arguments;
    std::string word;
    std::string command = arguments[0];

    while (stream >> word) {
      arguments.push_back(word);
    }

    if (commands.contains(command)){
      if (command == "exit") {
        break;
      }
      else if (command == "echo") {
        std::cout << input.substr(5) << std::endl;
      }
      else{
        std::string target = input.size() > 5 ? input.substr(5) : "";
        bool find = false;
        // Step 1: Check built-in commands
        if (commands.contains(target)) {
            std::cout << target << " is a shell builtin\n";
        }
        else{
          while (std::getline(ss, directory, ':')) {
            std::filesystem::path fullPath = std::filesystem:: path(directory);
            if (std::filesystem::is_regular_file(fullPath) && access(fullPath.c_str(), X_OK) == 0){
              std::cout << target << " is " << fullPath.string() << std::endl;
              find = true;
              break;
            }
          }
          if (find == false){
            std:: cout << target << ": not found" << std::endl;
          }
        }
      }
    }
    else {
      bool find = false;
      std::filesystem::path fullPath;
      while (std::getline(ss, directory, ':')) {
        fullPath = std::filesystem:: path(directory) / command;

        if (std::filesystem::is_regular_file(fullPath) && access(fullPath.c_str(), X_OK) == 0){
          find = true;
          break;
        }

      }
      if (find == false){
        std::cout << input << ": command not found"<< std::endl;
      }
      else{
        std::vector<char*> argv;

        for(auto& arg : arguments){
          argv.push_back(arg.data());
        }

        argv.push_back(nullptr);

        pid_t  pid = fork();

        if (pid == 0){
        execv(fullPath.c_str(), argv.data());

        // Only runs if execv fails
        perror("execv");
        _exit(127);
        }
        else if (pid > 0){
          waitpid(pid, nullptr, 0);
        }
        else{
          perror("fork");
        }
      }
    }
  }

  return 0;
}
