#include "DataToConnectWithClient.h"

template <>
bool _VectorData<int>::equalApproximately(const _VectorData<int>& other, int eps) const {
    // ������ �׳� ��Ȯ ��
    return x == other.x && y == other.y && z == other.z;
}