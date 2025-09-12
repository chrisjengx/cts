#include "cts_framework.h"
#include <iostream>
#include <thread>
#include <chrono>

// 简单的测试fixture示例
class SampleFixture : public CTSBase {
protected:
    void SetUp() override {
        test_value = 42;
        std::cout << "SampleFixture SetUp - test_value: " << test_value << std::endl;
    }
    
    void TearDown() override {
        std::cout << "SampleFixture TearDown called" << std::endl;
        CTSBase::TearDown(); // 调用父类TearDown以执行PostCheck
    }
    
    void PostCheck() override {
        // 二次校验逻辑
        std::string result = GetTestResult("calculation_result");
        if (!result.empty()) {
            int value = std::stoi(result);
            EXPECT_GT(value, 0) << "PostCheck: calculation result should be positive";
            std::cout << "PostCheck: verified calculation result = " << value << std::endl;
        }
    }
    
protected:
    int test_value;
};

// 网络测试fixture示例，展示不同的PostCheck策略
class NetworkFixture : public CTSBase {
protected:
    void PostCheck() override {
        // 检查网络连接状态
        std::string status = GetTestResult("connection_status");
        if (status == "open") {
            ADD_FAILURE() << "PostCheck: Network connection should be closed after test";
        } else if (status == "closed") {
            std::cout << "PostCheck: Network connection properly closed" << std::endl;
        }
    }
};

// 示例1：简单的CTS_TEST用例
CTS_TEST(BasicMath, Addition, "MATH_ADD", "v1.0") {
    int result = 2 + 3;
    EXPECT_EQ(result, 5);
    std::cout << "Basic addition test: 2 + 3 = " << result << std::endl;
}

// 示例2：简单的计算测试
CTS_TEST(BasicMath, Multiplication, "MATH_MULTIPLY", "v1.0") {
    int result = 6 * 7;
    EXPECT_EQ(result, 42);
    std::cout << "Multiplication test: 6 * 7 = " << result << std::endl;
}

// 示例3：带超时的快速测试
CTS_TEST_WITH_TIMEOUT(Performance, QuickOperation, "PERF_QUICK", "v1.0", 1000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    EXPECT_TRUE(true);
    std::cout << "Quick operation completed within timeout" << std::endl;
}

// 示例4：会超时的测试（演示超时机制）
CTS_TEST_WITH_TIMEOUT(Performance, SlowOperation, "PERF_SLOW", "v1.0", 800) {
    // 这个测试会超时
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    EXPECT_TRUE(false) << "This should not be reached due to timeout";
}

// 示例5：基于fixture的测试，带二次校验
CTS_TEST_F(SampleFixture, CalculationTest, "FIXTURE_CALC", "v1.0") {
    int result = test_value * 2;
    EXPECT_EQ(result, 84);
    
    // 设置测试结果供PostCheck使用
    SetTestResult("calculation_result", std::to_string(result));
    std::cout << "Fixture calculation test: " << test_value << " * 2 = " << result << std::endl;
}

// 示例6：基于fixture的超时测试
CTS_TEST_F_WITH_TIMEOUT(SampleFixture, SlowCalculation, "FIXTURE_SLOW", "v1.0", 1500) {
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    int result = test_value + 10;
    EXPECT_EQ(result, 52);
    SetTestResult("calculation_result", std::to_string(result));
    std::cout << "Slow fixture calculation: " << test_value << " + 10 = " << result << std::endl;
}

// 示例7：网络连接测试（正常关闭）
CTS_TEST_F(NetworkFixture, GoodConnection, "NETWORK_GOOD", "v2.0") {
    // 模拟建立连接
    SetTestResult("connection_status", "open");
    std::cout << "Network connection established" << std::endl;
    
    // 执行网络操作
    bool operation_success = true;
    EXPECT_TRUE(operation_success);
    
    // 正确关闭连接
    SetTestResult("connection_status", "closed");
    std::cout << "Network operation completed, connection closed" << std::endl;
}

// 示例8：网络连接测试（忘记关闭，PostCheck会失败）
CTS_TEST_F(NetworkFixture, BadConnection, "NETWORK_BAD", "v2.0") {
    // 模拟建立连接但忘记关闭
    SetTestResult("connection_status", "open");
    std::cout << "Network connection established" << std::endl;
    
    bool operation_success = true;
    EXPECT_TRUE(operation_success);
    
    // 故意不关闭连接，让PostCheck检测到问题
    std::cout << "Network operation completed, but connection left open (PostCheck will catch this)" << std::endl;
}

// 主函数：注册功能全集并运行测试
int main(int argc, char** argv) {
    // 初始化Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // 注册项目的功能全集
    CTSBase::RegisterAllFunctions({
        {"MATH_ADD", "v1.0"},
        {"MATH_MULTIPLY", "v1.0"},
        {"MATH_DIVIDE", "v1.0"},        // 未覆盖的功能
        {"PERF_QUICK", "v1.0"},
        {"PERF_SLOW", "v1.0"},
        {"PERF_MEDIUM", "v1.0"},        // 未覆盖的功能
        {"FIXTURE_CALC", "v1.0"},
        {"FIXTURE_SLOW", "v1.0"},
        {"NETWORK_GOOD", "v2.0"},
        {"NETWORK_BAD", "v2.0"},
        {"NETWORK_ADVANCED", "v2.1"},   // 未覆盖的功能
    });
    
    std::cout << "\n=== Running CTS Sample Tests ===" << std::endl;
    std::cout << "This example demonstrates:" << std::endl;
    std::cout << "1. Basic CTS_TEST usage" << std::endl;
    std::cout << "2. CTS_TEST_WITH_TIMEOUT for timeout handling" << std::endl;
    std::cout << "3. CTS_TEST_F with custom fixtures" << std::endl;
    std::cout << "4. CTS_TEST_F_WITH_TIMEOUT for fixture-based timeout tests" << std::endl;
    std::cout << "5. PostCheck mechanism for secondary validation" << std::endl;
    std::cout << "6. Function coverage tracking and reporting" << std::endl;
    std::cout << "======================================\n" << std::endl;
    
    // 运行所有测试
    int result = RUN_ALL_TESTS();
    
    // 输出覆盖率报告
    std::cout << std::endl;
    CTSBase::ReportUncovered();
    
    double coverage = CTSBase::GetCoveragePercentage();
    std::cout << "\nFinal Coverage: " << std::fixed << std::setprecision(1) << coverage << "%" << std::endl;
    
    if (coverage >= 80.0) {
        std::cout << "✓ Good coverage achieved!" << std::endl;
    } else {
        std::cout << "⚠ Consider adding more test cases to improve coverage" << std::endl;
    }
    
    return result;
}