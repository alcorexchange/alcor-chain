#include <eosio/chain/genesis_state.hpp>

// these are required to serialize a genesis_state

namespace eosio { namespace chain {

genesis_state::genesis_state() {
   initial_timestamp = fc::time_point::from_iso_string( "2025-01-01T00:00:00" ); // alcor-chain: not before the block timestamp epoch
   initial_key = fc::variant(eosio_root_key).as<public_key_type>();
}

chain::chain_id_type genesis_state::compute_chain_id() const {
   digest_type::encoder enc;
   fc::raw::pack( enc, *this );
   return chain_id_type{enc.result()};
}

} } // namespace eosio::chain
