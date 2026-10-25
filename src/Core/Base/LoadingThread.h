#pragma once
// The game's loading thread (0x0059A6F0: CreateThread on 0x0059AD80, its loop 0x0059A7A0): the jobs queued for it run
// one at a time in the order they came. The loop waits for its event, takes the first job off the list under the
// list's lock (the event reset once the list is empty), marks it running (+0x08 2), runs it (vt+4), waits until the
// renderer's work the job left for the main thread is done (0x0051D270; here until the main thread has run the
// renderer's queue once more, setJobEnd) and marks it done (+0x08 3). A job still on the list can be taken off
// (0x0059A9D0); one running goes on to its end.

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>

namespace oj {

class LoadJob {
public:
    enum State { kQueued = 1, kRunning = 2, kDone = 3 };  // +0x08

    State state() const { return m_state.load(); }
    bool done() const { return state() == kDone; }

private:
    friend class LoadingThread;
    std::function<void()> m_run;  // vt+4
    std::atomic<State> m_state{kQueued};
};

class LoadingThread {
public:
    LoadingThread() = default;
    LoadingThread(const LoadingThread&) = delete;
    LoadingThread& operator=(const LoadingThread&) = delete;
    // The jobs still queued are dropped; a running one is let finish.
    ~LoadingThread();

    // 0x0059A930: the job added at the list's end and the event set; the thread starts with the first job.
    std::shared_ptr<LoadJob> queue(std::function<void()> run);
    // 0x0059A9D0: a job still on the list taken off; false when it has started (or is not there).
    bool cancel(const std::shared_ptr<LoadJob>& job);
    // The stock code's waits for a job (0x0045A35B: until its +0x08 is 3, each 10 ms the renderer's queued work
    // serviced): `pump` before each check.
    void wait(const LoadJob& job, const std::function<void()>& pump);
    // True on the loading thread.
    bool onThread() const;
    // 0x0051D270, run after each job before it is marked done: the wait for the main thread to run the renderer's
    // queue (the game sets it with its device; none without).
    void setJobEnd(std::function<void()> wait);

private:
    void run();

    mutable std::mutex m_lock;  // the list's lock
    std::condition_variable m_event;
    std::deque<std::shared_ptr<LoadJob>> m_jobs;
    std::thread m_thread;
    bool m_stop = false;
    std::function<void()> m_jobEnd;
};

// The game's one loading thread.
LoadingThread& loadingThread();

}  // namespace oj
