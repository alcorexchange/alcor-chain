#include <boost/test/unit_test.hpp>

#include <eosio/testing/tester.hpp>

#include <fc/variant_object.hpp>

#include <test_contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;

// transaction_trace::auth_checked tells the producer whether a failed trx may be charged to its first authorizer
// (subjective billing, failure limit). Only a trx whose authorizations this attempt verified may be.
BOOST_AUTO_TEST_SUITE(auth_checked_tests)

namespace {

transaction_trace_ptr push_procassert(validating_tester& t, int8_t condition, bool sign) {
   signed_transaction trx;
   trx.actions.emplace_back(t.get_action("alice"_n, "procassert"_n, {{"alice"_n, config::active_name}},
                                         fc::mutable_variant_object()("condition", condition)("message", "no")));
   t.set_transaction_headers(trx);
   if (sign)
      trx.sign(t.get_private_key("alice"_n, "active"), t.get_chain_id());
   else
      trx.sign(t.get_private_key("bob"_n, "active"), t.get_chain_id()); // claims alice, signed by someone else
   return t.push_transaction(trx, fc::time_point::maximum(), validating_tester::DEFAULT_BILLED_CPU_TIME_US, true);
}

} // namespace

BOOST_AUTO_TEST_CASE(auth_checked) {
   validating_tester t;
   t.create_accounts({"alice"_n, "bob"_n});
   t.set_code("alice"_n, test_contracts::asserter_wasm());
   t.set_abi("alice"_n, test_contracts::asserter_abi());
   t.produce_block();

   auto bad_sig = push_procassert(t, 1, false);
   BOOST_REQUIRE(bad_sig->except);
   BOOST_TEST(!bad_sig->auth_checked);

   auto failed_in_contract = push_procassert(t, 0, true);
   BOOST_REQUIRE(failed_in_contract->except);
   BOOST_TEST(failed_in_contract->auth_checked);

   auto ok = push_procassert(t, 1, true);
   BOOST_REQUIRE(!ok->except);
   BOOST_TEST(ok->auth_checked);
   t.produce_block();
}

BOOST_AUTO_TEST_SUITE_END()
