#pragma once

#include "ui/window.h"
#include <QStringList>

namespace webcam {
struct Options {
    WindowOptions window;
    QStringList arguments;
    bool help = false;
    bool version = false;
    bool dryRun = false;
};
Options parseOptions(const QStringList &arguments);
void printHelp();
int runCommand(const Options &options);
}
