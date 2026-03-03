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
  chi::u16 port_;
  chi::u16 num_threads_;

  static constexpr const char* chimod_lib_name = "dt_provenance_dt_proxy";

  CreateParams() : port_(9090), num_threads_(8) {}

  CreateParams(chi::u16 port, chi::u16 num_threads)
      : port_(port), num_threads_(num_threads) {}

  template <class Archive>
  void serialize(Archive& ar) {
    ar(port_, num_threads_);
  }

  /** Load from compose YAML config */
  void LoadConfig(const chi::PoolConfig& pool_config) {
    YAML::Node config = YAML::Load(pool_config.config_);
    if (config["port"]) {
      port_ = config["port"].as<chi::u16>();
    }
    if (config["num_threads"]) {
      num_threads_ = config["num_threads"].as<chi::u16>();
    }
  }
};

using CreateTask = chimaera::admin::GetOrCreatePoolTask<CreateParams>;
using DestroyTask = chimaera::admin::DestroyTask;

}  // namespace dt_provenance::proxy

#endif  // DT_PROVENANCE_PROXY_TASKS_H_
