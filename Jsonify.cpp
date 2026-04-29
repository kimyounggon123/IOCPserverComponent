#include "Jsonify.h"
// JSON 파싱
bool Jsonity::parse(const std::string& jsonStr)
{
    try {
        data = json::parse(jsonStr);
        return true;
    }
    catch (...) {
        return false;
    }
}

// JSON을 문자열로 반환
std::string Jsonity::dump(int indent) const
{
    return indent == -1 ? data.dump() : data.dump(indent);
}

// 키가 존재하는지
bool Jsonity::has(const std::string& key) const
{
    return data.contains(key);
}

// 원본 JSON에 접근
const json& Jsonity::raw() const { return data; }


