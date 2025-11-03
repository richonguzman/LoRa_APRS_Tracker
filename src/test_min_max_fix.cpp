// 这个文件用于测试min/max宏冲突的解决方案

// 在任何其他包含之前无条件解除min/max宏的定义
#undef min
#undef max

#include <Arduino.h>

// 简单的测试函数，不依赖于任何可能重新定义min/max的库
void test_min_max() {
    int a = 5;
    int b = 10;
    
    // 使用标准的比较，避免使用min/max函数
    int result = (a < b) ? a : b;
    Serial.print("较小的值是：");
    Serial.println(result);
}

// 这个函数不会被实际调用，只是为了测试编译