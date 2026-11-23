#pragma once
#include <eosio/chain/types.hpp>

#include <fc/static_variant.hpp>

namespace eosio::chain {

   /// Variant indexes of the alcor-chain key types, the same in public_key_type and signature_type.
   inline constexpr size_t em_key_type = fc::get_index<public_key_type::storage_type, fc::em::public_key_shim>();
   inline constexpr size_t ed_key_type = fc::get_index<public_key_type::storage_type, fc::ed::public_key_shim>();
   static_assert(em_key_type == 3 && ed_key_type == 4);
   static_assert(em_key_type == fc::get_index<signature_type::storage_type, fc::em::signature_shim>());
   static_assert(ed_key_type == fc::get_index<signature_type::storage_type, fc::ed::signature_shim>());

   /// The EM and ED keys of browser wallets, usable from genesis without a protocol feature.
   inline constexpr bool is_wallet_key_type(size_t which) {
      return which == em_key_type || which == ed_key_type;
   }

   /**
    * Whether a key type may be used in an account authority. K1 and R1 come from genesis and WA with the
    * WEBAUTHN_KEY protocol feature, both counted in protocol_state_object::num_supported_key_types; EM and ED
    * are supported from genesis.
    */
   inline constexpr bool is_supported_key_type(size_t which, uint32_t num_supported_key_types) {
      return which < num_supported_key_types || is_wallet_key_type(which);
   }

   /// EM and ED keys sign transactions only, never blocks: they cannot be producer (block signing) keys.
   inline constexpr bool is_block_signing_key_type(size_t which) {
      return !is_wallet_key_type(which);
   }

} // namespace eosio::chain
