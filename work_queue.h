#ifndef WORK_QUEUE_H
#define WORK_QUEUE_H

#include <algorithm>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <atomic>

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
     * \param flush_on_halt defaults to false; when the halt_flag is set, should the queue flush remaining work or
     *        allow it to be processed?
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
     * \brief enqueue adds the work item to the queue, dropping the oldest item if the queue is saturated;
     *                if the queue is shutting down, the item is not enqueued (i.e. the std::unique_ptr to it is not
     *                moved and the caller retains ownership). Empty std::unique_ptrs are ignored -- i.e. not pushed.
     * \param work_item a std::unique_ptr<T> to a work item to be enqueued for processing.
     */
    void enqueue(std::unique_ptr<T> work_item)
    {
        {   // locked context
            std::unique_lock<std::mutex> l(m);

            // don't enqueue when shutting down, or when passed an "empty" unique_ptr.
            if (shutting_down || !work_item) return;

            if (unguarded_queue.size() >= max) {
                n_dropped++;
                unguarded_queue.pop();
            }
            unguarded_queue.push(std::move(work_item));
        }   // end locked context

        cv.notify_one();
    }

    /*!
     * \brief enqueue adds all the elements of the vector "bulk" to the queue, unless the queue is shutting down,
     *                in which case the items are unaffected (i.e. the std::unique_ptrs in bulk are not
     *                moved and the caller retains ownership). Empty std::unique_ptrs in bulk are ignored-- i.e.
     *                not pushed.
     *                This supports bulk enqueueing without toggling the lock for each entry.
     * \param bulk a std::vector of std::unique_ptr<T> objects
     */
    void enqueue(std::vector<std::unique_ptr<T> > & bulk)
    {
        {   // locked context:
            std::unique_lock<std::mutex> l(m);

            if (shutting_down) return;

            // only count non-empty unique_ptrs, since only those are pushed.
            size_t bulk_size = 0;
            for (auto & work_item: bulk)
                if (work_item) bulk_size++;

            // nothing to do:
            if (bulk_size == 0) return;

            const int plus_bulk = (unguarded_queue.size() + bulk_size);
            if (plus_bulk > max) {
                for (int i = 0; i < plus_bulk - max; i++) {
                    n_dropped++;
                    unguarded_queue.pop();
                }
            }

            for (auto & work_item: bulk) {
                unguarded_queue.push(std::move(work_item));
            }
        }   // end locked context

        cv.notify_one();
    }

    /*!
     * \brief dequeue remove the oldest work item on the queue, and return it
     * \return a work item if available; otherwise blocks until shutting down or a work item becomes available.
     *
     * Returns the null value of T in the case of shutdown.  See also parameter wait_interval, default 100ms.
     * The atomic variable represented locally as shutting_down is set by the caller to initiate an orderly shutdown.
     */
    std::unique_ptr<T> dequeue() {
        std::unique_lock<std::mutex> l(m);
        auto waiting = unguarded_queue.empty() && !shutting_down;

        while (waiting) {
            waiting = !cv.wait_for(l, wait_interval*1ms, [&]{ return (shutting_down || !unguarded_queue.empty()); });
        }

        if (shutting_down) {
            return std::unique_ptr<T>{};
        } else if (!unguarded_queue.empty()) {
            n_handled++;
            std::unique_ptr<T> val = std::move(unguarded_queue.front());
            unguarded_queue.pop();
            return val;
        }
    }

    /*!
     * \brief size returns the number of work items in the queue
     * \return the count of items in the queue (or 0 if shutting down)
     */
    size_t size() const {
        std::unique_lock<std::mutex> l(m);

        if (shutting_down) return 0;

        return unguarded_queue.size();
    }

    /*!
     * \brief dropped returns the number of work items dropped so far because the queue was saturated.
     *        resets the counter of dropped items to zero.
     *        intended to be called rarely/for diagnostic or debugging purposes.
     * \return number items dropped since last call
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
     * \return number of work items handled since last call
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

    std::queue<std::unique_ptr<T> > unguarded_queue;

};

#endif // WORK_QUEUE_H
