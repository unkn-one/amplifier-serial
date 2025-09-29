#include "esphome/core/log.h"
#include "transport.h"

namespace esphome {
namespace amplifier_serial {

static const char *TAG = "amplifier_serial.transport";

void SerialTransport::setup() {
  // Any setup code for the transport layer
}

void SerialTransport::loop() {
  this->read_available_bytes();
}

void SerialTransport::read_available_bytes() {
  uint32_t current_time = millis();

  if (!this->frame_handler_.is_idle() && 
      (current_time - this->last_byte_time_ > FRAME_TIMEOUT_MS)) {
    ESP_LOGW(TAG, "Frame reading timeout");
    this->frame_handler_.reset_state();
  }

  while (this->available()) {
    this->last_byte_time_ = current_time;
    this->frame_handler_.deserialize_frame_byte(this->read());
  }
}

bool SerialTransport::send_command(Command command_code, const vector<uint8_t>& data, uint8_t zone) {
  if (unsupported_commands_.count(command_code)) {
    ESP_LOGD(TAG, "Not sending unsupported command: %s (%02X)", 
             command_to_string(command_code), static_cast<uint8_t>(command_code));
    return false;
  }
  
  RequestFrame frame {
    .zone = zone,
    .command_code = command_code,
    .data = data
  };

  ESP_LOGD(TAG, "Sending frame: %s (%02X), Data: %s, Zone: %d",
           command_to_string(frame.command_code), static_cast<uint8_t>(frame.command_code),
           to_hex_string(frame.data).c_str(), frame.zone);  

  this->write_array(this->frame_handler_.serialize_frame(frame));

  return true;
}

bool SerialTransport::handle_frame(const ResponseFrame& frame) {

  ESP_LOGD(TAG, "Received frame: %s (%02X), Data: %s, Zone: %d",
           command_to_string(frame.command_code), static_cast<uint8_t>(frame.command_code), 
           to_hex_string(frame.data).c_str(), frame.zone);

  if (frame.answer_code == Answer::COMMAND_INVALID) {
    ESP_LOGW(TAG, "Command not supported: %s (%02X) and will be ignored",
             command_to_string(frame.command_code), static_cast<uint8_t>(frame.command_code));
    this->unsupported_commands_.insert(frame.command_code);
    return false;
  }

  if (frame.answer_code != Answer::STATUS_UPDATE) {
    ESP_LOGW(TAG, "Error response: %s (%02X)", 
             answer_to_string(frame.answer_code), static_cast<uint8_t>(frame.answer_code));
    return false;
  }

  return true;
}

}  // namespace amplifier_serial
}  // namespace esphome
