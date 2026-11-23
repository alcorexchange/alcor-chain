// alcor-chain EM (secp256k1, MetaMask) and ED (Ed25519, Solana) keys: string and binary forms, the transaction
// signing text, EM recovery over the EIP-191 personal message, ED verification.
//
// Vectors: RFC 8032 section 7.1 (Ed25519 test vectors 1-3); the web3.js documentation example of
// accounts.sign('Some data', key) for the EIP-191 hash; secp256k1 points 2G, G and 6G; and keys, digest and
// signatures made by gen_em_ed_vectors.py (plain Python secp256k1 with an RFC 6979 nonce, pycryptodome Ed25519).

#include <boost/test/unit_test.hpp>

#include <fc/crypto/elliptic_ed.hpp>
#include <fc/crypto/elliptic_em.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/private_key.hpp>
#include <fc/crypto/public_key.hpp>
#include <fc/crypto/signature.hpp>
#include <fc/crypto/signing_message.hpp>
#include <fc/io/raw.hpp>

using namespace fc::crypto;
using namespace fc;

namespace {
   std::vector<char> unhex(const std::string& hex) {
      std::vector<char> bytes(hex.size() / 2);
      BOOST_REQUIRE_EQUAL(fc::from_hex(hex, bytes.data(), bytes.size()), bytes.size());
      return bytes;
   }

   std::string packed_hex(const auto& value) {
      return fc::to_hex(fc::raw::pack(value));
   }

   // a variant from its index and bytes, as the chain receives it
   template<typename T>
   T unpack_variant(uint8_t index, const std::string& hex) {
      auto bytes = unhex(hex);
      bytes.insert(bytes.begin(), (char)index);
      return fc::raw::unpack<T>(bytes);
   }

   fc::sha256 test_digest() {
      return fc::sha256::hash(std::string("alcor-chain EM/ED test digest"));
   }

   // gen_em_ed_vectors.py
   const std::string digest_hex   = "f60924b013448a6eb5e29786ef89dbe2e7f45170410b36d218422516081aed1b";
   const std::string em_private   = "PVT_EM_0x2c364e9dbe0deaecc71b5b6301595828dcf8b94a70d5ec5970b9340ee7168cc6";
   const std::string em_public    = "PUB_EM_04fcd5d189de6a33c6ce28c9ac8a5b1a84474ac201414457a61dfcf37875ab3148ab1b6c3adb2071df1e83d27c84ab9c2e2b7c13c1592364b509c63354b7709e84";
   const std::string em_compressed = "02fcd5d189de6a33c6ce28c9ac8a5b1a84474ac201414457a61dfcf37875ab3148";
   const std::string em_msg_hash  = "74fafd8f087b0e20e7d4c5f92b5e789d0a5c494358e12b877c2ad45fac451923";
   const std::string em_sig_hex   = "a799f1239cfb7b0c4f9968e74e98beb73e44731ff580b4113321c57c7903a93132b812eef85c96b7336ea25b043c4d512daa6bf2651c26ceb0a1d4ece5bd532f1b";
   const std::string em_sig_high_s = "a799f1239cfb7b0c4f9968e74e98beb73e44731ff580b4113321c57c7903a931cd47ed1107a36948cc915da4fbc3b2ad8d0470f44a2c796d0f30899fea78ee121c";
   const std::string ed_private   = "PVT_ED_yqx4e7niU5bZmroYr5UagPH37Aryb7n4xzfxstUpDkqFUQEBrQNAY4FHcZeD5xyAoyRwsAedWmNBdeB1JtFyoyR";
   const std::string ed_public    = "PUB_ED_GGvqKkLxvpJJCubyhQQh161Z4ennhhc3sN2hnTBAR1sV";
   const std::string ed_public_hex = "e2f3e2a82a3e233b35e1d917b9e93d73dc6f2cb80219b9ab9db0d9518ba76e00";
   const std::string ed_signature = "SIG_ED_2MBQpK1g9nyNJtnTqTbtm1FQmKLDJDJzDqPMjFa3KoWthTWzd7MVqygrSUaLcD3t68KFK8szaHCaSXZ5BVV132RTmRvkFYZj7P7juKpJn3a3inzM5yTMfAKJo8dXWJAesvPR";
   const std::string ed_sig_hex   = "8dbaf271902ef6a7cb32cb52a7ee00f07b34a90262a464016b08ae3e7001e66abefefec56f37f6d09faae2fed821cfe08445282aa8bb017845660d0b70b09c08";

   // RFC 8032 section 7.1, test vectors 1-3: secret seed, public key, message, signature
   struct rfc8032_vector { std::string seed, pub, msg, sig; };
   const std::vector<rfc8032_vector> rfc8032 = {
      {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
       "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
       "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
      {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
       "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
       "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
      {"c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
       "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
       "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
   };

   bool rfc8032_verify(const std::string& sig, const std::string& msg, const std::string& pub) {
      const auto s = unhex(sig), m = unhex(msg), p = unhex(pub);
      return fc::ed::verify_detached(s.data(), m.data(), m.size(), p.data());
   }
}

BOOST_AUTO_TEST_SUITE(em_ed_keys)

BOOST_AUTO_TEST_CASE(signing_text) try {
   const auto digest = test_digest();
   BOOST_CHECK_EQUAL(digest.str(), digest_hex);
   BOOST_CHECK_EQUAL(transaction_signing_text(digest),
                     "Alcor Chain transaction\nSign only in an app you trust\n" + digest_hex);
   BOOST_CHECK_EQUAL(transaction_signing_text(digest).size(), 54u + 64u);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(em_personal_message) try {
   // web3.js docs, web3.eth.accounts.sign('Some data', ...): messageHash
   const auto message = fc::em::personal_message("Some data");
   BOOST_CHECK_EQUAL(message, std::string("\x19" "Ethereum Signed Message:\n9Some data"));
   BOOST_CHECK_EQUAL(fc::sha3::hash(message.data(), message.size(), false).str(),
                     "1da44b586eb0729ff70a73c326926f6ed5a25f5b056e7f47fbc6e58d86871655");
   BOOST_CHECK_EQUAL(fc::em::message_hash(test_digest()).str(), em_msg_hash);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(em_keys_and_signature) try {
   const private_key priv(em_private);
   BOOST_CHECK_EQUAL(priv.to_string({}), em_private);
   BOOST_CHECK_EQUAL(private_key("PVT_EM_" + em_private.substr(9)).to_string({}), em_private); // without 0x

   const auto pub = priv.get_public_key();
   BOOST_CHECK_EQUAL(pub.to_string({}), em_public);
   BOOST_CHECK_EQUAL(pub.which(), 3u);
   BOOST_CHECK(pub.valid());
   BOOST_CHECK_EQUAL(packed_hex(pub), "03" + em_compressed);

   // RFC 6979: the signature is deterministic and must be what the independent implementation made
   const auto sig = priv.sign(test_digest());
   BOOST_CHECK_EQUAL(sig.to_string(), "SIG_EM_0x" + em_sig_hex);
   BOOST_CHECK_EQUAL(sig.which(), 3u);
   BOOST_CHECK_EQUAL(packed_hex(sig), "03" + em_sig_hex);
   BOOST_CHECK(public_key(sig, test_digest()) == pub);

   // the signature made outside fc recovers to the key
   const auto parsed = signature("SIG_EM_0x" + em_sig_hex);
   BOOST_CHECK(parsed == sig);
   BOOST_CHECK(public_key(parsed, test_digest()) == pub);
   BOOST_CHECK(signature("SIG_EM_" + em_sig_hex) == sig);                        // without 0x
   BOOST_CHECK(public_key(unpack_variant<signature>(3, em_sig_hex), test_digest()) == pub);

   // a random key round-trips through sign and recover
   const auto random = private_key::generate<fc::em::private_key_shim>();
   BOOST_CHECK(public_key(random.sign(test_digest()), test_digest()) == random.get_public_key());
   BOOST_CHECK(private_key(random.to_string({})) == random);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(em_recovery_rules) try {
   const auto pub = private_key(em_private).get_public_key();
   const auto digest = test_digest();
   const auto with_v = [](uint8_t v) {
      return unpack_variant<signature>(3, em_sig_hex.substr(0, 128) + fc::to_hex((const char*)&v, 1));
   };

   // one encoding: v = 27 + recovery id
   BOOST_CHECK(public_key(with_v(27), digest) == pub);
   // the other recovery id recovers another key
   BOOST_CHECK(public_key(with_v(28), digest) != pub);
   // anything else is refused, the bare recovery id 0/1 included
   for (uint8_t v : {0, 1, 2, 3, 26, 29, 30, 31, 35, 255})
      BOOST_CHECK_THROW(public_key(with_v(v), digest), fc::exception);

   // high s (n - s, recovery id flipped) is the same ECDSA signature, refused so each signature has one form,
   // also when the caller asks for no canonical check
   BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, em_sig_high_s), digest), fc::exception);
   BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, em_sig_high_s), digest, false), fc::exception);

   // r = 0, s = 0, r = n, s = n are not signatures
   const std::string zero(64, '0');
   const std::string n = "fffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141";
   for (const auto& v : {"1b", "1c"}) {
      BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, zero + em_sig_hex.substr(64, 64) + v), digest), fc::exception);
      BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, em_sig_hex.substr(0, 64) + zero + v), digest), fc::exception);
      BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, n + em_sig_hex.substr(64, 64) + v), digest), fc::exception);
      BOOST_CHECK_THROW(public_key(unpack_variant<signature>(3, em_sig_hex.substr(0, 64) + n + v), digest), fc::exception);
   }

   // another digest recovers another key, never ours
   BOOST_CHECK(public_key(with_v(27), fc::sha256::hash(std::string("other"))) != pub);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(em_public_key_forms) try {
   // 2G
   const std::string two_g = "PUB_EM_04c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee51ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a";
   BOOST_CHECK_EQUAL(public_key(two_g).to_string({}), two_g);
   BOOST_CHECK_EQUAL(packed_hex(public_key(two_g)), "0302c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5");
   // compressed with or without 0x, uncompressed without 04, any hex case
   for (const std::string form : {
           "PUB_EM_0x02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",
           "PUB_EM_02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",
           "PUB_EM_c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee51ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a",
           "PUB_EM_0x04C6047F9441ED7D6D3045406E95C07CD85C778E4B8CEF3CA7ABAC09B95C709EE51AE168FEA63DC339A3C58419466CEAEEF7F632653266D0E1236431A950CFE52A"})
      BOOST_CHECK_EQUAL(public_key(form).to_string({}), two_g);
   // both y parities: G (even y), 6G (odd y)
   BOOST_CHECK_EQUAL(public_key("PUB_EM_0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798").to_string({}),
                     "PUB_EM_0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");
   BOOST_CHECK_EQUAL(public_key("PUB_EM_03fff97bd5755eeea420453a14355235d382f6472f8568a18b2f057a1460297556").to_string({}),
                     "PUB_EM_04fff97bd5755eeea420453a14355235d382f6472f8568a18b2f057a1460297556ae12777aacfbb620f3be96017f45c560de80f0f6518fe4a03c870c36b075f297");

   // a stored key off the curve (x = 5) is not valid, prints as its compressed bytes and parses back to them
   const auto off_curve = unpack_variant<public_key>(3, "020000000000000000000000000000000000000000000000000000000000000005");
   BOOST_CHECK(!off_curve.valid());
   BOOST_CHECK_EQUAL(off_curve.to_string({}), "PUB_EM_020000000000000000000000000000000000000000000000000000000000000005");
   BOOST_CHECK(public_key(off_curve.to_string({})) == off_curve);

   // refused forms
   for (const std::string bad : {
           "PUB_EM_04c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee51ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52b", // off the curve
           "PUB_EM_06c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee51ae168fea63dc339a3c58419466ceaeef7f632653266d0e1236431a950cfe52a", // hybrid
           "PUB_EM_0xc6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",   // bare x
           "PUB_EM_05c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",   // bad prefix
           "PUB_EM_02z6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5",   // not hex
           "PUB_EM_0X02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5", // only lowercase 0x
           "PUB_EM_"})
      BOOST_CHECK_THROW(public_key{bad}, fc::exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(em_signature_and_private_forms) try {
   BOOST_CHECK_THROW(signature("SIG_EM_0x" + em_sig_hex.substr(0, 128)), fc::exception);   // 64 bytes
   BOOST_CHECK_THROW(signature("SIG_EM_0x" + em_sig_hex + "00"), fc::exception);           // 66 bytes
   BOOST_CHECK_EQUAL(packed_hex(private_key("PVT_EM_0x0000000000000000000000000000000000000000000000000000000000000001")),
                     "020000000000000000000000000000000000000000000000000000000000000001");
   BOOST_CHECK_EQUAL(private_key("PVT_EM_0x0000000000000000000000000000000000000000000000000000000000000001").get_public_key().to_string({}),
                     "PUB_EM_0479be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798483ada7726a3c4655da4fbfc0e1108a8fd17b448a68554199c47d08ffb10d4b8");
   BOOST_CHECK_THROW(private_key("PVT_EM_0x00000000000000000000000000000000000000000000000000000000000001"), fc::exception); // 31 bytes
   BOOST_CHECK_THROW(private_key("PVT_EM_0x" + std::string(64, '0')), fc::exception);       // zero is not a secret
   BOOST_CHECK_THROW(private_key("PVT_EM_0xfffffffffffffffffffffffffffffffebaaedce6af48a03bbfd25e8cd0364141"), fc::exception); // n
   BOOST_CHECK_THROW(private_key(em_private).generate_shared_secret(private_key(em_private).get_public_key()), fc::exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(ed_rfc8032) try {
   for (const auto& v : rfc8032) {
      BOOST_CHECK(rfc8032_verify(v.sig, v.msg, v.pub));

      // the seed gives the RFC public key, and signing the message gives the RFC signature
      const auto secret = fc::ed::secret_key_from_seed(fc::sha256(v.seed));
      BOOST_CHECK_EQUAL(fc::to_hex(secret.data + 32, 32), v.pub);
      const auto msg = unhex(v.msg);
      const auto sig = fc::ed::sign_detached(secret, msg.data(), msg.size());
      BOOST_CHECK_EQUAL(fc::to_hex(sig.data, sig.size()), v.sig);

      // a flipped bit anywhere fails
      auto flip = [](std::string hex, size_t at) { hex[at] = hex[at] == '0' ? '1' : '0'; return hex; };
      BOOST_CHECK(!rfc8032_verify(flip(v.sig, 0), v.msg, v.pub));     // R
      BOOST_CHECK(!rfc8032_verify(flip(v.sig, 70), v.msg, v.pub));    // s
      BOOST_CHECK(!rfc8032_verify(v.sig, v.msg + "00", v.pub));       // message
      BOOST_CHECK(!rfc8032_verify(v.sig, v.msg, flip(v.pub, 2)));     // key
   }

   // s + L, the same signature with a non-canonical s, is refused (RFC 8032 5.1.7: s must be in [0, L))
   {
      const auto& v = rfc8032[0];
      auto sig = unhex(v.sig);
      const uint8_t l[32] = {0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2, 0xde, 0xf9, 0xde, 0x14,
                             0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10};
      unsigned carry = 0;
      for (size_t i = 0; i < 32; ++i) { // little endian
         unsigned sum = (uint8_t)sig[32 + i] + l[i] + carry;
         sig[32 + i] = (char)(sum & 0xff);
         carry = sum >> 8;
      }
      BOOST_REQUIRE_EQUAL(carry, 0u);
      const auto pub = unhex(v.pub);
      BOOST_CHECK(!fc::ed::verify_detached(sig.data(), nullptr, 0, pub.data()));
   }

   // a small-order public key (the identity point) is refused even with a "signature" that satisfies the equation
   {
      const auto identity = unhex("0100000000000000000000000000000000000000000000000000000000000000");
      const auto sig = unhex("0100000000000000000000000000000000000000000000000000000000000000"
                             "0000000000000000000000000000000000000000000000000000000000000000");
      BOOST_CHECK(!fc::ed::verify_detached(sig.data(), "x", 1, identity.data()));
   }
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(ed_keys_and_signature) try {
   const private_key priv(ed_private);
   BOOST_CHECK_EQUAL(priv.to_string({}), ed_private);

   const auto pub = priv.get_public_key();
   BOOST_CHECK_EQUAL(pub.to_string({}), ed_public);
   BOOST_CHECK_EQUAL(pub.which(), 4u);
   BOOST_CHECK(pub.valid());
   BOOST_CHECK_EQUAL(packed_hex(pub), "04" + ed_public_hex);

   // Ed25519 is deterministic: the signature is what pycryptodome made over the signing text
   const auto sig = priv.sign(test_digest());
   BOOST_CHECK_EQUAL(sig.to_string(), ed_signature);
   BOOST_CHECK_EQUAL(sig.which(), 4u);
   BOOST_CHECK_EQUAL(packed_hex(sig), "04" + ed_public_hex + ed_sig_hex);
   BOOST_CHECK(signature(ed_signature) == sig);
   BOOST_CHECK(public_key(sig, test_digest()) == pub);

   // recovery returns the embedded key only if it signed this digest's text
   BOOST_CHECK_THROW(public_key(sig, fc::sha256::hash(std::string("other"))), fc::exception);
   auto tampered = ed_sig_hex;
   tampered[10] = tampered[10] == '0' ? '1' : '0';
   BOOST_CHECK_THROW(public_key(unpack_variant<signature>(4, ed_public_hex + tampered), test_digest()), fc::exception);
   // another key embedded in front of the signature
   BOOST_CHECK_THROW(public_key(unpack_variant<signature>(4, rfc8032[0].pub + ed_sig_hex), test_digest()), fc::exception);
   // the key is the one derived from its seed, sha256("alcor-chain ED test key"), as the chain tester derives keys
   const auto secret = fc::ed::secret_key_from_seed(fc::sha256::hash(std::string("alcor-chain ED test key")));
   BOOST_CHECK(private_key::regenerate<fc::ed::private_key_shim>(secret) == priv);

   // a random key round-trips
   const auto random = private_key::generate<fc::ed::private_key_shim>();
   BOOST_CHECK(public_key(random.sign(test_digest()), test_digest()) == random.get_public_key());
   BOOST_CHECK(private_key(random.to_string({})) == random);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(ed_forms) try {
   // RFC 8032 test vector 1 in the forms of the abieos fork
   BOOST_CHECK_EQUAL(public_key("PUB_ED_FVen3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z").to_string({}), "PUB_ED_FVen3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z");
   BOOST_CHECK_EQUAL(packed_hex(public_key("PUB_ED_FVen3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z")), "04" + rfc8032[0].pub);
   BOOST_CHECK_EQUAL(packed_hex(signature("SIG_ED_2HBfyaziHCpyEmPqurRhohVzQLjAVUmDZRRiw5pSSgUMtBWhAWhdKtjjBmmCr3KmzTE6tenJRL4Xzt9J2bH5BWnfH2VqGtMURkHwKbZuPsFArSWFLWjwgFdWGvtECzWZya8r")),
                     "04" + rfc8032[0].pub + rfc8032[0].sig);
   BOOST_CHECK_EQUAL(packed_hex(private_key("PVT_ED_49W385L4rePHy6PAaQUovbD2aacgN4HsKXSMeUzRg4fmwXszN91JuMFrQRj3vMDpZuRF3ZknQBuRBoWQJEfXstMw")),
                     "03" + rfc8032[0].seed + rfc8032[0].pub);
   // 32 zero bytes: leading zeros are '1's
   BOOST_CHECK_EQUAL(public_key("PUB_ED_11111111111111111111111111111111").to_string({}), "PUB_ED_11111111111111111111111111111111");
   BOOST_CHECK(!public_key("PUB_ED_11111111111111111111111111111111").valid());

   for (const std::string bad : {
           "PUB_ED_1111111111111111111111111111111",                                               // 31 bytes
           "PUB_ED_5Jj5w8Jgtj6czioch8VRc9HCwJfdcjRquSg4b4Rqou49YD4LDTa6GTZWeHR3JWMZaGdcgsHn1kik3jJJost7b4MP", // too long
           "PUB_ED_0Ven3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z",                                  // not base58
           "PUB_ED_"})
      BOOST_CHECK_THROW(public_key{bad}, fc::exception);
   BOOST_CHECK_THROW(signature("SIG_ED_FVen3X669xLzsi6N2V91DoiyzHzg1uAgqiT8jZ9nS96Z"), fc::exception); // key only
   // a private key whose public half is not the seed's
   BOOST_CHECK_THROW(private_key("PVT_ED_" + fc::to_base58(unhex(rfc8032[0].seed + rfc8032[1].pub), {})), fc::exception);
   BOOST_CHECK_THROW(private_key(ed_private).generate_shared_secret(private_key(ed_private).get_public_key()), fc::exception);
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_CASE(variant_indexes) try {
   // K1, R1 and WA keep their indexes; EM and ED follow
   BOOST_CHECK_EQUAL(private_key::generate<ecc::private_key_shim>().get_public_key().which(), 0u);
   BOOST_CHECK_EQUAL(private_key::generate<r1::private_key_shim>().get_public_key().which(), 1u);
   BOOST_CHECK_EQUAL(private_key(em_private).get_public_key().which(), 3u);
   BOOST_CHECK_EQUAL(private_key(ed_private).get_public_key().which(), 4u);
   BOOST_CHECK_EQUAL(packed_hex(private_key(em_private)).substr(0, 2), "02");
   BOOST_CHECK_EQUAL(packed_hex(private_key(ed_private)).substr(0, 2), "03");
   // keys order by type, then bytes
   BOOST_CHECK(private_key::generate<ecc::private_key_shim>().get_public_key() < private_key(em_private).get_public_key());
   BOOST_CHECK(private_key(em_private).get_public_key() < private_key(ed_private).get_public_key());
} FC_LOG_AND_RETHROW();

BOOST_AUTO_TEST_SUITE_END()
