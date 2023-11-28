#pragma once 
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <stack>
#include <map>
// #include <string>
// #include <stdexcept>
// #include <algorithm>
// #include <cctype>
// #include <cstring>
#include <cstdlib>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <unistd.h>
// #include <fcntl.h>
// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <arpa/inet.h>
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define RESET "\033[0m"
#define USAGE YELLOW "Usage: ./webserv [config_file] if no config file is provided, default.conf will be used" RESET
#define OPEN_BR "{"
#define CLOSE_BR "}"
#define ERR RED "Error: " RESET
using namespace std;
void brackets(string  const &);
void parseConfig(string const &);