#ifndef CTS_FRAMEWORK_H
#define CTS_FRAMEWORK_H

#include <gtest/gtest.h>
#include <unordered_set>
#include <unordered_map>
#include <string>
#include <chrono>
#include <thread>
#include <functional>
#include <iostream>
#include <mutex>
#include <atomic>
#include <future>
#include <exception>
#include <vector>
#include <iomanip>

// CTS 功能信息结构
struct CTSFunctionInfo {
    std::string function_id;
    std::string function_version;
    
    bool operator==(const CTSFunctionInfo& other) const {
        return function_id == other.function_id && function_version == other.function_version;
    }
    
    std::string to_string() const {
        return function_id + ":" + function_version;
    }
};

// 为CTSFunctionInfo添加hash支持
namespace std {
    template <>
    struct hash<CTSFunctionInfo> {
        size_t operator()(const CTSFunctionInfo& info) const {
            return hash<string>()(info.function_id) ^ hash<string>()(info.function_version);
        }
    };
}

// CTS 基类
class CTSBase : public ::testing::Test {
protected:
    static std::unordered_set<CTSFunctionInfo> registered_cases;
    static std::unordered_set<CTSFunctionInfo> all_functions;
    static std::mutex mtx;
    static std::unordered_map<std::string, std::string> test_results; // 存储测试结果用于二次校验

public:
    // 注册用例
    static void RegisterCase(const CTSFunctionInfo& info) {
        std::lock_guard<std::mutex> lock(mtx);
        registered_cases.insert(info);
    }

    // 静态注册功能全集 - 需要在main()开始时调用
    static void RegisterAllFunctions(const std::unordered_set<CTSFunctionInfo>& all) {
        std::lock_guard<std::mutex> lock(mtx);
        all_functions = all;
    }

    // 设置测试结果（供二次校验使用）
    void SetTestResult(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mtx);
        test_results[key] = value;
    }

    // 获取测试结果（供二次校验使用）
    std::string GetTestResult(const std::string& key) {
        std::lock_guard<std::mutex> lock(mtx);
        auto it = test_results.find(key);
        return (it != test_results.end()) ? it->second : "";
    }

    // 用于二次校验 - 用户可以重写此方法
    virtual void PostCheck() {
        // 默认实现为空，用户可以重写进行自定义二次校验
    }

    // Google Test的TearDown，会自动调用PostCheck进行二次校验
    void TearDown() override {
        try {
            PostCheck();
        } catch (const std::exception& e) {
            ADD_FAILURE() << "PostCheck failed with exception: " << e.what();
        } catch (...) {
            ADD_FAILURE() << "PostCheck failed with unknown exception";
        }
    }

    // 安全的超时执行函数 - 使用线程+future实现
    template<typename Func>
    static bool ExecuteWithTimeout(Func&& test_func, int timeout_ms) {
        std::promise<bool> promise;
        std::future<bool> future = promise.get_future();
        
        std::thread test_thread([&promise, test_func = std::forward<Func>(test_func)]() {
            try {
                test_func();
                promise.set_value(true);  // 测试成功
            } catch (const std::exception& e) {
                std::cerr << "Test failed with exception: " << e.what() << std::endl;
                promise.set_value(false);  // 测试失败
            } catch (...) {
                std::cerr << "Test failed with unknown exception" << std::endl;
                promise.set_value(false);  // 测试失败
            }
        });
        
        // 等待超时时间
        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) == std::future_status::timeout) {
            // 超时 - 注意：我们无法强制终止线程，只能detach并返回失败
            test_thread.detach();
            std::cerr << "Test timed out after " << timeout_ms << " ms (thread detached)" << std::endl;
            return false;
        } else {
            // 正常完成
            test_thread.join();
            return future.get();
        }
    }

    // 用于输出未覆盖的功能和统计信息
    static void ReportUncovered() {
        std::lock_guard<std::mutex> lock(mtx);
        std::cout << "\n=== CTS Coverage Report ===" << std::endl;
        std::cout << "Total functions defined: " << all_functions.size() << std::endl;
        std::cout << "Test cases registered: " << registered_cases.size() << std::endl;
        
        // 查找未覆盖的功能
        std::vector<CTSFunctionInfo> uncovered;
        for (const auto& func : all_functions) {
            if (registered_cases.find(func) == registered_cases.end()) {
                uncovered.push_back(func);
            }
        }
        
        if (uncovered.empty()) {
            std::cout << "\n✓ All functions are covered!" << std::endl;
        } else {
            std::cout << "\n✗ Uncovered functions (" << uncovered.size() << "):" << std::endl;
            for (const auto& func : uncovered) {
                std::cout << "  - " << func.to_string() << std::endl;
            }
        }
        
        // 查找重复注册的功能
        std::unordered_map<CTSFunctionInfo, int> count_map;
        for (const auto& func : registered_cases) {
            count_map[func]++;
        }
        
        std::cout << "\nRegistered functions:" << std::endl;
        for (const auto& pair : count_map) {
            std::cout << "  - " << pair.first.to_string();
            if (pair.second > 1) {
                std::cout << " (WARNING: registered " << pair.second << " times)";
            }
            std::cout << std::endl;
        }
        
        double coverage = all_functions.empty() ? 0.0 : 
                         (double)(all_functions.size() - uncovered.size()) / all_functions.size() * 100.0;
        std::cout << "\nCoverage: " << std::fixed << std::setprecision(1) << coverage << "%" << std::endl;
        std::cout << "=========================" << std::endl;
    }

    // 获取覆盖率统计
    static double GetCoveragePercentage() {
        std::lock_guard<std::mutex> lock(mtx);
        if (all_functions.empty()) return 0.0;
        
        int covered = 0;
        for (const auto& func : all_functions) {
            if (registered_cases.find(func) != registered_cases.end()) {
                covered++;
            }
        }
        return (double)covered / all_functions.size() * 100.0;
    }
};

// 静态成员定义
std::unordered_set<CTSFunctionInfo> CTSBase::registered_cases;
std::unordered_set<CTSFunctionInfo> CTSBase::all_functions;
std::unordered_map<std::string, std::string> CTSBase::test_results;
std::mutex CTSBase::mtx;

// CTS_TEST - 不依赖fixture的简单测试用例
#define CTS_TEST(test_suite_name, test_name, function_id, function_version) \
class CTS_REGISTER_##test_suite_name##_##test_name { \
public: \
    CTS_REGISTER_##test_suite_name##_##test_name() { \
        CTSBase::RegisterCase({function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_suite_name##_##test_name cts_register_##test_suite_name##_##test_name; \
TEST(test_suite_name, test_name)

// CTS_TEST_F - 基于fixture的测试用例
#define CTS_TEST_F(test_fixture, test_name, function_id, function_version) \
class CTS_REGISTER_##test_fixture##_##test_name { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name() { \
        CTSBase::RegisterCase({function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name cts_register_##test_fixture##_##test_name; \
TEST_F(test_fixture, test_name)

// CTS_TEST_WITH_TIMEOUT - 不依赖fixture的超时测试用例
#define CTS_TEST_WITH_TIMEOUT(test_suite_name, test_name, function_id, function_version, timeout_ms) \
class CTS_REGISTER_##test_suite_name##_##test_name##_TIMEOUT { \
public: \
    CTS_REGISTER_##test_suite_name##_##test_name##_TIMEOUT() { \
        CTSBase::RegisterCase({function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_suite_name##_##test_name##_TIMEOUT cts_register_##test_suite_name##_##test_name##_timeout; \
class CTS_TIMEOUT_TEST_##test_suite_name##_##test_name : public ::testing::Test { \
protected: \
    void TestBody() override { \
        bool success = CTSBase::ExecuteWithTimeout([this]() { \
            this->TimeoutTestBody(); \
        }, timeout_ms); \
        ASSERT_TRUE(success) << "Test failed or timed out after " << timeout_ms << " ms"; \
    } \
    virtual void TimeoutTestBody(); \
}; \
TEST_F(CTS_TIMEOUT_TEST_##test_suite_name##_##test_name, test_name) {} \
void CTS_TIMEOUT_TEST_##test_suite_name##_##test_name::TimeoutTestBody()

// CTS_TEST_F_WITH_TIMEOUT - 基于fixture的超时测试用例
#define CTS_TEST_F_WITH_TIMEOUT(test_fixture, test_name, function_id, function_version, timeout_ms) \
class CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT() { \
        CTSBase::RegisterCase({function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT cts_register_##test_fixture##_##test_name##_timeout; \
class CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name : public test_fixture { \
protected: \
    void TestBody() override { \
        std::atomic<bool> finished{false}; \
        std::exception_ptr test_exception = nullptr; \
        \
        std::thread test_thread([this, &finished, &test_exception]() { \
            try { \
                this->TimeoutTestBody(); \
                finished = true; \
            } catch (...) { \
                test_exception = std::current_exception(); \
                finished = true; \
            } \
        }); \
        \
        auto start = std::chrono::steady_clock::now(); \
        while (!finished && std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) { \
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); \
        } \
        \
        if (!finished) { \
            test_thread.detach(); \
            FAIL() << "Test timed out after " << timeout_ms << " ms (Note: thread detached, may leak resources)"; \
        } else { \
            test_thread.join(); \
            if (test_exception) { \
                std::rethrow_exception(test_exception); \
            } \
        } \
    } \
    virtual void TimeoutTestBody(); \
}; \
TEST_F(CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name, test_name) {} \
void CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name::TimeoutTestBody()

#endif // CTS_FRAMEWORK_H