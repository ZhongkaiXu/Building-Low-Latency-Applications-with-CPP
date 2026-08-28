// 归纳变量优化，重复的计算变成简单的更新
// 现代编译器自己完成
int main()
{
    // 即使没使用 也别警告
    [[maybe_unused]] int a[100];

    // original
    for (auto i = 0; i < 100; ++i)
        // i 每次都要乘以10再加12，编译器可能无法优化
        // 结果上看是后一个数比前一个数多10
        a[i] = i * 10 + 12;

    // optimized
    int temp = 12;
    for (auto i = 0; i < 100; ++i) {
        a[i] = temp;
        // temp每次都加10
        temp += 10;
    }
}