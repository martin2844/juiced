#include "Core/Base/LoadingThread.h"

#ifdef _WIN32
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
#ifdef _WIN32
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
#ifdef _WIN32
    if (SUCCEEDED(com)) CoUninitialize();
#endif
}

LoadingThread& loadingThread() {
    static LoadingThread thread;
    return thread;
}

namespace {

thread_local bool t_turnsJob = false;  // this thread runs the job that takes turns with the frames

}  // namespace

void FrameTurns::beginFrame() {
    std::unique_lock lock(m_lock);
    m_frameWaiting = true;
    // A job waiting for the turn has one between two frames before the frame takes it.
    const auto ready = [this] {
        return m_holder == Holder::None && !(m_jobWaiting && m_jobTurns == m_jobTurnsAtFrameEnd);
    };
    while (!ready()) {
        if (!m_frameWaitService) {
            m_changed.wait(lock, ready);
        } else {
            const auto service = m_frameWaitService;
            lock.unlock();
            service();
            lock.lock();
            if (!ready()) m_changed.wait_for(lock, std::chrono::milliseconds(1));
        }
    }
    m_frameWaiting = false;
    m_holder = Holder::Frame;
}

void FrameTurns::setFrameWaitService(std::function<void()> service) {
    const std::lock_guard lock(m_lock);
    m_frameWaitService = std::move(service);
}

void FrameTurns::endFrame() {
    {
        const std::lock_guard lock(m_lock);
        if (m_holder != Holder::Frame) return;
        m_holder = Holder::None;
        m_jobTurnsAtFrameEnd = m_jobTurns;
    }
    m_changed.notify_all();
}

void FrameTurns::pause() {
    if (t_turnsJob) return;
    {
        const std::lock_guard lock(m_lock);
        if (m_holder != Holder::Frame) return;
    }
    endFrame();
    beginFrame();
}

void FrameTurns::takeForJob(std::unique_lock<std::mutex>& lock) {
    m_jobWaiting = true;
    m_changed.wait(lock, [this] {
        return m_holder == Holder::None && (!m_frameWaiting || m_jobTurns == m_jobTurnsAtFrameEnd);
    });
    m_jobWaiting = false;
    m_holder = Holder::Job;
    ++m_jobTurns;
    m_turnStart = std::chrono::steady_clock::now();
}

void FrameTurns::giveFromJob() {
    const std::chrono::duration<double, std::milli> turn = std::chrono::steady_clock::now() - m_turnStart;
    m_longestTurn = std::max(m_longestTurn, turn.count());
    m_holder = Holder::None;
    m_changed.notify_all();
}

void FrameTurns::beginJob() {
    std::unique_lock lock(m_lock);
    t_turnsJob = true;
    m_longestTurn = 0.0;
    takeForJob(lock);
}

void FrameTurns::endJob() {
    const std::lock_guard lock(m_lock);
    if (m_holder == Holder::Job) giveFromJob();
    t_turnsJob = false;
}

void FrameTurns::yield() {
    if (!t_turnsJob) return;
    std::unique_lock lock(m_lock);
    if (m_holder != Holder::Job || !m_frameWaiting) return;
    const std::chrono::duration<double, std::milli> turn = std::chrono::steady_clock::now() - m_turnStart;
    if (turn.count() < kJobTurnMs) return;
    giveFromJob();
    takeForJob(lock);
}

double FrameTurns::longestJobTurn() const {
    const std::lock_guard lock(m_lock);
    return m_longestTurn;
}

FrameTurns& frameTurns() {
    static FrameTurns turns;
    return turns;
}

}  // namespace oj
