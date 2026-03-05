#include <iostream>
#include <cstring>
#include "remon.h"
#include "tests.h"
#include "unit_tests.h"

using namespace std;
int main(int argc, char** argv) {
    if (argc > 1 && strcmp(argv[1], "--unit") == 0) {
        cout << "=== REMON Unit Tests ===" << endl;
        unit_tests ut;
        return ut.runAll();
    }

    // tests test;
    // test.activateThenTouch(true);
    // test.sortTest();
    // test.sortPartitionTestPromote();
    // test.sortPartitionTest();
    // test.sortParallelTest();
    // test.smallAllocation();
    // test.integrityTestManySmalls();
    // test.integrityTestManyHundredsMB();
    // test.integrityTestMixedLoad();
    // test.tinyAllocation();
    return 0;
}
