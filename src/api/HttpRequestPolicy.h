#pragma once

#include "../common/NetworkAddress.h"
#include <QNetworkReply>
#include <QUrl>

#include <algorithm>

namespace JellyfinNative {

enum class HttpOperation {
    Read,
    Mutation,
    PlaybackReport,
};

class HttpRequestPolicy final {
public:
    static bool allowsCredentialTransport(const QUrl& url)
    {
        if (!url.isValid() || url.host().isEmpty() || !url.userInfo().isEmpty())
            return false;
        if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0)
            return true;
        if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) != 0)
            return false;
        QHostAddress address;
        return address.setAddress(url.host()) && isPrivateNetworkAddress(address);
    }

    static bool sameOrigin(const QUrl& left, const QUrl& right)
    {
        const auto effectivePort = [](const QUrl& url) {
            return url.port(url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0 ? 443 : 80);
        };
        return left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0
            && left.host().compare(right.host(), Qt::CaseInsensitive) == 0
            && effectivePort(left) == effectivePort(right);
    }

    static constexpr int transferTimeoutMs()
    {
        return 30000;
    }

    static constexpr int maximumAttempts(HttpOperation operation)
    {
        return operation == HttpOperation::Mutation ? 1 : 3;
    }

    static bool shouldRetry(
        HttpOperation operation, int completedAttempts, int statusCode, QNetworkReply::NetworkError networkError)
    {
        if (completedAttempts >= maximumAttempts(operation) || statusCode == 401)
            return false;

        if (statusCode == 408 || statusCode == 425 || statusCode == 429 || statusCode >= 500) {
            return true;
        }

        if (networkError != QNetworkReply::NoError) {
            switch (networkError) {
            case QNetworkReply::ConnectionRefusedError:
            case QNetworkReply::RemoteHostClosedError:
            case QNetworkReply::HostNotFoundError:
            case QNetworkReply::TimeoutError:
            case QNetworkReply::TemporaryNetworkFailureError:
            case QNetworkReply::NetworkSessionFailedError:
            case QNetworkReply::ProxyConnectionRefusedError:
            case QNetworkReply::ProxyConnectionClosedError:
            case QNetworkReply::ProxyNotFoundError:
            case QNetworkReply::ProxyTimeoutError:
            case QNetworkReply::UnknownNetworkError:
            case QNetworkReply::UnknownProxyError:
            case QNetworkReply::UnknownServerError:
                return true;
            default:
                return false;
            }
        }

        return false;
    }

    static constexpr int retryDelayMs(int completedAttempts)
    {
        return std::min(2000, 250 << std::clamp(completedAttempts - 1, 0, 3));
    }
};

} // namespace JellyfinNative
