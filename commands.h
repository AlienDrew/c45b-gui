#ifndef COMMANDS_H
#define COMMANDS_H

#include <QObject>

enum class Commands : quint8
{
    Idle = 0,
    Connect,
    Program,
    DownloadLine,
    Disconnect,
};

#endif // COMMANDS_H
