#pragma once
#include <fc/array.hpp>
#include <fc/crypto/common.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/reflect/reflect.hpp>

#include <string>

/**
 * alcor-chain ED keys: the Ed25519 keys (RFC 8032) of Solana wallets (Phantom). Verification is libsodium's
 * crypto_sign_ed25519_verify_detached, vendored and pinned (libraries/libfc/libsodium): canonical s < L, canonical
 * public key, R and the public key not of small order, cofactorless equation.
 *
 * The wallet signs the transaction signing text of the digest (fc/crypto/signing_message.hpp) with signMessage.
 * Ed25519 has no key recovery, so the signature carries its public key.
 *
 *  public key   32 bytes (a Solana address); string PUB_ED_ + base58 without checksum
 *  signature    96 bytes, public key (32) ‖ Ed25519 signature (64); string SIG_ED_ + base58
 *  private key  64 bytes, libsodium's secret key seed (32) ‖ public key (32), as Phantom exports it;
 *               string PVT_ED_ + base58. Parsing checks that the public half belongs to the seed.
 */
namespace fc::ed {
   using public_key_data  = fc::array<char, 32>;
   using signature_data   = fc::array<char, 96>;
   using private_key_data = fc::array<char, 64>;

   constexpr size_t public_key_size         = 32;
   constexpr size_t detached_signature_size = 64;

   using detached_signature = fc::array<char, detached_signature_size>;

   /// RFC 8032 Ed25519 verification of a detached 64-byte signature of `msg` by the 32-byte `pub`.
   bool verify_detached(const char* sig, const char* msg, size_t msg_size, const char* pub);

   /// The detached RFC 8032 Ed25519 signature of `msg` by the seed half of `secret` (the public half is derived).
   detached_signature sign_detached(const private_key_data& secret, const char* msg, size_t msg_size);

   /// The 64-byte secret key (seed ‖ public key) of a 32-byte seed.
   private_key_data secret_key_from_seed(const fc::sha256& seed);

   struct public_key_shim : public crypto::shim<public_key_data> {
      using crypto::shim<public_key_data>::shim;

      /// a canonical point of the main subgroup, not of small order
      bool valid() const;

      std::string to_data_string() const;
      static public_key_shim from_data_string(const std::string& data);
   };

   struct signature_shim : public crypto::shim<signature_data> {
      using public_key_type = public_key_shim;
      using crypto::shim<signature_data>::shim;

      /// The embedded public key, if its signature of the transaction signing text of `digest` verifies;
      /// throws otherwise. `check_canonical` is unused: libsodium always requires the canonical form.
      public_key_type recover(const sha256& digest, bool check_canonical) const;

      std::string to_data_string() const;
      static signature_shim from_data_string(const std::string& data);
   };

   struct private_key_shim : public crypto::shim<private_key_data> {
      using crypto::shim<private_key_data>::shim;
      using signature_type  = signature_shim;
      using public_key_type = public_key_shim;

      /// Signs the transaction signing text of `digest` and returns public key ‖ signature.
      signature_type sign(const sha256& digest, bool require_canonical = true) const;

      public_key_type get_public_key() const;

      static private_key_shim generate();

      std::string to_data_string() const;
      static private_key_shim from_data_string(const std::string& data);
   };
} // namespace fc::ed

FC_REFLECT_DERIVED( fc::ed::public_key_shim, (fc::crypto::shim<fc::ed::public_key_data>), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( fc::ed::signature_shim, (fc::crypto::shim<fc::ed::signature_data>), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( fc::ed::private_key_shim, (fc::crypto::shim<fc::ed::private_key_data>), BOOST_PP_SEQ_NIL )
