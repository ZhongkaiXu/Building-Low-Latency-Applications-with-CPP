// 奇异递归模板，是一种编程技巧
#include <cstdio>

class RuntimeExample {
public:
    virtual void placeOrder()
    {
        printf("RuntimeExample::placeOrder()\n");
    }
};

// 运行时多态，override成员函数
class SpecificRuntimeExample : public RuntimeExample {
public:
    void placeOrder() override
    {
        printf("SpecificRuntimeExample::placeOrder()\n");
    }
};

// 编译器多态，通过模板实现
template <typename actual_type>
class CRTPExample {
public:
    void placeOrder()
    {
        static_cast<actual_type*>(this)->actualPlaceOrder();
    }

    void actualPlaceOrder()
    {
        printf("CRTPExample::actualPlaceOrder()\n");
    }
};

class SpecificCRTPExample : public CRTPExample<SpecificCRTPExample> {
public:
    // 更容易内联
    void actualPlaceOrder()
    {
        printf("SpecificCRTPExample::actualPlaceOrder()\n");
    }
};

int main(int, char**)
{
    RuntimeExample* runtime_example = new SpecificRuntimeExample();
    runtime_example->placeOrder();

    SpecificCRTPExample crtp_example;
    crtp_example.placeOrder();

    return 0;
}
