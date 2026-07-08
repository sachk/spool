#include "api/HttpRequestPolicy.h"

#include <QCoreApplication>

#include <cstdlib>
#include <iostream>

using namespace JellyfinNative;

namespace {

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

    require(HttpRequestPolicy::maximumAttempts(HttpOperation::Read) == 3, "reads should be retried");
    require(HttpRequestPolicy::maximumAttempts(HttpOperation::Mutation) == 1, "mutations must not be duplicated");
    require(
        HttpRequestPolicy::maximumAttempts(HttpOperation::PlaybackReport) == 3, "playback reports should be retried");
    require(
        HttpRequestPolicy::shouldRetry(HttpOperation::Read, 1, 503, QNetworkReply::NoError), "503 should be retried");
    require(HttpRequestPolicy::shouldRetry(HttpOperation::PlaybackReport, 2, 429, QNetworkReply::NoError),
        "rate-limited playback reports should be retried");
    require(HttpRequestPolicy::shouldRetry(HttpOperation::Read, 1, 0, QNetworkReply::TimeoutError),
        "network timeouts should be retried");
    require(!HttpRequestPolicy::shouldRetry(HttpOperation::Read, 1, 401, QNetworkReply::AuthenticationRequiredError),
        "401 must expire the session without retrying");
    require(!HttpRequestPolicy::shouldRetry(HttpOperation::Mutation, 1, 503, QNetworkReply::NoError),
        "mutations must remain single-attempt");
    require(!HttpRequestPolicy::shouldRetry(HttpOperation::Read, 3, 503, QNetworkReply::NoError),
        "retry limit must be bounded");
    require(HttpRequestPolicy::retryDelayMs(1) == 250 && HttpRequestPolicy::retryDelayMs(2) == 500,
        "retry delay should back off");
    return 0;
}
