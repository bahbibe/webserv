#include "inc/webserv.hpp"

int main(int argc, char const *argv[])
{
    try
    {
        ifstream conf;
        (argc == 1) ? conf.open("default.conf") : (argc == 2) ? conf.open(argv[1])
                                                                     : throw runtime_error(USAGE);
        if (conf.is_open())
        {
            string buff;
            getline(conf, buff, '\0');
            Server server;
            server.parseConfig(buff);
            server.print();
        }
        else
            throw runtime_error("Unable to open file");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
    }
}
