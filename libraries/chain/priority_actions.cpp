#include <eosio/chain/priority_actions.hpp>
#include <eosio/chain/config.hpp>
#include <eosio/chain/exceptions.hpp>

namespace eosio::chain {

bool is_priority_transaction(const priority_actions& prio, const transaction& trx) {
   if (prio.empty() || trx.actions.empty() || !trx.context_free_actions.empty())
      return false;
   for (const auto& a : trx.actions) {
      if (prio.find(priority_action{a.account, a.name}) == prio.end())
         return false;
   }
   return true;
}

bool is_priority_transaction(const priority_actions_ptr& prio, const transaction_metadata& trx) {
   return prio && trx.get_trx_type() == transaction_metadata::trx_type::input &&
          is_priority_transaction(*prio, trx.packed_trx()->get_transaction());
}

priority_actions unpack_priority_actions(const char* data, size_t size) {
   fc::datastream<const char*> ds(data, size);
   fc::unsigned_int count;
   fc::raw::unpack(ds, count);
   EOS_ASSERT(count.value <= config::max_priority_actions, chain_exception,
              "${n} priority actions, at most ${max} allowed", ("n", count.value)("max", config::max_priority_actions));
   priority_actions result;
   result.reserve(count.value);
   for (uint32_t i = 0; i < count.value; ++i) {
      priority_action a;
      fc::raw::unpack(ds, a);
      result.insert(a);
   }
   EOS_ASSERT(ds.remaining() == 0, chain_exception, "${n} extra bytes after the priority actions", ("n", ds.remaining()));
   return result;
}

} // namespace eosio::chain
