// 接口设计方面
#include <cstdio>
#include <vector>

struct Order {
    int id;
    double price;
};

// 直接继承std::vector<Order>，OrderBook的接口就包含了std::vector<Order>的所有接口
// 外界会直接调用内部order，危险
class InheritanceOrderBook : public std::vector<Order> {
};

class CompositionOrderBook {
    // 使用组合的方式，内部private成员orders_，外界无法直接访问内部的std::vector<Order>，只能通过OrderBook提供的接口访问
    std::vector<Order> orders_;

public:
    auto size() const noexcept
    {
        return orders_.size();
    }
};

int main()
{
    InheritanceOrderBook i_book;
    CompositionOrderBook c_book;

    printf("InheritanceOrderBook::size():%lu CompositionOrderBook:%lu\n", i_book.size(), c_book.size());
}