#include "DataToConnectWithClient.h"

template <>
bool _VectorData<int>::equalApproximately(const _VectorData<int>& other, int eps) const {
    // 정수는 그냥 정확 비교
    return x == other.x && y == other.y && z == other.z;
}