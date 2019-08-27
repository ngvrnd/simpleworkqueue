#include <cxxtest/TestSuite.h>
#include <iostream>
#define development//_trace
#include "work_queue.h"


class work_queue_test : public CxxTest::TestSuite
{
public:
    std::atomic<bool> haltflag;

    struct workpiece {
        int intvec[1000];
    };

    void testCreation(void)
    {


        work_queue<int> intqueue(haltflag);
        TS_TRACE("Testing work_queue creation:");
        TS_ASSERT_EQUALS(intqueue.size(),0);
        TS_TRACE("Finished testig work_queue creation");
    }


    void testInsertion(void) {

        std::unique_ptr<workpiece> unique_workpiece = std::make_unique<workpiece>();

        unique_workpiece->intvec[1]   = 1;
        unique_workpiece->intvec[10]  = 1;
        unique_workpiece->intvec[100] = 1;
        unique_workpiece->intvec[110] = 1;


        TS_TRACE("Check unique_ptr semantics");
        haltflag = false;
        work_queue<workpiece> intqueue(haltflag);
        TS_ASSERT_DIFFERS(unique_workpiece.get(), nullptr);
        intqueue.enqueue(std::move(unique_workpiece));
        TS_ASSERT_EQUALS(unique_workpiece.get(), nullptr);


        std::unique_ptr<workpiece> output = intqueue.dequeue();
        TS_ASSERT_DIFFERS(output.get(), nullptr);

        TS_ASSERT_EQUALS(output->intvec[1], 1);
        TS_ASSERT_EQUALS(output->intvec[10], 1);
        TS_ASSERT_EQUALS(output->intvec[100], 1);
        TS_ASSERT_EQUALS(output->intvec[110], 1);

        TS_ASSERT_EQUALS(output->intvec[3], 0);
        TS_ASSERT_EQUALS(output->intvec[33], 0);
        TS_ASSERT_EQUALS(output->intvec[333], 0);

        TS_TRACE("Basic unique_ptr semantics test finished.");

    }



    void populate_workpieces(std::vector<std::unique_ptr<workpiece> > & workpease)
    {
        for (int i = 0; i < 10; i++) {
            std::unique_ptr<workpiece> wp = std::make_unique<workpiece>();
            wp->intvec[i] = i;
            workpease.push_back(std::move(wp));
        }
    }

    void testEnqDeq(void) {

        TS_TRACE("Testing basic enq/deq functionality");
      // std::unique_ptr<workpiece> flerg = std::make_unique<workpiece>();

        std::vector<std::unique_ptr<workpiece> > workpease;


        populate_workpieces(workpease);

        work_queue<workpiece> wpq(haltflag);

        TS_ASSERT_EQUALS(wpq.size(), 0);

        wpq.enqueue(workpease);

        TS_ASSERT_EQUALS(wpq.size(), 10);

        for (int i = 0; i < 10; i++) {
            TS_ASSERT_EQUALS(workpease[i].get(), nullptr);
        }
        TS_TRACE("dequeueing work items");

        for (int i = 0; i < 10; i++) {
            workpease[i] = wpq.dequeue();
        }

        for (int i = 0; i < 10; i++) {
            TS_ASSERT_DIFFERS(workpease[i].get(), nullptr);
        }

        TS_ASSERT_EQUALS(wpq.size(), 0);

        wpq.enqueue(workpease);

        TS_ASSERT_EQUALS(wpq.size(), 10);

        for (int i = 0; i < 10; i++) {
            TS_ASSERT_EQUALS(workpease[i].get(), nullptr);
        }
        TS_TRACE("Set halt flag; test that all items are discarded.");

        haltflag = true;

        TS_TRACE("dequeueing work items with halt set");
        TS_ASSERT(!wpq.dequeue().get());

        for (int i = 0; i < 10; i++) {
            workpease[i] = wpq.dequeue();
        }

        for (int i = 0; i < 10; i++) {
            TS_ASSERT_EQUALS(workpease[i].get(), nullptr);
        }

        TS_ASSERT_EQUALS(wpq.size(), 0);

        TS_TRACE("calling dequeue on an empty queue with halt flag set, should not block.");
        TS_ASSERT_EQUALS(wpq.dequeue().get(),nullptr);
        TS_ASSERT_EQUALS(wpq.dequeue().get(), nullptr);
        TS_ASSERT_EQUALS(wpq.dequeue().get(), nullptr);
        TS_ASSERT_EQUALS(wpq.dequeue().get(), nullptr);
        TS_ASSERT_EQUALS(wpq.dequeue().get(), nullptr);
    }

    void testClearingHaltFlag(void) {
        haltflag = false;

        std::vector<std::unique_ptr<workpiece> > workpease;

        populate_workpieces(workpease);

        work_queue<workpiece> wpq(haltflag);

        TS_ASSERT_EQUALS(wpq.size(), 0);

        wpq.enqueue(workpease);

        TS_ASSERT_EQUALS(wpq.size(), 10);
    }

    void testEnqueueNullPtr(void) {

        haltflag = false;

        work_queue<workpiece> wpq(haltflag);

        TS_TRACE("Enqueuing null unique ptrs.  Test that the queue stays empty.");

        wpq.enqueue(std::unique_ptr<workpiece>{});
        TS_ASSERT_EQUALS(wpq.size(), 0);

        std::vector<std::unique_ptr<workpiece> > workpease;

        for (int i = 0; i < 10; i++) {
            workpease.push_back(std::unique_ptr<workpiece>());
        }

        TS_ASSERT_EQUALS(workpease.size(), 10);

        wpq.enqueue(workpease);

        TS_ASSERT_EQUALS(wpq.size(), 0);

        TS_ASSERT_EQUALS(workpease.size(), 10);

        for (auto & workpeace : workpease) {
            TS_ASSERT_EQUALS(workpeace.get(), nullptr);
        }

        TS_TRACE("Enqueuing null unique ptrs has no effect.");

    }

    void testWithThreads(void) {


    }

};
