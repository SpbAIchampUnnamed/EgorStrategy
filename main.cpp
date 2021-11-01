#include "runner.hpp"
#include "MyStrategy.hpp"
#include <string>

int main(int argc, char* argv[])
{
    std::string host = argc < 2 ? "127.0.0.1" : argv[1];
    int port = argc < 3 ? 31001 : atoi(argv[2]);
    std::string token = argc < 4 ? "0000000000000000" : argv[3];
    Runner runner(host, port, token);
    MyStrategy myStrategy;
    myStrategy.play(runner);
    return 0;
}