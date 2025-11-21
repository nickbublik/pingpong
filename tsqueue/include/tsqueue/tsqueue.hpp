#pragma once

#include <condition_variable>
#include <deque>
#include <mutex>

namespace Net
{
template <typename T>
class TSQueue
{
  public:
    TSQueue() = default;
    TSQueue(const TSQueue<T> &) = delete;
    ~TSQueue() { clear(); }

    const T &front()
    {
        std::scoped_lock lock(m_mutex);
        return m_deq.front();
    }

    const T &back()
    {
        std::scoped_lock lock(m_mutex);
        return m_deq.back();
    }

    bool empty() const
    {
        std::scoped_lock lock(m_mutex);
        return m_deq.empty();
    }

    size_t size() const
    {
        std::scoped_lock lock(m_mutex);
        return m_deq.size();
    }

    void clear()
    {
        std::scoped_lock lock(m_mutex);
        m_deq.clear();
    }

    T pop_front()
    {
        std::scoped_lock lock(m_mutex);
        auto t = std::move(m_deq.front());
        m_deq.pop_front();
        return t;
    }

    T pop_back()
    {
        std::scoped_lock lock(m_mutex);
        auto t = std::move(m_deq.back());
        m_deq.pop_back();
        return t;
    }

    void push_back(T item)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_deq.emplace_back(std::move(item));
        }
        m_blocking_cv.notify_one();
    }

    void push_front(T item)
    {
        {
            std::scoped_lock lock(m_mutex);
            m_deq.emplace_front(std::move(item));
        }
        m_blocking_cv.notify_one();
    }

    void wait()
    {
        std::unique_lock<std::mutex> ul(m_mutex);
        m_blocking_cv.wait(ul, [this]()
                           { return !m_deq.empty(); });
    }

  private:
    mutable std::mutex m_mutex;
    std::condition_variable m_blocking_cv;
    std::deque<T> m_deq;
};
} // namespace Net
