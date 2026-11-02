#pragma once
// The game's loading thread (0x0059A6F0: CreateThread on 0x0059AD80, its loop 0x0059A7A0): the jobs queued for it run
// one at a time in the order they came. The loop waits for its event, takes the first job off the list under the
// list's lock (the event reset once the list is empty), marks it running (+0x08 2), runs it (vt+4), waits until the
// renderer's work the job left for the main thread is done (0x0051D270; here until the main thread has run the
// renderer's queue once more, setJobEnd) and marks it done (+0x08 3). A job still on the list can be taken off
// (0x0059A9D0); one running goes on to its end.

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
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

// Not of the game: the stock's race load (0x00461390) and its making of the front end after a race (0x00461A10) run
// on the loading thread while the main thread's frames go on, both threads using the game's objects (the sounds, the
// network, the interface, the input) with no lock. Here such a job takes turns with the frames: a frame holds the
// turn from its start to its present, the job runs between the frames and gives the turn back at its waits (the files
// it reads, the engine's loads) once a frame waits for it and its turn has lasted kJobTurnMs. The frames go on as the
// stock's (the loading window moves, the network's frames keep the peers) and neither thread meets the other's work
// halfway.
class FrameTurns {
public:
    // A turn of the job's at least this long before a frame takes the turn back (a frame limited to 60 a second leaves
    // the job more between its frames; without a limit the job still loads).
    static constexpr double kJobTurnMs = 4.0;

    // The main thread: its frame's start (after the job's turn) and its work done (before the present and the frame
    // limiter's wait).
    void beginFrame();
    void endFrame();
    // Native GL contexts belong to the main thread. Service queued uploads
    // while a loading job holds the turn and the frame waits to acquire it.
    void setFrameWaitService(std::function<void()> service);
    // The main thread waiting inside its frame for what the job holds (the engine's loads' lock): the turn given for
    // a moment.
    void pause();
    // The job's thread: the turn taken at the job's start (between the frames) and given back at its end.
    void beginJob();
    void endJob();
    // A wait of the job's (a file, a load): on the job's thread the turn given to a frame that waits for it, then
    // taken back after that frame; nothing on another thread.
    void yield();
    // The job's longest turn (ms) since its start, for the log.
    double longestJobTurn() const;

private:
    enum class Holder { None, Frame, Job };
    void takeForJob(std::unique_lock<std::mutex>& lock);
    void giveFromJob();

    mutable std::mutex m_lock;
    std::condition_variable m_changed;
    Holder m_holder = Holder::None;
    bool m_frameWaiting = false;
    bool m_jobWaiting = false;
    uint64_t m_jobTurns = 0;           // the turns the job has taken
    uint64_t m_jobTurnsAtFrameEnd = 0;  // their count as the last frame ended
    std::chrono::steady_clock::time_point m_turnStart;
    double m_longestTurn = 0.0;
    std::function<void()> m_frameWaitService;
};

// The turns of the game's frames and its loading job.
FrameTurns& frameTurns();

}  // namespace oj
