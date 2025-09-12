# CTS 测试框架

基于Google Test的增强测试框架，提供用例管理、二次校验和超时检查功能。

## 核心特性

### 1. 用例管理
- 按 `function_id` 和 `function_version` 注册用例
- 静态配置功能全集，运行时自动注册执行用例
- 自动对比测试全集与实际执行用例，确保不重不漏
- 提供覆盖率统计和报告

### 2. 二次校验
- 每个用例执行后支持 `PostCheck()` 二次校验
- 不仅仅是执行过程中的断言，还能在测试完成后进行额外检查
- 支持 `SetTestResult()`/`GetTestResult()` 在测试和校验间传递数据

### 3. 超时检查
- 支持 `CTS_TEST_WITH_TIMEOUT` 和 `CTS_TEST_F_WITH_TIMEOUT`
- 使用线程+future机制实现安全超时检测
- 超时后继续执行后续用例，不影响整体测试流程

## API 接口

### 基础测试宏
```cpp
// 简单测试用例
CTS_TEST(test_suite_name, test_name, function_id, function_version) {
    // 测试代码
}

// 基于fixture的测试用例
CTS_TEST_F(test_fixture, test_name, function_id, function_version) {
    // 测试代码
}

// 带超时的测试用例
CTS_TEST_WITH_TIMEOUT(test_suite_name, test_name, function_id, function_version, timeout_ms) {
    // 测试代码
}

// 带超时的fixture测试用例
CTS_TEST_F_WITH_TIMEOUT(test_fixture, test_name, function_id, function_version, timeout_ms) {
    // 测试代码
}
```

## 使用示例

### 1. 简单测试用例
```cpp
CTS_TEST(BasicMath, Addition, "MATH_ADD", "v1.0") {
    int result = 2 + 3;
    EXPECT_EQ(result, 5);
}
```

### 2. 带二次校验的测试
```cpp
class MyFixture : public CTSBase {
protected:
    void PostCheck() override {
        std::string result = GetTestResult("calculation_result");
        if (!result.empty()) {
            int value = std::stoi(result);
            EXPECT_GT(value, 0) << "Result should be positive";
        }
    }
};

CTS_TEST_F(MyFixture, Calculation, "MATH_CALC", "v1.0") {
    int result = 10 * 5;
    EXPECT_EQ(result, 50);
    SetTestResult("calculation_result", std::to_string(result));
}
```

### 3. 超时测试
```cpp
CTS_TEST_WITH_TIMEOUT(SlowTest, LongOperation, "SLOW_OP", "v1.0", 2000) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    EXPECT_TRUE(true);
}
```



## 主程序 & 功能配置

```cpp
// 注册所有需要测试的功能
CTSBase::RegisterAllFunctions({
    {"funcA", "v1"},
    {"funcB", "v2"},
    {"funcC", "v1"}
});

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    // 注册功能全集
    CTSBase::RegisterAllFunctions({...});
    
    // 运行测试
    int result = RUN_ALL_TESTS();
    
    // 输出未覆盖的功能
    CTSBase::ReportUncovered();
    
    return result;
}
```