#pragma once

#include <eosio/eosio.hpp>

#include <vector>

/// Deployed on eosio in the priority action tests. Writes the row the node reads (table prioacts, scope = self,
/// primary key prioacts) as given, without the system contract's checks, so tests can also write a bad row.
class [[eosio::contract]] priority_actions_test : public eosio::contract {
public:
   using eosio::contract::contract;

   [[eosio::action]]
   void setrow( const std::vector<char>& value );

   [[eosio::action]]
   void delrow();
};
