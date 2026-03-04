#ifndef DT_PROVENANCE_PROXY_TASKS_H_
#define DT_PROVENANCE_PROXY_TASKS_H_

#include <chimaera/admin/admin_tasks.h>
#include <chimaera/chimaera.h>
#include <chimaera/config_manager.h>
#include <yaml-cpp/yaml.h>

#include "autogen/dt_proxy_methods.h"

namespace dt_provenance::proxy {

using MonitorTask = chimaera::admin::MonitorTask;

/**
 * CreateParams for the HTTP Proxy ChiMod
 */
struct CreateParams {
  uint16_t port_;
  uint16_t num_threads_;

  static constexpr const char* chimod_lib_name = "dt_provenance_dt_proxy";

  CreateParams() : port_(9090), num_threads_(8) {}

  CreateParams(uint16_t port, uint16_t num_threads)
      : port_(port), num_threads_(num_threads) {}

  template <class Archive>
  void serialize(Archive& ar) {
    ar(port_, num_threads_);
  }

  /** Load from compose YAML config */
  void LoadConfig(const chi::PoolConfig& pool_config) {
    YAML::Node config = YAML::Load(pool_config.config_);
    if (config["port"]) {
      port_ = config["port"].as<uint16_t>();
    }
    if (config["num_threads"]) {
      num_threads_ = config["num_threads"].as<uint16_t>();
    }
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

}  // namespace dt_provenance::proxy

#endif  // DT_PROVENANCE_PROXY_TASKS_H_
