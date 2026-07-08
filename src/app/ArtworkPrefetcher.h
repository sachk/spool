#pragma once

#include <QStringList>

namespace JellyfinNative {

class ArtworkPrefetcher {
public:
    virtual ~ArtworkPrefetcher() = default;

    virtual void prefetch(const QStringList& urls) = 0;
    virtual void cancelPrefetches() = 0;
    virtual void configurePrefetch(int maxConcurrent) = 0;
};

} // namespace JellyfinNative
