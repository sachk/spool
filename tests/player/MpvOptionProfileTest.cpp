#include "player/MpvOptionProfile.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

QByteArray valueFor(const std::vector<MpvOption>& options, const QByteArray& name)
{
    for (const MpvOption& option : options) {
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
        MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktop, "vo") == "libmpv", "desktop should render through libmpv");
    require(valueFor(desktop, "hwdec") == "auto-safe", "desktop should enable safe hardware decoding");
    require(valueFor(desktop, "log-file") == "/tmp/mpv.log", "profile should carry the configured log path");
    require(valueFor(desktop, "curl-enabled") == "yes", "desktop should use the libcurl stream backend");
    require(valueFor(desktop, "curl-buffer-size") == "4194304", "desktop should use a 4 MiB network ring");
    require(valueFor(desktop, "curl-max-request-size") == "1048576", "desktop should issue 1 MiB ranges");
    require(valueFor(desktop, "curl-parallel-requests") == "4", "desktop should fetch four ranges concurrently");

    const auto customDemuxerBudget
        = MpvOptionProfile::startupOptions(MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"),
            QByteArrayLiteral("/tmp/mpv.log"), QByteArrayLiteral("123456789"), QByteArrayLiteral("9876543"));
    require(valueFor(customDemuxerBudget, "demuxer-max-bytes") == "123456789",
        "custom demuxer max byte budget was not propagated");
    require(valueFor(customDemuxerBudget, "demuxer-max-back-bytes") == "9876543",
        "custom demuxer back byte budget was not propagated");

    const auto webOSPcm = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("starfish-pcm"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSPcm, "vo") == "starfish", "webOS should use the Starfish video output");
    require(valueFor(webOSPcm, "ao") == "starfish,null", "PCM mode should use Starfish audio");
    require(valueFor(webOSPcm, "audio-format") == "s16", "Starfish PCM should use signed 16-bit samples");
    require(valueFor(webOSPcm, "curl-enabled") == "yes", "webOS should use the libcurl stream backend");
    require(valueFor(webOSPcm, "curl-buffer-size") == "2097152", "webOS should use a 2 MiB network ring");
    require(valueFor(webOSPcm, "curl-max-request-size") == "524288", "webOS should issue 512 KiB ranges");
    require(valueFor(webOSPcm, "curl-parallel-requests") == "4", "webOS should fetch four ranges concurrently");

    const auto webOSAlsa = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("alsa"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSAlsa, "ao") == "alsa,null", "ALSA mode should use the ALSA output");
    require(valueFor(webOSAlsa, "video-sync") == "display-resample", "ALSA mode should follow the display clock");

    SubtitlePreferences subtitles;
    subtitles.language = QStringLiteral("eng");
    subtitles.mode = QStringLiteral("OnlyForced");
    subtitles.styling = QStringLiteral("Native");
    subtitles.textSize = QStringLiteral("large");
    subtitles.textWeight = QStringLiteral("bold");
    subtitles.font = QStringLiteral("typewriter");
    subtitles.textColor = QStringLiteral("#00ffcc");
    subtitles.dropShadow = QStringLiteral("uniform");
    subtitles.verticalPosition = 4;
    const SubtitlePreferences identicalSubtitles = subtitles;
    require(identicalSubtitles == subtitles, "identical subtitle preferences should be idempotent");
    SubtitlePreferences changedSubtitles = subtitles;
    changedSubtitles.language = QStringLiteral("fra");
    require(changedSubtitles != subtitles, "changed subtitle preferences must invalidate prepared playback state");

    const auto subtitleOptions = MpvOptionProfile::subtitleOptions(subtitles, true);
    require(valueFor(subtitleOptions, "sid") == "auto", "enabled subtitles should select automatic subtitle tracks");
    require(valueFor(subtitleOptions, "slang") == "eng", "subtitle language was not propagated");
    require(
        valueFor(subtitleOptions, "sub-forced-events-only") == "yes", "OnlyForced mode should use forced events only");
    require(valueFor(subtitleOptions, "subs-fallback") == "no",
        "OnlyForced mode should disable non-forced fallback subtitles");
    require(valueFor(subtitleOptions, "sub-ass-override") == "no", "native styling should avoid forced ASS override");
    require(valueFor(subtitleOptions, "sub-font") == "Courier New", "subtitle font preference was not mapped");
    require(valueFor(subtitleOptions, "sub-font-size") == "66", "subtitle size preference was not mapped");
    require(valueFor(subtitleOptions, "sub-bold") == "yes", "subtitle bold preference was not mapped");
    require(valueFor(subtitleOptions, "sub-pos") == "0", "positive subtitle position should anchor at top");
    require(valueFor(subtitleOptions, "sub-margin-y") == "80", "subtitle vertical margin was not mapped");
    require(valueFor(subtitleOptions, "sub-color") == "#FF00FFCC", "subtitle color was not converted to ARGB");
    require(valueFor(subtitleOptions, "sub-border-size") == "4.5", "uniform shadow should increase border size");
    require(valueFor(subtitleOptions, "sub-shadow-offset") == "0", "uniform shadow should disable shadow offset");

    SubtitlePreferences hidden;
    hidden.mode = QStringLiteral("None");
    hidden.textColor = QStringLiteral("not-a-color");
    const auto hiddenSubtitleOptions = MpvOptionProfile::subtitleOptions(hidden, false);
    require(valueFor(hiddenSubtitleOptions, "sid") == "no", "disabled subtitles should select no subtitle track");
    require(valueFor(hiddenSubtitleOptions, "sub-visibility") == "no", "None mode should hide subtitles");
    require(valueFor(hiddenSubtitleOptions, "sub-color") == "#FFFFFFFF",
        "invalid subtitle color should use the white fallback");
    return 0;
}
