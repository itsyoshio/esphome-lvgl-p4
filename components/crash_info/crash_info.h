#pragma once

#include <string>
#include "esphome/core/component.h"

namespace esphome::crash_info {

// What the app is doing right now (e.g. "va:listening"); kept across a crash reset.
void set_ctx(const char *ctx);

class CrashInfo : public Component {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  // Why the previous boot ended, German, one line ("" before setup).
  const std::string &summary() const { return this->summary_; }
  // true if the previous boot ended in a panic / watchdog
  bool crashed() const { return this->crashed_; }
  bool has_coredump() const;
  void erase_coredump();

 protected:
  std::string summary_;
  bool crashed_{false};
};

}  // namespace esphome::crash_info
