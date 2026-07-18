#include "MpvOptionProfile.h"

#include <QLocale>
#include <QUrl>
#include <QtGlobal>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace JellyfinNative {

namespace {

    QLocale::Language languageFromCode(QString code)
    {
        code = code.trimmed();
        QLocale::Language language = QLocale::codeToLanguage(QStringView(code));
        if (language != QLocale::AnyLanguage)
            return language;

        const qsizetype hyphen = code.indexOf(QLatin1Char('-'));
        const qsizetype underscore = code.indexOf(QLatin1Char('_'));
        const qsizetype separator = hyphen < 0 ? underscore : (underscore < 0 ? hyphen : qMin(hyphen, underscore));
        if (separator > 0)
            language = QLocale::codeToLanguage(QStringView(code).left(separator));
        return language;
    }

    bool languagesMatch(const QString& requested, const QString& available)
    {
        const QString requestedCode = requested.trimmed();
        const QString availableCode = available.trimmed();
        if (requestedCode.isEmpty() || availableCode.isEmpty())
            return false;
        if (requestedCode.compare(availableCode, Qt::CaseInsensitive) == 0)
            return true;

        const QLocale::Language requestedLanguage = languageFromCode(requestedCode);
        return requestedLanguage != QLocale::AnyLanguage && requestedLanguage == languageFromCode(availableCode);
    }

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

    QByteArray scaledSubtitleColor(const QString& rgb, int percent)
    {
        QString color = rgb.trimmed();
        if (color.startsWith(QLatin1Char('#')))
            color.remove(0, 1);
        bool ok = false;
        const uint value = color.toUInt(&ok, 16);
        if (!ok || color.size() != 6)
            return QByteArrayLiteral("#FFFFFFFF");

        const int scale = qBound(40, percent, 100);
        const auto channel = [scale](uint component) { return qBound(0, qRound(component * scale / 100.0), 255); };
        return (QStringLiteral("#FF%1%2%3")
                    .arg(channel((value >> 16) & 0xff), 2, 16, QLatin1Char('0'))
                    .arg(channel((value >> 8) & 0xff), 2, 16, QLatin1Char('0'))
                    .arg(channel(value & 0xff), 2, 16, QLatin1Char('0'))
                    .toUpper())
            .toLatin1();
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
        if (value == QStringLiteral("serif"))
            return QByteArrayLiteral("Source Serif 4");
        if (value == QStringLiteral("typewriter"))
            return QByteArrayLiteral("Courier New");
        if (value == QStringLiteral("print"))
            return QByteArrayLiteral("Georgia");
        if (value.startsWith(QStringLiteral("system:")))
            return value.sliced(7).toUtf8();
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

    QByteArray subtitleBackgroundColor(const QString& value)
    {
        if (value == QStringLiteral("opaque"))
            return QByteArrayLiteral("#FF000000");
        if (value == QStringLiteral("translucent"))
            return QByteArrayLiteral("#A0000000");
        return QByteArrayLiteral("#00000000");
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

bool MpvOptionProfile::isHdrPlayback(const QList<MediaStreamInfo>& streams)
{
    for (const MediaStreamInfo& stream : streams) {
        if (stream.type.compare(QStringLiteral("Video"), Qt::CaseInsensitive) != 0)
            continue;
        const QString metadata
            = (stream.videoRange + QLatin1Char(' ') + stream.colorTransfer + QLatin1Char(' ') + stream.profile)
                  .toLower();
        if (metadata.contains(QStringLiteral("hdr")) || metadata.contains(QStringLiteral("dovi"))
            || metadata.contains(QStringLiteral("dolby")) || metadata.contains(QStringLiteral("2084"))
            || metadata.contains(QStringLiteral("pq")) || metadata.contains(QStringLiteral("b67"))
            || metadata.contains(QStringLiteral("hlg")))
            return true;
    }
    return false;
}

QByteArray MpvOptionProfile::preloadedSubtitleStreams(const PlaybackSession& session, const QString& preferredLanguage)
{
    if (session.playMethod.compare(QStringLiteral("DirectPlay"), Qt::CaseInsensitive) != 0
        || preferredLanguage.trimmed().isEmpty())
        return {};

    QList<int> indexes;
    for (const MediaStreamInfo& stream : session.mediaStreams) {
        if (stream.index < 0 || stream.isExternal
            || stream.type.compare(QStringLiteral("Subtitle"), Qt::CaseInsensitive) != 0
            || !languagesMatch(preferredLanguage, stream.language))
            continue;
        if (!indexes.contains(stream.index))
            indexes.push_back(stream.index);
    }

    QByteArrayList values;
    values.reserve(indexes.size());
    for (int index : indexes)
        values.push_back(QByteArray::number(index));
    return values.join(',');
}

MpvOptionProfile::NetworkProfile MpvOptionProfile::networkProfile(Platform platform, int parallelRequests)
{
    const int requests = std::clamp(parallelRequests, 1, 4);
    return platform == Platform::WebOS ? NetworkProfile { 2 * 1024 * 1024, 512 * 1024, requests }
                                       : NetworkProfile { 4 * 1024 * 1024, 1024 * 1024, requests };
}

QByteArray MpvOptionProfile::loadFileOptions(const PlaybackSession& session)
{
    if (session.playMethod.compare(QStringLiteral("Transcode"), Qt::CaseInsensitive) != 0)
        return {};

    const QUrl url(session.url);
    if (!url.path().endsWith(QStringLiteral(".m3u8"), Qt::CaseInsensitive))
        return {};

    // Jellyfin's HLS master manifest is a non-seekable HTTP response. Tell
    // lavf what it is up front so mpv does not repeatedly probe and seek the
    // small manifest back to byte zero before HLS can open its media playlist.
    return QByteArrayLiteral("demuxer=lavf,demuxer-lavf-format=hls,initial-audio-sync=no");
}

std::vector<MpvOption> MpvOptionProfile::startupOptions(Platform platform, const QString& audioOutputMode,
    const QByteArray& logPath, const QByteArray& demuxerMaxBytes, const QByteArray& demuxerMaxBackBytes,
    int parallelRequests)
{
    const bool webOS = platform == Platform::WebOS;
    const NetworkProfile network = networkProfile(platform, parallelRequests);
    const QString normalizedAudioOutput = webOS ? audioOutputMode : normalizedAudioOutputMode(audioOutputMode);
    const bool starfishAudio
        = webOS && (audioOutputMode == QStringLiteral("starfish") || audioOutputMode == QStringLiteral("starfish-pcm"));

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
        { "curl-buffer-size", QByteArray::number(network.ringBytes) },
        { "curl-max-request-size", QByteArray::number(network.rangeBytes) },
        { "curl-parallel-requests", QByteArray::number(network.parallelRequests) },
        { "force-window", "no" },
    };

    if (webOS) {
        options.push_back({ "initial-audio-sync", "no" });
        options.push_back({ "vo", "starfish" });
        options.push_back({ "vd", "starfish" });
        options.push_back({ "ao", starfishAudio ? "starfish,null" : "alsa,null" });
        options.push_back({ "vo-starfish-audio-hint", starfishAudio ? "yes" : "no" });
        if (!starfishAudio) {
            options.push_back({ "audio-device", "alsa/hw:0,7" });
            options.push_back({ "video-sync", "display-resample" });
        }
        options.push_back({ "audio-channels", "stereo" });
        options.push_back({ "audio-format", starfishAudio ? "s16" : "s32" });
        options.push_back({ "audio-samplerate", starfishAudio ? "192000" : "48000" });
        if (!starfishAudio) {
            options.push_back({ "audio-buffer", "0.050" });
            options.push_back({ "alsa-buffer-time", "40000" });
            options.push_back({ "alsa-periods", "8" });
            options.push_back({ "alsa-no-hw-pause", "yes" });
            options.push_back({ "alsa-bounded-io", "yes" });
        } else {
            options.push_back({ "ao-starfish-feed-ahead", "0.4" });
        }
    } else {
        options.push_back({ "vo", "libmpv" });
        options.push_back({ "hwdec", "auto-safe" });
        if (normalizedAudioOutput != QStringLiteral("auto"))
            options.push_back({ "ao", normalizedAudioOutput.toUtf8() });
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

std::vector<MpvOption> MpvOptionProfile::subtitleOptions(
    const SubtitlePreferences& preferences, bool subtitlesEnabled, bool hdrPlayback)
{
    const SubtitlePreferences prefs = preferences;
    const QString subtitleMode = prefs.mode.isEmpty() ? QStringLiteral("Default") : prefs.mode;
    const bool noSubtitles = subtitleMode == QStringLiteral("None");
    const bool onlyForced = subtitleMode == QStringLiteral("OnlyForced");
    const bool alwaysPlay = subtitleMode == QStringLiteral("Always");
    const bool smart = subtitleMode == QStringLiteral("Smart");
    const bool nativeStyling = prefs.styling == QStringLiteral("Native");
    const int vertical = qBound(0, prefs.verticalPosition, 100);
    const SubtitleShadowOptions shadow = subtitleShadowOptions(prefs.dropShadow);
    const QByteArray subtitleColor = hdrPlayback && prefs.dimInHdr
        ? scaledSubtitleColor(prefs.textColor, prefs.hdrBrightnessPercent)
        : mpvArgbColor(prefs.textColor, QByteArrayLiteral("#FFFFFFFF"));
    const QByteArray bitmapSmoothing = prefs.bitmapSmoothing == QStringLiteral("softer") ? QByteArrayLiteral("1.0")
        : prefs.bitmapSmoothing == QStringLiteral("sharp")                               ? QByteArrayLiteral("0.0")
                                                                                         : QByteArrayLiteral("0.5");

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
        { "sub-scale", QByteArray::number(qBound(50, prefs.scalePercent, 200) / 100.0, 'f', 2) },
        { "sub-gauss", bitmapSmoothing },
        { "sub-bold", mpvBool(prefs.textWeight == QStringLiteral("bold")) },
        { "sub-pos", QByteArray::number(vertical) },
        { "sub-color", subtitleColor },
        { "sub-border-size", shadow.borderSize },
        { "sub-border-color", "#FF000000" },
        { "sub-shadow-offset", shadow.shadowOffset },
        { "sub-shadow-color", shadow.shadowColor },
        { "sub-back-color", subtitleBackgroundColor(prefs.textBackground) },
    };
}

} // namespace JellyfinNative
