#ifndef SK_SEMVER_HPP
#define SK_SEMVER_HPP
#include <cstdint>
#include <regex>
#include <stdexcept>
#include <sstream>
#include <string_view>


namespace sk {


// Forward declarations.
template<typename Policy>
class version;
class prerelease;
class build_meta;


namespace detail {


template<typename Policy>
class has_kPattern_var {
private:
    template<typename T>
    static auto test(int) ->
        decltype(T::kPattern, std::true_type());

    template<typename>
    static auto test(...) ->
        std::false_type;

public:
    constexpr static bool
    value = decltype(test<Policy>(0))::value;
};


template<typename Policy>
class has_validateSchema_fn {
private:
    template<typename T>
    static auto test(int) ->
        decltype(T::validateSchema(std::declval<const std::cmatch&>()), std::true_type());

    template<typename>
    static auto test(...) ->
        std::false_type;

public:
    constexpr static bool
    value = decltype(test<Policy>(0))::value;
};


template<typename Policy>
inline constexpr bool has_validateSchema_fn_v =
    has_validateSchema_fn<Policy>::value;


template<typename Policy>
inline constexpr bool has_kPattern_var_v =
    has_kPattern_var<Policy>::value;


template<typename Policy>
struct is_parsing_policy {
    constexpr static bool
    value = has_kPattern_var_v<Policy> &&
            has_validateSchema_fn_v<Policy>;
};


template<typename Policy>
inline constexpr bool is_parsing_policy_v =
    is_parsing_policy<Policy>::value;



struct strict_version_parsing_policy final {
    constexpr static std::string_view
    kPattern = "^(0|[1-9]\\d*)\\.(0|[1-9]\\d*)\\.(0|[1-9]\\d*)"
               "(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))"
               "?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?$";

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
};



struct loose_version_parsing_policy final {
    constexpr static std::string_view
    kPattern = "^v?(0|[1-9]\\d*)(?:\\.(0|[1-9]\\d*))?(?:\\.(0|[1-9]\\d*))"
               "?(?:-((?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\\.(?:0|[1-9]\\d*|\\d*[a-zA-Z-][0-9a-zA-Z-]*))*))"
               "?(?:\\+([0-9a-zA-Z-]+(?:\\.[0-9a-zA-Z-]+)*))?$";

    static void
    validateSchema(const std::cmatch& match) {
        // Only need to validate the the major version is present.
        constexpr std::string_view kMajorRequired = "Major version is required";
        if (!match[1].matched) throw std::invalid_argument(kMajorRequired.data());
    }
};



inline std::uint64_t
convertNumeric(std::string_view number,
               std::string_view partName) {
    try {
        return std::stoull(number.data());
    } catch (const std::exception&) {
        throw std::invalid_argument(partName.data());
    }
}


} // namespace sk::detail


class prerelease final {
    class part final {
    public:
        constexpr part() = default;
                 ~part() = default;

        constexpr part(std::string_view value) noexcept
            : value_(value) {}

        constexpr part(const part&) = default;
        constexpr part& operator=(const part&) = default;

        constexpr part(part&&) noexcept = default;
        constexpr part& operator=(part&&) noexcept = default;

        constexpr static part
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

    #ifdef __cpp_impl_three_way_comparison
        constexpr std::strong_ordering
        operator<=>(const part& other) const noexcept {
            return value_ <=> other.value_;
        }
    #else
        constexpr bool
        operator==(const part& other) const noexcept {
            return value_ == other.value_;
        }

        constexpr bool
        operator!=(const part& other) const noexcept {
            return value_ != other.value_;
        }

        constexpr bool
        operator<(const part& other) const noexcept {
            return value_ < other.value_;
        }

        constexpr bool
        operator>(const part& other) const noexcept {
            return value_ > other.value_;
        }

        constexpr bool
        operator<=(const part& other) const noexcept {
            return value_ <= other.value_;
        }

        constexpr bool
        operator>=(const part& other) const noexcept {
            return value_ >= other.value_;
        }
    #endif

    private:
        std::string_view value_;
    };

public:
    constexpr prerelease() = default;
             ~prerelease() = default;

    constexpr prerelease(std::string_view value,
                         std::vector<part> parts) noexcept
        : value_(value)
        , parts_(std::move(parts)) {}

    constexpr prerelease(std::string_view value,
                         std::vector<std::string_view> parts) noexcept
        : value_(value) {
        for (auto substr : parts)
            parts_.emplace_back(part::parse(substr));
    }
    
    constexpr prerelease(const prerelease&) = default;
    constexpr prerelease(prerelease&&) noexcept = default;
    constexpr prerelease& operator=(const prerelease&) = default;
    constexpr prerelease& operator=(prerelease&&) noexcept = default;

    #ifdef __cpp_impl_three_way_comparison
        constexpr std::weak_ordering
        operator<=>(const prerelease& other) const noexcept {
            return parts_ <=> other.parts_;
        }
    #else

    #endif

    // TODO: attempt to make constexpr.
    static prerelease
    parse(std::string_view str) {
        if (str.empty())
            return prerelease{};

        std::vector<part> parts;
        for (auto substr : split(str))
            parts.emplace_back(part::parse(substr));
        return prerelease{ str, parts };
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



template<typename Policy = detail::strict_version_parsing_policy>
class version final {
    constexpr static std::string_view kPolicyError =
        "Policy must implement a static parse(const std::string&) function.";

    // Verify that the policy implements a static parse(const std::string&) function.
    static_assert(detail::is_parsing_policy_v<Policy>, kPolicyError.data());

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
        auto regex = std::regex(Policy::kPattern.data());

        std::cmatch match;
        if (!std::regex_match(input.data(), match, regex))
            throw std::invalid_argument("invalid version string");

        std::uint64_t major = 0;
        std::uint64_t minor = 0;
        std::uint64_t patch = 0;
        prerelease    prerel;
        build_meta    meta;
        try {
            Policy::validateSchema(match);
            major = convertNumeric(match[1].str(), "major");
            minor = match[2].matched
                ? convertNumeric(match[2].str(), "minor")
                : 0;
            patch = match[3].matched
                ? convertNumeric(match[3].str(), "patch")
                : 0;

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

        // return version<detail::strict_version_parsing_policy>.
        return { major, minor, patch, prerel, meta };
    }

private:
    prerelease prerelease_;
    build_meta build_meta_;

    std::string   value_;
    std::uint64_t major_;
    std::uint64_t minor_;
    std::uint64_t patch_;
};


} // namespace sk

#undef SK_CONSTEXPR
#endif // SK_SEMVER_HPP