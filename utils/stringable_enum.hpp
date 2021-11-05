#ifndef __STINGABLE_ENUM__HPP__
#define __STINGABLE_ENUM__HPP__

#include <array>
#include <string_view>
#include <cinttypes>
#include <limits>

namespace stringable_enum::detail {

template<class BaseType, size_t N>
struct EnumOptions {
    struct Seg {
        uint16_t l, r;
        BaseType v;
    };
    std::array<Seg, N> segs{};
    uint16_t count = 0;
    std::array<char, N> data{};
    constexpr EnumOptions(const char (&s)[N], BaseType) {
        static_assert(N <= (size_t) std::numeric_limits<uint16_t>::max());
        uint16_t pos = 0;
        bool in_name = 0, in_value = 0;
        uint16_t name_start = 0;
        BaseType val = 0;
        bool minus = 0;
        while (s[pos]) {
            data[pos] = s[pos];
            if (('a' <= s[pos] && s[pos] <= 'z')
                || ('A' <= s[pos] && s[pos] <= 'Z')
                || s[pos] == '$' || s[pos] == '_'
                || (in_name && '0' <= s[pos] && s[pos] <= '9'))
            {
                if (!in_name)
                    name_start = pos;
                in_name = 1;
            } else {
                if (in_name) {
                    segs[count++] = Seg{name_start, pos, val++};
                }
                in_name = 0;
                if (in_value && '0' <= s[pos] && s[pos] <= '9') {
                    val *= 10;
                    if (minus)
                        val -= s[pos] - '0';
                    else
                        val += s[pos] - '0';
                } else if (in_value && s[pos] == '-') {
                    minus = 1;
                }
                if (s[pos] == '=') {
                    in_value = 1;
                    minus = 0;
                    val = 0;
                } else if (s[pos] == ',') {
                    segs[count - 1].v = val;
                    in_value = 0;
                }
            }
            ++pos;
        }
        if (in_name) {
            segs[count++] = Seg{name_start, pos, val++};
        } else if (in_value) {
            segs[count - 1].v = val;
        }
    }
};

}; // namespace stringable_enum::detail

#define ENUM_TYPE(kind, Name, BaseType, ...) kind Name { __VA_ARGS__ }; \
namespace stringable_enum { \
    inline std::string_view to_string(Name e) { \
        static constexpr ::stringable_enum::detail::EnumOptions opts(#__VA_ARGS__, BaseType{}); \
        for (uint16_t i = 0; i < opts.count; ++i) { \
            auto [l, r, v] = opts.segs[i]; \
            if (e == static_cast<decltype(e)>(v)) \
                return std::string_view(opts.data.data() + l, r - l); \
        } \
        return ""; \
    } \
    template<class> struct EnumValues; \
    template<> \
    struct EnumValues<Name> { \
        static constexpr auto opts = ::stringable_enum::detail::EnumOptions(#__VA_ARGS__, BaseType{}); \
        static constexpr auto list = []<size_t... i>(std::index_sequence<i...>) { \
            return std::array<Name, opts.count> {(Name) opts.segs[i].v...}; \
        }(std::make_index_sequence<opts.count>{}); \
    }; \
};

#define ENUM(Name, BaseType, ...) ENUM_TYPE(enum, Name, BaseType, __VA_ARGS__)
#define ENUM_CLASS(Name, BaseType, ...) ENUM_TYPE(enum class, Name, BaseType, __VA_ARGS__)

#endif
