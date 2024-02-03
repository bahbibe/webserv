#include "inc/Server.hpp"
t_events ep;
map<string, int> socketMap;
vector<Server> servers;
int main(int argc, char const *argv[])
{
    try
    {
        ifstream conf;
        (argc == 1) ? conf.open(DEFAULT_CONF) : (argc == 2) ? conf.open(argv[1])
                                                                     : throw Server::ServerException(USAGE);
        if (conf.is_open())
        {
            string buff;
            getline(conf, buff, '\0');
            Webserver server;
            server.brackets(buff);
            for (size_t i = 0; i < server.serverCount(); i++)
            {
                Server tmp;
                tmp.parseServer(buff);
                tmp.setupSocket();
                servers.push_back(tmp);
            }
            map<string, int>::iterator it = socketMap.begin();
            while (it != socketMap.end())
            {
                cout << RED << it->first << " " << GREEN <<  it->second <<RESET<< endl;
                it++;
            }
            // server.start();
        }
        else
            throw Server::ServerException(ERR "Unable to open file");
    }
    catch (const exception &e)
    {
        cerr << e.what() << '\n';
    }
}
