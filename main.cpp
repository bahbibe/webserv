#include "webserv.hpp"

int main(int argc, char const *argv[])
{
    try
    {
        if (argc != 2)
            throw runtime_error(YELLOW "Usage: ./webserv <FILE.conf> [default.conf] is used if no file is specified)" RESET);
        ifstream conf(argv[1]);
        string buffer;
        if (conf.is_open())
        {
            getline(conf, buffer, '\0');
            stringstream ss(buffer);
            parseConfig(ss);   
        }
        else
            throw runtime_error("Unable to open file");
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }
}
