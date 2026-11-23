// alcor-chain: EM (secp256k1, MetaMask) and ED (Ed25519, Solana) keys in accounts from genesis, and the
// ed25519_verify host function.

#include <eosio/chain/authorization_manager.hpp>
#include <eosio/chain/key_types.hpp>
#include <eosio/chain/permission_object.hpp>
#include <eosio/testing/tester.hpp>

#include <fc/crypto/elliptic_ed.hpp>
#include <fc/crypto/elliptic_em.hpp>
#include <fc/crypto/hex.hpp>
#include <fc/crypto/signing_message.hpp>
#include <fc/io/json.hpp>
#include <fc/variant_object.hpp>

#include <boost/test/unit_test.hpp>

#include <test_contracts.hpp>
#include <test-data.hpp>

using namespace eosio;
using namespace eosio::chain;
using namespace eosio::testing;
using mvo = fc::mutable_variant_object;

namespace {

   const symbol tkn = symbol(4, "TKN");

   private_key_type em_key(const std::string& name) {
      return private_key_type::regenerate<fc::em::private_key_shim>(fc::sha256::hash(name));
   }

   private_key_type ed_key(const std::string& name) {
      return private_key_type::regenerate<fc::ed::private_key_shim>(fc::ed::secret_key_from_seed(fc::sha256::hash(name)));
   }

   // a key as stored, from its variant index and bytes
   public_key_type key_from_bytes(uint8_t index, const std::string& hex) {
      std::vector<char> bytes(1 + hex.size() / 2);
      bytes[0] = (char)index;
      fc::from_hex(hex, bytes.data() + 1, bytes.size() - 1);
      return fc::raw::unpack<public_key_type>(bytes);
   }

   signature_type flip_byte(const signature_type& sig, size_t at) {
      auto bytes = fc::raw::pack(sig);
      bytes.at(at) ^= 1;
      return fc::raw::unpack<signature_type>(bytes);
   }

   struct alcor_keys_tester : validating_tester {
      const private_key_type em_user_key = em_key("emuser");
      const private_key_type ed_user_key = ed_key("eduser");

      alcor_keys_tester() {
         create_accounts({"eosio.token"_n});
         set_code("eosio.token"_n, test_contracts::eosio_token_wasm());
         set_abi("eosio.token"_n, test_contracts::eosio_token_abi());

         create_account_with_key("emuser"_n, em_user_key.get_public_key());
         create_account_with_key("eduser"_n, ed_user_key.get_public_key());

         push_action("eosio.token"_n, "create"_n, "eosio.token"_n,
                     mvo()("issuer", "eosio.token")("maximum_supply", "1000000.0000 TKN"));
         push_action("eosio.token"_n, "issue"_n, "eosio.token"_n,
                     mvo()("to", "eosio.token")("quantity", "1000.0000 TKN")("memo", ""));
         push_action("eosio.token"_n, "transfer"_n, "eosio.token"_n,
                     mvo()("from", "eosio.token")("to", "emuser")("quantity", "100.0000 TKN")("memo", ""));
         produce_block();
      }

      // an account whose owner and active are one key, created by eosio
      transaction_trace_ptr create_account_with_key(account_name a, const public_key_type& key) {
         signed_transaction trx;
         trx.actions.emplace_back(vector<permission_level>{{config::system_account_name, config::active_name}},
                                  newaccount{
                                     .creator = config::system_account_name,
                                     .name    = a,
                                     .owner   = authority(key),
                                     .active  = authority(key),
                                  });
         set_transaction_headers(trx);
         trx.sign(get_private_key(config::system_account_name, "active"), get_chain_id());
         return push_transaction(trx);
      }

      signed_transaction transfer_trx(account_name from, account_name to, const std::string& quantity) {
         signed_transaction trx;
         trx.actions.emplace_back(get_action("eosio.token"_n, "transfer"_n, {{from, config::active_name}},
                                             mvo()("from", from)("to", to)("quantity", quantity)("memo", "wallet key")));
         set_transaction_headers(trx);
         return trx;
      }

      transaction_trace_ptr transfer(account_name from, account_name to, const std::string& quantity, const private_key_type& key) {
         auto trx = transfer_trx(from, to, quantity);
         trx.sign(key, get_chain_id());
         return push_transaction(trx);
      }

      asset balance(account_name a) {
         return get_currency_balance("eosio.token"_n, tkn, a);
      }

      public_key_type active_key(account_name a) {
         return control->get_authorization_manager().get_permission({a, config::active_name}).auth.to_authority().keys.at(0).key;
      }
   };

   // ed25519_verify called by a contract. Action data: i32 expected result, u32 signature size, u32 public key size,
   // then the signature, the public key and the message (the rest).
   const char ed25519_verify_wast[] = R"=====(
(module
 (import "env" "action_data_size" (func $action_data_size (result i32)))
 (import "env" "read_action_data" (func $read_action_data (param i32 i32) (result i32)))
 (import "env" "eosio_assert" (func $eosio_assert (param i32 i32)))
 (import "env" "ed25519_verify" (func $ed25519_verify (param i32 i32 i32 i32 i32 i32) (result i32)))
 (memory $0 3)
 (data (i32.const 0) "ed25519_verify returned an unexpected result\00")
 (export "apply" (func $apply))
 (func $apply (param $receiver i64) (param $account i64) (param $action i64)
  (local $size i32) (local $sig i32) (local $pub i32) (local $msg i32)
  (set_local $size (call $action_data_size))
  (drop (call $read_action_data (i32.const 64) (get_local $size)))
  (set_local $sig (i32.const 76))
  (set_local $pub (i32.add (get_local $sig) (i32.load (i32.const 68))))
  (set_local $msg (i32.add (get_local $pub) (i32.load (i32.const 72))))
  (call $eosio_assert
   (i32.eq
    (call $ed25519_verify
     (get_local $msg) (i32.sub (i32.add (i32.const 64) (get_local $size)) (get_local $msg))
     (get_local $sig) (i32.load (i32.const 68))
     (get_local $pub) (i32.load (i32.const 72)))
    (i32.load (i32.const 64)))
   (i32.const 0))
 )
)
)=====";

   std::vector<char> unhex(const std::string& hex) {
      std::vector<char> bytes(hex.size() / 2);
      fc::from_hex(hex, bytes.data(), bytes.size());
      return bytes;
   }

   std::vector<char> verify_call(int32_t expected, const std::vector<char>& sig, const std::vector<char>& pub, const std::vector<char>& msg) {
      std::vector<char> data(12);
      const uint32_t sig_size = sig.size(), pub_size = pub.size();
      std::memcpy(data.data(), &expected, 4);
      std::memcpy(data.data() + 4, &sig_size, 4);
      std::memcpy(data.data() + 8, &pub_size, 4);
      data.insert(data.end(), sig.begin(), sig.end());
      data.insert(data.end(), pub.begin(), pub.end());
      data.insert(data.end(), msg.begin(), msg.end());
      return data;
   }

} // namespace

BOOST_AUTO_TEST_SUITE(alcor_keys_tests)

BOOST_AUTO_TEST_CASE(em_and_ed_accounts_transfer) try {
   alcor_keys_tester t;
   BOOST_CHECK_EQUAL(t.active_key("emuser"_n).which(), em_key_type);
   BOOST_CHECK_EQUAL(t.active_key("eduser"_n).which(), ed_key_type);
   BOOST_CHECK(t.active_key("emuser"_n) == t.em_user_key.get_public_key());
   BOOST_CHECK(t.active_key("eduser"_n) == t.ed_user_key.get_public_key());

   t.transfer("emuser"_n, "eduser"_n, "10.0000 TKN", t.em_user_key);
   t.transfer("eduser"_n, "emuser"_n, "3.0000 TKN", t.ed_user_key);
   t.produce_block(); // the validating node replays both

   BOOST_CHECK_EQUAL(t.balance("emuser"_n), asset::from_string("93.0000 TKN"));
   BOOST_CHECK_EQUAL(t.balance("eduser"_n), asset::from_string("7.0000 TKN"));

   // the keys recovered from the transaction are the account keys
   auto trx = t.transfer_trx("emuser"_n, "eduser"_n, "1.0000 TKN");
   trx.sign(t.em_user_key, t.get_chain_id());
   trx.sign(t.ed_user_key, t.get_chain_id());
   flat_set<public_key_type> keys;
   trx.get_signature_keys(t.get_chain_id(), fc::time_point::maximum(), keys);
   BOOST_CHECK(keys == flat_set<public_key_type>({t.em_user_key.get_public_key(), t.ed_user_key.get_public_key()}));
   // an extra signature (ED) beyond what the authority needs is irrelevant and refused, as for K1
   BOOST_CHECK_THROW(t.push_transaction(trx), tx_irrelevant_sig);

   // updateauth signed by an EM key adds a permission with an ED key
   t.set_authority("emuser"_n, "spend"_n, authority(t.ed_user_key.get_public_key()), config::active_name,
                   {{"emuser"_n, config::active_name}}, {t.em_user_key});
   t.produce_block();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(wrong_signatures_refused) try {
   alcor_keys_tester t;

   // another EM key, another ED key: signatures recover, but to keys the accounts do not have
   BOOST_CHECK_THROW(t.transfer("emuser"_n, "eduser"_n, "1.0000 TKN", em_key("someone else")), unsatisfied_authorization);
   BOOST_CHECK_THROW(t.transfer("eduser"_n, "emuser"_n, "0.0001 TKN", ed_key("someone else")), unsatisfied_authorization);

   // a K1 key for the EM account
   BOOST_CHECK_THROW(t.transfer("emuser"_n, "eduser"_n, "1.0000 TKN", base_tester::get_private_key("emuser"_n, "active")),
                     unsatisfied_authorization);

   // an EM signature with a flipped bit in s recovers some other key
   {
      auto trx = t.transfer_trx("emuser"_n, "eduser"_n, "1.0000 TKN");
      trx.sign(t.em_user_key, t.get_chain_id());
      trx.signatures[0] = flip_byte(trx.signatures[0], 1 + 40);
      BOOST_CHECK_THROW(t.push_transaction(trx), fc::exception);
   }
   // an ED signature with a flipped bit does not verify: recovery throws
   {
      auto trx = t.transfer_trx("eduser"_n, "emuser"_n, "0.0001 TKN");
      trx.sign(t.ed_user_key, t.get_chain_id());
      trx.signatures[0] = flip_byte(trx.signatures[0], 1 + 32 + 10);
      BOOST_CHECK_THROW(t.push_transaction(trx), fc::exception);
   }
   // an ED signature of the right account key over another transaction
   {
      auto other = t.transfer_trx("eduser"_n, "emuser"_n, "0.0002 TKN");
      other.sign(t.ed_user_key, t.get_chain_id());
      auto trx = t.transfer_trx("eduser"_n, "emuser"_n, "0.0001 TKN");
      trx.signatures = other.signatures;
      BOOST_CHECK_THROW(t.push_transaction(trx), fc::exception);
   }
   // an EM signature over the bare digest, without the signing text and EIP-191, recovers another key
   {
      auto trx = t.transfer_trx("emuser"_n, "eduser"_n, "1.0000 TKN");
      const auto digest = trx.sig_digest(t.get_chain_id(), trx.context_free_data);
      const auto secret = fc::sha256::hash(std::string("emuser"));
      const auto k1 = fc::ecc::private_key::regenerate(secret).sign_compact(digest); // v ‖ r ‖ s, over the digest
      std::vector<char> em(1 + 65);
      em[0] = (char)em_key_type;
      std::memcpy(em.data() + 1, k1.data + 1, 64);
      em[65] = (char)(27 + ((k1.data[0] - 27) & 3));
      trx.signatures.push_back(fc::raw::unpack<signature_type>(em));
      BOOST_CHECK_THROW(t.push_transaction(trx), fc::exception);
   }
   t.produce_block();
   BOOST_CHECK_EQUAL(t.balance("emuser"_n), asset::from_string("100.0000 TKN"));
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(em_key_off_curve_refused) try {
   alcor_keys_tester t;
   // x = 5 has no point on secp256k1
   const auto off_curve = key_from_bytes(em_key_type, "020000000000000000000000000000000000000000000000000000000000000005");
   BOOST_REQUIRE(!off_curve.valid());

   BOOST_CHECK_EXCEPTION(t.set_authority("emuser"_n, "spend"_n, authority(off_curve), config::active_name,
                                         {{"emuser"_n, config::active_name}}, {t.em_user_key}),
                         public_key_type_exception, fc_exception_message_contains("is not a point on secp256k1"));
   BOOST_CHECK_EXCEPTION(t.create_account_with_key("offcurve"_n, off_curve),
                         public_key_type_exception, fc_exception_message_contains("is not a point on secp256k1"));

   // a key on the curve is fine, and an ED key is not checked for being a point (an unusable key, like a Solana PDA)
   t.set_authority("emuser"_n, "spend"_n, authority(em_key("spend").get_public_key()), config::active_name,
                   {{"emuser"_n, config::active_name}}, {t.em_user_key});
   t.set_authority("eduser"_n, "spend"_n, authority(key_from_bytes(ed_key_type, std::string(64, '0'))), config::active_name,
                   {{"eduser"_n, config::active_name}}, {t.ed_user_key});
   t.produce_block();
} FC_LOG_AND_RETHROW()

// The test bios contract is built with an upstream CDT that cannot unpack EM / ED keys at all, so this contract hands
// the packed schedule straight to the host functions. Action data: u8 format (0: set_proposed_producers with
// legacy producer_key, 1: set_proposed_producers_ex format 1 with producer_authority), then the packed schedule.
const char set_producers_wast[] = R"=====(
(module
 (import "env" "action_data_size" (func $action_data_size (result i32)))
 (import "env" "read_action_data" (func $read_action_data (param i32 i32) (result i32)))
 (import "env" "set_proposed_producers" (func $set_proposed_producers (param i32 i32) (result i64)))
 (import "env" "set_proposed_producers_ex" (func $set_proposed_producers_ex (param i64 i32 i32) (result i64)))
 (memory $0 1)
 (export "apply" (func $apply))
 (func $apply (param $receiver i64) (param $account i64) (param $action i64)
  (local $size i32)
  (set_local $size (call $action_data_size))
  (drop (call $read_action_data (i32.const 0) (get_local $size)))
  (if (i32.eq (i32.load8_u (i32.const 0)) (i32.const 1))
   (then (drop (call $set_proposed_producers_ex (i64.const 1) (i32.const 1) (i32.sub (get_local $size) (i32.const 1)))))
   (else (drop (call $set_proposed_producers (i32.const 1) (i32.sub (get_local $size) (i32.const 1))))))
 )
)
)=====";

BOOST_AUTO_TEST_CASE(wallet_keys_cannot_sign_blocks) try {
   alcor_keys_tester t;
   t.create_accounts({"prodsetter"_n, "emprod"_n, "edprod"_n});
   t.set_code("prodsetter"_n, set_producers_wast);
   t.push_action(config::system_account_name, "setpriv"_n, config::system_account_name, mvo()("account", "prodsetter")("is_priv", 1));
   t.produce_block();

   auto propose = [&](uint8_t format, const std::vector<char>& packed) {
      std::vector<char> data{(char)format};
      data.insert(data.end(), packed.begin(), packed.end());
      signed_transaction trx;
      trx.actions.emplace_back(vector<permission_level>{{"prodsetter"_n, config::active_name}}, "prodsetter"_n, "propose"_n, data);
      t.set_transaction_headers(trx);
      trx.sign(base_tester::get_private_key("prodsetter"_n, "active"), t.get_chain_id());
      return t.push_transaction(trx);
   };
   auto authorities = [](account_name producer, std::vector<key_weight> keys) {
      return fc::raw::pack(std::vector<producer_authority>{producer_authority{producer, block_signing_authority_v0{1, std::move(keys)}}});
   };
   auto refused = fc_exception_message_contains("these keys sign transactions only, not blocks");

   for (const auto& [producer, key] : {std::pair{"emprod"_n, em_key("emprod").get_public_key()},
                                       std::pair{"edprod"_n, ed_key("edprod").get_public_key()}}) {
      const auto k1 = base_tester::get_public_key(producer, "active");
      // set_proposed_producers_ex: alone, and next to a K1 key in a multi-key authority
      BOOST_CHECK_EXCEPTION(propose(1, authorities(producer, {{key, 1}})), wasm_execution_error, refused);
      BOOST_CHECK_EXCEPTION(propose(1, authorities(producer, {{k1, 1}, {key, 1}})), wasm_execution_error, refused);
      // set_proposed_producers, legacy producer_key
      BOOST_CHECK_EXCEPTION(propose(0, fc::raw::pack(std::vector<legacy::producer_key>{{producer, key}})), wasm_execution_error, refused);
   }
   // K1 producers still go through the same path
   propose(1, authorities("emprod"_n, {{base_tester::get_public_key("emprod"_n, "active"), 1}}));
   t.produce_block();
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_CASE(ed25519_verify_host_function) try {
   validating_tester t;
   t.create_accounts({"edverify"_n});
   t.set_code("edverify"_n, ed25519_verify_wast);
   t.produce_block();

   auto call = [&](const std::vector<char>& data) {
      signed_transaction trx;
      trx.actions.emplace_back(vector<permission_level>{{"edverify"_n, config::active_name}}, "edverify"_n, "verify"_n, data);
      t.set_transaction_headers(trx);
      trx.sign(base_tester::get_private_key("edverify"_n, "active"), t.get_chain_id());
      return t.push_transaction(trx);
   };

   // RFC 8032 section 7.1, test vectors 1-3: public key, message, signature
   const std::vector<std::array<std::string, 3>> rfc8032 = {
      {"d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
       "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
      {"3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
       "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
      {"fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
       "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"},
   };
   for (const auto& [pub, msg, sig] : rfc8032) {
      call(verify_call(0, unhex(sig), unhex(pub), unhex(msg)));
      auto bad = unhex(sig);
      bad[5] ^= 1;
      call(verify_call(-1, bad, unhex(pub), unhex(msg)));
      call(verify_call(-1, unhex(sig), unhex(pub), unhex(msg + "00")));
   }
   // the contract checks the result: a wrong expectation fails the action
   BOOST_CHECK_EXCEPTION(call(verify_call(-1, unhex(rfc8032[1][2]), unhex(rfc8032[1][0]), unhex(rfc8032[1][1]))),
                         eosio_assert_message_exception, eosio_assert_message_is("ed25519_verify returned an unexpected result"));

   // sizes: the signature must be 64 bytes and the key 32, else -1
   const auto sig = unhex(rfc8032[2][2]), pub = unhex(rfc8032[2][0]), msg = unhex(rfc8032[2][1]);
   call(verify_call(-1, std::vector<char>(sig.begin(), sig.end() - 1), pub, msg));
   call(verify_call(-1, [&] { auto s = sig; s.push_back(0); return s; }(), pub, msg));
   call(verify_call(-1, sig, std::vector<char>(pub.begin(), pub.end() - 1), msg));
   call(verify_call(-1, sig, [&] { auto p = pub; p.push_back(0); return p; }(), msg));
   call(verify_call(-1, {}, {}, {}));

   // messages up to 64 KiB verify; a longer one is refused even with a valid signature
   const auto secret = fc::ed::secret_key_from_seed(fc::sha256::hash(std::string("ed25519_verify")));
   const std::vector<char> key(secret.data + 32, secret.data + 64);
   for (size_t size : {size_t(1000), size_t(config::ed25519_verify_max_message_size), size_t(config::ed25519_verify_max_message_size + 1)}) {
      std::vector<char> long_msg(size, 'a');
      const auto long_sig = fc::ed::sign_detached(secret, long_msg.data(), long_msg.size());
      BOOST_REQUIRE(fc::ed::verify_detached(long_sig.data, long_msg.data(), long_msg.size(), key.data()));
      call(verify_call(size <= config::ed25519_verify_max_message_size ? 0 : -1,
                       std::vector<char>(long_sig.data, long_sig.data + 64), key, long_msg));
   }
   t.produce_block();
} FC_LOG_AND_RETHROW()

// Differential corpus: keys and signatures made by wallet libraries (@noble/curves), signed the way MetaMask
// personal_sign and Phantom signMessage sign the transaction signing text; generator tools/keys-oracle/gen.mjs.
// For each case the key and signature are parsed from their strings and the key is recovered against the digest:
//   accept    recovery gives exactly the key
//   mismatch  recovery gives another key
//   reject    recovery throws (for EM: any encoding but v 27/28, 0 < r < n, 0 < s <= n/2)
BOOST_AUTO_TEST_CASE(wallet_corpus) try {
   const auto corpus = fc::json::from_file(std::string(UNITTEST_TEST_DATA_DIR) + "/keys-oracle/corpus.json").get_object();
   BOOST_REQUIRE_EQUAL(corpus["prefix"].as_string(), std::string(fc::crypto::transaction_signing_prefix));

   std::map<std::string, size_t> passed, total;
   for (const auto& item : corpus["cases"].get_array()) {
      const auto& c = item.get_object();
      const auto kind = c["kind"].as_string(), name = c["case"].as_string(), expect = c["expect"].as_string();
      const auto label = kind + " " + expect;
      BOOST_TEST_CONTEXT(kind << " / " << name << " / " << c["sig"].as_string()) {
         const public_key_type pub(c["pub"].as_string());
         const signature_type sig(c["sig"].as_string());
         BOOST_REQUIRE_EQUAL(pub.which(), kind == "em" ? em_key_type : ed_key_type);
         BOOST_REQUIRE_EQUAL(sig.which(), kind == "em" ? em_key_type : ed_key_type);
         const fc::sha256 digest(c["digest"].as_string());

         std::optional<public_key_type> recovered;
         try {
            recovered = public_key_type(sig, digest);
         } catch (const fc::exception&) {}

         bool ok = false;
         if (expect == "accept")
            ok = recovered && *recovered == pub;
         else if (expect == "mismatch")
            ok = recovered && *recovered != pub;
         else if (expect == "reject")
            ok = !recovered;
         else
            BOOST_FAIL("unknown expect " + expect);
         BOOST_CHECK_MESSAGE(ok, "expected " << expect << ", recovered " << (recovered ? recovered->to_string({}) : std::string("nothing (threw)")));
         ++total[label];
         passed[label] += ok;
      }
   }
   size_t all = 0;
   for (const auto& [label, n] : total) {
      BOOST_TEST_MESSAGE("wallet corpus " << label << ": " << passed[label] << "/" << n);
      BOOST_CHECK_EQUAL(passed[label], n);
      all += n;
   }
   BOOST_CHECK_EQUAL(all, corpus["cases"].get_array().size());
} FC_LOG_AND_RETHROW()

BOOST_AUTO_TEST_SUITE_END()
