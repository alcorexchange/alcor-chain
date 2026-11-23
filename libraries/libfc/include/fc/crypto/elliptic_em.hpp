#pragma once
#include <fc/array.hpp>
#include <fc/crypto/common.hpp>
#include <fc/crypto/sha256.hpp>
#include <fc/crypto/sha3.hpp>
#include <fc/reflect/reflect.hpp>

#include <string>

/**
 * alcor-chain EM keys: the secp256k1 keys of Ethereum wallets (MetaMask).
 *
 * The wallet signs a transaction with personal_sign (EIP-191) over the transaction signing text of the digest
 * (fc/crypto/signing_message.hpp), so the hash under the ECDSA signature is
 *
 *    keccak256("\x19Ethereum Signed Message:\n" + decimal byte length of text + text)
 *
 * and the key is recovered from the signature, like K1.
 *
 *  public key   33 bytes, compressed point 02/03 ‖ x
 *               string PUB_EM_ + hex of the uncompressed point 04 ‖ x ‖ y (lowercase, no 0x)
 *               parses: optional 0x, hex in any case, compressed (02/03 ‖ x), uncompressed (04 ‖ x ‖ y) or
 *               uncompressed without the 04; an uncompressed point must be on the curve
 *  signature    65 bytes r ‖ s ‖ v, Ethereum's layout, v = 27 + recovery id (27 or 28); string SIG_EM_0x + hex
 *  private key  32-byte secret; string PVT_EM_0x + hex
 *
 * One encoding per signature: recovery accepts only v = 27 or 28, 0 < r < n and 0 < s <= n/2 (low s, as Ethereum
 * requires for transactions since EIP-2) and refuses everything else, v = 0/1 included (wallet plugins turn a
 * wallet's 0/1 into 27/28). Signing always produces this form.
 */
namespace fc::em {
   using public_key_data  = fc::array<char, 33>;
   using signature_data   = fc::array<unsigned char, 65>;
   using private_key_data = fc::sha256;

   /// The hash an EM key signs for a transaction signing digest: keccak256 of the EIP-191 personal message that
   /// wraps the transaction signing text.
   fc::sha3 message_hash(const sha256& digest);

   /// The EIP-191 personal message (prefix ‖ length ‖ text) that `message_hash` hashes.
   std::string personal_message(const std::string& text);

   /// Whether `key` is the compressed form of a point on secp256k1.
   bool is_on_curve(const public_key_data& key);

   struct public_key_shim : public crypto::shim<public_key_data> {
      using crypto::shim<public_key_data>::shim;

      /// on the curve
      bool valid() const;

      /// hex of the uncompressed point, or of the stored bytes if they are not a point on the curve
      std::string to_data_string() const;
      static public_key_shim from_data_string(const std::string& data);
   };

   struct signature_shim : public crypto::shim<signature_data> {
      using public_key_type = public_key_shim;
      using crypto::shim<signature_data>::shim;

      /// Recovers the key that signed the transaction signing text of `digest`; throws for any encoding but
      /// v 27/28, 0 < r < n, 0 < s <= n/2. `check_canonical` (the K1 interface's switch for its own stricter
      /// canonical form) does not relax that.
      public_key_type recover(const sha256& digest, bool check_canonical) const;

      std::string to_data_string() const;
      static signature_shim from_data_string(const std::string& data);
   };

   struct private_key_shim : public crypto::shim<private_key_data> {
      using crypto::shim<private_key_data>::shim;
      using signature_type  = signature_shim;
      using public_key_type = public_key_shim;

      /// Signs the transaction signing text of `digest` as MetaMask personal_sign does (RFC 6979 nonce, low-s,
      /// v = 27 + recovery id). `require_canonical` is unused: the signature is always low-s.
      signature_type sign(const sha256& digest, bool require_canonical = true) const;

      public_key_type get_public_key() const;

      static private_key_shim generate();

      std::string to_data_string() const;
      static private_key_shim from_data_string(const std::string& data);
   };
} // namespace fc::em

FC_REFLECT_DERIVED( fc::em::public_key_shim, (fc::crypto::shim<fc::em::public_key_data>), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( fc::em::signature_shim, (fc::crypto::shim<fc::em::signature_data>), BOOST_PP_SEQ_NIL )
FC_REFLECT_DERIVED( fc::em::private_key_shim, (fc::crypto::shim<fc::em::private_key_data>), BOOST_PP_SEQ_NIL )
