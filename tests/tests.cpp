#include "tests.h"
#include "remon.h"
#include "execution"
#include <tbb/parallel_for.h>
#include <queue>


template<typename type>
class customAllocator{
private:
    remon_vmm& vmm;
public:
    using value_type = type;
    customAllocator(remon_vmm& vmm) : vmm(vmm){

    }
    type * allocate(size_t size) {
        void* addr = vmm.remonMalloc(size * sizeof(type));
        cout << "allocate size: " << size << " @ " << addr << endl;
        return (type *) addr;
    }
    void deallocate(type* ptr, size_t size) {
        cout << "deallocate size: " << size << " @ " << ptr << endl;
        vmm.remonFree(ptr);
    }

    template <typename U>
    bool operator==(const customAllocator<U>&) const noexcept {
        return true;
    }

    template <typename U>
    bool operator!=(const customAllocator<U>&) const noexcept {
        return false;
    }
};
void tests::sortTest() {
    remon_vmm vmm;
    std::chrono::time_point<std::chrono::steady_clock> from = std::chrono::steady_clock::now();
    customAllocator<size_t> vmmAllocator(vmm);
    vector<size_t, customAllocator<size_t>> vec(vmmAllocator);
    double maxRam = vmm.getMaxPM();
    cout << "max ram in GB: " << maxRam <<endl;
    // vector<size_t> vec;
    cout << "sort size: " << BIG_SORT << endl;
    vec.reserve(BIG_SORT);
    for(size_t i = BIG_SORT; i > 0; i--) {
        vec.push_back(i);
    }
    cout << "sort prep done" << endl;
    std::chrono::time_point<std::chrono::steady_clock> toA = std::chrono::steady_clock::now();
    sort(vec.begin(), vec.end());
    std::chrono::time_point<std::chrono::steady_clock> toB = std::chrono::steady_clock::now();
    int debugCount = 20;
    for(size_t i = 1; i <= BIG_SORT; i++) {
        if(i != vec[i-1]) {
            cout << "sort_test failed" << i << " != " <<vec[i-1]<<endl;
            debugCount --;
        }
        if(debugCount <= 0) {
            break;
        }
    }
    int msA = std::chrono::duration_cast<std::chrono::milliseconds>(toA - from).count();
    int msB = std::chrono::duration_cast<std::chrono::milliseconds>(toB - toA).count();
    cout << "setting up used: " << msA << " sort used: " << msB << endl;
}

struct heapEntry {
    size_t value;
    size_t sourceIndex;
    size_t currentIndex;

    bool operator>(const heapEntry& other) const {
        return value > other.value;
    }
};

void tests::sortPartitionTest() {
    remon_vmm vmm;
    std::chrono::time_point<std::chrono::steady_clock> from = std::chrono::steady_clock::now();
    customAllocator<size_t> vmmAllocator(vmm);
    vector<size_t, customAllocator<size_t>> vec(vmmAllocator);
    double maxRam = vmm.getMaxPM();
    cout << "max ram in GB: " << maxRam <<endl;
    cout << "sort size: " << BIG_SORT << endl;
    vec.reserve(BIG_SORT);
    for(size_t i = BIG_SORT; i > 0; i--) {
        vec.push_back(i);
    }
    cout << "sort prep done" << endl;
    cout << "last/min element is: " << vec.back() << endl;
    std::chrono::time_point<std::chrono::steady_clock> toA = std::chrono::steady_clock::now();
    
    size_t segmentSize = 0.25*maxRam * GB / sizeof(size_t);
    cout << "segment_size: " << segmentSize << endl;
    cout << "vector size: " << vec.size() << endl;
    vector<size_t, customAllocator<size_t>> vecFroms(vmmAllocator);
    vecFroms.reserve(ceil(vec.size() / segmentSize));
    vector<size_t, customAllocator<size_t>> vecTos(vmmAllocator);
    vecTos.reserve(ceil(vec.size() / segmentSize));
    if(segmentSize > vec.size()) {
        std::sort(vec.begin(), vec.end());
    } else {
        size_t toIndexInclusive = vec.size() - 1;
        while (toIndexInclusive > segmentSize) {
            size_t fromIndex = toIndexInclusive - segmentSize + 1;
            cout << "sort: [" << fromIndex << " ~ " << toIndexInclusive << "]" << endl;\
            cout << "last element: " << *(vec.begin() + toIndexInclusive) << endl;
            vecFroms.push_back(fromIndex);
            vecTos.push_back(toIndexInclusive);
            sort(vec.begin() + fromIndex, vec.begin() + toIndexInclusive + 1);
            cout << "first sorted element: " << *(vec.begin() + fromIndex) << endl;
            toIndexInclusive -= segmentSize;
        }

        if (toIndexInclusive > 0) {
            size_t fromIndex = 0;
            cout << "sort: [" << fromIndex << " ~ " << toIndexInclusive << ")" << endl;
            vecFroms.push_back(fromIndex);
            vecTos.push_back(toIndexInclusive);
            sort(vec.begin() + fromIndex, vec.begin() + toIndexInclusive + 1);
        }

        vector<size_t, customAllocator<size_t>> vecDest(vmmAllocator);
        vecDest.reserve(vec.size());
        std::priority_queue<heapEntry, std::vector<heapEntry>, std::greater<heapEntry>> minHeap;
        for (size_t i = 0; i < vecFroms.size(); ++i) {
            cout << "range: " << vecFroms[i] << " ~ " << vecTos[i] << endl;
            cout << "init push: " << vec[vecFroms[i]] << endl;
            minHeap.push({vec[vecFroms[i]], i, 0});
        }

        while (!minHeap.empty()) {
            heapEntry minElement = minHeap.top();
            minHeap.pop();
            vecDest.push_back(minElement.value);
            if (minElement.currentIndex < vecTos[minElement.sourceIndex] - vecFroms[minElement.sourceIndex]) {
                size_t nextIndex = vecFroms[minElement.sourceIndex] + minElement.currentIndex + 1;
                minHeap.push({vec[nextIndex], minElement.sourceIndex, minElement.currentIndex + 1});
            }
        }

        cout << "move" << endl;
        vec = std::move(vecDest);
    }
    std::chrono::time_point<std::chrono::steady_clock> toB = std::chrono::steady_clock::now();
    int debugCount = 20;
    for(size_t i = 1; i <= BIG_SORT; i++) {
        if(i != vec[i-1]) {
            cout << "sort_test failed: " << i << " != " <<vec[i-1]<<endl;
            debugCount --;
        }
        if(debugCount <= 0) {
            break;
        }
    }
    int msA = std::chrono::duration_cast<std::chrono::milliseconds>(toA - from).count();
    int msB = std::chrono::duration_cast<std::chrono::milliseconds>(toB - toA).count();
    cout << "setting up used: " << msA << " sort used: " << msB << endl;
}


void tests::sortPartitionTestPromote() {
    remon_vmm vmm;
    std::chrono::time_point<std::chrono::steady_clock> from = std::chrono::steady_clock::now();
    customAllocator<size_t> vmmAllocator(vmm);
    vector<size_t, customAllocator<size_t>> vec(vmmAllocator);
    double maxRam = vmm.getMaxPM();
    cout << "max ram in GB: " << maxRam <<endl;
    cout << "sort size: " << BIG_SORT << endl;
    vec.reserve(BIG_SORT);
    for(size_t i = BIG_SORT; i > 0; i--) {
        vec.push_back(i);
    }
    cout << "sort prep done" << endl;
    cout << "last/min element is: " << vec.back() << endl;
    std::chrono::time_point<std::chrono::steady_clock> toA = std::chrono::steady_clock::now();
    size_t segmentSize = 0.25*maxRam * GB / sizeof(size_t);
    cout << "segment_size: " << segmentSize << endl;
    cout << "vector size: " << vec.size() << endl;
    vector<size_t, customAllocator<size_t>> vecFroms(vmmAllocator);
    vecFroms.reserve(ceil(vec.size() / segmentSize) + 1);
    vector<size_t, customAllocator<size_t>> vecTos(vmmAllocator);
    vecTos.reserve(ceil(vec.size() / segmentSize) + 1);
    if(segmentSize > vec.size()) {
        std::sort(vec.begin(), vec.end());
    } else {
        size_t toIndexInclusive = vec.size() - 1;
        while (toIndexInclusive > segmentSize) {
            size_t fromIndex = toIndexInclusive - segmentSize + 1;
            vecFroms.push_back(fromIndex);
            vecTos.push_back(toIndexInclusive);
            toIndexInclusive -= segmentSize;
        }

        if (toIndexInclusive > 0) {
            size_t fromIndex = 0; 
            vecFroms.push_back(fromIndex);
            vecTos.push_back(toIndexInclusive);
            // sort(vec.begin() + from_index, vec.begin() + to_index_inclusive + 1);
        }
        vmm.remonPromoteRange(&*(vec.begin() + vecFroms[0]), &*(vec.begin() + vecTos[0]));
        for (size_t i = 0; i < vecFroms.size(); ++i) {
            if(i + 1 < vecFroms.size()) {
                vmm.remonPromoteRange(&*(vec.begin() + vecFroms[i+1]), &*(vec.begin() + vecTos[i+1]));
            }
            sort(vec.begin() + vecFroms[i], vec.begin() + vecTos[i] + 1);
            if(i - 1 >= 0) {
                vmm.remonDemoteRange(&*(vec.begin() + vecFroms[i-1]), &*(vec.begin() + vecTos[i-1]), &*(vec.begin() + vecTos[i-1]));
            }
        }

        vector<size_t, customAllocator<size_t>> vecDest(vmmAllocator);
        vecDest.reserve(vec.size());
        std::priority_queue<heapEntry, std::vector<heapEntry>, std::greater<heapEntry>> minHeap;
        for (size_t i = 0; i < vecFroms.size(); ++i) {
            minHeap.push({vec[vecFroms[i]], i, 0});
        }

        size_t nthSegment = 0;
        size_t pageSize = vmm.getPageSize();
        size_t entryAPageCanDo = pageSize/sizeof(size_t);
        while (!minHeap.empty()) {
            heapEntry minElement = minHeap.top();
            minHeap.pop();
            vecDest.push_back(minElement.value);
            if(vecDest.size() % segmentSize == 0) {
                if(vecDest.begin() + (nthSegment+1) * segmentSize > vecDest.end()) {
                    vmm.remonPromoteRange(&*(vecDest.begin() + nthSegment * segmentSize), &*(vecDest.end()));
                } else {
                    vmm.remonPromoteRange(&*(vecDest.begin() + nthSegment * segmentSize), &*(vecDest.begin() + (nthSegment+1)* segmentSize));
                }
                nthSegment++;
            }
            if (minElement.currentIndex < vecTos[minElement.sourceIndex] - vecFroms[minElement.sourceIndex]) {
                size_t nextIndex = vecFroms[minElement.sourceIndex] + minElement.currentIndex + 1;
                minHeap.push({vec[nextIndex], minElement.sourceIndex, minElement.currentIndex + 1});
            }
        }

        cout << "move" << endl;
        vec = std::move(vecDest);
    }
    std::chrono::time_point<std::chrono::steady_clock> toB = std::chrono::steady_clock::now();
    int debugCount = 20;
    for(size_t i = 1; i <= BIG_SORT; i++) {
        if(i != vec[i-1]) {
            cout << "sort_test failed: " << i << " != " <<vec[i-1]<<endl;
            debugCount --;
        }
        if(debugCount <= 0) {
            break;
        }
    }
    int msA = std::chrono::duration_cast<std::chrono::milliseconds>(toA - from).count();
    int msB = std::chrono::duration_cast<std::chrono::milliseconds>(toB - toA).count();
    cout << "setting up used: " << msA << " sort used: " << msB << endl;
}

void tests::sortParallelTest() {
    remon_vmm vmm;
    std::chrono::time_point<std::chrono::steady_clock> from = std::chrono::steady_clock::now();
    customAllocator<size_t> vmmAllocator(vmm);
    vector<size_t, customAllocator<size_t>> vec(vmmAllocator);
    double maxRam = vmm.getMaxPM();
    cout << "max ram in GB: " << maxRam <<endl;
    cout << "sort size: " << BIG_SORT << endl;
    vec.reserve(BIG_SORT);
    for(size_t i = BIG_SORT; i > 0; i--) {
        vec.push_back(i);
    }
    cout << "sort prep done" << endl;
    std::chrono::time_point<std::chrono::steady_clock> toA = std::chrono::steady_clock::now();
    sort(std::execution::par, vec.begin(), vec.end());
    std::chrono::time_point<std::chrono::steady_clock> toB = std::chrono::steady_clock::now();
    int debugCount = 20;
    for(size_t i = 1; i <= BIG_SORT; i++) {
        if(i != vec[i-1]) {
            cout << "sort_test failed" << i << " != " <<vec[i-1]<<endl;
            debugCount --;
        }
        if(debugCount <= 0) {
            break;
        }
    }
    int msA = std::chrono::duration_cast<std::chrono::milliseconds>(toA - from).count();
    int msB = std::chrono::duration_cast<std::chrono::milliseconds>(toB - toA).count();
    cout << "setting up used: " << msA << " sort used: " << msB << endl;
}

void tests::smallAllocation() {
    remon_vmm vmm;
    void* ptr = vmm.remonMalloc(GB);
    if(!ptr) {
        exit(-1);
    }
    memset(ptr, '0',GB);
    cout << "OK GB" << endl;
    void* ptr0 = vmm.remonMalloc(M256);
    if(!ptr0) {
        exit(-2);
    }
    memset(ptr0, '1', M256);
    cout << "OK M256" << endl;
    void* ptr1 = vmm.remonMalloc(M128);
    if(!ptr1) {
        exit(-3);
    }
    memset(ptr1, '2',M128);
    cout << "OK M128" << endl;
    vmm.remonFree(ptr);
    cout << "OK free GB" << endl;
    vmm.remonFree(ptr0);
    cout << "OK free M256" << endl;
    vmm.remonFree(ptr1);
    cout << "OK free M128" << endl;
}

void tests::integrityTest() {
    remon_vmm vmm;
    int testingSize = 9;
    vector<char*> pointers;
    for(int i = 0; i< testingSize; i++) {
        char* hugePage = (char*) vmm.remonMalloc(1073741824);
        memset(hugePage, to_string(i)[0], 1073741824);
        pointers.push_back(hugePage);
    }
    for(int i = 0; i< testingSize; i++) {
        char* hugePage = pointers[i];
        int bad = 0;
        for(int j = 0; j < 1073741824; j++) {
            if(hugePage[j] != to_string(i)[0]) {
                cout << "mismatch: expecting " << i << " got: " << hugePage[j] << endl;
                bad = 1;
                break;
            }
        }
        if(bad == 0) {
            cout << "match: " << i << endl;
        }
    }
}

void tests::integrityTestManySmalls() {
    remon_vmm vmm;
    size_t baseSize = 4573;
    int repeat = 400000;
    int mallocFreeLoop = 1;
    vector<char*> pointers;
    vector<size_t> lengths;
    vector<char> answers;
    vector<int> nums {0,1,2,3,4,5,6,7,8,9};
    auto rng = std::default_random_engine {};

    for(int loopI = 0; loopI < mallocFreeLoop; loopI++) {
        for (int k = 0; k < repeat; k++) {
            std::shuffle(std::begin(nums), std::end(nums), rng);

            for (int i = 0; i < nums.size(); i++) {
                size_t num = nums[i];
                size_t size = baseSize + num * 128;
                cout << "malloc for: " << num << " size requested: " << size << endl;
                char *ptr = (char *) vmm.remonMalloc(size);
                if(ptr == nullptr) {exit(-3);}
                memset(ptr, to_string(num)[0], size);
                pointers.push_back(ptr);
                lengths.push_back(size);
                answers.push_back(to_string(num)[0]);
            }
        }
        for (int i = 0; i < pointers.size(); i++) {
            char *ptr = pointers[i];
            size_t size = lengths[i];
            int bad = 0;
            for (int j = 0; j < size; j++) {
                if (ptr[j] != answers[i]) {
                    cout << "mismatch: expecting " << i << " got: " << ptr[j] << endl;
                    bad = 1;
                    exit(-1);
                    break;
                }
            }
            if (bad == 0) {
                cout << "match: " << i << endl;
            }
            vmm.remonFree(ptr);
        }
        pointers.clear();
        lengths.clear();
        answers.clear();
    }
    cout << "All match, gracefully return" << endl;
}

void tests::integrityTestManyHundredsMB() {
    remon_vmm vmm;
    size_t baseSize = 1024*1024*127;
    int repeat = 2;
    int mallocFreeLoop = 10;
    vector<char*> pointers;
    vector<size_t> lengths;
    vector<char> answers;
    vector<int> nums {0,1,2,3,4,5,6,7,8,9};
    auto rng = std::default_random_engine {};

    for(int loopI = 0; loopI < mallocFreeLoop; loopI++) {
        for (int k = 0; k < repeat; k++) {
            std::shuffle(std::begin(nums), std::end(nums), rng);
            for (int i = 0; i < nums.size(); i++) {
                size_t num = nums[i];
                size_t size = baseSize + num * 1024 * 1024 * 128;
                cout << "malloc for: " << num << " size requested: " << size << endl;
                char *ptr = (char *) vmm.remonMalloc(size);
                cout << "malloc for: " << num << " size allocated: " << size << endl;
                if(ptr == nullptr) {exit(-3);}
                memset(ptr, to_string(num)[0], size);
                pointers.push_back(ptr);
                lengths.push_back(size);
                answers.push_back(to_string(num)[0]);
            }
        }
        for (int i = 0; i < pointers.size(); i++) {
            char *ptr = pointers[i];
            size_t size = lengths[i];
            int bad = 0;
            for (int j = 0; j < size; j++) {
                if (ptr[j] != answers[i]) {
                    cout << "mismatch: expecting " << i << " got: " << ptr[j] << endl;
                    bad = 1;
                    exit(-1);
                    break;
                }
            }
            if (bad == 0) {
                cout << "match: " << i << endl;
            }
            vmm.remonFree(ptr);
        }
        pointers.clear();
        lengths.clear();
        answers.clear();
    }
    cout << "All match, gracefully return" << endl;
}

void tests::mallocMultiPages() {
    remon_vmm vmm;
    for(int i = 0; i < 10; i++) {
        void *ptr = vmm.remonMalloc(1024 * 1024 * 512);
        vmm.remonFree(ptr);
        ptr = vmm.remonMalloc(1024 * 1024 * 512 + 2);
        vmm.remonFree(ptr);
    }
}

void tests::tinyAllocation() {
    remon_vmm vmm;

    void* ptr = vmm.remonMalloc(24);
    if(!ptr) {
        exit(-1);
    }
    memset(ptr, '0',24);
    cout << "OK 24" << endl;
    void* ptr0 = vmm.remonMalloc(56);
    if(!ptr0) {
        exit(-2);
    }
    memset(ptr0, '1', 56);
    cout << "OK 56" << endl;
    void* ptr1 = vmm.remonMalloc(78);
    if(!ptr1) {
        exit(-3);
    }
    memset(ptr1, '2',78);
    cout << "OK 78" << endl;
    vmm.remonFree(ptr);
    cout << "OK free 24" << endl;
    vmm.remonFree(ptr0);
    cout << "OK free 56" << endl;
    vmm.remonFree(ptr1);
    cout << "OK free 78" << endl;
}

void tests::integrityTestMixedLoad() {
    remon_vmm vmm;
    size_t baseSize = 1024*1024*127;
    size_t smallBaseSize = 4573;
    int repeat = 20000;
    int mallocFreeLoop = 10;
    vector<char*> pointers;
    vector<size_t> lengths;
    vector<char> answers;
    vector<int> nums {0,1,2,3,4,5,6,7,8,9};
    auto rng = std::default_random_engine {};

    for(int loopI = 0; loopI < mallocFreeLoop; loopI++) {
        for (int k = 0; k < repeat; k++) {
            std::shuffle(std::begin(nums), std::end(nums), rng);

            for (int i = 0; i < nums.size(); i++) {
                size_t num = nums[i];
                size_t size;
                uniform_real_distribution<double> distribution(0.0, 1.0);
                random_device rd;
                mt19937 mt(rd());
                if(distribution(mt) > 0.9999) {
                    size = baseSize + num * 1024 * 1024 * 128;
                } else {
                    size = smallBaseSize + num * 128;
                }
                cout << "malloc for: " << num << " size requested: " << size << endl;
                char *ptr = (char *) vmm.remonMalloc(size);
                if(ptr == nullptr) {exit(-3);}
                memset(ptr, to_string(num)[0], size);
                pointers.push_back(ptr);
                lengths.push_back(size);
                answers.push_back(to_string(num)[0]);
            }
        }

        for (int i = 0; i < pointers.size(); i++) {
            char *ptr = pointers[i];
            size_t size = lengths[i];
            int bad = 0;
            for (int j = 0; j < size; j++) {
                if (ptr[j] != answers[i]) {
                    cout << "mismatch: expecting " << i << " got: " << ptr[j] << endl;
                    bad = 1;
                    exit(-1);
                    break;
                }
            }
            if (bad == 0) {
                cout << "match: " << i << endl;
            }
            vmm.remonFree(ptr);
        }
        pointers.clear();
        lengths.clear();
        answers.clear();
    }
    cout << "All match, gracefully return" << endl;
}
