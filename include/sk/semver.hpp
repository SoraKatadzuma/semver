#ifndef SK_SEMVER_HPP
#define SK_SEMVER_HPP
#include <cstdint>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string_view>


#if __cplusplus < 202002L
#define SK_NO_CXX20 1
#endif



namespace sk {
namespace detail {


template<typename Policy>
class has_parse_fn {
private:
    template<typename T>
    static auto test(int) ->
        decltype(T::parse(std::declval<const std::string&>()), std::true_type());

    template<typename>
    static auto test(...) ->
        std::false_type;

public:
    constexpr static bool
    value = decltype(test<Policy>(0))::value;
};


template<typename>
struct is_policy 
    : std::false_type {};

template<>
struct is_policy<struct strict_policy> 
    : std::true_type {};

template<>
struct is_policy<struct loose_policy> 
    : std::true_type {};


} // namespace sk::detail


template<typename Policy>
class version;
class prerelease;
class build_meta;


struct strict_policy final {
    constexpr static std::string_view
    kPattern = "^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)"
               "(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))"
               "?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?$";


    static version<strict_policy>
    parse(const std::string& input);


    static void
    validateSchema(const std::cmatch& match) {
        constexpr std::string_view kMajorRequired = "Major version is required";
        constexpr std::string_view kMinorRequired = "Minor version is required";
        constexpr std::string_view kPatchRequired = "Patch version is required";

        // The first three parts are required.
        if (!match[1].matched) throw std::invalid_argument(kMajorRequired.data());
        if (!match[2].matched) throw std::invalid_argument(kMajorRequired.data());
        if (!match[3].matched) throw std::invalid_argument(kPatchRequired.data());
    }

    static std::uint64_t
    convertNumeric(std::string_view number,
                   std::string_view partName) {
        try {
            return std::stoull(number.data());
        } catch (const std::exception&) {
            throw std::invalid_argument(partName.data());
        }
    }
};



struct loose_policy final {
    constexpr static std::string_view
    kPattern = "^v?(0|[1-9]\\d*)(?:\\.(0|[1-9]\\d*))?(?:\\.(0|[1-9]\\d*))"
               "?(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))"
               "?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?$";
};




/// @brief 
class prerelease final {
    class part final {
    public:
        part() = default;
        ~part() = default;

        part(std::string_view value) noexcept
            : value_(value) {}

        part(const part&) = default;
        part& operator=(const part&) = default;

        part(part&&) noexcept = default;
        part& operator=(part&&) noexcept = default;

        static part
        parse(std::string_view text) {
            auto is_valid = std::all_of(text.begin(), text.end(), [](char c) {
                return std::isalnum(c) || c == '-';
            });

            if (text.empty() || !is_valid)
                throw std::invalid_argument("Empty prerelease part.");

            if (text.size() > 1 && text[0] == '0')
                throw std::invalid_argument("Leading zero in prerelease part.");

            return part{ text };
        }

    #if SK_NO_CXX20
        bool
        operator==(const part& other) const noexcept {
            return value_ == other.value_;
        }

        bool
        operator!=(const part& other) const noexcept {
            return value_ != other.value_;
        }

        bool
        operator<(const part& other) const noexcept {
            return value_ < other.value_;
        }

        bool
        operator>(const part& other) const noexcept {
            return value_ > other.value_;
        }

        bool
        operator<=(const part& other) const noexcept {
            return value_ <= other.value_;
        }

        bool
        operator>=(const part& other) const noexcept {
            return value_ >= other.value_;
        }
    #else
        std::strong_ordering
        operator<=>(const part& other) const noexcept {
            return value_ <=> other.value_;
        }
    #endif

    private:
        std::string_view value_;
    };

public:
    /// @brief  Parses a string into a prerelease.
    ///         A valid prerelease string is a non-empty string consisting of
    ///         one or more dot-separated parts. Each part must be a non-empty
    ///         string consisting of only alphanumeric characters and hyphens.
    /// @param  str The string to parse.
    /// @return A valid prerelease object constructed from the parsed input.
    /// @throws std::invalid_argument when the string is not a valid prerelease.
    static prerelease
    parse(std::string_view str) {
        if (str.empty())
            return prerelease{};

        prerelease result;
        result.value_ = str;
        for (auto substr : split(result.value_))
            result.parts_.emplace_back(part::parse(substr));
        return result;
    }

private:
    static std::vector<std::string_view>
    split(std::string_view str) {
        constexpr char kDelimiter = '.';

        std::size_t current  = 0;
        std::size_t previous = 0;
        std::vector<std::string_view> result;
        
        current = str.find(kDelimiter);
        while (current != std::string_view::npos) {
            result.emplace_back(str.substr(previous, current - previous));
            previous = current + 1;
            current  = str.find(kDelimiter, previous);
        }

        result.emplace_back(str.substr(previous, current - previous));
        return result;
    }

private:
    std::vector<part> parts_;
    std::string_view  value_;
};


class build_meta final {
public:
    build_meta() = default;
    ~build_meta() = default;

    build_meta(std::string_view value) noexcept
        : value_(value) {}

    build_meta(const build_meta&) = default;
    build_meta& operator=(const build_meta&) = default;

    build_meta(build_meta&&) noexcept = default;
    build_meta& operator=(build_meta&&) noexcept = default;

    static build_meta
    parse(std::string_view text) {
        auto is_valid = std::all_of(text.begin(), text.end(), [](char c) {
                return std::isalnum(c) || c == '-';
            });

        if (text.empty() || !is_valid)
            throw std::invalid_argument("Empty build meta.");

        return build_meta{ text };
    }

private:
    std::string_view value_;
};



template<typename Policy = strict_policy>
class version final {
public:
    version() = default;
    ~version() = default;

    version(const version&) = default;
    version(version&&) noexcept = default;
    version& operator=(const version&) = default;
    version& operator=(version&&) noexcept = default;

    version(std::uint64_t major,
            std::uint64_t minor,
            std::uint64_t patch,
            prerelease prerelease = prerelease{},
            build_meta build_meta = build_meta{}) noexcept
        : prerelease_(std::move(prerelease))
        , build_meta_(std::move(build_meta))
        , major_(major)
        , minor_(minor)
        , patch_(patch) {}


    static version
    parse(const std::string& str) {
        return Policy::parse(str);
    }

private:
    prerelease prerelease_;
    build_meta build_meta_;

    std::string   value_;
    std::uint64_t major_;
    std::uint64_t minor_;
    std::uint64_t patch_;
};



template<>
class version<loose_policy> final {

};



inline version<strict_policy>
strict_policy::parse(const std::string& input)  {
    auto regex = std::regex(kPattern.data());

    std::cmatch match;
    if (!std::regex_match(input.data(), match, regex))
        throw std::invalid_argument("invalid version string");

    std::uint64_t major = 0;
    std::uint64_t minor = 0;
    std::uint64_t patch = 0;
    prerelease    prerel;
    build_meta    meta;
    try {
        validateSchema(match);
        major = convertNumeric(match[1].str(), "major");
        minor = convertNumeric(match[2].str(), "minor");
        patch = convertNumeric(match[3].str(), "patch");

        if (match[4].matched) prerel = prerelease::parse(match[4].str());
        if (match[5].matched) meta   = build_meta::parse(match[5].str());
    } catch (const std::exception& e) {
        constexpr std::string_view kErrorMessage =
            "Failed to parse version string: ";
        
        std::ostringstream out;
        out << kErrorMessage
            << e.what();
        
        throw std::invalid_argument(out.str());
    }

    return version<strict_policy>{ major, minor, patch, prerel, meta };
}


} // namespace sk


#undef SK_NO_CXX20
#endif // SK_SEMVER_HPP