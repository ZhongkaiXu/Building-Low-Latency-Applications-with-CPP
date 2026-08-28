// 返回值优化，编译器优化，避免不必要的拷贝
#include <iostream>

struct LargeClass {
    int i;
    char c;
    double d;
};

auto rvoExample(int i, char c, double d)
{
    return LargeClass { i, c, d };
}

int main()
{
    LargeClass lc_obj = rvoExample(10, 'c', 3.14);
}