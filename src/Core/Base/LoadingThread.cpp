#include "Core/Base/LoadingThread.h"

#if defined(_WIN32)
#include <windows.h>
#include <objbase.h>
#endif

#include <algorithm>
#include <chrono>

namespace oj {

LoadingThread::~LoadingThread() {
    {
        const std::lock_guard lock(m_lock);
        m_stop = true;
        m_jobs.clear();
    }
    m_event.notify_all();
    if (m_thread.joinable()) m_thread.join();
}

std::shared_ptr<LoadJob> LoadingThread::queue(std::function<void()> run) {
    auto job = std::make_shared<LoadJob>();
    job->m_run = std::move(run);
    {
        const std::lock_guard lock(m_lock);
        if (!m_thread.joinable()) m_thread = std::thread([this] { this->run(); });
        m_jobs.push_back(job);
    }
    m_event.notify_one();
    return job;
}

bool LoadingThread::cancel(const std::shared_ptr<LoadJob>& job) {
    const std::lock_guard lock(m_lock);
    const auto it = std::find(m_jobs.begin(), m_jobs.end(), job);
    if (it == m_jobs.end()) return false;
    m_jobs.erase(it);
    return true;
}

void LoadingThread::wait(const LoadJob& job, const std::function<void()>& pump) {
    while (!job.done()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (pump) pump();
    }
}

bool LoadingThread::onThread() const {
    const std::lock_guard lock(m_lock);
    return m_thread.joinable() && std::this_thread::get_id() == m_thread.get_id();
}

void LoadingThread::setJobEnd(std::function<void()> wait) {
    const std::lock_guard lock(m_lock);
    m_jobEnd = std::move(wait);
}

void LoadingThread::run() {
    // 0x0059AD80: the thread's COM (CoInitialize, CoUninitialize at its end).
#if defined(_WIN32)
    const HRESULT com = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
#endif
    for (;;) {
        std::shared_ptr<LoadJob> job;
        std::function<void()> jobEnd;
        {
            std::unique_lock lock(m_lock);
            m_event.wait(lock, [this] { return m_stop || !m_jobs.empty(); });
            if (m_stop) break;
            job = m_jobs.front();
            m_jobs.pop_front();
            job->m_state = LoadJob::kRunning;
            jobEnd = m_jobEnd;
        }
        job->m_run();
        job->m_run = nullptr;
        // 0x0051D270: done once the main thread has run the renderer's queue after it.
        if (jobEnd) jobEnd();
        job->m_state = LoadJob::kDone;
    }
#if defined(_WIN32)
    if (SUCCEEDED(com)) CoUninitialize();
#endif
}

LoadingThread& loadingThread() {
    static LoadingThread thread;
    return thread;
}

}  // namespace oj
