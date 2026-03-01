/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCHIPS_CLIENT_NATIVE_CLIENT_H_
#define MCHIPS_CLIENT_NATIVE_CLIENT_H_

#include <chimaera/chimaera.h>

#include <mchips/mcp_gateway/gateway_client.h>
#include <mchips/protocol/mcp_types.h>

#include <string>
#include <vector>

namespace mchips::client {

/// Native IOWarp client that bypasses HTTP and talks to the Gateway ChiMod
/// directly via Chimaera IPC (SHM, IPC, or TCP transport).
///
/// Architecture:
///   NativeClient → Gateway ChiMod (pool 700) → routes to MChiP pools
///
/// The native client sends HandleHttpRequestTask to the Gateway, which
/// handles routing, tool discovery, and session management identically
/// to the HTTP path. This ensures consistent behavior regardless of
/// transport.
///
/// Usage:
/// ```cpp
/// NativeClient::Init();
/// NativeClient client;
/// client.Connect();
/// client.Initialize();
/// auto tools = client.ListTools();
/// auto result = client.CallTool("demo__add", {{"a", 17}, {"b", 25}});
/// NativeClient::Finalize();
/// ```
class NativeClient {
 public:
  NativeClient() = default;
  ~NativeClient() = default;

  /// Initialize the Chimaera client connection. Must be called once per process.
  ///
  /// Set CHI_IPC_MODE=SHM for same-node shared memory transport (fastest),
  /// or leave default for TCP transport (cross-node).
  static void Init() {
    chi::CHIMAERA_INIT(chi::ChimaeraMode::kClient, false);
  }

  /// Finalize the Chimaera client connection.
  static void Finalize() {
    chi::CHIMAERA_FINALIZE();
  }

  /// Connect to the Gateway ChiMod by pool ID.
  ///
  /// @param gateway_pool_id Pool ID of the gateway (default 700.0)
  void Connect(const chi::PoolId& gateway_pool_id = chi::PoolId(700, 0));

  /// Initialize the MCP session via the Gateway.
  ///
  /// @param protocol_version MCP protocol version to negotiate
  /// @return Session ID assigned by the gateway
  std::string Initialize(
      const std::string& protocol_version = "2025-11-25");

  /// List all tools available through the Gateway.
  ///
  /// The Gateway aggregates tools from all registered MChiPs and
  /// returns them with qualified names (e.g., "cte__put_blob").
  std::vector<protocol::ToolDefinition> ListTools();

  /// Call a tool through the Gateway.
  ///
  /// @param tool_name Qualified tool name (e.g., "demo__add")
  /// @param args      JSON arguments for the tool
  /// @return Tool result (content array + isError flag)
  protocol::json CallTool(const std::string& tool_name,
                          const protocol::json& args);

  /// Get the current session ID.
  const std::string& GetSessionId() const { return session_id_; }

 private:
  /// Build a JSON-RPC request body.
  std::string BuildJsonRpcRequest(const std::string& method,
                                  const protocol::json& params);

  /// Send a JSON-RPC request through the Gateway and parse the response.
  protocol::json SendRequest(const std::string& body);

  mcp_gateway::Client gateway_client_;
  std::string session_id_;
  int next_id_ = 1;
};

}  // namespace mchips::client

#endif  // MCHIPS_CLIENT_NATIVE_CLIENT_H_
