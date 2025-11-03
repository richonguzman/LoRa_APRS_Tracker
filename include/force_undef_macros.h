/*
 * 这个文件用于在编译前强制解除min/max宏的定义
 * 需要在platformio.ini中使用--include强制包含
 */

// 无条件解除min/max宏的定义，不检查是否已定义
#undef min
#undef max

// 避免任何可能的模板函数定义，防止与STL冲突