#include "inc/Server.hpp"

int main(int argc, char const *argv[])
{
    try
    {
        ifstream conf;
        (argc == 1) ? conf.open("default.conf") : (argc == 2) ? conf.open(argv[1])
                                                                     : throw Server::ServerException(USAGE);
        if (conf.is_open())
        {
            string buff;
            getline(conf, buff, '\0');
            Server server;
            server.parseServer(buff);
            server.print();
            // server.start();
        }
        else
            throw Server::ServerException("Unable to open file");
    }
    catch (const exception &e)
    {
        cerr << e.what() << '\n';
    }
}
