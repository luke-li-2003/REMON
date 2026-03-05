#include "unit_tests.h"
#include "helperFunctions.h"
#include "SpinLock.h"
#include "config.h"
#include "define.h"
#include "pod.h"
#include "TenantContext.h"
#include "tenantPod.h"

#include <thread>
#include <atomic>
#include <sys/mman.h>
#include <unistd.h>

int unit_tests::runAll() {
    UnitTestRunner runner;

    // Shared / utility tests
    std::cout << "\n--- Shared / Utility Tests ---" << std::endl;
    runner.addTest("tokenizeString_basic", [this]() { test_tokenizeString_basic(); });
    runner.addTest("tokenizeString_empty", [this]() { test_tokenizeString_empty(); });
    runner.addTest("tokenizeString_single_token", [this]() { test_tokenizeString_single_token(); });
    runner.addTest("tokenizeString_whitespace_variations", [this]() { test_tokenizeString_whitespace_variations(); });
    runner.addTest("spinlock_lock_unlock", [this]() { test_spinlock_lock_unlock(); });
    runner.addTest("spinlock_trylock", [this]() { test_spinlock_trylock(); });
    runner.addTest("config_defaults", [this]() { test_config_defaults(); });
    runner.addTest("clearSwapDir_empty_path", [this]() { test_clearSwapDir_empty_path(); });
    runner.addTest("clearSwapDir_nonexistent_path", [this]() { test_clearSwapDir_nonexistent_path(); });
    runner.addTest("clearSwapDir_real_dir", [this]() { test_clearSwapDir_real_dir(); });

    // Compute node tests
    std::cout << "\n--- Compute Node Tests ---" << std::endl;
    runner.addTest("pod_create_and_state", [this]() { test_pod_create_and_state(); });
    runner.addTest("pod_state_transitions", [this]() { test_pod_state_transitions(); });
    runner.addTest("pod_lock_unlock", [this]() { test_pod_lock_unlock(); });
    runner.addTest("helperFunctions_pointerArith", [this]() { test_helperFunctions_pointerArith(); });

    // Memory node tests
    std::cout << "\n--- Memory Node Tests ---" << std::endl;
    runner.addTest("tenantPod_create_destroy", [this]() { test_tenantPod_create_destroy(); });
    runner.addTest("tenantPod_payload_readwrite", [this]() { test_tenantPod_payload_readwrite(); });
    runner.addTest("tenantContext_create_destroy", [this]() { test_tenantContext_create_destroy(); });
    runner.addTest("tenantContext_pod_insert_lookup_delete", [this]() { test_tenantContext_pod_insert_lookup_delete(); });
    runner.addTest("tenantContext_pod_not_found", [this]() { test_tenantContext_pod_not_found(); });

    return runner.run();
}

void unit_tests::test_tokenizeString_basic() {
    auto tokens = tokenizeString("hello world foo");
    ASSERT_EQ(tokens.size(), (size_t)3);
    ASSERT_EQ(tokens[0], "hello");
    ASSERT_EQ(tokens[1], "world");
    ASSERT_EQ(tokens[2], "foo");
}

void unit_tests::test_tokenizeString_empty() {
    auto tokens = tokenizeString("");
    ASSERT_EQ(tokens.size(), (size_t)0);
}

void unit_tests::test_tokenizeString_single_token() {
    auto tokens = tokenizeString("single");
    ASSERT_EQ(tokens.size(), (size_t)1);
    ASSERT_EQ(tokens[0], "single");
}

void unit_tests::test_tokenizeString_whitespace_variations() {
    auto tokens = tokenizeString("  a   b  c  ");
    ASSERT_EQ(tokens.size(), (size_t)3);
    ASSERT_EQ(tokens[0], "a");
    ASSERT_EQ(tokens[1], "b");
    ASSERT_EQ(tokens[2], "c");
}

void unit_tests::test_spinlock_lock_unlock() {
    SpinLock sl("test_lock");
    sl.lock("test");
    sl.unlock("test");
    sl.lock("test2");
    sl.unlock("test2");
}

void unit_tests::test_spinlock_trylock() {
    SpinLock sl("test_trylock");

    ASSERT_TRUE(sl.tryLock("test"));
    sl.unlock("test");

    sl.lock("main_hold");
    std::atomic<bool> tryResult{false};
    std::thread t([&]() {
        tryResult = sl.tryLock("other_thread");
        if (tryResult) {
            sl.unlock("other_thread");
        }
    });
    t.join();
    ASSERT_FALSE(tryResult.load());
    sl.unlock("main_hold");
}

void unit_tests::test_config_defaults() {
    ASSERT_TRUE(MAX_RAM_DEFAULT > 0);
    ASSERT_TRUE(MAX_VM_DEFAULT > 0);
    ASSERT_TRUE(DEFAULT_PAGE_SIZE > 0);
    ASSERT_EQ(DEFAULT_PAGE_SIZE % K4, (size_t)0);
}

void unit_tests::test_clearSwapDir_empty_path() {
    config cfg;
    ASSERT_NO_THROW(cfg.clearSwapDir(getpid(), ""));
}

void unit_tests::test_clearSwapDir_nonexistent_path() {
    config cfg;
    ASSERT_NO_THROW(cfg.clearSwapDir(getpid(), "/tmp/remon_test_nonexistent_path_xyz_99999"));
}

void unit_tests::test_clearSwapDir_real_dir() {
    config cfg;
    std::string testDir = "/tmp/remon_unit_test_clearswap_" + std::to_string(getpid());
    std::filesystem::create_directories(testDir);
    std::ofstream ofs(testDir + "/dummy.txt");
    ofs << "test data";
    ofs.close();
    ASSERT_TRUE(std::filesystem::exists(testDir + "/dummy.txt"));

    cfg.clearSwapDir(getpid(), testDir);
    ASSERT_FALSE(std::filesystem::exists(testDir + "/dummy.txt"));
    std::filesystem::remove_all(testDir);
}

void unit_tests::test_pod_create_and_state() {
    size_t pageSize = K4;
    void* mem = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(mem, MAP_FAILED);

    int fd = -1;
    pod p(pageSize, mem, 0, fd);
    ASSERT_EQ(p.getState(), podState::podStateT::init);
    ASSERT_EQ(p.getPayload(), mem);
    ASSERT_EQ(p.getI(), 0);
}

void unit_tests::test_pod_state_transitions() {
    size_t pageSize = K4;
    void* mem = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(mem, MAP_FAILED);

    pod p(pageSize, mem, 1, -1);

    ASSERT_EQ(p.getState(), podState::podStateT::init);
    bool ok = p.initToLocal();
    ASSERT_TRUE(ok);
    ASSERT_EQ(p.getState(), podState::podStateT::local);

    p.localToLocalPinned();
    ASSERT_EQ(p.getState(), podState::podStateT::localPinned);

    p.localPinnedToLocal();
    ASSERT_EQ(p.getState(), podState::podStateT::local);

    bool freed = p.localToInit();
    ASSERT_TRUE(freed);
    ASSERT_EQ(p.getState(), podState::podStateT::init);
}

void unit_tests::test_pod_lock_unlock() {
    size_t pageSize = K4;
    void* mem = mmap(nullptr, pageSize, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    ASSERT_NE(mem, MAP_FAILED);

    pod p(pageSize, mem, 2, -1);
    p.lock("test_lock");
    p.unlock("test_lock");

    bool got = p.tryLock("test_trylock");
    ASSERT_TRUE(got);
    p.unlock("test_trylock");
}

void unit_tests::test_helperFunctions_pointerArith() {
    void* base = (void*)0x1000;
    void* result = pointerPlusOffset(base, 0x100);
    ASSERT_EQ((uintptr_t)result, (uintptr_t)0x1100);

    size_t diff = pointerMinusPointer(result, base);
    ASSERT_EQ(diff, (size_t)0x100);
}

void unit_tests::test_tenantPod_create_destroy() {
    size_t pageSize = K4;
    {
        tenantPod tp(pageSize);
        ASSERT_NE(tp.getPayload(), nullptr);
        ASSERT_NE(tp.getPayload(), (char*)MAP_FAILED);
        ASSERT_EQ(tp.getPageSize(), pageSize);
    }
}

void unit_tests::test_tenantPod_payload_readwrite() {
    size_t pageSize = K4;
    tenantPod tp(pageSize);
    char* payload = tp.getPayload();
    for (size_t i = 0; i < pageSize; i++) {
        payload[i] = (char)(i & 0xFF);
    }
    for (size_t i = 0; i < pageSize; i++) {
        ASSERT_EQ((unsigned char)payload[i], (unsigned char)(i & 0xFF));
    }
}

void unit_tests::test_tenantContext_create_destroy() {
    {
        TenantContext ctx(1.0);
        ASSERT_TRUE(ctx.tenantPods.empty());
        ASSERT_NE(ctx.ramWatcher, nullptr);
    }
}

void unit_tests::test_tenantContext_pod_insert_lookup_delete() {
    TenantContext ctx(1.0);
    size_t pageSize = K4;
    auto* tp = new tenantPod(pageSize);
    std::string key = "42";

    ctx.lockTenantPodsLock();
    ctx.tenantPods.insert(std::make_pair(key, tp));
    ctx.lockTenantPodsUnlock();

    ASSERT_EQ(ctx.tenantPods.size(), (size_t)1);

    ctx.lockTenantPodsLock();
    auto it = ctx.tenantPods.find(key);
    ASSERT_TRUE(it != ctx.tenantPods.end());
    ASSERT_EQ(it->second, tp);
    ASSERT_EQ(it->second->getPageSize(), pageSize);
    ctx.lockTenantPodsUnlock();

    ctx.lockTenantPodsLock();
    it = ctx.tenantPods.find(key);
    delete it->second;
    ctx.tenantPods.erase(it);
    ctx.lockTenantPodsUnlock();

    ASSERT_TRUE(ctx.tenantPods.empty());
}

void unit_tests::test_tenantContext_pod_not_found() {
    TenantContext ctx(1.0);
    ctx.lockTenantPodsLock();
    auto it = ctx.tenantPods.find("nonexistent_key");
    ASSERT_TRUE(it == ctx.tenantPods.end());
    ctx.lockTenantPodsUnlock();
}
