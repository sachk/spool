#pragma once

#include <QByteArray>
#include <QtGlobal>

namespace JellyfinNative {

struct MemoryBudget {
    qint64 memTotalBytes = 0;
    qint64 networkDiskCacheBytes = 0;
    qint64 qmlImageDiskCacheBytes = 0;
    int artworkByteCacheBytes = 0;
    QByteArray mpvDemuxerMaxBytes;
    QByteArray mpvDemuxerMaxBackBytes;

    static MemoryBudget detect();
};

} // namespace JellyfinNative
