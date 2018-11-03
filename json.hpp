#include <boost/hana.hpp>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include <map>
#include <string>
#include <vector>
#include <codecvt>

namespace hana = boost::hana;

template<typename T>
struct Val
{
    using Type = T;
    rapidjson::Value const& v;
};

/**
 * Parse json value as int
 */
int parseValue(Val<int> const& value)
{
    return value.v.GetInt();
}

/**
 * Parse value value as std::string
 */
std::string parseValue(Val<std::string> const& value)
{
    return value.v.GetString();
}

/**
 * Parse value value as std::string
 */
std::wstring parseValue(Val<std::wstring> const& value)
{
    std::string tmp = value.v.GetString();
    std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
    return myconv.from_bytes(tmp);
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
                member = parseValue(Val<ValueType>{value.v[boost::hana::to<char const*>(key)]});
            });

    return t;
};

using JsonWriter = rapidjson::Writer<rapidjson::StringBuffer>;

void packValue(std::string const& value, JsonWriter & writer)
{
    writer.String(value.c_str());
}

void packValue(int value, JsonWriter & writer)
{
    writer.Int(value);
}

template<typename T>
void packValue(std::vector<T> const& t, JsonWriter & writer)
{
    writer.StartArray();
    for (auto const& e : t)
    {
        packValue(e, writer);
    }
    writer.EndArray();
}

template<typename T>
void packValue(T const& t, JsonWriter & writer)
{

    writer.StartObject();

    boost::hana::for_each(boost::hana::keys(t), [&](auto key) {
                auto &member = boost::hana::at_key(t, key);

                char const * json_key = boost::hana::to<char const*>(key);
                writer.Key(json_key);
                packValue(member, writer);
            });

    writer.EndObject();

}

template<typename T>
T fromJson(std::string const& s)
{
    rapidjson::Document doc;
    doc.Parse(s.c_str());
    return parseValue(Val<T>{doc});
}

template<typename T>
std::string toJson(T const& value)
{
    rapidjson::StringBuffer buffer;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

    packValue(value, writer);

    return buffer.GetString();
}
