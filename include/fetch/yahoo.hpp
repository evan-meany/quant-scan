#pragma once

#include <optional>
#include <string>
#include <type_traits>
#include <chrono>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "fetch.hpp"   // expects: std::optional<std::string> http_get(const std::string& url);
#include "request.hpp" // expects: struct OptionRequest { std::string symbol; /* optional: expiration */ using result_t = nlohmann::json; }

namespace quant_scan::fetch
{
    namespace detail
    {
        template <typename T>
        concept HasSymbol = requires(const T& t) { { t.symbol } -> std::convertible_to<std::string>; };

        template <typename T>
        concept HasOptionalExpiration = requires(const T& t)
        {
            // t.expiration is an optional-like with value().  Adapt if your type differs.
            { t.expiration.has_value() } -> std::convertible_to<bool>;
            { t.expiration.value() };
        };
    }

    template <>
    struct Fetcher<Provider::yahoo>
    {
        template <typename Request>
        static std::optional<typename Request::result_t> fetch(const Request& request)
        {
            if constexpr (std::is_same_v<Request, OptionRequest>)
            {
                const auto url = build_options_url(request);
                auto body = http_get(url);
                if (!body) { return std::nullopt; }

                nlohmann::json j = nlohmann::json::parse(*body, /*cb*/nullptr, /*allow_exceptions*/false);
                if (j.is_discarded()) { return std::nullopt; }

                // Yahoo: { "optionChain": { "result": [ {...} ], "error": null } }
                if (!j.contains("optionChain")) { return std::nullopt; }
                const auto& chain = j["optionChain"];
                if (!chain.contains("result") || !chain["result"].is_array() || chain["result"].empty())
                {
                    return std::nullopt;
                }

                // If your Request::result_t == nlohmann::json, we can return this directly.
                if constexpr (std::is_same_v<typename Request::result_t, nlohmann::json>)
                {
                    return chain["result"].front();
                }
                else
                {
                    // Map into your custom result type here
                    return parse_option_result<typename Request::result_t>(chain["result"].front());
                }
            }

            // Unknown request type for Yahoo
            return std::nullopt;
        }

    private:
        // -------- URL building --------

        // Overload when request has an expiration (optional)
        template <typename Request>
        requires (detail::HasSymbol<Request> && detail::HasOptionalExpiration<Request>)
        static std::string build_options_url(const Request& req)
        {
            std::string url = "https://query1.finance.yahoo.com/v7/finance/options/";
            url += req.symbol;

            if (req.expiration.has_value())
            {
                const auto unix_ts = to_unix_timestamp(req.expiration.value());
                url += "?date=" + std::to_string(unix_ts);
            }
            return url;
        }

        // Overload when request has only a symbol
        template <typename Request>
        requires (detail::HasSymbol<Request> && (!detail::HasOptionalExpiration<Request>))
        static std::string build_options_url(const Request& req)
        {
            std::string url = "https://query1.finance.yahoo.com/v7/finance/options/";
            url += req.symbol; // no date parameter
            return url;
        }

        // Convert your core::Date (year_month_day) to UNIX timestamp (00:00:00 UTC)
        static std::int64_t to_unix_timestamp(const quant_scan::core::Date& date)
        {
            using namespace std::chrono;

            // Interpret as midnight UTC of that civil date
            const sys_seconds sec = time_point_cast<seconds>(sys_days(date.ymd));
            return sec.time_since_epoch().count();
        }

        // -------- Result mapping (optional) --------
        template <typename ResultT>
        static std::optional<ResultT> parse_option_result(const nlohmann::json& j)
        {
            // Default: not implemented. Specialize/overload for your ResultT if needed.
            return std::nullopt;
        }
    };
}
