#pragma once

#include <cstdint>
#include <unordered_set>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/uart/uart.h"
#include "protocol.h"
#include "units.h"

using namespace std;
using namespace esphome::uart;

namespace esphome {
namespace amplifier_serial {

const uint32_t FRAME_TIMEOUT_MS = 3 * units::SECOND;

class SerialTransport : public UARTDevice {
 public:
  SerialTransport(uart::UARTComponent *parent) : UARTDevice(parent) {}

  void setup();
  void loop();

  bool send_command(Command command_code, const vector<uint8_t>& data, uint8_t zone=1);
  inline bool send_command(Command command_code, uint8_t data, uint8_t zone=1) { return this->send_command(command_code, vector<uint8_t>{data}, zone); }
  void set_frame_handler(function<void(const ResponseFrame&)> handler) { frame_handler_.set_frame_handler(handler); }

 protected:
  FrameHandler frame_handler_;
  unordered_set<Command> unsupported_commands_;
  uint32_t last_byte_time_ = 0;

  void read_available_bytes();
  bool handle_frame(const ResponseFrame& frame);
};

}  // namespace amplifier_serial
}  // namespace esphome
