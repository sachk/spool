#pragma once

#include <QString>

namespace JellyfinNative {

struct CpuTopology {
    int logicalCpus = 1;
    int physicalCores = 1;
    bool smtDetected = false;
    int artworkDecodeThreads = 1;
    QString source;
};

CpuTopology detectCpuTopology();

} // namespace JellyfinNative
