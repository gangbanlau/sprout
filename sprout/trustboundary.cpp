/**
 * @file trustboundary.cpp Trust boundary processing
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


extern "C" {
#include <pjsip.h>
#include <pjlib-util.h>
#include <pjlib.h>
#include <stdint.h>
}

#include <string>

#include "log.h"
#include "constants.h"
#include "pjutils.h"
#include "trustboundary.h"

/// Strip headers as appropriate when crossing a trust boundary.
static void proxy_strip_trusted(pjsip_tx_data *tdata)
{
  LOG_DEBUG("Strip trusted headers");

  PJUtils::delete_header(tdata->msg, &STR_P_A_N_I);
  PJUtils::delete_header(tdata->msg, &STR_P_SERVED_USER);
}

TrustBoundary::TrustBoundary(std::string description, pj_bool_t strip_request, pj_bool_t strip_response) :
  _strip_request(strip_request),
  _strip_response(strip_response),
  _description(description)
{
}

void TrustBoundary::process_request(pjsip_tx_data* tdata)
{
  if (_strip_request)
  {
    proxy_strip_trusted(tdata);
  }
}

void TrustBoundary::process_response(pjsip_tx_data* tdata)
{
  if (_strip_response)
  {
    proxy_strip_trusted(tdata);
  }
}

void TrustBoundary::process_stateless_message(pjsip_tx_data* tdata)
{
  LOG_DEBUG("Strip trusted headers - stateless");
  proxy_strip_trusted(tdata);
}

std::string TrustBoundary::to_string()
{
  return _description + "(" + (_strip_request  ? "-req" : "") +
                        "," + (_strip_response ? "-rsp" : "") + ")";
}

/// Trust boundary instance: no boundary;
TrustBoundary TrustBoundary::TRUSTED("TRUSTED", false, false);

/// Trust boundary instance: from client to core.  Allow client to
/// provide trusted data to the core, but don't allow it to see
/// the core's internal data. I.e., strip from responses.
TrustBoundary TrustBoundary::INBOUND_EDGE_CLIENT("INBOUND_EDGE_CLIENT", false, true);

/// Trust boundary instance: from core to client.  Allow client to
/// provide trusted data to the core, but don't allow it to see
/// the core's internal data. I.e., strip from requests.
TrustBoundary TrustBoundary::OUTBOUND_EDGE_CLIENT("OUTBOUND_EDGE_CLIENT", true, false);

/// Trust boundary instance: edge processing, but we don't know which
/// direction. Don't allow trusted data to pass in either direction.
TrustBoundary TrustBoundary::UNKNOWN_EDGE_CLIENT("UNKNOWN_EDGE_CLIENT", true, true);

/// Trust boundary instance: from trunk to core.  Don't allow
/// trusted data to pass in either direction.
TrustBoundary TrustBoundary::INBOUND_TRUNK("INBOUND_TRUNK", true, true);

/// Trust boundary instance: from core to trunk.  Don't allow
/// trusted data to pass in either direction.
TrustBoundary TrustBoundary::OUTBOUND_TRUNK("OUTBOUND_TRUNK", true, true);

