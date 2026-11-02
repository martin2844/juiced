// Unit tests: the game's loading thread (Core/Base/LoadingThread.h).

#include "Framework/Test.h"

#include "Core/Base/LoadingThread.h"
#include "Core/Base/Random.h"
#include "Game/Race/TrackView/TrackView.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

using oj::test::check;

// The time given, kept on the clock (a sleep takes the system's timer: a millisecond's sleep lasts up to 15.6 ms).
void spinFor(std::chrono::microseconds time) {
    const auto end = std::chrono::steady_clock::now() + time;
    while (std::chrono::steady_clock::now() < end) std::this_thread::yield();
}

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

// A job taking turns with the frames (FrameTurns) runs only between them: never while a frame holds the turn, the
// frames going on meanwhile (each waits at most for the job's next wait), and the job's turn handed back only at its
// waits once a frame waits and kJobTurnMs has passed.
OJ_UNIT_TEST(FrameTurns, JobBetweenFrames) {
    oj::FrameTurns turns;
    oj::LoadingThread thread;
    std::atomic<bool> inFrame = false;
    std::atomic<int> met = 0;      // the job's work seen inside a frame
    std::atomic<int> waits = 0;    // the job's waits passed
    const auto job = thread.queue([&] {
        turns.beginJob();
        for (int i = 0; i < 400; ++i) {
            if (inFrame) ++met;
            spinFor(std::chrono::microseconds(500));  // the job's work
            if (inFrame) ++met;
            turns.yield();
            ++waits;
        }
        turns.endJob();
    });
    int frames = 0;
    double longestFrameWait = 0.0;
    while (!job->done()) {
        const auto before = std::chrono::steady_clock::now();
        turns.beginFrame();
        const std::chrono::duration<double, std::milli> waited = std::chrono::steady_clock::now() - before;
        longestFrameWait = std::max(longestFrameWait, waited.count());
        inFrame = true;
        spinFor(std::chrono::milliseconds(2));  // the frame's work
        inFrame = false;
        turns.endFrame();
        spinFor(std::chrono::milliseconds(8));  // the present and the limiter
        ++frames;
    }
    check(met == 0 && waits == 400, "the job's work never inside a frame (" + std::to_string(met.load()) +
                                        " times), all of it done");
    check(frames >= 10, "the frames went on during the job (" + std::to_string(frames) + ")");
    check(longestFrameWait < 50.0, "a frame waited for the job's next wait only (" +
                                       std::to_string(longestFrameWait) + " ms)");
    check(turns.longestJobTurn() >= oj::FrameTurns::kJobTurnMs,
          "the job's turns last kJobTurnMs at least (" + std::to_string(turns.longestJobTurn()) + " ms)");
}

// Frames with no time between them (no limiter, no vsync) still leave the job a turn after each: it loads.
OJ_UNIT_TEST(FrameTurns, JobWithoutLimiter) {
    oj::FrameTurns turns;
    oj::LoadingThread thread;
    std::atomic<int> waits = 0;
    const auto job = thread.queue([&] {
        turns.beginJob();
        for (int i = 0; i < 50; ++i) {
            spinFor(std::chrono::milliseconds(1));
            turns.yield();
            ++waits;
        }
        turns.endJob();
    });
    const auto start = std::chrono::steady_clock::now();
    int frames = 0;
    while (!job->done() && std::chrono::steady_clock::now() - start < std::chrono::seconds(10)) {
        turns.beginFrame();
        turns.endFrame();
        ++frames;
    }
    check(job->done() && waits == 50, "the job done between frames with no gap (" + std::to_string(frames) +
                                          " frames)");
}

// A frame waiting for what the job holds (the engine's loads' lock) gives it the turn meanwhile (pause): the job goes
// on to let it go, and the frame's wait ends.
OJ_UNIT_TEST(FrameTurns, PauseLetsTheJobOn) {
    oj::FrameTurns turns;
    oj::LoadingThread thread;
    std::mutex held;  // what the job holds across its waits
    std::atomic<bool> holding = false;
    std::atomic<bool> release = false;
    const auto job = thread.queue([&] {
        turns.beginJob();
        held.lock();
        holding = true;
        while (!release) {
            spinFor(std::chrono::milliseconds(1));
            turns.yield();
        }
        for (int i = 0; i < 10; ++i) {
            spinFor(std::chrono::milliseconds(1));
            turns.yield();
        }
        held.unlock();
        turns.endJob();
    });
    while (!holding) spinFor(std::chrono::milliseconds(1));
    turns.beginFrame();
    release = true;
    int pauses = 0;
    while (!held.try_lock()) {
        turns.pause();
        ++pauses;
        spinFor(std::chrono::milliseconds(1));
    }
    held.unlock();
    turns.endFrame();
    thread.wait(*job, nullptr);
    check(job->done() && pauses >= 1, "the frame's wait ended after the job's turns (" + std::to_string(pauses) +
                                          " pauses)");
}

// The C runtime's rand() is the thread's: on the game's loading thread its jobs draw the loading thread's generator,
// one for the whole game, its state going on from job to job; elsewhere the game's. The race's load draws it
// (TrackView::loading), never made again for a race.
OJ_UNIT_TEST(LoadingThread, ItsOwnRand) {
    oj::CrtRandom& game = oj::gameRandom();
    oj::CrtRandom& loading = oj::loadingRandom();
    check(&oj::threadRandom() == &game, "the main thread: the game's rand()");
    const oj::CrtRandom gameBefore = game;
    oj::CrtRandom expected = loading;
    const int first = expected.next();
    const int second = expected.next();
    oj::CrtRandom* onThread = nullptr;
    int drawnFirst = -1, drawnSecond = -1;
    const auto job1 = oj::loadingThread().queue([&] {
        onThread = &oj::threadRandom();
        drawnFirst = oj::threadRandom().next();
    });
    oj::loadingThread().wait(*job1, nullptr);
    const auto job2 = oj::loadingThread().queue([&] { drawnSecond = oj::threadRandom().next(); });
    oj::loadingThread().wait(*job2, nullptr);
    check(onThread == &loading, "on the loading thread: its own rand()");
    check(drawnFirst == first && drawnSecond == second, "its state goes on from one job to the next");
    oj::CrtRandom gameAfter = game;
    oj::CrtRandom gameSame = gameBefore;
    check(gameAfter.next() == gameSame.next() && game.draws() == gameBefore.draws(), "the game's rand() untouched");
    auto view = std::make_unique<app::TrackView>();
    check(&view->loading == &loading, "the race's load draws the loading thread's rand()");
}
