#include <eosio/chain/block_timestamp.hpp>
#include <eosio/chain/config.hpp>

#include <boost/test/unit_test.hpp>

#include <fc/time.hpp>
#include <fc/exception/exception.hpp>

using namespace eosio;
using namespace chain;

BOOST_AUTO_TEST_SUITE(block_timestamp_tests)


BOOST_AUTO_TEST_CASE(constructor_test) {
	block_timestamp_type bt;
        BOOST_TEST( bt.slot == 0u, "Default constructor gives wrong value");
        
	// alcor-chain: in units of the epoch and interval (upstream: 2000 epoch, 2 slots per second)
	constexpr int64_t epoch_s = config::block_timestamp_epoch / 1000;
	fc::time_point t(fc::seconds(epoch_s + 31536000)); // a year after the epoch
	block_timestamp_type bt2(t);
	BOOST_TEST( bt2.slot == 31536000u * 1000 / config::block_interval_ms, "Time point constructor gives wrong value");
}

BOOST_AUTO_TEST_CASE(conversion_test) {
	block_timestamp_type bt;
	fc::time_point t = (fc::time_point)bt;
	BOOST_TEST(t.time_since_epoch().to_seconds() == int64_t(config::block_timestamp_epoch / 1000), "Time point conversion failed");

	block_timestamp_type bt1(200);
	t = (fc::time_point)bt1;
	BOOST_TEST(t.time_since_epoch().to_seconds() == int64_t((config::block_timestamp_epoch + 200 * config::block_interval_ms) / 1000), "Time point conversion failed");

}

BOOST_AUTO_TEST_SUITE_END()
