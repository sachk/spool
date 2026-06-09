#pragma once

#include <QCoroTask>

#include <QDebug>
#include <QPointer>

#include <exception>
#include <functional>
#include <type_traits>
#include <utility>

namespace JellyfinNative::Async {

namespace detail {

inline QString exceptionText(const std::exception_ptr &error)
{
    try {
        if (error)
            std::rethrow_exception(error);
    } catch (const std::exception &exception) {
        return QString::fromUtf8(exception.what());
    } catch (...) {
        return QStringLiteral("unknown exception");
    }
    return QStringLiteral("unknown exception");
}

template<typename Failure>
void reportFailure(const char *operation, Failure &failure, const std::exception_ptr &error)
{
    qWarning().noquote() << "async:" << operation << "failed:" << exceptionText(error);
    std::invoke(failure, error);
}

} // namespace detail

template<typename T, typename Success, typename Failure>
void runDetached(QCoro::Task<T> task, Success success, Failure failure,
                 const char *operation = "detached task")
{
#ifdef JELLYFIN_USE_BUNDLED_QCORO
    QCoro::runDetached(
        std::move(task), std::move(success),
        [failure = std::move(failure), operation](const std::exception_ptr &error) mutable {
            detail::reportFailure(operation, failure, error);
        });
#else
    std::move(task).then(
        std::move(success),
        [failure = std::move(failure), operation](const std::exception &) mutable {
            detail::reportFailure(operation, failure, std::current_exception());
        });
#endif
}

template<typename Context, typename T, typename Success, typename Failure>
void runScoped(Context *context, QCoro::Task<T> task, Success success, Failure failure,
               const char *operation = "detached task")
{
    QPointer<Context> guard(context);
    auto guardedFailure =
        [guard, failure = std::move(failure)](const std::exception_ptr &error) mutable {
        if (guard)
            std::invoke(failure, error);
    };

    if constexpr (std::is_void_v<T>) {
        runDetached(
            std::move(task),
            [guard, success = std::move(success)]() mutable {
                if (guard)
                    std::invoke(success);
            },
            std::move(guardedFailure), operation);
    } else {
        runDetached(
            std::move(task),
            [guard, success = std::move(success)](T value) mutable {
                if (guard)
                    std::invoke(success, std::move(value));
            },
            std::move(guardedFailure), operation);
    }
}

} // namespace JellyfinNative::Async
