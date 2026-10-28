#include <iostream>
#include <string>
#include <unordered_set>


int main() {
  // Flush after every std::cout / std:cerr
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  // TODO: Uncomment the code below to pass the first stage
  while (true){
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
        if (commands.contains(input.substr(5))){
          std::cout << input.substr(5) << " is a shell builtin" << std::endl;
        }    
        else {
          std::cout << input.substr(5) << ": command not found"<< std::endl;
        }
        
      }
    }
    else {
      std::cout << input << ": command not found"<< std::endl;
    }
  }


}
