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
    require(HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("https://public.example"))),
        "HTTPS should allow credentials");
    require(HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://127.0.0.1:8096"))),
        "IPv4 loopback HTTP should be allowed");
    require(HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://192.168.1.4:8096"))),
        "private IPv4 HTTP should be allowed");
    require(HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://[::1]:8096"))),
        "IPv6 loopback HTTP should be allowed");
    require(HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://[fe80::1]:8096"))),
        "IPv6 link-local HTTP should be allowed");
    require(!HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://8.8.8.8:8096"))),
        "public IPv4 HTTP must be denied");
    require(!HttpRequestPolicy::allowsCredentialTransport(QUrl(QStringLiteral("http://media.example:8096"))),
        "HTTP hostnames must be denied to prevent DNS rebinding");
    require(!HttpRequestPolicy::sameOrigin(
                QUrl(QStringLiteral("https://one.example/a")), QUrl(QStringLiteral("https://two.example/b"))),
        "cross-origin redirects must not retain authorization");
    require(HttpRequestPolicy::sameOrigin(
                QUrl(QStringLiteral("https://one.example/a")), QUrl(QStringLiteral("https://ONE.example:443/b"))),
        "same-origin paths should compare by normalized origin");
    return 0;
}
