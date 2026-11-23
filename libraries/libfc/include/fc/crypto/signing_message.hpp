#pragma once
#include <fc/crypto/sha256.hpp>

#include <string>
#include <string_view>

namespace fc::crypto {

   /**
    * alcor-chain: the text that an EM key (MetaMask personal_sign) or an ED key (Phantom signMessage) signs for a
    * transaction signing digest: this prefix, then the 32-byte digest as 64 lowercase hex characters, UTF-8 (ASCII),
    * no trailing newline:
    *
    *    "Alcor Chain transaction\nSign only in an app you trust\n" + hex(digest)
    *
    * The wallet shows the text to the user, so a signature given to a site as a login or a "nonce" is never a
    * transaction signature, and the user sees what the signature is for.
    *
    * Consensus: EM and ED signatures in transactions are checked over exactly these bytes. Wallets build the same
    * text (WharfKit fork: transactionSigningText).
    */
   inline constexpr std::string_view transaction_signing_prefix = "Alcor Chain transaction\nSign only in an app you trust\n";

   /// The text to sign for a transaction signing digest.
   std::string transaction_signing_text(const sha256& digest);

} // namespace fc::crypto
