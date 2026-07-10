#include "MpvOptionProfile.h"

#include <QtGlobal>

#include <cmath>
#include <iterator>

namespace JellyfinNative {

namespace {

    QByteArray mpvBool(bool value)
    {
        return value ? QByteArrayLiteral("yes") : QByteArrayLiteral("no");
    }

    QByteArray mpvArgbColor(const QString& rgb, QByteArray fallback)
    {
        QString color = rgb.trimmed();
        if (color.startsWith(QLatin1Char('#')))
            color.remove(0, 1);
        if (color.size() != 6)
            return fallback;

        for (const QChar ch : color) {
            if (!ch.isDigit() && (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f')))
                return fallback;
        }

        return QByteArrayLiteral("#FF") + color.toUpper().toLatin1();
    }

    QByteArray subtitleFontSize(const QString& value)
    {
        if (value == QStringLiteral("smaller"))
            return QByteArrayLiteral("44");
        if (value == QStringLiteral("small"))
            return QByteArrayLiteral("50");
        if (value == QStringLiteral("large"))
            return QByteArrayLiteral("66");
        if (value == QStringLiteral("larger"))
            return QByteArrayLiteral("76");
        if (value == QStringLiteral("extralarge"))
            return QByteArrayLiteral("84");
        return QByteArrayLiteral("55");
    }

    QByteArray subtitleFontFamily(const QString& value)
    {
        if (value == QStringLiteral("typewriter"))
            return QByteArrayLiteral("Courier New");
        if (value == QStringLiteral("print"))
            return QByteArrayLiteral("Georgia");
        if (value == QStringLiteral("console"))
            return QByteArrayLiteral("Consolas");
        if (value == QStringLiteral("cursive"))
            return QByteArrayLiteral("Lucida Handwriting");
        if (value == QStringLiteral("casual"))
            return QByteArrayLiteral("Segoe Print");
        if (value == QStringLiteral("smallcaps"))
            return QByteArrayLiteral("Copperplate Gothic");
        return QByteArrayLiteral("sans-serif");
    }

    struct SubtitleShadowOptions {
        QByteArray borderSize = QByteArrayLiteral("3.5");
        QByteArray shadowOffset = QByteArrayLiteral("1");
        QByteArray shadowColor = QByteArrayLiteral("#80000000");
    };

    SubtitleShadowOptions subtitleShadowOptions(const QString& value)
    {
        SubtitleShadowOptions options;
        if (value == QStringLiteral("none")) {
            options.shadowOffset = QByteArrayLiteral("0");
            options.shadowColor = QByteArrayLiteral("#00000000");
        } else if (value == QStringLiteral("raised")) {
            options.shadowOffset = QByteArrayLiteral("1");
            options.shadowColor = QByteArrayLiteral("#A0000000");
        } else if (value == QStringLiteral("depressed")) {
            options.shadowOffset = QByteArrayLiteral("-1");
            options.shadowColor = QByteArrayLiteral("#A0000000");
        } else if (value == QStringLiteral("uniform")) {
            options.borderSize = QByteArrayLiteral("4.5");
            options.shadowOffset = QByteArrayLiteral("0");
            options.shadowColor = QByteArrayLiteral("#00000000");
        }
        return options;
    }

} // namespace

std::vector<MpvOption> MpvOptionProfile::startupOptions(Platform platform, const QString& audioOutputMode,
    const QByteArray& logPath, const QByteArray& demuxerMaxBytes, const QByteArray& demuxerMaxBackBytes)
{
    const bool webOS = platform == Platform::WebOS;
    const bool starfishPcm
        = audioOutputMode == QStringLiteral("starfish") || audioOutputMode == QStringLiteral("starfish-pcm");
    const bool starfishAudio = webOS && starfishPcm;

    std::vector<MpvOption> options {
        { "config", "no" },
        { "terminal", "no" },
        { "msg-level", webOS ? "all=warn,starfish=info,sub=v" : "all=warn,sub=v" },
        { "log-file", logPath },
        { "ytdl", "no" },
        { "demuxer-lavf-analyzeduration", "1" },
        { "demuxer-lavf-probesize", "1048576" },
        { "cache", "yes" },
        { "cache-pause", "no" },
        { "demuxer-max-bytes", demuxerMaxBytes },
        { "demuxer-max-back-bytes", demuxerMaxBackBytes },
        { "curl-enabled", "yes" },
        { "curl-buffer-size", webOS ? "2097152" : "4194304" },
        { "curl-max-request-size", webOS ? "524288" : "1048576" },
        { "curl-parallel-requests", "4" },
        { "initial-audio-sync", "no" },
        { "force-window", "no" },
    };

    if (webOS) {
        options.push_back({ "vo", "starfish" });
        options.push_back({ "vd", "starfish" });
        options.push_back({ "ao", starfishAudio ? "starfish,null" : "alsa,null" });
        if (!starfishAudio) {
            options.push_back({ "audio-device", "alsa/hw:0,7" });
            options.push_back({ "video-sync", "display-resample" });
        }
        options.push_back({ "audio-channels", "stereo" });
        options.push_back({ "audio-format", starfishPcm ? "s16" : "s32" });
        options.push_back({ "audio-samplerate", starfishPcm ? "48000" : (starfishAudio ? "192000" : "48000") });
        if (!starfishAudio) {
            options.push_back({ "audio-buffer", "0.050" });
            options.push_back({ "alsa-buffer-time", "40000" });
            options.push_back({ "alsa-periods", "8" });
        }
    } else {
        options.push_back({ "vo", "libmpv" });
        options.push_back({ "hwdec", "auto-safe" });
        options.push_back({ "ao", "pipewire,pulse,alsa" });
    }

    const MpvOption applicationOptions[] = {
        { "osd-bar", "no" },
        { "osd-duration", "0" },
        { "audio-file-auto", "no" },
        { "osc", "no" },
        { "load-console", "no" },
        { "load-auto-profiles", "no" },
        { "load-select", "no" },
        { "load-positioning", "no" },
        { "load-commands", "no" },
        { "load-context-menu", "no" },
        { "load-scripts", "no" },
        { "input-default-bindings", "no" },
        { "input-vo-keyboard", "no" },
        { "keep-open", "no" },
        { "idle", "yes" },
    };
    options.insert(options.end(), std::begin(applicationOptions), std::end(applicationOptions));
    return options;
}

std::vector<MpvOption> MpvOptionProfile::subtitleOptions(const SubtitlePreferences& preferences, bool subtitlesEnabled)
{
    const SubtitlePreferences prefs = preferences;
    const QString subtitleMode = prefs.mode.isEmpty() ? QStringLiteral("Default") : prefs.mode;
    const bool noSubtitles = subtitleMode == QStringLiteral("None");
    const bool onlyForced = subtitleMode == QStringLiteral("OnlyForced");
    const bool alwaysPlay = subtitleMode == QStringLiteral("Always");
    const bool smart = subtitleMode == QStringLiteral("Smart");
    const bool nativeStyling = prefs.styling == QStringLiteral("Native");
    const int vertical = qBound(-16, prefs.verticalPosition, 16);
    const int margin = vertical < 0 ? std::abs(vertical + 1) * 20 : vertical * 20;
    const SubtitleShadowOptions shadow = subtitleShadowOptions(prefs.dropShadow);

    return {
        { "sid", !subtitlesEnabled || noSubtitles ? QByteArrayLiteral("no") : QByteArrayLiteral("auto") },
        { "slang", prefs.language.toUtf8() },
        { "sub-auto", "all" },
        { "sub-visibility", mpvBool(!noSubtitles) },
        { "sub-forced-events-only", mpvBool(onlyForced) },
        { "subs-with-matching-audio", mpvBool(alwaysPlay) },
        { "subs-fallback", mpvBool(!noSubtitles && !onlyForced && !smart) },
        { "subs-fallback-forced", "yes" },
        { "sub-ass", "yes" },
        { "sub-ass-override", nativeStyling ? QByteArrayLiteral("no") : QByteArrayLiteral("force") },
        { "sub-use-margins", "yes" },
        { "sub-font", subtitleFontFamily(prefs.font) },
        { "sub-font-size", subtitleFontSize(prefs.textSize) },
        { "sub-bold", mpvBool(prefs.textWeight == QStringLiteral("bold")) },
        { "sub-pos", vertical < 0 ? QByteArrayLiteral("100") : QByteArrayLiteral("0") },
        { "sub-margin-y", QByteArray::number(margin) },
        { "sub-color", mpvArgbColor(prefs.textColor, QByteArrayLiteral("#FFFFFFFF")) },
        { "sub-border-size", shadow.borderSize },
        { "sub-border-color", "#FF000000" },
        { "sub-shadow-offset", shadow.shadowOffset },
        { "sub-shadow-color", shadow.shadowColor },
    };
}

} // namespace JellyfinNative
