#ifndef REMON_TESTS_H
#define REMON_TESTS_H

#define BIG_SORT ((size_t) GB * (size_t)4)

#include <remon.h>
#include <algorithm>
#include <random>

class tests {
public:
    void activateThenDeactivate(remon_vmm &vmm);
    void activateThenDeactivateNtimes(int n);

    void activateThenTouch(bool ifTouch);
    void smallAllocation();
    void sortTest();
    void sortPartitionTest();
    void integrityTest();
    void integrityTestManySmalls();

    void mallocMultiPages();

    void integrityTestManyHundredsMB();

    void tinyAllocation();

    void integrityTestMixedLoad();

    void sortParallelTest();

    void sortPartitionTestPromote();
};


#endif //REMON_TESTS_H
