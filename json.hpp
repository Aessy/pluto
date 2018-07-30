#ifndef JSON_HPP_INCLUDED
#define JSON_HPP_INCLUDED

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include <boost/hana.hpp>

namespace json
{


template<typename T>
struct Val
{
    using Type = T;
    rapidjson::Value const& v;
};

/**
 * Parse json value as int
 */
inline int parseValue(Val<int> const& value)
{
    return value.v.GetInt();
}

/**
 * Parse value value as std::string
 */
inline std::string parseValue(Val<std::string> const& value)
{
    return value.v.GetString();
}

/**
 * Parse json value value as vector of elements.
 */
template<typename T>
std::vector<T> parseValue(Val<std::vector<T>> const& value)
{
    std::vector<T> objects;
    for (size_t i = 0; i < value.v.Size(); ++i)
    {
        objects.push_back(parseValue(Val<T>{value.v[i]}));
    }

    return objects;
}

/**
 * \brief parse json value as a object.
 */
template<typename T>
T parseValue(Val<T> const& value)
{
    T t;
    boost::hana::for_each(boost::hana::keys(t), [&](auto key) {
                auto &member = boost::hana::at_key(t, key);
                using ValueType = typename std::remove_reference<decltype(member)>::type;
                char const * char_key = boost::hana::to<char const*>(key);
                if (value.v.HasMember(char_key))
                {
                    member = parseValue(Val<ValueType>{value.v[boost::hana::to<char const*>(key)]});
                }
            });

    return t;
};

template<typename T>
T fromJson(std::string const& s)
{
    rapidjson::Document doc;
    doc.Parse(s.c_str());
    return parseValue(Val<T>{doc});
}

}

#endif
