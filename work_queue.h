#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>

using namespace std::chrono_literals;

/*!
 * work_queue - a templated class to manage a work queue between producer and consumer threads.
 * the work items are the template parameter T.
 */
template <class T>
class work_queue
{
public:
    work_queue(size_t max_size, std::atomic<bool> & halt_flag)
        : max(max_size)
        , underlying_container()
        , m()
        , cv()
        , shutting_down(halt_flag) {  }

    ~work_queue() {  }

    void enqueue(T work_item) {
        std::unique_lock<std::mutex> l(m);
        if (underlying_container.size() >= max) {
            // two thoughts: 1) ought never to be > max; raise an error if so?
            //               2) ought to call cv.notify here?
            //                  a) should not be necessary therefore no
            //                  b) what harm & could avoid deadlock therefore yes
            //std::cerr << "_" <<underlying_container.size() << "_  (max=" << max << ")";
            drops++;
            //discard the oldest item.
            underlying_container.pop();
        }
        underlying_container.push(work_item);
        cv.notify_one();
    }

    T dequeue() {
        std::unique_lock<std::mutex> l(m);
        jobs++;
        bool still_waiting = !cv.wait_for(l, 100ms, [&]{ return (shutting_down || !underlying_container.empty()); });

        while (still_waiting) {
            still_waiting = !cv.wait_for(l, 100ms, [&]{ return (shutting_down || !underlying_container.empty()); });
        }
        T val;
        if (shutting_down) {
        } else {
            val = underlying_container.front();
            underlying_container.pop();
        }
        return val;
    }

    size_t size() {
        std::unique_lock<std::mutex> l(m);
        return underlying_container.size();
    }

    int dropped () {
        int so_far = drops;
        drops = 0;
        return so_far;
    }

    int handled () {
        int so_far = jobs;
        jobs = 0;
        return so_far;
    }

private:

    std::atomic<bool> & shutting_down;

    int drops;
    int jobs;
    size_t max;
    mutable std::mutex m;
    std::condition_variable cv;

    std::queue<T> underlying_container;

};

#endif // WORK_QUEUE_H
