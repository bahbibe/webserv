#include <iostream>
#include <fstream>
int main(int argc, char const *argv[])
{
    std::ifstream file("./WWW/vid.mp4", std::ios::binary | std::ios::ate);
    
    return 0;
}
