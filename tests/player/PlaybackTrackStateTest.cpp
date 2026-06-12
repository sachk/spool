#include "player/PlaybackTrackState.h"

#include <QDebug>
#include <QVariantMap>

#include <cstdlib>

using JellyfinNative::ParsedPlaybackTracks;
using JellyfinNative::PlaybackTrackState;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    qCritical() << message;
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    PlaybackTrackState state;

    ParsedPlaybackTracks tracks;
    tracks.subtitleLabels = {QStringLiteral("Off"), QStringLiteral("English"), QStringLiteral("French")};
    tracks.subtitleIds = {-1, 3, 4};
    tracks.selectedSubtitleIndex = 1;
    tracks.audioLabels = {QStringLiteral("Stereo"), QStringLiteral("Commentary")};
    tracks.audioIds = {1, 2};
    tracks.selectedAudioIndex = 0;
    state.applyParsedTracks(tracks);

    require(state.subtitlesEnabled(), "selected subtitle track should enable subtitles");
    require(state.toggleSubtitleTarget() == 0, "toggle should turn subtitles off");
    require(state.cycleSubtitleTarget() == 2, "cycle should advance subtitle index");
    require(state.cycleAudioTarget() == 1, "cycle should advance audio index");
    require(state.subtitleCommand(0) == QByteArray("no-osd set sid no"),
            "off subtitle command was wrong");
    require(state.subtitleCommand(2) == QByteArray("no-osd set sid 4"),
            "subtitle command used wrong mpv id");
    require(state.audioCommand(1) == QByteArray("no-osd set aid 2"),
            "audio command used wrong mpv id");

    state.applySubtitleSelection(0);
    require(!state.subtitlesEnabled(), "off subtitle selection should disable subtitles");
    require(state.enableSubtitleTarget() == 1, "enable subtitles should select first real track");

    QVariantList chapters;
    chapters.push_back(QVariantMap{{QStringLiteral("title"), QStringLiteral("Chapter 1")},
                                   {QStringLiteral("start"), 0.0}});
    chapters.push_back(QVariantMap{{QStringLiteral("title"), QStringLiteral("Chapter 2")},
                                   {QStringLiteral("start"), 30.0}});
    state.setChapters(chapters);
    require(state.hasChapters(), "two chapters should enable chapter actions");
    require(state.setCurrentChapter(1), "chapter change should report changed");
    require(!state.setCurrentChapter(1), "same chapter should not report changed");
    require(state.clearChapters(), "clearing populated chapters should report changed");
    require(!state.hasChapters(), "cleared chapters should disable chapter actions");

    state.resetForPlayback();
    require(state.subtitleTracks() == QStringList({QStringLiteral("Off")}),
            "playback reset should restore subtitle labels");
    require(state.audioTracks().isEmpty(), "playback reset should clear audio labels");

    return EXIT_SUCCESS;
}
