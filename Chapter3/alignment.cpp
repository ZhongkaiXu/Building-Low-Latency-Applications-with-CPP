// 对齐问题，占用空间的问题
#include <cstddef>
#include <cstdint>
#include <cstdio>

struct PoorlyAlignedData {
    char c; // offset 0: char
            // offset 1: padding
    uint16_t u; // offset 2-3: uint16_t
                // offset 4-7: padding
    double d; // offset 8-15: double
    int16_t i; // offset 16-17: int16_t
               // offset 18-23: padding
};

// 把大的数据成员放在前面，省去很多padding
struct WellAlignedData {
    double d; // 0-7
    uint16_t u; // 8-9
    int16_t i; // 10-11
    char c; // 12
            // 13-15: padding
};

// 紧密排列，不padding，不对齐，访问PackedData的成员可能会比WellAlignedData慢很多
// 通常用于网络传输，或者文件存储
#pragma pack(push, 1)
struct PackedData {
    double d;
    uint16_t u;
    int16_t i;
    char c;
};
#pragma pack(pop)

int main()
{
    printf("PoorlyAlignedData c:%lu u:%lu d:%lu i:%lu size:%lu\n",
        offsetof(struct PoorlyAlignedData, c), offsetof(struct PoorlyAlignedData, u), offsetof(struct PoorlyAlignedData, d), offsetof(struct PoorlyAlignedData, i), sizeof(PoorlyAlignedData));
    printf("WellAlignedData d:%lu u:%lu i:%lu c:%lu size:%lu\n",
        offsetof(struct WellAlignedData, d), offsetof(struct WellAlignedData, u), offsetof(struct WellAlignedData, i), offsetof(struct WellAlignedData, c), sizeof(WellAlignedData));
    printf("PackedData d:%lu u:%lu i:%lu c:%lu size:%lu\n",
        offsetof(struct PackedData, d), offsetof(struct PackedData, u), offsetof(struct PackedData, i), offsetof(struct PackedData, c), sizeof(PackedData));
}