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

    // PreCheck 和 PostCheck 函数, 用于测试前检查和结果校验
    std::function<void()> pre_check_func;   // PreCheck 函数
    std::function<void()> post_check_func;  // PostCheck 函数
    
    // 默认构造函数
    CTSFunctionInfo() : function_id(""), function_version(""), pre_check_func(nullptr), post_check_func(nullptr) {}
    
    // 构造函数
    CTSFunctionInfo(const std::string& id, const std::string& version) 
        : function_id(id), function_version(version), pre_check_func(nullptr), post_check_func(nullptr) {}
    
    CTSFunctionInfo(const std::string& id, const std::string& version, std::function<void()> pre_check, std::function<void()> post_check) 
        : function_id(id), function_version(version), pre_check_func(pre_check), post_check_func(post_check) {}

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
    static std::unordered_set<CTSFunctionInfo> all_functions;           // pull from UAV
    static std::unordered_map<std::string, CTSFunctionInfo> registered_functions;  // 测试名称到功能信息的映射
    static std::mutex mtx;
public:
    // 注册用例
    static void RegisterCase(const std::string& test_suite, const std::string& test_name, const CTSFunctionInfo& info) {
        std::lock_guard<std::mutex> lock(mtx);
        std::string full_test_name = test_suite + "." + test_name;
        registered_functions[full_test_name] = info;
    }

    // 静态注册功能全集 - 需要在main()开始时调用
    static void RegisterAllFunctions(const std::unordered_set<CTSFunctionInfo>& all) {
        std::lock_guard<std::mutex> lock(mtx);
        all_functions = all;
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

    // 自动根据当前测试执行PreCheck
    static void ExecutePreCheckForCurrentTest() {
        std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string suite_name = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        std::string full_test_name = suite_name + "." + test_name;
        
        std::lock_guard<std::mutex> lock(mtx);
        auto it = registered_functions.find(full_test_name);
        if (it != registered_functions.end() && it->second.pre_check_func) {
            std::cout << "Executing PreCheck for " << it->second.function_id << ":" << it->second.function_version << std::endl;
            try {
                it->second.pre_check_func();
            } catch (const std::exception& e) {
                ADD_FAILURE() << "PreCheck failed with exception: " << e.what();
            } catch (...) {
                ADD_FAILURE() << "PreCheck failed with unknown exception";
            }
        }
    }

    // 自动根据当前测试执行PostCheck
    static void ExecutePostCheckForCurrentTest() {
        std::string test_name = ::testing::UnitTest::GetInstance()->current_test_info()->name();
        std::string suite_name = ::testing::UnitTest::GetInstance()->current_test_info()->test_case_name();
        std::string full_test_name = suite_name + "." + test_name;
        
        std::lock_guard<std::mutex> lock(mtx);
        auto it = registered_functions.find(full_test_name);
        if (it != registered_functions.end() && it->second.post_check_func) {
            std::cout << "Executing PostCheck for " << it->second.function_id << ":" << it->second.function_version << std::endl;
            try {
                it->second.post_check_func();
            } catch (const std::exception& e) {
                ADD_FAILURE() << "PostCheck failed with exception: " << e.what();
            } catch (...) {
                ADD_FAILURE() << "PostCheck failed with unknown exception";
            }
        }
    }

    // 用于输出未覆盖的功能和统计信息
    // TBD 放到Manager实现
    static void ReportUncovered() {
        std::lock_guard<std::mutex> lock(mtx);
        
        // 收集已注册的功能信息
        std::unordered_set<CTSFunctionInfo> registered_function_infos;
        for (const auto& pair : registered_functions) {
            registered_function_infos.insert(pair.second);
        }
        
        std::cout << "\n=== CTS Coverage Report ===" << std::endl;
        std::cout << "Total functions defined: " << all_functions.size() << std::endl;
        std::cout << "Test cases registered: " << registered_function_infos.size() << std::endl;
        
        // 查找未覆盖的功能
        std::vector<CTSFunctionInfo> uncovered;
        for (const auto& func : all_functions) {
            if (registered_function_infos.find(func) == registered_function_infos.end()) {
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
        for (const auto& func : registered_function_infos) {
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
        
        // 收集已注册的功能信息
        std::unordered_set<CTSFunctionInfo> registered_function_infos;
        for (const auto& pair : registered_functions) {
            registered_function_infos.insert(pair.second);
        }
        
        int covered = 0;
        for (const auto& func : all_functions) {
            if (registered_function_infos.find(func) != registered_function_infos.end()) {
                covered++;
            }
        }
        return (double)covered / all_functions.size() * 100.0;
    }
};

// 静态成员定义
std::unordered_map<std::string, CTSFunctionInfo> CTSBase::registered_functions;
std::unordered_set<CTSFunctionInfo> CTSBase::all_functions;
std::mutex CTSBase::mtx;

// CTS_TEST
#define CTS_TEST(test_suite_name, test_name, function_id, function_version) \
class CTS_REGISTER_##test_suite_name##_##test_name { \
public: \
    CTS_REGISTER_##test_suite_name##_##test_name() { \
        CTSBase::RegisterCase(#test_suite_name, #test_name, {function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_suite_name##_##test_name cts_register_##test_suite_name##_##test_name; \
TEST(test_suite_name, test_name)

// CTS_TEST_F
#define CTS_TEST_F(test_fixture, test_name, function_id, function_version) \
class CTS_REGISTER_##test_fixture##_##test_name { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name() { \
        CTSBase::RegisterCase(#test_fixture, #test_name, {function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name cts_register_##test_fixture##_##test_name; \
TEST_F(test_fixture, test_name)

// CTS_TEST_F_WITH_POSTCHECK
#define CTS_TEST_F_WITH_POSTCHECK(test_fixture, test_name, function_id, function_version, pre_check, post_check) \
class CTS_REGISTER_##test_fixture##_##test_name { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name() { \
        CTSBase::RegisterCase(#test_fixture, #test_name, {function_id, function_version, pre_check, post_check}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name cts_register_##test_fixture##_##test_name; \
TEST_F(test_fixture, test_name)

// CTS_TEST_WITH_TIMEOUT
#define CTS_TEST_WITH_TIMEOUT(test_suite_name, test_name, function_id, function_version, timeout_ms) \
class CTS_REGISTER_##test_suite_name##_##test_name##_TIMEOUT { \
public: \
    CTS_REGISTER_##test_suite_name##_##test_name##_TIMEOUT() { \
        CTSBase::RegisterCase(#test_suite_name, #test_name, {function_id, function_version}); \
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

// CTS_TEST_F_WITH_TIMEOUT
#define CTS_TEST_F_WITH_TIMEOUT(test_fixture, test_name, function_id, function_version, timeout_ms) \
class CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT() { \
        CTSBase::RegisterCase(#test_fixture, #test_name, {function_id, function_version}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name##_TIMEOUT cts_register_##test_fixture##_##test_name##_timeout; \
class CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name : public ::testing::Test { \
protected: \
    void TestBody() override { \
        bool success = CTSBase::ExecuteWithTimeout([this]() { \
            this->TimeoutTestBody(); \
        }, timeout_ms); \
        ASSERT_TRUE(success) << "Test failed or timed out after " << timeout_ms << " ms"; \
    } \
    virtual void TimeoutTestBody(); \
}; \
TEST_F(CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name, test_name) {} \
void CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name::TimeoutTestBody()

//CTS_TEST_F_WITH_POSTCHECK_TIMEOUT
#define CTS_TEST_F_WITH_POSTCHECK_TIMEOUT(test_fixture, test_name, function_id, function_version, pre_check, post_check, timeout_ms) \
class CTS_REGISTER_##test_fixture##_##test_name##_POSTCHECK_TIMEOUT { \
public: \
    CTS_REGISTER_##test_fixture##_##test_name##_POSTCHECK_TIMEOUT() { \
        CTSBase::RegisterCase(#test_fixture, #test_name, {function_id, function_version, pre_check, post_check}); \
    } \
}; \
static CTS_REGISTER_##test_fixture##_##test_name##_POSTCHECK_TIMEOUT cts_register_##test_fixture##_##test_name##_postcheck_timeout; \
class CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name : public ::testing::Test { \
protected: \
    void TestBody() override { \
        bool success = CTSBase::ExecuteWithTimeout([this]() { \
            this->TimeoutTestBody(); \
        }, timeout_ms); \
        ASSERT_TRUE(success) << "Test failed or timed out after " << timeout_ms << " ms"; \
        if (post_check) post_check(); \
    } \
    virtual void TimeoutTestBody(); \
}; \
TEST_F(CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name, test_name) {} \
void CTS_TIMEOUT_TEST_F_##test_fixture##_##test_name::TimeoutTestBody()


#endif // CTS_FRAMEWORK_H