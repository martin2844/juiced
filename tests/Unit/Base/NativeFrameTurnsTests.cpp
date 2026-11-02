#include "Framework/Test.h"
#include "Core/Base/LoadingThread.h"
#include <atomic>
#include <chrono>
#include <future>
#include <thread>

OJ_UNIT_TEST(FrameTurns, NativeUploadWhileWaiting) {
    oj::FrameTurns turns;
    std::promise<void> jobStarted;
    std::atomic<bool> uploadDone = false;
    bool servicedOnOwner = false;
    const auto owner = std::this_thread::get_id();
    turns.setFrameWaitService([&] {
        servicedOnOwner = std::this_thread::get_id() == owner;
        uploadDone = true;
    });
    std::thread loading([&] {
        turns.beginJob();
        jobStarted.set_value();
        // Model a GL upload: the loader needs work performed by the owner
        // before releasing its turn. A deadline makes failure finite.
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!uploadDone && std::chrono::steady_clock::now() < deadline)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        turns.endJob();
    });
    jobStarted.get_future().wait();
    turns.beginFrame();
    turns.endFrame();
    loading.join();
    turns.setFrameWaitService(nullptr);
    oj::test::check(uploadDone && servicedOnOwner,
                    "a frame waiting on the loader services its GL upload on the context owner");
}
