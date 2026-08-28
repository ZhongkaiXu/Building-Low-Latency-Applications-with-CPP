void func(int* a, int* b, int n)
{
    for (int i = 0; i < n; ++i) {
        // 编译器必须考虑b和a重叠的情况
        // *b可能随着a而改变，编译器无法优化，必须每次都重新读取*b
        a[i] = *b;
    }
}

void func_restrict(int* __restrict a, int* __restrict b, int n)
{
    // 保证a和b不重叠，编译器可以优化
    for (int i = 0; i < n; ++i) {
        a[i] = *b;
    }
}

int main()
{
    int a[10], b;
    func(a, &b, 10);
    func_restrict(a, &b, 10);
}