#include <fc/crypto/elliptic_ed.hpp>
#include <fc/crypto/rand.hpp>
#include <fc/crypto/signing_message.hpp>
#include <fc/exception/exception.hpp>

#include <sodium.h>

#include "_key_string_forms.hpp"

namespace fc::ed {

   static_assert(crypto_sign_ed25519_PUBLICKEYBYTES == public_key_size);
   static_assert(crypto_sign_ed25519_BYTES == detached_signature_size);
   static_assert(crypto_sign_ed25519_SECRETKEYBYTES == sizeof(private_key_data::data));
   static_assert(crypto_sign_ed25519_SEEDBYTES == sizeof(fc::sha256));
   static_assert(sizeof(signature_data::data) == public_key_size + detached_signature_size);

   namespace {
      // sodium_init is thread safe and idempotent. Ed25519 itself needs no runtime setup; this is for the rest of
      // the library, called once before the first use.
      void ensure_sodium() {
         static const int result = sodium_init();
         FC_ASSERT(result >= 0, "libsodium failed to initialize");
      }

      // the secret key of the seed half of `secret`: the public half is always derived, never trusted
      private_key_data derive(const char* seed) {
         ensure_sodium();
         unsigned char pub[crypto_sign_ed25519_PUBLICKEYBYTES];
         private_key_data secret;
         FC_ASSERT(crypto_sign_ed25519_seed_keypair(pub, (unsigned char*)secret.data, (const unsigned char*)seed) == 0);
         return secret;
      }
   } // namespace

   bool verify_detached(const char* sig, const char* msg, size_t msg_size, const char* pub) {
      ensure_sodium();
      return crypto_sign_ed25519_verify_detached((const unsigned char*)sig, (const unsigned char*)msg, msg_size,
                                                 (const unsigned char*)pub) == 0;
   }

   detached_signature sign_detached(const private_key_data& secret, const char* msg, size_t msg_size) {
      const auto derived = derive(secret.data);
      detached_signature sig;
      FC_ASSERT(crypto_sign_ed25519_detached((unsigned char*)sig.data, nullptr, (const unsigned char*)msg, msg_size,
                                             (const unsigned char*)derived.data) == 0);
      return sig;
   }

   private_key_data secret_key_from_seed(const fc::sha256& seed) {
      return derive(seed.data());
   }

   // ---- public key

   bool public_key_shim::valid() const {
      ensure_sodium();
      return crypto_core_ed25519_is_valid_point((const unsigned char*)_data.data) == 1;
   }

   std::string public_key_shim::to_data_string() const {
      return fc::to_base58(_data.data, _data.size(), {});
   }

   public_key_shim public_key_shim::from_data_string(const std::string& data) {
      return public_key_shim(crypto::detail::to_array<public_key_data>(
         crypto::detail::base58_to_bytes(data, public_key_size, "ED public key")));
   }

   // ---- signature

   public_key_shim signature_shim::recover(const sha256& digest, bool /*check_canonical*/) const {
      const char* pub = _data.data;
      const char* sig = _data.data + public_key_size;
      const auto text = crypto::transaction_signing_text(digest);
      FC_ASSERT(verify_detached(sig, text.data(), text.size(), pub), "ED signature does not verify against its public key");
      public_key_data key;
      std::memcpy(key.data, pub, public_key_size);
      return public_key_shim(key);
   }

   std::string signature_shim::to_data_string() const {
      return fc::to_base58(_data.data, _data.size(), {});
   }

   signature_shim signature_shim::from_data_string(const std::string& data) {
      return signature_shim(crypto::detail::to_array<signature_data>(
         crypto::detail::base58_to_bytes(data, sizeof(signature_data::data), "ED signature")));
   }

   // ---- private key

   signature_shim private_key_shim::sign(const sha256& digest, bool /*require_canonical*/) const {
      const auto text = crypto::transaction_signing_text(digest);
      const auto pub = get_public_key();
      const auto sig = sign_detached(_data, text.data(), text.size());
      signature_data result;
      std::memcpy(result.data, pub._data.data, public_key_size);
      std::memcpy(result.data + public_key_size, sig.data, detached_signature_size);
      return signature_shim(result);
   }

   public_key_shim private_key_shim::get_public_key() const {
      const auto secret = derive(_data.data);
      public_key_data key;
      std::memcpy(key.data, secret.data + public_key_size, public_key_size);
      return public_key_shim(key);
   }

   private_key_shim private_key_shim::generate() {
      char seed[crypto_sign_ed25519_SEEDBYTES];
      rand_bytes(seed, sizeof(seed));
      return private_key_shim(derive(seed));
   }

   std::string private_key_shim::to_data_string() const {
      return fc::to_base58(_data.data, _data.size(), {});
   }

   private_key_shim private_key_shim::from_data_string(const std::string& data) {
      const auto secret = crypto::detail::to_array<private_key_data>(
         crypto::detail::base58_to_bytes(data, sizeof(private_key_data::data), "ED private key"));
      FC_ASSERT(derive(secret.data) == secret, "ED private key: the public half does not belong to the seed");
      return private_key_shim(secret);
   }

} // namespace fc::ed
