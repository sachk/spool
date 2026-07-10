#pragma once

#include <QStringList>

namespace JellyfinNative {
struct MovieItem;

class ArtworkPrefetcher {
public:
    virtual ~ArtworkPrefetcher() = default;

    virtual void prefetch(const QStringList& urls) = 0;
    virtual void cancelPrefetches() = 0;
    virtual void configurePrefetch(int maxConcurrent) = 0;
    virtual QString itemUrl(const MovieItem& item, bool landscape, int width = 0) const = 0;
};

} // namespace JellyfinNative
