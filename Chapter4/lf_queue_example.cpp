#include "lf_queue.h"
#include "thread_utils.h"

#include <atomic>

struct MyStruct {
    int d_[3];
};

using namespace Common;

void consumeFunction(LFQueue<MyStruct>* lfq, const std::atomic<bool>* producer_done)
{
    while (!producer_done->load(std::memory_order_acquire) || !lfq->empty()) {
        if (const auto* next = lfq->getNextToRead()) {
            const auto value = *next;
            lfq->updateReadIndex();

            std::cout << "consumeFunction read elem:" << value.d_[0] << "," << value.d_[1] << "," << value.d_[2] << " lfq-size:" << lfq->size() << std::endl;
        } else {
            std::this_thread::yield();
        }
    }

    std::cout << "consumeFunction exiting." << std::endl;
}

int main(int, char**)
{
    LFQueue<MyStruct> lfq(20);
    std::atomic<bool> producer_done { false };

    auto ct = createAndStartThread(-1, "", consumeFunction, &lfq, &producer_done);

    for (auto i = 0; i < 50; ++i) {
        const MyStruct d { i, i * 10, i * 100 };

        auto* next = lfq.getNextToWriteTo();
        while (next == nullptr) {
            std::this_thread::yield();
            next = lfq.getNextToWriteTo();
        }

        *next = d;
        lfq.updateWriteIndex();

        std::cout << "main constructed elem:" << d.d_[0] << "," << d.d_[1] << "," << d.d_[2] << " lfq-size:" << lfq.size() << std::endl;

        // use 1s 1ms
        using namespace std::literals::chrono_literals;
        std::this_thread::sleep_for(1s);
    }

    producer_done.store(true, std::memory_order_release);
    ct.join();

    std::cout << "main exiting." << std::endl;

    return 0;
}
