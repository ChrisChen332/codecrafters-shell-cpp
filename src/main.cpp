
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>
#include <optional>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

namespace fs = std::filesystem;

// Holds the result of parsing one command line.
struct Command {
  std::vector<std::string> arguments;
  std::optional<std::string> outputFile;
  std::string error;
  int redirectFD = 1;
};

const std::unordered_set<std::string> builtins = {
  "echo", "exit", "type", "pwd", "cd"
};

// Parse arguments, quotes, escapes, and stdout redirection.
Command parseCommand(const std::string& input) {
  Command result;

  struct Token {
      std::string text;
      bool isRedirect = false;
      int redirectFd = 1;
  };

  std::vector<Token> tokens;
  std::string current;

  bool started = false;
  bool singleQuote = false;
  bool doubleQuote = false;

  auto finishToken = [&]() {
      if (started) {
          tokens.push_back({current, false});
          current.clear();
          started = false;
      }
  };

  for (size_t i = 0; i < input.size(); ++i) {
      char c = input[i];

      // Single quotes preserve everything literally.
      if (singleQuote) {
          if (c == '\'') {
              singleQuote = false;
          } else {
              current += c;
          }
          continue;
      }

      // Double quotes preserve spaces but allow certain escapes.
      if (doubleQuote) {
          if (c == '"') {
              doubleQuote = false;
          } else if (c == '\\' && i + 1 < input.size()) {
              char next = input[i + 1];

              if (next == '"' || next == '\\' ||
                  next == '$' || next == '`') {
                  current += next;
                  ++i;
              } else {
                  current += c;
              }
          } else {
              current += c;
          }
          continue;
      }

      // Outside quotes, backslash escapes the next character.
      if (c == '\\' && i + 1 < input.size()) {
          current += input[++i];
          started = true;
      }
      else if (c == '\'') {
          singleQuote = true;
          started = true;
      }
      else if (c == '"') {
          doubleQuote = true;
          started = true;
      }
      else if (std::isspace(static_cast<unsigned char>(c))) {
          finishToken();
      }
      else if (c == '>' ||
            ((c == '1' || c == '2') && i + 1 < input.size() &&
            input[i + 1] == '>' && !started)) {
        finishToken();
        int redirectFd;
        if (c == '1' || c == '2') {
        redirectFd = c - '0';
        ++i;
        
        }

        tokens.push_back({">", true, redirectFd});
      }
      else {
          current += c;
          started = true;
      }
  }

  if (singleQuote || doubleQuote) {
      result.error = "Unclosed quote";
      return result;
  }

  finishToken();

  // Separate command arguments from redirection syntax.
  for (size_t i = 0; i < tokens.size(); ++i) {
      if (tokens[i].isRedirect) {
          if (i + 1 >= tokens.size() ||
              tokens[i + 1].isRedirect) {
              result.error = "Missing output filename";
              return result;
          }
          result.redirectFD = tokens[i].redirectFd;
          result.outputFile = tokens[++i].text;
      }
      else {
          result.arguments.push_back(tokens[i].text);
      }
  }

  return result;
}

// Search PATH and return the first executable match.
std::optional<fs::path> findExecutable(
  const std::string& name
) {
  const char* rawPath = std::getenv("PATH");

  if (rawPath == nullptr) {
      return std::nullopt;
  }

  std::stringstream paths(rawPath);
  std::string directory;

  while (std::getline(paths, directory, ':')) {
      fs::path candidate =
          fs::path(directory.empty() ? "." : directory) / name;

      std::error_code ec;

      if (fs::is_regular_file(candidate, ec) &&
          access(candidate.c_str(), X_OK) == 0) {
          return candidate;
      }
  }

  return std::nullopt;
}

// Execute a builtin in the parent shell process.
void executeBuiltin(const Command& cmd) {
  const auto& args = cmd.arguments;
  const std::string& name = args[0];

  std::ofstream file;
  std::ostream* output = &std::cout;

  // Choose an output destination for this command only.
  if (cmd.outputFile && cmd.redirectFD == 1) {
      file.open(*cmd.outputFile, std::ios::out | std::ios::trunc);

      if (!file) {
          std::cerr << "Cannot open output file: "
                    << *cmd.outputFile << "\n";
          return;
      }

      output = &file;
  }

  if (name == "echo") {
      for (size_t i = 1; i < args.size(); ++i) {
          if (i > 1) {
              *output << " ";
          }

          *output << args[i];
      }

      *output << "\n";
  }
  else if (name == "pwd") {
      *output << fs::current_path().string() << "\n";
  }
  else if (name == "cd") {
      std::string target = args.size() > 1 ? args[1] : "~";

      if (target == "~") {
          const char* home = std::getenv("HOME");

          if (home == nullptr) {
              std::cerr << "cd: HOME not set\n";
              return;
          }

          target = home;
      }

      std::error_code ec;
      fs::current_path(target, ec);

      if (ec) {
          std::cerr << "cd: " << target
                    << ": No such file or directory\n";
      }
  }
  else if (name == "type") {
      if (args.size() < 2) {
          return;
      }

      const std::string& target = args[1];

      if (builtins.contains(target)) {
          *output << target << " is a shell builtin\n";
      }
      else {
          auto path = findExecutable(target);

          if (path) {
              *output << target << " is "
                      << path->string() << "\n";
          }
          else {
              *output << target << ": not found\n";
          }
      }
  }
}

// Run an external executable in a child process.
void executeExternal(const Command& cmd) {
  std::string name = cmd.arguments[0];

  auto fullPath = findExecutable(name);

  if (!fullPath) {
      std::cerr << name << ": command not found\n";
      return;
  }

  // execv expects an array of C-style string pointers.
  std::vector<std::string> args = cmd.arguments;
  std::vector<char*> argv;

  for (auto& arg : args) {
      argv.push_back(arg.data());
  }

  argv.push_back(nullptr);

  pid_t pid = fork();

  if (pid == 0) {
      // Child: redirect stdout before replacing the process.
      if (cmd.outputFile) {
          int fd = open(
              cmd.outputFile->c_str(),
              O_WRONLY | O_CREAT | O_TRUNC,
              0644
          );

          if (fd == -1) {
              perror("open");
              _exit(1);
          }

          if (dup2(fd, cmd.redirectFD) == -1) {
              perror("dup2");
              close(fd);
              _exit(1);
          }

          close(fd);
      }

      execv(fullPath->c_str(), argv.data());

      perror("execv");
      _exit(127);
  }
  else if (pid > 0) {
      // Parent waits for the foreground command.
      waitpid(pid, nullptr, 0);
  }
  else {
      perror("fork");
  }
}

int main() {
  std::cout << std::unitbuf;
  std::cerr << std::unitbuf;

  while (true) {
      std::cout << "$ ";

      std::string input;

      if (!std::getline(std::cin, input)) {
          break;
      }

      Command cmd = parseCommand(input);

      if (!cmd.error.empty()) {
          std::cerr << cmd.error << "\n";
          continue;
      }

      if (cmd.arguments.empty()) {
          continue;
      }

      const std::string& name = cmd.arguments[0];

      if (name == "exit") {
          break;
      }

      if (builtins.contains(name)) {
          executeBuiltin(cmd);
      }
      else {
          executeExternal(cmd);
      }
  }

  return 0;
}
