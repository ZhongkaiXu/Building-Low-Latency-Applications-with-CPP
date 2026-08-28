#include <cstdint>
#include <cstdio>

int main()
{
    double x = 100;
    const auto orig_x = x;

    // 通过uint64_t*指针修改double的最高位，改变double的符号
    // 非法的别名，违反了strict aliasing规则，可能导致编译器优化错误
    auto x_as_ui = (uint64_t*)(&x);
    *x_as_ui |= 0x8000000000000000;

    printf("orig_x:%0.2f x:%0.2f &x:%p &x_as_ui:%p\n", orig_x, x, &x, x_as_ui);
}
