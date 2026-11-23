#include <fc/crypto/signing_message.hpp>
#include <fc/crypto/hex.hpp>

namespace fc::crypto {

   std::string transaction_signing_text(const sha256& digest) {
      return std::string(transaction_signing_prefix) + fc::to_hex(digest.data(), digest.data_size());
   }

} // namespace fc::crypto
