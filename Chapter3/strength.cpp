// 强度降低，比如除法变成乘法
#include <cstdint>

int main()
{
    const auto price = 10.125; // prices are like: 10.125, 10.130, 10.135...
    constexpr auto min_price_increment = 0.005;
    [[maybe_unused]] int64_t int_price = 0;

    // no strength reduction
    // 提前计算除法，变成乘法，减少CPU的开销
    int_price = price / min_price_increment;

    // strength reduction
    constexpr auto inv_min_price_increment = 1 / min_price_increment;
    int_price = price * inv_min_price_increment;
}

/* constexpr 表示可以在编译期计算 */