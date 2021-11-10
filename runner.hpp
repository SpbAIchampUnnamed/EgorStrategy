#ifndef __RUNNER_HPP__
#define __RUNNER_HPP__

//#include "DebugInterface.hpp"
#include "TcpStream.hpp"
#include "codegame/ServerMessage.hpp"
#include "codegame/ClientMessage.hpp"
#include <memory>
#include <cstdlib>

class Runner {
public:
    Runner(const std::string& host, int port, const std::string& token): tcpStream(host, port)
    {
        tcpStream.write(token);
        tcpStream.write(int(1));
        tcpStream.write(int(0));
        tcpStream.write(int(0));
        tcpStream.flush();
        while (true) {
            auto message = codegame::ServerMessage::readFrom(tcpStream);
            if (auto getActionMessage = std::dynamic_pointer_cast<codegame::ServerMessage::GetAction>(message)) {
                model::game = getActionMessage->playerView;
                break;
            } else if (auto finishMessage = std::dynamic_pointer_cast<codegame::ServerMessage::Finish>(message)) {
                exit(0);
            } else if (auto debugUpdateMessage = std::dynamic_pointer_cast<codegame::ServerMessage::DebugUpdate>(message)) {
                //myStrategy.debugUpdate(debugUpdateMessage->playerView, debugInterface);
                codegame::ClientMessage::DebugUpdateDone().writeTo(tcpStream);
                tcpStream.flush();
            }
        }
    }
    void update(const model::Action &action) {
        codegame::ClientMessage::ActionMessage(action).writeTo(tcpStream);
        tcpStream.flush();
        while (true) {
            auto message = codegame::ServerMessage::readFrom(tcpStream);
            if (auto getActionMessage = std::dynamic_pointer_cast<codegame::ServerMessage::GetAction>(message)) {
                model::game.extend(std::move(getActionMessage->playerView));
                return;
            } else if (auto finishMessage = std::dynamic_pointer_cast<codegame::ServerMessage::Finish>(message)) {
                exit(0);
            } else if (auto debugUpdateMessage = std::dynamic_pointer_cast<codegame::ServerMessage::DebugUpdate>(message)) {
                //myStrategy.debugUpdate(debugUpdateMessage->playerView, debugInterface);
                codegame::ClientMessage::DebugUpdateDone().writeTo(tcpStream);
                tcpStream.flush();
            }
        }
    }

private:
    TcpStream tcpStream;
};

#endif