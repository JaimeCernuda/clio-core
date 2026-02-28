/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "mchips/mcp_gateway/http_server.h"

// TODO(Phase B.4): Implement HttpServer using cpp-httplib
//
// Start():
//   - Create httplib::Server
//   - Register POST /mcp handler → calls RequestHandler callback
//   - Register GET /mcp handler → SSE stream
//   - Register DELETE /mcp handler → calls DeleteHandler callback
//   - server.listen(host, port) in a background thread
//
// Stop():
//   - server.stop()
//   - Join background thread

namespace mchips::mcp_gateway {
}  // namespace mchips::mcp_gateway
