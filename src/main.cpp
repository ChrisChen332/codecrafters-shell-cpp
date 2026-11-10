#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <unistd.h>
#include <fcntl.h>
#include <filesystem>
#include <sys/wait.h>
#include <sys/stat.h>
#include <vector>
#include <cstdlib>  // std::getenv
#include <cstdio>   // perror
int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true){
    std::cout << "$ ";
    std:: string input;
    std::unordered_set<std::string> commands = {"echo", "exit", "type", "pwd", "cd"};

    
    const char* path = std::getenv("PATH");
    std::string pathString = path != nullptr ? path : "";
    std::stringstream ss(pathString);
    std::string directory;

    std::getline(std::cin, input);    

    std::vector<std::string> arguments;
    std::string current;
    
    bool inSingleQuotes = false;
    bool inDoubleQuotes = false;
    bool argumentStarted = false;
    bool isbackslash = false;
  
    for (char c : input){
      if (isbackslash) {
        current += c;
        argumentStarted = true;
        isbackslash = false;
        continue;
      }
      if (c == '\'' && !inDoubleQuotes && !isbackslash){
        inSingleQuotes = !inSingleQuotes;
        argumentStarted = true;
      }
      else if (c == '\\'  && !inSingleQuotes){
        if (!inDoubleQuotes){
          isbackslash = true;
        }
        else if (c == '\"' || c == '\\' || c == '\$' || c == '\`' || c == '\n'){
          isbackslash = true;
        }
      
      }
      else if (c == '\"' && !isbackslash && !inSingleQuotes){
        inDoubleQuotes = !inDoubleQuotes;
        argumentStarted = true;
      }
      else if ((std:: isspace(c) && !inSingleQuotes && !inDoubleQuotes && !isbackslash)){
        if (argumentStarted){
          arguments.push_back(current);
          current.clear();
          argumentStarted = false;
        }
      }
      else{
        current += c;
        argumentStarted = true;
      }
    }
    if (argumentStarted) {
    arguments.push_back(current);
    }

    if (arguments.empty()) {
    continue;
    }
    
    bool redirectOutput = false;
    std::string outputFile;

    for (size_t i = 0; i < arguments.size(); i++){
      if (arguments[i] == '>' || arguments[i] == '1>'){
        redirectOutput = true;

        if (i + 1 < arguments.size()){
          outputFile = arguments[i + 1];
        }

        arguments.resize(i)
        break;
      }
    }
    std::string command = arguments[0];

    //Build-in Command

    if (commands.contains(command)){
      std::ofstream outFile;
      std::streambuf* originalBuffer = nullptr;      
      if (redirectOutput == true){
        outFile.open(outputFile, std::ios::out | std::ios::trunc);

        if (!outFile) {
            std::cerr << "Cannot open output file: " << outputFile << "\n";
            continue;
        }

        originalBuffer = std::cout.rdbuf(outFile.rdbuf());
      }
      if (command == "exit") {
        break;
      } 
      else if (command == "echo") {
        for (size_t i = 1; i < arguments.size(); i++){
          if (i > 1){
            std::cout << " ";
          }
          std::cout << arguments[i];
        }
        std::cout << std:: endl;
      }
      else if (command == "pwd"){
        std::cout << std::filesystem::current_path().string() << std::endl;
      }
      else if (command == "cd"){
        std::string target = arguments.size() > 1 ? arguments[1] : "";
        std::error_code ec;
        if (target == "~"){
          const char* home = std::getenv("HOME");
          std::filesystem::current_path(home, ec);         
        }else{
          std::filesystem::current_path(target, ec);
        }
        

        if (ec) {
            std::cout << "cd: " << target
                      << ": No such file or directory\n";
        }
      }
      //Command "type"
      else {
        std::string target = input.size() > 5 ? input.substr(5) : "";
        bool find = false;
        // Step 1: Check built-in commands
        if (commands.contains(target)) {
            std::cout << target << " is a shell builtin\n";
        }
        else{
          while (std::getline(ss, directory, ':')) {
            std::filesystem::path fullPath = std::filesystem:: path(directory)/target;
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

      if (originalBuffer != nullptr) {
        std::cout.flush();
        std::cout.rdbuf(originalBuffer);
        outFile.close();
      }
    }
    //Not build-in command try excution
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
          if (redirectOutput){

            // Open the output file for writing.
            // Create it if missing, overwrite if it exists.
            int fd = open(
                outputFile.c_str(),
                O_WRONLY | O_CREAT | O_TRUNC,
                0644
            );

            // Stop if opening the file failed.
            if (fd == -1) {
                perror("open");
                _exit(1);
            }

            // Make standard output (FD 1) refer to the file.
            if (dup2(fd, STDOUT_FILENO) == -1) {
                perror("dup2");
                close(fd);
                _exit(1);
            }

            // The extra descriptor is no longer needed.
            // FD 1 remains connected to the file.
            close(fd);
          }
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
