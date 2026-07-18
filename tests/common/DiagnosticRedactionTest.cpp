#include "common/JellyfinTypes.h"

#include <QString>

#include <cstdlib>
#include <iostream>

using JellyfinNative::sanitizedDiagnosticUrl;

namespace {

void require(bool condition, const char *message)
{
    if (condition)
        return;
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

} // namespace

int main()
{
    const QString query
        = sanitizedDiagnosticUrl(QStringLiteral("https://server/Videos/id/stream?api_key=secret&MediaSourceId=source"));
    require(!query.contains(QStringLiteral("secret")), "query token should be removed from diagnostics");
    require(query.contains(QStringLiteral("api_key=<redacted>")), "query token should retain a useful key marker");

    const QString mpvOption
        = sanitizedDiagnosticUrl(QStringLiteral("http-header-fields=X-Emby-Token: top-secret,Accept: video/*"));
    require(!mpvOption.contains(QStringLiteral("top-secret")), "mpv token header should be removed from diagnostics");
    require(mpvOption.contains(QStringLiteral("X-Emby-Token: <redacted>")),
        "mpv token header should retain a useful name marker");

    const QString authorization
        = sanitizedDiagnosticUrl(QStringLiteral("Authorization: MediaBrowser Client=\"Tern\", Token=\"hidden\""));
    require(!authorization.contains(QStringLiteral("hidden")), "authorization token should be removed");
    require(authorization == QStringLiteral("Authorization: <redacted>"),
        "authorization redaction should remove the whole credential value");
    return EXIT_SUCCESS;
}
