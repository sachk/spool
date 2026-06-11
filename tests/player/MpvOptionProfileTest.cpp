#include "player/MpvOptionProfile.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

QByteArray valueFor(const std::vector<MpvOption> &options, const QByteArray &name)
{
    for (const MpvOption &option : options) {
        if (option.name == name)
            return option.value;
    }
    return {};
}

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(1);
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    const auto desktop = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"),
        QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktop, "vo") == "libmpv",
            "desktop should render through libmpv");
    require(valueFor(desktop, "hwdec") == "auto-safe",
            "desktop should enable safe hardware decoding");
    require(valueFor(desktop, "log-file") == "/tmp/mpv.log",
            "profile should carry the configured log path");

    const auto webOSPcm = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("starfish-pcm"),
        QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSPcm, "vo") == "starfish",
            "webOS should use the Starfish video output");
    require(valueFor(webOSPcm, "ao") == "starfish,null",
            "PCM mode should use Starfish audio");
    require(valueFor(webOSPcm, "audio-format") == "s16",
            "Starfish PCM should use signed 16-bit samples");

    const auto webOSAlsa = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("alsa"),
        QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSAlsa, "ao") == "alsa,null",
            "ALSA mode should use the ALSA output");
    require(valueFor(webOSAlsa, "video-sync") == "display-resample",
            "ALSA mode should follow the display clock");
    return 0;
}
