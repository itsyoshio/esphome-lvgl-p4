#include "crash_info.h"

#include <cstring>
#include <esp_attr.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#ifdef USE_CRASH_INFO_COREDUMP
#include <esp_core_dump.h>
#endif

namespace esphome::crash_info {

static const char *const TAG = "crash_info";
static constexpr uint32_t CTX_MAGIC = 0xC7C70001;
static constexpr uint32_t WDT_MAGIC = 0x7D7D0001;

// survives a software / panic reset, garbage after power-on (validated by the magics)
struct Record {
  uint32_t ctx_magic;
  char ctx[48];
  uint32_t wdt_magic;
  uint32_t uptime_s;
  char tasks[80];      // tasks that did not reset the task watchdog
  char running[48];    // what each core was running
  char component[40];  // component the main loop was in
};
static Record __attribute__((section(".noinit"))) s_rec;  // NOLINT

// copy of the previous boot's record, taken before anything writes a new ctx
static Record s_prev;  // NOLINT
static bool s_prev_taken = false;  // NOLINT
static void take_prev() {
  if (s_prev_taken)
    return;
  s_prev_taken = true;
  memcpy(&s_prev, &s_rec, sizeof(Record));
  s_prev.ctx[sizeof(s_prev.ctx) - 1] = 0;
  s_prev.tasks[sizeof(s_prev.tasks) - 1] = 0;
  s_prev.running[sizeof(s_prev.running) - 1] = 0;
  s_prev.component[sizeof(s_prev.component) - 1] = 0;
  s_rec.wdt_magic = 0;
  s_rec.ctx_magic = 0;
}

void set_ctx(const char *ctx) {
  take_prev();
  strncpy(s_rec.ctx, ctx, sizeof(s_rec.ctx) - 1);
  s_rec.ctx[sizeof(s_rec.ctx) - 1] = 0;
  s_rec.ctx_magic = CTX_MAGIC;
}

static void append(char *buf, size_t cap, const char *s) {
  size_t n = strnlen(buf, cap);
  while (*s && n + 1 < cap)
    buf[n++] = *s++;
  buf[n] = 0;
}

struct Collect {
  bool first;
};
static void on_wdt_msg(void *opaque, const char *msg) {
  auto *c = static_cast<Collect *>(opaque);
  if (c->first) {  // caption
    c->first = false;
    return;
  }
  if (strcmp(msg, "\n - ") == 0) {
    if (s_rec.tasks[0])
      append(s_rec.tasks, sizeof(s_rec.tasks), ", ");
    return;
  }
  append(s_rec.tasks, sizeof(s_rec.tasks), msg);
}

}  // namespace esphome::crash_info

// Called by the task watchdog ISR right before it panics.
extern "C" void esp_task_wdt_isr_user_handler(void) {
  using namespace esphome::crash_info;
  take_prev();
  s_rec.tasks[0] = 0;
  s_rec.running[0] = 0;
  s_rec.component[0] = 0;
  Collect c{true};
  esp_task_wdt_print_triggered_tasks(on_wdt_msg, &c, nullptr);
  for (int x = 0; x < CONFIG_FREERTOS_NUMBER_OF_CORES; x++) {
    if (x)
      append(s_rec.running, sizeof(s_rec.running), ", ");
    append(s_rec.running, sizeof(s_rec.running), pcTaskGetName(xTaskGetCurrentTaskHandleForCore(x)));
  }
  esphome::Component *comp = esphome::App.get_current_component();
  if (comp != nullptr)
    append(s_rec.component, sizeof(s_rec.component), LOG_STR_ARG(comp->get_component_log_str()));
  s_rec.uptime_s = (uint32_t) (esp_timer_get_time() / 1000000);
  s_rec.wdt_magic = WDT_MAGIC;
}

namespace esphome::crash_info {

static const char *reason_text(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON:
      return "Stromversorgung";
    case ESP_RST_SW:
      return "Neustart (Software)";
    case ESP_RST_PANIC:
      return "Absturz";
    case ESP_RST_INT_WDT:
      return "Interrupt-Watchdog";
    case ESP_RST_TASK_WDT:
      return "Task-Watchdog";
    case ESP_RST_WDT:
      return "Watchdog";
    case ESP_RST_BROWNOUT:
      return "Unterspannung";
    case ESP_RST_DEEPSLEEP:
      return "Tiefschlaf";
    case ESP_RST_EXT:
      return "Reset-Taste";
    default:
      return "unbekannt";
  }
}

void CrashInfo::setup() {
  take_prev();
  esp_reset_reason_t r = esp_reset_reason();
  this->crashed_ = r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT;
  std::string s = reason_text(r);
  if (this->crashed_) {
    bool wdt = s_prev.wdt_magic == WDT_MAGIC;
    if (wdt) {
      s = "Task-Watchdog";
      if (s_prev.tasks[0])
        s += std::string(", hing: ") + s_prev.tasks;
      if (s_prev.component[0])
        s += std::string(", in ") + s_prev.component;
      char b[24];
      snprintf(b, sizeof(b), ", nach %u min", (unsigned) (s_prev.uptime_s / 60));
      s += b;
    }
    if (s_prev.ctx_magic == CTX_MAGIC && s_prev.ctx[0])
      s += std::string(", zuletzt: ") + s_prev.ctx;
    if (this->has_coredump())
      s += ", Coredump vorhanden";
  }
  this->summary_ = s;
}

void CrashInfo::dump_config() {
  ESP_LOGCONFIG(TAG, "Last boot ended: %s", this->summary_.c_str());
  if (s_prev.wdt_magic == WDT_MAGIC)
    ESP_LOGCONFIG(TAG, "  running: %s", s_prev.running);
}

bool CrashInfo::has_coredump() const {
#ifdef USE_CRASH_INFO_COREDUMP
  return esp_core_dump_image_check() == ESP_OK;
#else
  return false;
#endif
}

void CrashInfo::erase_coredump() {
#ifdef USE_CRASH_INFO_COREDUMP
  esp_core_dump_image_erase();
#endif
}

}  // namespace esphome::crash_info
