// yahoo_fetcher_test.cpp
#include <gtest/gtest.h>
#include <optional>
#include <string>

#include "fetch/yahoo.hpp"

namespace quant_scan::fetch
{
    // ---- Test-side http_get mock + helpers ----
    //
    // We define http_get in this TU so Fetcher<Provider::yahoo> will
    // call this instead of a real network function. Make sure you
    // don't link a second definition of http_get into this test binary.

    inline std::optional<std::string> http_get_return;
    inline std::string last_http_get_url;

    std::optional<std::string> http_get(const std::string& url)
    {
        last_http_get_url = url;
        return http_get_return;
    }
} // namespace quant_scan::fetch

// ---- Test fixture ----

class YahooFetcherTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        using namespace quant_scan::fetch;
        http_get_return.reset();
        last_http_get_url.clear();
    }

    using fetch_t = quant_scan::fetch::Fetcher<quant_scan::fetch::Provider::yahoo>;
};

// ---- Tests ----

TEST_F(YahooFetcherTest, ReturnsNulloptWhenHttpGetReturnsNullopt)
{
    using namespace quant_scan::fetch;

    OptionRequest request{.symbol = "AAPL"};

    http_get_return = std::nullopt;

    auto result = fetch_t::fetch(request);
    EXPECT_FALSE(result.has_value());
}

TEST_F(YahooFetcherTest, ReturnsNulloptWhenJsonParseFails)
{
    using namespace quant_scan::fetch;

    OptionRequest req{};
    req.symbol = "AAPL";

    // Invalid JSON
    http_get_return = std::make_optional<std::string>("not json at all");

    auto result = fetch_t::fetch(req);
    EXPECT_FALSE(result.has_value());
}

TEST_F(YahooFetcherTest, ReturnsNulloptWhenOptionChainMissing)
{
    using namespace quant_scan::fetch;

    OptionRequest req{};
    req.symbol = "AAPL";

    nlohmann::json j = {
        { "foo", 123 }
    };
    http_get_return = j.dump();

    auto result = fetch_t::fetch(req);
    EXPECT_FALSE(result.has_value());
}

TEST_F(YahooFetcherTest, ReturnsNulloptWhenResultArrayMissingOrEmpty)
{
    using namespace quant_scan::fetch;

    OptionRequest req{};
    req.symbol = "AAPL";

    // optionChain present but result missing
    {
        nlohmann::json j = {
            { "optionChain", nlohmann::json{
                { "error", nullptr }
            } }
        };
        http_get_return = j.dump();
        auto result = fetch_t::fetch(req);
        EXPECT_FALSE(result.has_value());
    }

    // optionChain.result present but empty
    {
        nlohmann::json j = {
            { "optionChain", nlohmann::json{
                { "result", nlohmann::json::array() },
                { "error", nullptr }
            } }
        };
        http_get_return = j.dump();
        auto result = fetch_t::fetch(req);
        EXPECT_FALSE(result.has_value());
    }
}

TEST_F(YahooFetcherTest, ReturnsFirstResultWhenValidJsonAndResultTIsJson)
{
    using namespace quant_scan::fetch;

    OptionRequest req{};
    req.symbol = "AAPL";

    nlohmann::json first_result = {
        { "symbol", "AAPL" },
        { "dummyField", 42 }
    };

    nlohmann::json j = {
        { "optionChain", nlohmann::json{
            { "result", nlohmann::json::array({ first_result }) },
            { "error", nullptr }
        } }
    };

    http_get_return = j.dump();

    auto result = fetch_t::fetch(req);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, first_result);

    // Also verify that the URL we built looks like a plain options URL
    EXPECT_EQ(last_http_get_url,
              "https://query1.finance.yahoo.com/v7/finance/options/" + req.symbol);
}

TEST_F(YahooFetcherTest, NonOptionRequestReturnsNullopt)
{
    using namespace quant_scan::fetch;

    struct DummyRequest
    {
        using result_t = nlohmann::json;
        // NOTE: no symbol field; this should *not* be treated as OptionRequest.
    };

    DummyRequest req{};

    auto result = fetch_t::fetch(req);
    EXPECT_FALSE(result.has_value());
}
