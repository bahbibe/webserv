#include "../includes/webserv.hpp"

void parseConfig(stringstream &ss)
{
    string buffer;
    while (getline(ss, buffer, '\n'))
    {
        cout << buffer << endl;
    }
}