#pragma once
// String forms of the alcor-chain EM and ED keys, shared by elliptic_em.cpp and elliptic_ed.cpp.
// EM data is hex (optional lowercase "0x", hex digits in any case), ED data is base58 without a checksum.

#include <fc/crypto/base58.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/exception/exception.hpp>

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace fc::crypto::detail {

   /// `s` without one leading lowercase "0x"
   inline std::string_view trim_hex_prefix(std::string_view s) {
      return s.starts_with("0x") ? s.substr(2) : s;
   }

   /// Decodes hex of exactly `size` bytes, or throws.
   inline std::vector<char> hex_to_bytes(std::string_view hex, size_t size, const char* what) {
      FC_ASSERT(hex.size() == 2 * size, "${what}: expected ${n} hex characters, got ${got}",
                ("what", what)("n", 2 * size)("got", hex.size()));
      std::vector<char> bytes(size);
      FC_ASSERT(fc::from_hex(std::string(hex), bytes.data(), bytes.size()) == size, "${what}: invalid hex", ("what", what));
      return bytes;
   }

   /// Decodes base58 (no checksum) of exactly `size` bytes, or throws. Strings longer than the longest base58 form
   /// of `size` bytes are refused before decoding, which is quadratic in the length.
   inline std::vector<char> base58_to_bytes(const std::string& b58, size_t size, const char* what) {
      // the longest base58 form of n bytes is ceil(n * log(256) / log(58)) characters
      const size_t max_length = (size * 1366 + 999) / 1000;
      FC_ASSERT(!b58.empty() && b58.size() <= max_length, "${what}: base58 string of ${got} characters", ("what", what)("got", b58.size()));
      auto bytes = fc::from_base58(b58);
      FC_ASSERT(bytes.size() == size, "${what}: expected ${n} bytes, got ${got}", ("what", what)("n", size)("got", bytes.size()));
      return bytes;
   }

   template<typename Array>
   Array to_array(const std::vector<char>& bytes) {
      Array result;
      FC_ASSERT(bytes.size() == sizeof(result.data));
      std::memcpy(result.data, bytes.data(), bytes.size());
      return result;
   }

} // namespace fc::crypto::detail
