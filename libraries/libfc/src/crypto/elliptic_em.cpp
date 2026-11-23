#include <fc/crypto/elliptic_em.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/crypto/signing_message.hpp>
#include <fc/exception/exception.hpp>

#include <algorithm>

#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include "_elliptic_impl_priv.hpp"
#include "_key_string_forms.hpp"

namespace fc::em {

   namespace {
      // EIP-191 version 0x45 ("E"), personal_sign. Split so that "\x19" does not swallow the "E" as a hex digit.
      constexpr const char personal_message_prefix[] = "\x19" "Ethereum Signed Message:\n";

      constexpr size_t uncompressed_size = 65;

      const secp256k1_context* context() {
         return fc::ecc::detail::_get_context();
      }

      bool parse_point(const unsigned char* data, size_t size, secp256k1_pubkey& point) {
         return secp256k1_ec_pubkey_parse(context(), &point, data, size) == 1;
      }

      public_key_data compressed(const secp256k1_pubkey& point) {
         public_key_data result;
         size_t size = result.size();
         FC_ASSERT(secp256k1_ec_pubkey_serialize(context(), (unsigned char*)result.data, &size, &point, SECP256K1_EC_COMPRESSED));
         FC_ASSERT(size == result.size());
         return result;
      }

      // the recovery id of v; only Ethereum's 27/28, so each signature has one encoding (wallet plugins turn a
      // wallet's 0/1 into 27/28)
      int recovery_id(unsigned char v) {
         FC_ASSERT(v == 27 || v == 28, "EM signature: invalid v ${v}, expected 27 or 28", ("v", (int)v));
         return v - 27;
      }

      bool is_zero(const unsigned char* scalar) {
         return std::all_of(scalar, scalar + 32, [](unsigned char b) { return b == 0; });
      }
   } // namespace

   std::string personal_message(const std::string& text) {
      return personal_message_prefix + std::to_string(text.size()) + text;
   }

   fc::sha3 message_hash(const sha256& digest) {
      const auto message = personal_message(crypto::transaction_signing_text(digest));
      return fc::sha3::hash(message.data(), message.size(), false); // false: keccak-256, not NIST SHA3-256
   }

   bool is_on_curve(const public_key_data& key) {
      secp256k1_pubkey point;
      return parse_point((const unsigned char*)key.data, key.size(), point);
   }

   // ---- public key

   bool public_key_shim::valid() const {
      return is_on_curve(_data);
   }

   std::string public_key_shim::to_data_string() const {
      secp256k1_pubkey point;
      if (!parse_point((const unsigned char*)_data.data, _data.size(), point)) {
         // Not a point on the curve. Authorities refuse such keys, but action data can hold any bytes, and printing
         // must not fail a block or a table row. The compressed hex parses back to the same bytes (abieos does the same).
         return fc::to_hex(_data.data, _data.size());
      }
      unsigned char uncompressed[uncompressed_size];
      size_t size = sizeof(uncompressed);
      FC_ASSERT(secp256k1_ec_pubkey_serialize(context(), uncompressed, &size, &point, SECP256K1_EC_UNCOMPRESSED));
      FC_ASSERT(size == sizeof(uncompressed));
      return fc::to_hex((const char*)uncompressed, sizeof(uncompressed));
   }

   public_key_shim public_key_shim::from_data_string(const std::string& data) {
      const auto hex = crypto::detail::trim_hex_prefix(data);
      if (hex.size() == 2 * public_key_data().size()) {
         // compressed 02/03 ‖ x, kept as given (authorities check the curve)
         auto bytes = crypto::detail::hex_to_bytes(hex, public_key_data().size(), "EM public key");
         FC_ASSERT(bytes[0] == 0x02 || bytes[0] == 0x03, "EM public key: compressed form must start with 02 or 03");
         return public_key_shim(crypto::detail::to_array<public_key_data>(bytes));
      }
      // uncompressed 04 ‖ x ‖ y, or x ‖ y without the 04
      FC_ASSERT(hex.size() == 2 * uncompressed_size || hex.size() == 2 * (uncompressed_size - 1),
                "EM public key: expected 66, 128 or 130 hex characters, got ${n}", ("n", hex.size()));
      auto bytes = crypto::detail::hex_to_bytes(hex, hex.size() / 2, "EM public key");
      if (bytes.size() == uncompressed_size - 1)
         bytes.insert(bytes.begin(), 0x04);
      // checked here: secp256k1_ec_pubkey_parse also takes the hybrid forms 06/07
      FC_ASSERT(bytes[0] == 0x04, "EM public key: uncompressed form must start with 04");
      secp256k1_pubkey point;
      FC_ASSERT(parse_point((const unsigned char*)bytes.data(), bytes.size(), point), "EM public key: point is not on the curve");
      return public_key_shim(compressed(point));
   }

   // ---- signature

   public_key_shim signature_shim::recover(const sha256& digest, bool /*check_canonical*/) const {
      // one encoding per signature: v 27/28, 0 < r < n, 0 < s <= n/2
      const unsigned char* rs = _data.data;
      const int recid = recovery_id(_data.data[64]);
      FC_ASSERT(!is_zero(rs) && !is_zero(rs + 32), "EM signature: r or s is zero");

      secp256k1_ecdsa_recoverable_signature signature;
      // fails when r or s >= n
      FC_ASSERT(secp256k1_ecdsa_recoverable_signature_parse_compact(context(), &signature, rs, recid),
                "EM signature: r or s out of range");

      secp256k1_ecdsa_signature plain;
      secp256k1_ecdsa_recoverable_signature_convert(context(), &plain, &signature);
      // returns 1 when the input was high-s
      FC_ASSERT(secp256k1_ecdsa_signature_normalize(context(), nullptr, &plain) == 0, "EM signature: high s, not canonical");

      const auto hash = message_hash(digest);
      secp256k1_pubkey point;
      FC_ASSERT(secp256k1_ecdsa_recover(context(), &point, &signature, (const unsigned char*)hash.data()),
                "EM signature: unable to recover the public key");
      return public_key_shim(compressed(point));
   }

   std::string signature_shim::to_data_string() const {
      return "0x" + fc::to_hex((const char*)_data.data, _data.size());
   }

   signature_shim signature_shim::from_data_string(const std::string& data) {
      const auto bytes = crypto::detail::hex_to_bytes(crypto::detail::trim_hex_prefix(data), signature_data().size(), "EM signature");
      signature_data sig;
      std::memcpy(sig.data, bytes.data(), bytes.size());
      return signature_shim(sig);
   }

   // ---- private key

   signature_shim private_key_shim::sign(const sha256& digest, bool /*require_canonical*/) const {
      const auto hash = message_hash(digest);
      secp256k1_ecdsa_recoverable_signature signature;
      // nonce: RFC 6979; libsecp256k1 always produces low-s
      FC_ASSERT(secp256k1_ecdsa_sign_recoverable(context(), &signature, (const unsigned char*)hash.data(),
                                                 (const unsigned char*)_data.data(), nullptr, nullptr),
                "EM private key: invalid secret");
      signature_data result;
      int recid = 0;
      secp256k1_ecdsa_recoverable_signature_serialize_compact(context(), result.data, &recid, &signature);
      result.data[64] = 27 + recid;
      return signature_shim(result);
   }

   public_key_shim private_key_shim::get_public_key() const {
      secp256k1_pubkey point;
      FC_ASSERT(secp256k1_ec_pubkey_create(context(), &point, (const unsigned char*)_data.data()), "EM private key: invalid secret");
      return public_key_shim(compressed(point));
   }

   private_key_shim private_key_shim::generate() {
      private_key_data secret;
      do {
         rand_bytes(secret.data(), secret.data_size());
      } while (!secp256k1_ec_seckey_verify(context(), (const unsigned char*)secret.data()));
      return private_key_shim(secret);
   }

   std::string private_key_shim::to_data_string() const {
      return "0x" + fc::to_hex(_data.data(), _data.data_size());
   }

   private_key_shim private_key_shim::from_data_string(const std::string& data) {
      const auto bytes = crypto::detail::hex_to_bytes(crypto::detail::trim_hex_prefix(data), private_key_data().data_size(), "EM private key");
      private_key_data secret;
      std::memcpy(secret.data(), bytes.data(), bytes.size());
      FC_ASSERT(secp256k1_ec_seckey_verify(context(), (const unsigned char*)secret.data()), "EM private key: not a valid secp256k1 secret");
      return private_key_shim(secret);
   }

} // namespace fc::em
