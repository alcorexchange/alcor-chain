#include <boost/test/unit_test.hpp>

#include <eosio/chain/priority_actions.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/variant_object.hpp>

#include <test_contracts.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;

// Priority actions are a producer policy (producer_plugin batch); here: which trxs are priority ones and how the
// node reads the list (eosio table prioacts). The block order is checked live on a node (stand/load/prio-test.sh).
BOOST_AUTO_TEST_SUITE(priority_actions_tests)

namespace {

const priority_action alice_doit{"alice"_n, "doit"_n};

transaction make_trx(const std::vector<std::pair<account_name, action_name>>& acts) {
   transaction trx;
   for (const auto& [account, act] : acts)
      trx.actions.emplace_back(vector<permission_level>{{account, config::active_name}}, account, act, bytes{});
   return trx;
}

// eosio runs priority_actions_test, which writes the row the node reads as given (no checks)
struct list_tester : validating_tester {
   list_tester() {
      set_code(config::system_account_name, test_contracts::priority_actions_test_wasm());
      set_abi(config::system_account_name, test_contracts::priority_actions_test_abi());
      produce_block();
   }
   void set_row(const std::vector<char>& value) {
      push_action(config::system_account_name, "setrow"_n, config::system_account_name, fc::mutable_variant_object()("value", value));
   }
   void set_list(const std::vector<priority_action>& list) { set_row(fc::raw::pack(list)); }
   void del_row() {
      push_action(config::system_account_name, "delrow"_n, config::system_account_name, fc::mutable_variant_object());
   }
};

} // namespace

BOOST_AUTO_TEST_CASE(classify) {
   const priority_actions prio{alice_doit, {"alice"_n, "cancel"_n}};

   BOOST_TEST(is_priority_transaction(prio, make_trx({{"alice"_n, "doit"_n}})));
   BOOST_TEST(is_priority_transaction(prio, make_trx({{"alice"_n, "doit"_n}, {"alice"_n, "cancel"_n}})));
   BOOST_TEST(!is_priority_transaction(prio, make_trx({})));
   BOOST_TEST(!is_priority_transaction(prio, make_trx({{"alice"_n, "doit"_n}, {"bob"_n, "doit"_n}}))); // a cancel can't carry a trade
   BOOST_TEST(!is_priority_transaction(prio, make_trx({{"bob"_n, "doit"_n}})));
   BOOST_TEST(!is_priority_transaction(priority_actions{}, make_trx({{"alice"_n, "doit"_n}})));

   auto with_cfa = make_trx({{"alice"_n, "doit"_n}});
   with_cfa.context_free_actions.emplace_back(vector<permission_level>{}, "alice"_n, "doit"_n, bytes{});
   BOOST_TEST(!is_priority_transaction(prio, with_cfa));
}

BOOST_AUTO_TEST_CASE(unpack) {
   const std::vector<priority_action> list{alice_doit, {"bob"_n, "doit"_n}, alice_doit};
   const auto packed = fc::raw::pack(list);
   BOOST_CHECK(unpack_priority_actions(packed.data(), packed.size()) == (priority_actions{alice_doit, {"bob"_n, "doit"_n}}));

   const auto empty = fc::raw::pack(std::vector<priority_action>{});
   BOOST_TEST(unpack_priority_actions(empty.data(), empty.size()).empty());

   std::vector<priority_action> too_long(config::max_priority_actions + 1, alice_doit);
   const auto long_packed = fc::raw::pack(too_long);
   BOOST_CHECK_THROW(unpack_priority_actions(long_packed.data(), long_packed.size()), chain_exception);

   auto trailing = packed;
   trailing.push_back(0);
   BOOST_CHECK_THROW(unpack_priority_actions(trailing.data(), trailing.size()), chain_exception);

   BOOST_CHECK_THROW(unpack_priority_actions(packed.data(), packed.size() - 1), fc::exception);
}

BOOST_AUTO_TEST_CASE(read_list) {
   list_tester t;
   BOOST_TEST(!t.control->get_priority_actions());

   t.set_list({alice_doit});
   auto prio = t.control->get_priority_actions(); // the current state, also mid-block
   BOOST_REQUIRE(prio);
   BOOST_CHECK(*prio == priority_actions{alice_doit});
   t.produce_block();
   BOOST_TEST(t.control->get_priority_actions() == prio); // unchanged row: the same list object

   t.set_list(std::vector<priority_action>(config::max_priority_actions + 1, alice_doit)); // too long: no list
   BOOST_TEST(!t.control->get_priority_actions());
   t.set_row({'x'}); // does not parse: no list
   BOOST_TEST(!t.control->get_priority_actions());
   t.set_list({}); // empty: no list
   BOOST_TEST(!t.control->get_priority_actions());

   t.set_list({alice_doit, {"bob"_n, "doit"_n}});
   BOOST_REQUIRE(t.control->get_priority_actions());
   BOOST_TEST(t.control->get_priority_actions()->size() == 2u);
   t.del_row();
   BOOST_TEST(!t.control->get_priority_actions());
   t.produce_block();
}

BOOST_AUTO_TEST_SUITE_END()
