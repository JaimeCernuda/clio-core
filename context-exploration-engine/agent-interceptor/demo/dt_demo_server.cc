/**
 * DTProvenance Demo Server
 *
 * Initializes the Chimaera runtime in server mode and loads the
 * compose configuration which starts all DTProvenance ChiMods:
 * - HTTP Proxy (pool 800, port 9090)
 * - Anthropic Interception (pool 801)
 * - Conversation Tracker (pool 810)
 *
 * Same pattern as MChiPs demo/demo_server.cc and all IOWarp programs.
 */

#include <chimaera/chimaera.h>
#include <csignal>
#include <iostream>

static volatile bool running = true;

void SignalHandler(int sig) {
  (void)sig;
  running = false;
}

int main(int argc, char* argv[]) {
  // Register signal handlers for clean shutdown
  signal(SIGINT, SignalHandler);
  signal(SIGTERM, SignalHandler);

  // Initialize Chimaera in server mode
  // This reads the YAML config, starts the runtime, and creates all
  // ChiMods specified in the compose section.
  chi::CHIMAERA_INIT(chi::ChimaeraMode::kServer, true);

  std::cout << "DTProvenance server started. Press Ctrl+C to stop."
            << std::endl;

  // Wait for shutdown signal
  while (running) {
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  std::cout << "DTProvenance server shutting down..." << std::endl;

  // Chimaera runtime handles cleanup via RAII
  return 0;
}
