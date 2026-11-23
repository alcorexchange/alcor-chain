#pragma once

#include <eosio/chain/types.hpp>
#include <eosio/chain/transaction.hpp>
#include <eosio/chain/transaction_metadata.hpp>

#include <memory>
#include <tuple>

namespace eosio::chain {

/**
 * Priority actions: a block producer policy, not a consensus rule.
 *
 * The system contract keeps a list of (contract, action) pairs in one row: account `eosio`, table `prioacts`,
 * scope `eosio`, primary key `prioacts`, value = the serialized vector of pairs. A transaction is a priority
 * transaction when it has at least one action, no context-free actions and every action is in the list.
 * While the list is not empty, the producer applies the transactions of each block in one batch at the end of its
 * slot: priority transactions first, then the rest in the order they came (producer_plugin). Validators do not check
 * it; anyone can check that a block has its priority transactions first.
 */
struct priority_action {
   account_name contract;
   action_name  action;

   friend bool operator<(const priority_action& a, const priority_action& b) {
      return std::tie(a.contract, a.action) < std::tie(b.contract, b.action);
   }
   friend bool operator==(const priority_action& a, const priority_action& b) {
      return a.contract == b.contract && a.action == b.action;
   }
};

using priority_actions     = flat_set<priority_action>;
using priority_actions_ptr = std::shared_ptr<const priority_actions>;

inline constexpr name priority_actions_table = "prioacts"_n;

/// Every action of `trx` is a priority action. False for a trx without actions or with context-free actions.
bool is_priority_transaction(const priority_actions& prio, const transaction& trx);

/// An input transaction (not implicit, read-only or dry-run) that is a priority transaction under `prio`.
/// False when `prio` is null.
bool is_priority_transaction(const priority_actions_ptr& prio, const transaction_metadata& trx);

/// The list from the row's value. Throws if the value is not a vector of at most `config::max_priority_actions`
/// pairs with nothing after it.
priority_actions unpack_priority_actions(const char* data, size_t size);

} // namespace eosio::chain

FC_REFLECT(eosio::chain::priority_action, (contract)(action))
