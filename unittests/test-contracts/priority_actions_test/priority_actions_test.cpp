#include "priority_actions_test.hpp"

using namespace eosio::internal_use_do_not_use;

static constexpr uint64_t prioacts = "prioacts"_n.value;

void priority_actions_test::setrow( const std::vector<char>& value ) {
   const uint64_t self = get_self().value;
   const int      itr  = db_find_i64( self, self, prioacts, prioacts );
   if( itr >= 0 )
      db_update_i64( itr, self, value.data(), value.size() );
   else
      db_store_i64( self, prioacts, self, prioacts, value.data(), value.size() );
}

void priority_actions_test::delrow() {
   const uint64_t self = get_self().value;
   const int      itr  = db_find_i64( self, self, prioacts, prioacts );
   if( itr >= 0 )
      db_remove_i64( itr );
}
