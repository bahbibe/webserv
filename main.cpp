#include "inc/Server.hpp"
t_events ep;
map<string, int> socketMap;
int main(int argc, char const *argv[])
{
    try
    {
        ifstream conf;
        (argc == 1) ? conf.open(DEFAULT_CONF) : (argc == 2) ? conf.open(argv[1])
                                                                     : throw Server::ServerException(USAGE);
        if (!conf.is_open())
            throw Server::ServerException(ERR "Unable to open file");
        string buff;
        getline(conf, buff, '\0');
        Webserver server;
        server.brackets(buff);
        for (size_t i = 0; i < server._servers.size(); i++)
            server[i].parseServer(buff);
        server.start();
    }
    catch (const exception &e)
    {      
        cerr << e.what() << '\n';
    }
}
