#include "MpvOptionProfile.h"

#include <iterator>

namespace JellyfinNative {

std::vector<MpvOption> MpvOptionProfile::startupOptions(
    Platform platform, const QString &audioOutputMode,
    const QByteArray &logPath)
{
    const bool webOS = platform == Platform::WebOS;
    const bool starfishPcm = audioOutputMode == QStringLiteral("starfish") ||
                             audioOutputMode == QStringLiteral("starfish-pcm");
    const bool starfishAudio = webOS && starfishPcm;

    std::vector<MpvOption> options{
        {"config", "no"},
        {"terminal", "no"},
        {"msg-level", webOS ? "all=warn,starfish=info,sub=v" : "all=warn,sub=v"},
        {"log-file", logPath},
        {"ytdl", "no"},
        {"demuxer-lavf-analyzeduration", "1"},
        {"demuxer-lavf-probesize", "1048576"},
        {"cache", "yes"},
        {"cache-pause", "no"},
        {"demuxer-max-bytes", "64M"},
        {"demuxer-max-back-bytes", "32M"},
        {"initial-audio-sync", "no"},
        {"force-window", "no"},
    };

    if (webOS) {
        options.push_back({"vo", "starfish"});
        options.push_back({"vd", "starfish"});
        options.push_back({"ao", starfishAudio ? "starfish,null" : "alsa,null"});
        if (!starfishAudio) {
            options.push_back({"audio-device", "alsa/hw:0,7"});
            options.push_back({"video-sync", "display-resample"});
        }
        options.push_back({"audio-channels", "stereo"});
        options.push_back({"audio-format", starfishPcm ? "s16" : "s32"});
        options.push_back(
            {"audio-samplerate", starfishPcm ? "48000"
                                             : (starfishAudio ? "192000" : "48000")});
        if (!starfishAudio) {
            options.push_back({"audio-buffer", "0.050"});
            options.push_back({"alsa-buffer-time", "40000"});
            options.push_back({"alsa-periods", "8"});
        }
    } else {
        options.push_back({"vo", "libmpv"});
        options.push_back({"hwdec", "auto-safe"});
        options.push_back({"ao", "pipewire,pulse,alsa"});
    }

    const MpvOption applicationOptions[] = {
        {"osd-bar", "no"},
        {"osd-duration", "0"},
        {"audio-file-auto", "no"},
        {"osc", "no"},
        {"load-console", "no"},
        {"load-auto-profiles", "no"},
        {"load-select", "no"},
        {"load-positioning", "no"},
        {"load-commands", "no"},
        {"load-context-menu", "no"},
        {"load-scripts", "no"},
        {"input-default-bindings", "no"},
        {"input-vo-keyboard", "no"},
        {"keep-open", "no"},
        {"idle", "yes"},
    };
    options.insert(options.end(), std::begin(applicationOptions),
                   std::end(applicationOptions));
    return options;
}

} // namespace JellyfinNative
