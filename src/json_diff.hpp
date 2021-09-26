#pragma once

#include <boost/json/value.hpp>

namespace json = boost::json;

namespace btd{

// This function inspired from nlohmann and jsoncons

json::array
static diff(const json::value & source, const json::value & target,
            const std::string & path = "")
{
    json::array result;

    if (source == target)
    {
        return result;
    }

    if (source.kind() != target.kind())
    {
        result.push_back(json::value{
            {"op", "replace"}, {"path", path}, {"value", target}
        });
        return result;
    }

    switch (source.kind())
    {
        case json::kind::array:
        {
            const auto _src = source.get_array();
            const auto _tgt = target.get_array();
            std::size_t i = 0;
            while (i < _src.size() && i < _tgt.size())
            {
                auto temp_diff = diff(_src[i], _tgt[i], path + "/" + std::to_string(i));
                result.insert(result.end(), temp_diff.begin(), temp_diff.end());
                ++i;
            }

            const std::size_t end_index = _tgt.size();
            for (std::size_t i = _src.size(); i-- > _tgt.size();)
            {
                result.push_back(json::value({
                    {"op", "remove"},
                    {"path", path + "/" + std::to_string(i)}
                }));
            }

            for (std::size_t i = _src.size(); i < _tgt.size(); ++i)
            {
                result.push_back(json::value({
                    {"op", "add"},
                    {"path", path + "/-"},
                    {"value", _tgt[i]}
                }));
                ++i;
            }
            break;
        }
        case json::kind::object:
        {
            const auto _src = source.get_object();
            const auto _tgt = target.get_object();

            for (auto it = _src.cbegin(); it != _src.cend(); ++it)
            {
                const auto path_key = path + "/" + it->key().to_string();
                const auto found = _tgt.find(it->key());
                if (found != _tgt.end())
                {
                    auto temp_diff = diff(it->value(), found->value(), path_key);
                    result.insert(result.end(), temp_diff.begin(), temp_diff.end());
                }
                else
                {
                    // found a key that is not in o -> remove it
                    result.push_back(json::value({
                        {"op", "remove"}, {"path", path_key}
                    }));
                }
            }

            // second pass: traverse other object's elements
            for (auto it = _tgt.cbegin(); it != _tgt.cend(); ++it)
            {
                if (_src.find(it->key()) == _src.end())
                {
                    // found a key that is not in this -> add it
                    const auto path_key = path + "/" + it->key().to_string();
                    result.push_back(json::value({
                        {"op", "add"}, {"path", path_key},
                        {"value", it->value()}
                    }));
                }
            }
            break;
        }
        case json::kind::null:
        case json::kind::bool_:
        case json::kind::int64:
        case json::kind::uint64:
        case json::kind::double_:
        case json::kind::string:
        default:
        {
            result.push_back(json::value{
                {"op", "replace"}, {"path", path}, {"value", target}
            });
            break;
        }
    }

    return result;
}

} // namespace

