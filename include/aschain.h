/**
 * @file aschain.h The AS chain data type.
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2013  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

///
///

#pragma once

extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
}

#include <string>
#include <vector>

#include "log.h"
#include "sessioncase.h"
#include "ifchandler.h"


// Forward declarations.
class CallServices;
class UASTransaction;

/// Short-lived data structure holding the details of a calculated target.
struct target
{
  pj_bool_t from_store;
  pj_bool_t upstream_route;
  std::string aor;
  std::string binding_id;
  pjsip_uri* uri;
  std::list<pjsip_uri*> paths;
  pjsip_transport* transport;
  int liveness_timeout;

  // Default constructor.
  target() :
    from_store(PJ_FALSE),
    upstream_route(PJ_FALSE),
    aor(),
    binding_id(),
    uri(NULL),
    paths(),
    transport(NULL),
    liveness_timeout(0)
  {
  }
};
typedef std::list<target> target_list;

class AsChainTable;

/// The AS chain.
//
// Clients should use AsChainLink, not this class directly.
//
// AsChain objects are constructed by AsChainLink::create_as_chain,
// which also returns a reference to the created object.
//
// References can also be obtained via AsChainTable::lookup().
//
// References are released by AsChainLink::release().
//
// AsChain objects are destroyed by AsChain::request_destroy().
//
class AsChain
{
public:
  void request_destroy();

private:
  friend class AsChainLink;
  friend class AsChainTable;

  AsChain(AsChainTable* as_chain_table,
          const SessionCase& session_case,
          const std::string& served_user,
          bool is_registered,
          SAS::TrailId trail,
          Ifcs* ifcs);
  ~AsChain();

  void inc_ref()
  {
    ++_refs;
    LOG_DEBUG("AsChain inc ref %p -> %d", this, _refs.load());
  }

  void dec_ref()
  {
    int count = --_refs;
    LOG_DEBUG("AsChain dec ref %p -> %d", this, count);
    pj_assert(count >= 0);
    if (count == 0)
    {
      delete this;
    }
  }

  std::string to_string(size_t index) const;
  const SessionCase& session_case() const;
  size_t size() const;
  bool matches_target(pjsip_tx_data* tdata) const;
  SAS::TrailId trail() const;

  AsChainTable* const _as_chain_table;
  std::atomic<int> _refs;

  /// ODI tokens, one for each step.
  std::vector<std::string> _odi_tokens;

  const SessionCase& _session_case;
  const std::string _served_user;
  const bool _is_registered;
  const SAS::TrailId _trail;
  const Ifcs* _ifcs;  //< List of iFCs. Owned by this object.
};


/// A single link in the AsChain. Clients always access an AsChain
// through one of these.
//
// AsChainLink also acts as a context: until release() is called, the
// underlying AsChain object cannot be deleted.
class AsChainLink
{
public:
  AsChainLink() :
    _as_chain(NULL),
    _index(0u)
  {
  }

  ~AsChainLink()
  {
  }

  AsChain* as_chain() const
  {
    return _as_chain;
  }

  bool is_set() const
  {
    return (_as_chain != NULL);
  }

  bool complete() const
  {
    return ((_as_chain == NULL) || (_index == _as_chain->size()));
  }

  /// Get the next link in the chain.
  AsChainLink next() const
  {
    pj_assert(!complete());
    return AsChainLink(_as_chain, _index + 1);
  }

  /// Create a new reference to the underlying AsChain object.  Caller
  // must call release() when they have finished using this duplicate.
  AsChainLink duplicate() const
  {
    if (_as_chain != NULL)
    {
      _as_chain->inc_ref();
    }
    return *this;
  }

  /// Caller has finished using this link.
  void release() const
  {
    if (_as_chain != NULL)
    {
      _as_chain->dec_ref();
    }
  }

  SAS::TrailId trail() const
  {
    return ((_as_chain == NULL) ? 0 : _as_chain->trail());
  }

  std::string to_string() const
  {
    return is_set() ? _as_chain->to_string(_index) : "None";
  }

  const SessionCase& session_case() const
  {
    return _as_chain->session_case();
  }

  const std::string& served_user() const
  {
    return _as_chain->_served_user;
  }

  bool matches_target(pjsip_tx_data* tdata) const
  {
    return _as_chain->matches_target(tdata);
  }

  /// Returns default handling for this chain link.  If false, errors or
  /// timeouts from the AS result in the transaction failing.  If true,
  /// timeout or 5xx errors from the AS result in the transaction continuing
  /// with the next AS in the chain.
  bool default_handling() const
  {
    return _default_handling;
  }

  /// Resets the default handling.  This should be called when the AS
  /// responds with a 100 Trying response as this indicates the AS has
  /// received and is processing the transaction.
  void reset_default_handling()
  {
    _default_handling = false;
  }

  /// Disposition of a request. Suggests what to do next.
  enum Disposition {
    /// The request has been completely handled. Processing should
    // stop.
    Stop,

    /// The request is being passed to an external application
    // server. Processing should skip to target processing,
    // omitting any subsequent stages.
    Skip,

    /// There are no links left on the chain. Processing should
    // continue with the next stage.
    Complete,

    /// The internal application server (if any) has processed the
    // message according to the curren link. Processing should
    // continue with the next link.
    Next
  };

  static AsChainLink create_as_chain(AsChainTable* as_chain_table,
                                     const SessionCase& session_case,
                                     const std::string& served_user,
                                     bool is_registered,
                                     SAS::TrailId trail,
                                     Ifcs* ifcs);

  Disposition on_initial_request(CallServices* call_services,
                                 UASTransaction* uas_data,
                                 pjsip_tx_data* tdata,
                                 target** target);

private:
  friend class AsChainTable;

  AsChainLink(AsChain* as_chain, size_t index) :
    _as_chain(as_chain),
    _index(index)
  {
  }

  /// Returns the ODI token of the next AsChainLink in this chain.
  const std::string& next_odi_token() const
  {
    return _as_chain->_odi_tokens[_index];
  }

  AsChain* _as_chain;
  size_t _index;
  bool _default_handling;

  /// Application server timeouts (in seconds).
  static const int AS_TIMEOUT_CONTINUE = 2;
  static const int AS_TIMEOUT_TERMINATE = 4;

};


/// Lookup table of AsChain objects.
class AsChainTable
{
public:
  AsChainTable();
  ~AsChainTable();

  /// Lookup the next step to follow when receiving the given
  // token. The 0th token thus indicates the 1st step, the 1st token
  // the 2nd step, and so on.
  AsChainLink lookup(const std::string& token);

private:
  friend class AsChain;

  void register_(AsChain* as_chain, std::vector<std::string>& tokens);
  void unregister(std::vector<std::string>& tokens);

  static const int TOKEN_LENGTH = 10;

  /// Map from token to pair of (AsChain, index).
  std::map<std::string, AsChainLink> _t2c_map;
  pthread_mutex_t _lock;
};
