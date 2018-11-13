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

    /*!
     * \brief work_queue creates an instance of a work queue with given capacity,
     *        wait interval on dequeue and atomic boolean halt flag.
     *
     * \param halt_flag a reference to an atomic<bool> variable.  When the variable becomes true,
     *        any pending dequeue calls will return within one wait interval.
     * \param max_depth the maximum number of queued work items -- older items will be dropped
     *        when this limit is exceeded. Defaults to SIZE_MAX, i.e. essentially unbounded.
     * \param wait_interval_ms wait interval in whole milliseconds, defaulting to 100ms
     *        the dequeue method will wait for this amount of time for new work to arrive before
     *        stopping to check the halt_flag. NOTE THAT the dequeue method still waits until new work
     *        is available.
     *        The condition on which the dequeue method returns is (work available || halting).
     */
    work_queue(std::atomic<bool> & halt_flag, size_t max_depth = SIZE_MAX, int wait_interval_ms = 100)
        : shutting_down(halt_flag)
        , wait_interval(wait_interval_ms)
        , n_dropped(0)
        , n_handled(0)
        , max(max_depth)
        , m()
        , cv()
        , unguarded_queue()
    { }

    ~work_queue() {  }

    /*!
     * \brief enqueue adds the work item to the queue, dropping the oldest item if the queue is saturated
     * \param work_item
     */
    void enqueue(T work_item) {
        {
            std::unique_lock<std::mutex> l(m);
            if (unguarded_queue.size() >= max) {
                unguarded_queue.pop();
            }
            unguarded_queue.push(work_item);
        }
        cv.notify_one();
    }

    /*!
     * \brief dequeue remove the oldest work item on the queue, and return it
     * \return a work item if available; otherwise blocks until shutting down.
     * Returns the null value of T in that case.  See also parameter wait_interval, default 100ms.
     */
    T dequeue() {
        std::unique_lock<std::mutex> l(m);
        auto still_waiting = true;
        while (still_waiting) {
            // the island of doubt
            still_waiting = !cv.wait_for(l, wait_interval*1ms, [&]{ return (shutting_down || !unguarded_queue.empty()); });
        }
        T val;
        if (!unguarded_queue.empty()) {
            n_handled++;

            val = unguarded_queue.front();
            unguarded_queue.pop();
        }
        return val;
    }
    /*!
     * \brief size returns the number of work items in the queue
     * \return the count of items in the queue
     */
    size_t size() const {
        std::unique_lock<std::mutex> l(m);
        return unguarded_queue.size();
    }

    /*!
     * \brief dropped returns the number of work items dropped so far because the queue was saturated.
     *        resets the counter of dropped items to zero.
     *        intended to be called rarely/for diagnostic or debugging purposes.
     * \return number items dropped since last reset
     */
    int dropped () {
        std::unique_lock<std::mutex> l(m);

        int so_far = n_dropped;
        n_dropped = 0;
        return so_far;
    }
    /*!
     * \brief handled returns the number of work items handled (dequeued) so far.
     *        resets the counter of handled items to zero.
     *        intended to be called rarely/for diagnostic or debugging purposes.
     * \return number of work items handled since last reset
     */
    int handled () {
        std::unique_lock<std::mutex> l(m);

        int so_far = n_handled;
        n_handled = 0;
        return so_far;
    }

    size_t getMax() const {
        std::unique_lock<std::mutex> l(m);
        return max;
    }
    void setMax(const size_t &value) {
        std::unique_lock<std::mutex> l(m);
        max = value;
    }

    int getWaitInterval() const {
        std::unique_lock<std::mutex> l(m);
        return wait_interval;
    }
    void setWaitInterval(int value) {
        std::unique_lock<std::mutex> l(m);
        wait_interval = value;
    }

private:

    std::atomic<bool> & shutting_down;
    int wait_interval; // units 1msec

    int n_dropped;
    int n_handled;

    size_t max;

    mutable std::mutex m;
    std::condition_variable cv;

    std::queue<T> unguarded_queue;

};

#endif // WORK_QUEUE_H
