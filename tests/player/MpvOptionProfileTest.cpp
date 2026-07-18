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

    const auto desktopNetwork = MpvOptionProfile::networkProfile(MpvOptionProfile::Platform::Desktop);
    require(desktopNetwork.ringBytes == 4 * 1024 * 1024, "desktop curl ring profile changed");
    require(desktopNetwork.rangeBytes == 1024 * 1024, "desktop curl range profile changed");
    require(desktopNetwork.parallelRequests == 1, "desktop curl should default to one request");

    const auto desktop = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktop, "vo") == "libmpv", "desktop should render through libmpv");
    require(valueFor(desktop, "hwdec") == "auto-safe", "desktop should enable safe hardware decoding");
    require(valueFor(desktop, "log-file") == "/tmp/mpv.log", "profile should carry the configured log path");
    require(valueFor(desktop, "curl-enabled") == "yes", "desktop should use the libcurl stream backend");
    require(valueFor(desktop, "curl-buffer-size") == "4194304", "desktop should use a 4 MiB network ring");
    require(valueFor(desktop, "curl-max-request-size") == "1048576", "desktop should issue 1 MiB ranges");
    require(valueFor(desktop, "curl-parallel-requests") == "1", "desktop should default to one range request");
    require(
        valueFor(desktop, "initial-audio-sync").isEmpty(), "desktop should retain mpv's initial audio sync default");

    PlaybackSession hlsTranscode;
    hlsTranscode.playMethod = QStringLiteral("Transcode");
    hlsTranscode.url = QStringLiteral("https://media.example/Videos/1/master.m3u8");
    require(
        MpvOptionProfile::loadFileOptions(hlsTranscode) == "demuxer=lavf,demuxer-lavf-format=hls,initial-audio-sync=no",
        "HLS transcodes should bypass manifest probing and blocking initial audio sync");
    PlaybackSession directHls = hlsTranscode;
    directHls.playMethod = QStringLiteral("DirectPlay");
    require(MpvOptionProfile::loadFileOptions(directHls).isEmpty(),
        "direct-play URLs should retain mpv's normal demuxer detection");

    MediaStreamInfo englishSubtitle;
    englishSubtitle.index = 2;
    englishSubtitle.type = QStringLiteral("Subtitle");
    englishSubtitle.language = QStringLiteral("eng");
    MediaStreamInfo regionalEnglishSubtitle = englishSubtitle;
    regionalEnglishSubtitle.index = 4;
    regionalEnglishSubtitle.language = QStringLiteral("en-US");
    MediaStreamInfo frenchSubtitle = englishSubtitle;
    frenchSubtitle.index = 5;
    frenchSubtitle.language = QStringLiteral("fra");
    MediaStreamInfo externalEnglishSubtitle = englishSubtitle;
    externalEnglishSubtitle.index = 7;
    externalEnglishSubtitle.isExternal = true;
    PlaybackSession subtitleSession;
    subtitleSession.playMethod = QStringLiteral("DirectPlay");
    subtitleSession.mediaStreams
        = { englishSubtitle, regionalEnglishSubtitle, frenchSubtitle, externalEnglishSubtitle };
    require(MpvOptionProfile::preloadedSubtitleStreams(subtitleSession, QStringLiteral("en")) == "2,4",
        "direct play should preload internal subtitle streams matching the preferred language");
    require(MpvOptionProfile::preloadedSubtitleStreams(subtitleSession, QStringLiteral("fre")) == "5",
        "ISO-639 aliases should select the matching subtitle stream");
    require(MpvOptionProfile::preloadedSubtitleStreams(subtitleSession, QString()).isEmpty(),
        "an unspecified language should not retain every subtitle stream");
    subtitleSession.playMethod = QStringLiteral("Transcode");
    require(MpvOptionProfile::preloadedSubtitleStreams(subtitleSession, QStringLiteral("eng")).isEmpty(),
        "transcodes should not reuse source-file stream indexes");

    const auto customDemuxerBudget
        = MpvOptionProfile::startupOptions(MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"),
            QByteArrayLiteral("/tmp/mpv.log"), QByteArrayLiteral("123456789"), QByteArrayLiteral("9876543"), 2);
    require(valueFor(customDemuxerBudget, "demuxer-max-bytes") == "123456789",
        "custom demuxer max byte budget was not propagated");
    require(valueFor(customDemuxerBudget, "demuxer-max-back-bytes") == "9876543",
        "custom demuxer back byte budget was not propagated");
    require(valueFor(customDemuxerBudget, "curl-parallel-requests") == "2",
        "measured request parallelism was not propagated");

    const auto desktopAuto = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("auto"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopAuto, "ao").isEmpty(), "automatic desktop audio should leave output probing to mpv");
#if defined(Q_OS_LINUX)
    const auto desktopPipeWire = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("pipewire"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopPipeWire, "ao") == "pipewire", "Linux PipeWire selection was not applied");
    const auto desktopPulse = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("pulse"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopPulse, "ao") == "pulse", "Linux PulseAudio selection was not applied");
    const auto desktopAlsa = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("alsa"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopAlsa, "ao") == "alsa", "Linux ALSA selection was not applied");
#elif defined(Q_OS_WIN)
    const auto desktopWasapi = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("wasapi"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopWasapi, "ao") == "wasapi", "Windows WASAPI selection was not applied");
#elif defined(Q_OS_MACOS)
    const auto desktopCoreAudio = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::Desktop, QStringLiteral("coreaudio"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(desktopCoreAudio, "ao") == "coreaudio", "macOS CoreAudio selection was not applied");
#endif

    const auto webOSNetwork = MpvOptionProfile::networkProfile(MpvOptionProfile::Platform::WebOS);
    require(webOSNetwork.ringBytes == 2 * 1024 * 1024, "webOS curl ring profile changed");
    require(webOSNetwork.rangeBytes == 512 * 1024, "webOS curl range profile changed");
    require(webOSNetwork.parallelRequests == 1, "webOS curl should default to one request");

    const auto webOSPcm = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("starfish-pcm"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSPcm, "vo") == "starfish", "webOS should use the Starfish video output");
    require(valueFor(webOSPcm, "ao") == "starfish,null", "PCM mode should use Starfish audio");
    require(valueFor(webOSPcm, "audio-format") == "s16", "Starfish PCM should use signed 16-bit samples");
    require(valueFor(webOSPcm, "vo-starfish-audio-hint") == "yes", "Starfish PCM should advertise pipeline audio");
    require(valueFor(webOSPcm, "ao-starfish-feed-ahead") == "0.4", "Starfish PCM should use the measured feed window");
    require(valueFor(webOSPcm, "curl-enabled") == "yes", "webOS should use the libcurl stream backend");
    require(valueFor(webOSPcm, "curl-buffer-size") == "2097152", "webOS should use a 2 MiB network ring");
    require(valueFor(webOSPcm, "curl-max-request-size") == "524288", "webOS should issue 512 KiB ranges");
    require(valueFor(webOSPcm, "curl-parallel-requests") == "1", "webOS should default to one range request");

    const auto webOSAlsa = MpvOptionProfile::startupOptions(
        MpvOptionProfile::Platform::WebOS, QStringLiteral("alsa"), QByteArrayLiteral("/tmp/mpv.log"));
    require(valueFor(webOSAlsa, "ao") == "alsa,null", "ALSA mode should use the ALSA output");
    require(valueFor(webOSAlsa, "video-sync") == "display-resample", "ALSA mode should follow the display clock");
    require(valueFor(webOSAlsa, "initial-audio-sync") == "no", "webOS should retain its Starfish sync workaround");
    require(valueFor(webOSAlsa, "vo-starfish-audio-hint") == "no",
        "split-clock playback should not advertise pipeline audio");
    require(valueFor(webOSAlsa, "alsa-no-hw-pause") == "yes", "webOS ALSA should avoid the broken hardware pause path");
    require(valueFor(webOSAlsa, "alsa-bounded-io") == "yes", "webOS ALSA should use bounded direct-device I/O");

    MediaStreamInfo sdrStream;
    sdrStream.type = QStringLiteral("Video");
    sdrStream.videoRange = QStringLiteral("SDR");
    require(!MpvOptionProfile::isHdrPlayback({ sdrStream }), "SDR metadata was misidentified as HDR");
    MediaStreamInfo dolbyVisionStream = sdrStream;
    dolbyVisionStream.videoRange = QStringLiteral("DOVI");
    dolbyVisionStream.colorTransfer = QStringLiteral("smpte2084");
    require(MpvOptionProfile::isHdrPlayback({ dolbyVisionStream }),
        "Dolby Vision metadata did not enable HDR subtitle handling");

    SubtitlePreferences subtitles;
    subtitles.language = QStringLiteral("eng");
    subtitles.mode = QStringLiteral("OnlyForced");
    subtitles.styling = QStringLiteral("Native");
    subtitles.textSize = QStringLiteral("large");
    subtitles.textWeight = QStringLiteral("bold");
    subtitles.font = QStringLiteral("typewriter");
    subtitles.textColor = QStringLiteral("#00ffcc");
    subtitles.dropShadow = QStringLiteral("uniform");
    subtitles.verticalPosition = 40;
    subtitles.scalePercent = 125;
    subtitles.bitmapSmoothing = QStringLiteral("softer");
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
    require(valueFor(subtitleOptions, "sub-pos") == "40", "subtitle vertical percentage was not mapped");
    require(valueFor(subtitleOptions, "sub-color") == "#FF00FFCC", "subtitle color was not converted to ARGB");
    require(valueFor(subtitleOptions, "sub-border-size") == "4.5", "uniform shadow should increase border size");
    require(valueFor(subtitleOptions, "sub-shadow-offset") == "0", "uniform shadow should disable shadow offset");
    require(valueFor(subtitleOptions, "sub-scale") == "1.25", "overall subtitle scale was not propagated");
    require(valueFor(subtitleOptions, "sub-gauss") == "1.0", "bitmap subtitle smoothing was not propagated");
    subtitles.textBackground = QStringLiteral("translucent");
    require(valueFor(MpvOptionProfile::subtitleOptions(subtitles, true), "sub-back-color") == "#A0000000",
        "subtitle background preference was not mapped");

    SubtitlePreferences hdrSubtitles = subtitles;
    hdrSubtitles.textColor = QStringLiteral("#ffffff");
    hdrSubtitles.hdrBrightnessPercent = 75;
    const auto hdrSubtitleOptions = MpvOptionProfile::subtitleOptions(hdrSubtitles, true, true);
    require(valueFor(hdrSubtitleOptions, "sub-color") == "#FFBFBFBF",
        "HDR subtitle colour was not reduced to the configured paper-white level");
    hdrSubtitles.font = QStringLiteral("serif");
    require(valueFor(MpvOptionProfile::subtitleOptions(hdrSubtitles, true), "sub-font") == "Source Serif 4",
        "bundled Source Serif preference was not mapped");
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
