// Unit tests: the game's loading thread (Core/Base/LoadingThread.h).

#include "Framework/Test.h"

#include "Core/Base/LoadingThread.h"

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace {

using oj::test::check;

}  // namespace

// The jobs run one at a time in the order they came, off the caller's thread; a job still queued is taken off and never
// runs (0x0059A9D0), one that has started or ended cannot be.
OJ_UNIT_TEST(LoadingThread, OrderAndCancel) {
    oj::LoadingThread thread;
    std::atomic<bool> release = false;
    std::atomic<bool> plugStarted = false;
    std::atomic<bool> elsewhere = false;
    const std::thread::id caller = std::this_thread::get_id();
    std::string ran;  // written by the loading thread alone, read after its jobs ended
    const auto plug = thread.queue([&] {
        plugStarted = true;
        elsewhere = std::this_thread::get_id() != caller;
        while (!release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        ran += "p";
    });
    const auto first = thread.queue([&] { ran += "1"; });
    const auto second = thread.queue([&] { ran += "2"; });
    const auto third = thread.queue([&] { ran += "3"; });
    while (!plugStarted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool running = plug->state() == oj::LoadJob::kRunning && first->state() == oj::LoadJob::kQueued;
    const bool taken = thread.cancel(second);
    const bool notThePlug = !thread.cancel(plug);
    release = true;
    thread.wait(*third, nullptr);
    check(running && elsewhere, "the first job running on the loading thread, the others queued");
    check(taken && notThePlug, "a queued job taken off; the running one not");
    check(ran == "p13" && first->done() && second->state() == oj::LoadJob::kQueued,
          "the jobs run in their order, the one taken off never (" + ran + ")");
    check(!thread.cancel(first), "a job that has ended cannot be taken off");
}

// The waits run their pump between checks, every 10 ms (0x0045A35B).
OJ_UNIT_TEST(LoadingThread, WaitPumps) {
    oj::LoadingThread thread;
    const auto job = thread.queue([] { std::this_thread::sleep_for(std::chrono::milliseconds(50)); });
    int pumped = 0;
    thread.wait(*job, [&] { ++pumped; });
    check(job->done() && pumped >= 2, "the wait pumped while the job ran (" + std::to_string(pumped) + " times)");
}

// A job is marked done only once the job's end has passed (0x0051D270: the main thread has run the renderer's queue
// again), run after the job's own work.
OJ_UNIT_TEST(LoadingThread, DoneAfterJobEnd) {
    oj::LoadingThread thread;
    std::atomic<bool> served = false;
    std::atomic<bool> waited = false;
    thread.setJobEnd([&] {
        waited = true;
        while (!served) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    });
    std::atomic<bool> ran = false;
    const auto job = thread.queue([&] { ran = true; });
    while (!waited) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool notYet = ran && job->state() == oj::LoadJob::kRunning;
    served = true;
    thread.wait(*job, nullptr);
    check(notYet && job->done(), "the job's work ran, done only after the wait for the main thread");
}
