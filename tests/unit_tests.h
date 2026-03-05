#ifndef REMON_UNIT_TESTS_H
#define REMON_UNIT_TESTS_H

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <cassert>
#include <functional>
#include <cstring>
#include <filesystem>

struct TestResult {
    std::string name;
    bool passed;
    std::string message;
};

class UnitTestRunner {
private:
    std::vector<std::pair<std::string, std::function<void()>>> tests;
    std::vector<TestResult> results;
public:
    void addTest(const std::string& name, std::function<void()> fn) {
        tests.push_back({name, fn});
    }

    int run() {
        int passed = 0, failed = 0;
        for (auto& [name, fn] : tests) {
            try {
                fn();
                results.push_back({name, true, ""});
                std::cout << "  [PASS] " << name << std::endl;
                passed++;
            } catch (const std::exception& e) {
                results.push_back({name, false, e.what()});
                std::cout << "  [FAIL] " << name << ": " << e.what() << std::endl;
                failed++;
            }
        }
        std::cout << "\n=== " << passed << " passed, " << failed << " failed, "
                  << (passed + failed) << " total ===" << std::endl;
        return failed > 0 ? 1 : 0;
    }
};

#define ASSERT_TRUE(cond) \
    do { if (!(cond)) throw std::runtime_error(std::string("ASSERT_TRUE failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); } while(0)

#define ASSERT_FALSE(cond) \
    do { if ((cond)) throw std::runtime_error(std::string("ASSERT_FALSE failed: ") + #cond + " at " + __FILE__ + ":" + std::to_string(__LINE__)); } while(0)

#define ASSERT_EQ(a, b) \
    do { if ((a) != (b)) { std::ostringstream _ss; _ss << "ASSERT_EQ failed: " << #a << " (" << (a) << ") != " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__; throw std::runtime_error(_ss.str()); } } while(0)

#define ASSERT_NE(a, b) \
    do { if ((a) == (b)) { std::ostringstream _ss; _ss << "ASSERT_NE failed: " << #a << " (" << (a) << ") == " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__; throw std::runtime_error(_ss.str()); } } while(0)

#define ASSERT_GE(a, b) \
    do { if ((a) < (b)) { std::ostringstream _ss; _ss << "ASSERT_GE failed: " << #a << " (" << (a) << ") < " << #b << " (" << (b) << ") at " << __FILE__ << ":" << __LINE__; throw std::runtime_error(_ss.str()); } } while(0)

#define ASSERT_NO_THROW(expr) \
    do { try { expr; } catch (const std::exception& e) { throw std::runtime_error(std::string("ASSERT_NO_THROW failed: ") + e.what() + " at " + __FILE__ + ":" + std::to_string(__LINE__)); } } while(0)

class unit_tests {
public:
    int runAll();

    void test_tokenizeString_basic();
    void test_tokenizeString_empty();
    void test_tokenizeString_single_token();
    void test_tokenizeString_whitespace_variations();

    void test_spinlock_lock_unlock();
    void test_spinlock_trylock();

    void test_config_defaults();
    void test_clearSwapDir_empty_path();
    void test_clearSwapDir_nonexistent_path();
    void test_clearSwapDir_real_dir();

    void test_tenantPod_create_destroy();
    void test_tenantPod_payload_readwrite();
    void test_tenantContext_create_destroy();
    void test_tenantContext_pod_insert_lookup_delete();
    void test_tenantContext_pod_not_found();

    void test_pod_create_and_state();
    void test_pod_state_transitions();
    void test_pod_lock_unlock();
    void test_helperFunctions_pointerArith();
};

#endif //REMON_UNIT_TESTS_H
