/*
 * Copyright (c) 2026, Gnosis Research Center, Illinois Institute of Technology
 * All rights reserved.
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * demo_server.cc — MChiPs gateway demo launcher
 *
 * Brings up the full IOWarp runtime with the MCP gateway (pool 700) and
 * three MChiP ChiMods — CTE (701), CAE (702), Cluster (703) — then
 * runs until SIGINT or SIGTERM.
 *
 * Usage:
 *   ./mchips_demo_server [--config wrp_conf.yaml] [--port 8080]
 *                        [--host 0.0.0.0] [--threads 4]
 */

#include <chimaera/chimaera.h>
#include <csignal>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <mchips/mcp_gateway/gateway_client.h>
#include <mchips/mchip_cte/mchip_cte_client.h>
#include <mchips/mchip_cae/mchip_cae_client.h>
#include <wrp_cte/core/core_client.h>
#include <wrp_cte/core/content_transfer_engine.h>
#include <mchips/mchip_cluster/mchip_cluster_client.h>

namespace {

volatile sig_atomic_t g_keep_running = 1;

void SignalHandler(int /*sig*/) {
  g_keep_running = 0;
}

void PrintUsage(const char* prog) {
  std::cout << "Usage: " << prog
            << " [--config <yaml>] [--port <port>]"
               " [--host <host>] [--threads <n>]\n"
            << "  --config   Chimaera config file (default: wrp_conf.yaml)\n"
            << "  --port     HTTP port for MCP gateway (default: 8080)\n"
            << "  --host     HTTP host to bind (default: 0.0.0.0)\n"
            << "  --threads  HTTP worker threads (default: 4)\n";
}

}  // namespace

int main(int argc, char* argv[]) {
  std::string config_path = "wrp_conf.yaml";
  std::string host        = "0.0.0.0";
  uint32_t    port        = 8080;
  uint32_t    num_threads = 4;

  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--help") == 0 ||
        std::strcmp(argv[i], "-h") == 0) {
      PrintUsage(argv[0]);
      return 0;
    } else if (std::strcmp(argv[i], "--config") == 0 && i + 1 < argc) {
      config_path = argv[++i];
    } else if (std::strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
      port = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else if (std::strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
      host = argv[++i];
    } else if (std::strcmp(argv[i], "--threads") == 0 && i + 1 < argc) {
      num_threads = static_cast<uint32_t>(std::stoul(argv[++i]));
    } else {
      std::cerr << "Unknown argument: " << argv[i] << "\n";
      PrintUsage(argv[0]);
      return 1;
    }
  }

  std::signal(SIGINT,  SignalHandler);
  std::signal(SIGTERM, SignalHandler);

  std::cout << "MChiPs Demo Server\n"
            << "  config:  " << config_path << "\n"
            << "  host:    " << host << "\n"
            << "  port:    " << port << "\n"
            << "  threads: " << num_threads << "\n\n";

  // ── Initialize Chimaera runtime ──────────────────────────────────────────
  // CHI_SERVER_CONF (not CHIMAERA_CONF) is the env var read by GetServerConfigPath().
  // With the correct var, CHIMAERA_INIT processes the compose section in wrp_conf.yaml,
  // which creates wrp_cte_core (pool 512) with RAM storage config via LoadConfig().
  setenv("CHI_SERVER_CONF", config_path.c_str(), /*overwrite=*/1);

  std::cout << "[1/5] Initializing Chimaera runtime (compose: bdev+CTE core)...\n";
  if (!chi::CHIMAERA_INIT(chi::ChimaeraMode::kRuntime, /*is_restart=*/false)) {
    std::cerr << "ERROR: Failed to initialize Chimaera runtime. "
              << "Ensure a config file exists at: " << config_path << "\n";
    return 1;
  }
  std::cout << "      Chimaera runtime started (wrp_cte_core pool 512 ready).\n";

  // ── Create CTE MChiP pool (701) ──────────────────────────────────────────
  // Pools 700-703 are created manually (not via compose) so the gateway gets
  // the correct port/threads from command-line args.
  std::cout << "[2/5] Creating CTE MChiP pool (701)...\n";
  {
    mchips::mchip_cte::Client cte_client;
    auto create_task = cte_client.AsyncCreate(
        chi::PoolQuery::Broadcast(),
        "mchip_cte",
        chi::PoolId(701, 0));
    create_task.Wait();
    cte_client.Init(chi::PoolId(701, 0));
  }
  std::cout << "      CTE MChiP ready.\n";

  // ── Create CAE MChiP pool (702) ──────────────────────────────────────────
  std::cout << "[3/5] Creating CAE MChiP pool (702)...\n";
  {
    mchips::mchip_cae::Client cae_client;
    auto create_task = cae_client.AsyncCreate(
        chi::PoolQuery::Broadcast(),
        "mchip_cae",
        chi::PoolId(702, 0));
    create_task.Wait();
    cae_client.Init(chi::PoolId(702, 0));
  }
  std::cout << "      CAE MChiP ready.\n";

  // ── Create Cluster MChiP pool (703) ─────────────────────────────────────
  std::cout << "[4/5] Creating Cluster MChiP pool (703)...\n";
  {
    mchips::mchip_cluster::Client cluster_client;
    auto create_task = cluster_client.AsyncCreate(
        chi::PoolQuery::Broadcast(),
        "mchip_cluster",
        chi::PoolId(703, 0));
    create_task.Wait();
    cluster_client.Init(chi::PoolId(703, 0));
  }
  std::cout << "      Cluster MChiP ready.\n";

  // ── Initialize CTE client directly (bypass WRP_CTE_CLIENT_INIT) ──────────
  // WRP_CTE_CLIENT_INIT calls ClientInit which calls AsyncCreate for pool 512
  // then Wait(). Pool 512 was already created by compose, so the admin returns
  // "existing pool" but Wait() never unblocks — deadlock in the runtime process.
  // Fix: directly connect g_cte_client to pool 512 and mark CTE initialized.
  // IMPORTANT: do this BEFORE creating the gateway so CTE is ready before
  //            the HTTP server starts accepting requests.
  std::cout << "Initializing CTE client (pool 512, direct)...\n";
  {
    if (wrp_cte::core::g_cte_client == nullptr) {
      wrp_cte::core::g_cte_client = new wrp_cte::core::Client();
    }
    wrp_cte::core::g_cte_client->Init(chi::PoolId(512, 0));
    CTE_MANAGER->ForceSetInitialized(true);
    std::cout << "      CTE client ready (pool_id=512, direct init).\n";
  }

  // ── Create MCP Gateway pool (700) — starts HTTP server in Create() ──────
  std::cout << "[5/5] Creating MCP Gateway pool (700) on "
            << host << ":" << port << "...\n";
  {
    mchips::mcp_gateway::Client gw_client;
    // Pass port/threads in CreateParams so Create() starts HTTP on the right port
    auto create_task = gw_client.AsyncCreate(
        chi::PoolQuery::Broadcast(),
        "mcp_gateway",
        chi::PoolId(700, 0),
        mchips::mcp_gateway::CreateParams{port, num_threads});
    create_task.Wait();
    gw_client.Init(chi::PoolId(700, 0));
    // HTTP server is started inside Create() — no AsyncStartHttpServer needed.
  }

  std::cout << "\nMChiPs gateway running at http://" << host << ":" << port
            << "/mcp\n"
            << "Tools available: cte__* (12), cae__* (2), cluster__* (3)\n"
            << "Press Ctrl-C to stop.\n\n";

  // ── Run until signal ─────────────────────────────────────────────────────
  while (g_keep_running) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  std::cout << "\nShutting down...\n";
  // Chimaera finalizes automatically on destruction of the runtime object.
  return 0;
}
