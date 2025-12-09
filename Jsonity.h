#ifndef _JSONITY_H
#define _JSONITY_H
#include "json.hpp"
using json = nlohmann::json;

class Jsonity
{
    json data;

public:
    Jsonity() = default;
    Jsonity(const json& j) : data(j) 
    {}
    Jsonity(const std::string& jsonStr)
    {
        parse(jsonStr);
    }

    // JSON 파싱
    bool parse(const std::string& jsonStr);
    
    // JSON을 문자열로 반환
    std::string dump(int indent = -1) const;

    // 키가 존재하는지
    bool has(const std::string& key) const;

    // 원본 JSON에 접근
    const json& raw() const;


    // 안전한 getter (템플릿)
    template <typename T>
    T get(const std::string& key, const T& defaultValue = T()) const
    {
        if (data.contains(key) && !data[key].is_null()) {
            try {
                return data[key].get<T>();
            }
            catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    // 값 설정
    template <typename T>
    void set(const std::string& key, const T& value)
    {
        data[key] = value;
    }

    // 연산자 오버로드: JSONity["key"] 처럼 쓰기
    json& operator[](const std::string& key)
    {
        return data[key];
    }
};

#endif