#include "cts_framework.h"
#include <vector>
#include <algorithm>
#include <numeric>
#include <thread>
#include <chrono>

// 计算测试Fixture - 自动执行PreCheck和PostCheck
class CalculationFixture : public CTSBase {
protected:
    void SetUp() override {
        data = {1, 2, 3, 4, 5};
        result = 0;
        // 自动根据当前测试查找并执行PreCheck
        ExecutePreCheckForCurrentTest();
    }
    
    void TearDown() override {
        // 自动根据当前测试查找并执行PostCheck
        ExecutePostCheckForCurrentTest();
    }
    
    std::vector<int> data;
    int result;
};

// 网络测试Fixture - 自动执行PreCheck和PostCheck
class NetworkFixture : public CTSBase {
protected:
    void SetUp() override {
        connection_status = false;
        response_time = 0;
    }
    
    void TearDown() override {
    }
    
    bool connection_status;
    int response_time;
};

// 简单测试Fixture - 不使用PreCheck/PostCheck
class SampleFixture : public CTSBase {
protected:
    void SetUp() override {
        data = {1, 2, 3, 4, 5};
        result = 0;
    }
    
    std::vector<int> data;
    int result;
};

// PreCheck和PostCheck函数定义
void PreCheckCalculation() {
    std::cout << "PreCheck: Initializing calculation environment..." << std::endl;
    EXPECT_TRUE(true) << "Calculation PreCheck passed";
}

void PostCheckCalculation() {
    std::cout << "PostCheck: Verifying calculation results..." << std::endl;
    EXPECT_TRUE(true) << "Calculation PostCheck passed";
}

void PostCheckGoodConnection() {
    std::cout << "PostCheck: Verifying good connection cleanup..." << std::endl;
    EXPECT_TRUE(true) << "Good connection PostCheck passed";
}

void PostCheckBadConnection() {
    std::cout << "PostCheck: Verifying bad connection handling..." << std::endl;
    EXPECT_TRUE(true) << "Bad connection PostCheck passed";
}

// 测试用例
CTS_TEST_F_WITH_PREPOSTCHECK(CalculationFixture, CalculationTest, "CALC", "1.0", PreCheckCalculation, PostCheckCalculation) {
    result = std::accumulate(data.begin(), data.end(), 0);
    EXPECT_EQ(result, 15);
    std::cout << "Test: Sum calculated = " << result << std::endl;
}

CTS_TEST_F_WITH_PREPOSTCHECK(NetworkFixture, GoodConnection, "NET", "1.0", nullptr, PostCheckGoodConnection) {
    connection_status = true;
    response_time = 100;
    EXPECT_TRUE(connection_status);
    EXPECT_LT(response_time, 200);
    std::cout << "Test: Good connection established, response time: " << response_time << "ms" << std::endl;
}

CTS_TEST_F_WITH_PREPOSTCHECK(NetworkFixture, BadConnection, "NET", "1.1", nullptr, PostCheckBadConnection) {
    connection_status = false;
    response_time = 5000;
    EXPECT_FALSE(connection_status);
    EXPECT_GT(response_time, 1000);
    std::cout << "Test: Bad connection detected, response time: " << response_time << "ms" << std::endl;
}

CTS_TEST_F(SampleFixture, SimpleTest, "SIMPLE", "1.0") {
    result = data.size();
    EXPECT_EQ(result, 5);
    std::cout << "Test: Array size = " << result << std::endl;
}

CTS_TEST_WITH_TIMEOUT(TimeoutTest, QuickTest, "TIMEOUT", "1.0", 3000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10000));
    EXPECT_TRUE(true);
    std::cout << "Quick test completed" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::unordered_set<CTSFunctionInfo> all_functions = {
        {"CALC", "1.0"},
        {"NET", "1.0"},
        {"NET", "1.1"},
        {"SIMPLE", "1.0"},
        {"TIMEOUT", "1.0"},
        {"UNCOVERED", "1.0"}
    };
    
    CTSBase::RegisterAllFunctions(all_functions);
    
    std::cout << "\n=== CTS Framework Demo ===" << std::endl;
    std::cout << "This example demonstrates:" << std::endl;
    std::cout << "1. PreCheck execution in SetUp()" << std::endl;
    std::cout << "2. PostCheck execution in TearDown()" << std::endl;
    std::cout << "3. Different fixtures with different check strategies" << std::endl;
    std::cout << "4. Function coverage tracking and reporting" << std::endl;
    std::cout << "==============================\n" << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    CTSBase::ReportUncovered();
    
    return result;
}
