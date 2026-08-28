int main()
{
    { // original code.
        int a[5];
        a[0] = 0;
        for (int i = 1; i < 5; ++i)
            a[i] = a[i - 1] + 1;
    }

    { // loop unrolled code.
        // 展开，减少循环分支的开销，减少循环分支预测失败的概率
        int a[5];
        a[0] = 0;
        a[1] = a[0] + 1;
        a[2] = a[1] + 1;
        a[3] = a[2] + 1;
        a[4] = a[3] + 1;
    }
}