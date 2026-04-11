#pragma once

#include <coroutine>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace QCoro {

template<typename T>
class Task;

namespace detail {

template<typename T>
class TaskPromiseBase {
public:
    std::exception_ptr exception;
    std::function<void()> completionHandler;
    std::coroutine_handle<> continuation;

    std::suspend_always initial_suspend() const noexcept
    {
        return {};
    }

    struct FinalAwaiter {
        bool await_ready() const noexcept
        {
            return false;
        }

        template<typename Promise>
        void await_suspend(std::coroutine_handle<Promise> handle) const noexcept
        {
            Promise &promise = handle.promise();
            if (promise.completionHandler)
                promise.completionHandler();
            if (promise.continuation)
                promise.continuation.resume();
        }

        void await_resume() const noexcept
        {
        }
    };

    FinalAwaiter final_suspend() const noexcept
    {
        return {};
    }

    void unhandled_exception()
    {
        exception = std::current_exception();
    }
};

template<typename T>
class TaskPromise final : public TaskPromiseBase<T> {
public:
    Task<T> get_return_object() noexcept;

    template<typename U>
    requires std::convertible_to<U, T>
    void return_value(U &&value)
    {
        result = std::forward<U>(value);
    }

    T takeResult()
    {
        return std::move(*result);
    }

private:
    std::optional<T> result;
};

template<>
class TaskPromise<void> final : public TaskPromiseBase<void> {
public:
    Task<void> get_return_object() noexcept;

    void return_void() noexcept
    {
    }
};

} // namespace detail

template<typename T>
class Task {
public:
    using promise_type = detail::TaskPromise<T>;

    Task() = default;
    explicit Task(std::coroutine_handle<promise_type> handle) noexcept
        : m_handle(handle)
    {
    }

    Task(Task &&other) noexcept
        : m_handle(std::exchange(other.m_handle, {}))
    {
    }

    Task &operator=(Task &&other) noexcept
    {
        if (this != &other) {
            if (m_handle)
                m_handle.destroy();
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    ~Task()
    {
        if (m_handle)
            m_handle.destroy();
    }

    bool isReady() const
    {
        return !m_handle || m_handle.done();
    }

    void start()
    {
        if (m_handle)
            m_handle.resume();
    }

    void setCompletionHandler(std::function<void()> handler)
    {
        if (m_handle)
            m_handle.promise().completionHandler = std::move(handler);
    }

    T result()
    {
        if (m_handle.promise().exception)
            std::rethrow_exception(m_handle.promise().exception);
        return m_handle.promise().takeResult();
    }

    bool await_ready() const noexcept
    {
        return isReady();
    }

    void await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        m_handle.promise().continuation = continuation;
        start();
    }

    T await_resume()
    {
        return result();
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

template<>
class Task<void> {
public:
    using promise_type = detail::TaskPromise<void>;

    Task() = default;
    explicit Task(std::coroutine_handle<promise_type> handle) noexcept
        : m_handle(handle)
    {
    }

    Task(Task &&other) noexcept
        : m_handle(std::exchange(other.m_handle, {}))
    {
    }

    Task &operator=(Task &&other) noexcept
    {
        if (this != &other) {
            if (m_handle)
                m_handle.destroy();
            m_handle = std::exchange(other.m_handle, {});
        }
        return *this;
    }

    Task(const Task &) = delete;
    Task &operator=(const Task &) = delete;

    ~Task()
    {
        if (m_handle)
            m_handle.destroy();
    }

    bool isReady() const
    {
        return !m_handle || m_handle.done();
    }

    void start()
    {
        if (m_handle)
            m_handle.resume();
    }

    void setCompletionHandler(std::function<void()> handler)
    {
        if (m_handle)
            m_handle.promise().completionHandler = std::move(handler);
    }

    void result()
    {
        if (m_handle.promise().exception)
            std::rethrow_exception(m_handle.promise().exception);
    }

    bool await_ready() const noexcept
    {
        return isReady();
    }

    void await_suspend(std::coroutine_handle<> continuation) noexcept
    {
        m_handle.promise().continuation = continuation;
        start();
    }

    void await_resume()
    {
        result();
    }

private:
    std::coroutine_handle<promise_type> m_handle;
};

namespace detail {

template<typename T>
Task<T> TaskPromise<T>::get_return_object() noexcept
{
    return Task<T>{std::coroutine_handle<TaskPromise<T>>::from_promise(*this)};
}

inline Task<void> TaskPromise<void>::get_return_object() noexcept
{
    return Task<void>{std::coroutine_handle<TaskPromise<void>>::from_promise(*this)};
}

} // namespace detail

template<typename T, typename Success, typename Failure>
void runDetached(Task<T> task, Success success, Failure failure)
{
    auto holder = std::make_shared<Task<T>>(std::move(task));
    holder->setCompletionHandler([holder, success = std::move(success), failure = std::move(failure)]() mutable {
        try {
            if constexpr (std::is_void_v<T>) {
                holder->result();
                success();
            } else {
                success(holder->result());
            }
        } catch (...) {
            failure(std::current_exception());
        }
    });
    holder->start();
}

template<typename Success, typename Failure>
void runDetached(Task<void> task, Success success, Failure failure)
{
    auto holder = std::make_shared<Task<void>>(std::move(task));
    holder->setCompletionHandler([holder, success = std::move(success), failure = std::move(failure)]() mutable {
        try {
            holder->result();
            success();
        } catch (...) {
            failure(std::current_exception());
        }
    });
    holder->start();
}

} // namespace QCoro
