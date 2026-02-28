/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * demo_client.cc — MChiPs standalone C++ MCP client demo
 *
 * Connects to a running MChiPs gateway, lists all tools, then demonstrates
 * a put/get blob round-trip using the CTE MChiP.
 *
 * Usage:
 *   ./mchips_demo_client [--url http://localhost:8080/mcp]
 */

#include <mchips/client/mcp_client.h>
#include <mchips/protocol/mcp_types.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <string>

// Base64 encode (same portable impl used in cte_runtime.cc)
static std::string Base64Encode(const std::string& data) {
  static const char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((data.size() + 2) / 3) * 4);
  for (size_t i = 0; i < data.size(); i += 3) {
    uint32_t b = static_cast<uint8_t>(data[i]) << 16;
    if (i + 1 < data.size()) b |= static_cast<uint8_t>(data[i + 1]) << 8;
    if (i + 2 < data.size()) b |= static_cast<uint8_t>(data[i + 2]);
    out += kAlphabet[(b >> 18) & 0x3F];
    out += kAlphabet[(b >> 12) & 0x3F];
    out += (i + 1 < data.size()) ? kAlphabet[(b >>  6) & 0x3F] : '=';
    out += (i + 2 < data.size()) ? kAlphabet[(b      ) & 0x3F] : '=';
  }
  return out;
}

namespace {

void PrintUsage(const char* prog) {
  std::cout << "Usage: " << prog << " [--url <endpoint>]\n"
            << "  --url   MCP endpoint URL (default: http://localhost:8080/mcp)\n";
}

void PrintSeparator() {
  std::cout << std::string(60, '-') << "\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string url = "http://localhost:8080/mcp";

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 ||
        std::strcmp(argv[i], "-h") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else if (std::strcmp(argv[i], "--url") == 0 && i + 1 < argc) {
      url = argv[++i];
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  std::cout << "MChiPs Demo Client\n";
  std::cout << "  endpoint: " << url << "\n\n";

  mchips::client::McpClient client(mchips::client::McpClientConfig{url});

  try {
    // ── 1. Initialize session ──────────────────────────────────────────────
    PrintSeparator();
    std::cout << "[1] Initializing MCP session...\n";
    client.Initialize();
    std::cout << "    Session established.\n";

    // ── 2. List tools ──────────────────────────────────────────────────────
    PrintSeparator();
    std::cout << "[2] Listing all available tools:\n";
    auto tools = client.ListTools();
    if (tools.empty()) {
      std::cout << "    (no tools — is the gateway running with MChiPs?)\n";
    } else {
      for (const auto& td : tools) {
        std::cout << "    • " << td.name;
        if (!td.description.empty()) {
          // Truncate description at 60 chars for display
          std::string desc = td.description;
          if (desc.size() > 60) desc = desc.substr(0, 57) + "...";
          std::cout << " — " << desc;
        }
        std::cout << "\n";
      }
    }
    std::cout << "    Total: " << tools.size() << " tool(s)\n";

    // ── 3. Call cte__put_blob ─────────────────────────────────────────────
    PrintSeparator();
    std::cout << "[3] Calling cte__put_blob...\n";
    const std::string blob_payload = "Hello from MChiPs demo client!";
    const std::string blob_b64 = Base64Encode(blob_payload);

    mchips::protocol::json put_args = {
        {"tag_name",  "mchips_demo"},
        {"blob_name", "hello_world"},
        {"data",      blob_b64},
        {"priority",  0.5}
    };
    auto put_result = client.CallTool("cte__put_blob", put_args);

    std::cout << "    isError: " << (put_result.isError ? "true" : "false") << "\n";
    for (const auto& c : put_result.content) {
      if (c.text.has_value()) std::cout << "    " << *c.text << "\n";
    }

    // ── 4. Call cte__get_blob ─────────────────────────────────────────────
    PrintSeparator();
    std::cout << "[4] Calling cte__get_blob...\n";
    mchips::protocol::json get_args = {
        {"tag_name",  "mchips_demo"},
        {"blob_name", "hello_world"}
    };
    auto get_result = client.CallTool("cte__get_blob", get_args);

    std::cout << "    isError: " << (get_result.isError ? "true" : "false") << "\n";
    for (const auto& c : get_result.content) {
      if (c.text.has_value()) std::cout << "    " << *c.text << "\n";
    }

    // ── 5. Call cluster__cluster_status ──────────────────────────────────
    PrintSeparator();
    std::cout << "[5] Calling cluster__cluster_status...\n";
    auto status_result = client.CallTool("cluster__cluster_status",
                                         mchips::protocol::json::object());
    std::cout << "    isError: " << (status_result.isError ? "true" : "false") << "\n";
    for (const auto& c : status_result.content) {
      if (c.text.has_value()) std::cout << "    " << *c.text << "\n";
    }

    // ── 6. Close session ──────────────────────────────────────────────────
    PrintSeparator();
    std::cout << "[6] Closing MCP session...\n";
    client.Close();
    std::cout << "    Session closed.\n";

  } catch (const std::exception& e) {
    std::cerr << "\nERROR: " << e.what() << "\n";
    std::cerr << "Is the MChiPs demo server running at " << url << "?\n";
    return 1;
  }

  PrintSeparator();
  std::cout << "\nDemo complete.\n";
  return 0;
}
