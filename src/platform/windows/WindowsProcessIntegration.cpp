#include "platform/PlatformProcess.h"

namespace JellyfinNative {

ProcessStartupTiming captureProcessStartupTiming()
{
    return {};
}

struct TerminationSignalHandler::PlatformData { };

TerminationSignalHandler::TerminationSignalHandler(QCoreApplication&)
    : m_platform(std::make_unique<PlatformData>())
{
}

TerminationSignalHandler::~TerminationSignalHandler() = default;

} // namespace JellyfinNative
