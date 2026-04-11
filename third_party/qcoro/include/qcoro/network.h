#pragma once

#include <QNetworkReply>
#include <QObject>
#include <QPointer>

#include <coroutine>

namespace QCoro {

class NetworkReplyAwaiter {
public:
    explicit NetworkReplyAwaiter(QNetworkReply *reply)
        : m_reply(reply)
    {
    }

    bool await_ready() const noexcept
    {
        return m_reply && m_reply->isFinished();
    }

    void await_suspend(std::coroutine_handle<> continuation)
    {
        QObject::connect(m_reply, &QNetworkReply::finished, m_reply, [continuation]() mutable {
            continuation.resume();
        });
    }

    QNetworkReply *await_resume() const noexcept
    {
        return m_reply.data();
    }

private:
    QPointer<QNetworkReply> m_reply;
};

inline NetworkReplyAwaiter waitFor(QNetworkReply *reply)
{
    return NetworkReplyAwaiter(reply);
}

} // namespace QCoro
