#pragma once

#include <cstdint>
#include <string>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/api/custom_api_device.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "protocol.h"
#include "transport.h"

namespace esphome {
namespace amplifier_serial {

enum class State {
  UNDEFINED,
  UNAVAILABLE,
  UNINITIALIZED,
  INITIALIZING,
  IDLE,
  PLAYING,
};

class AmplifierSerial : public SerialTransport, 
                        public PollingComponent, 
                        public media_player::MediaPlayer,
                        public api::CustomAPIDevice {
public:
  AmplifierSerial(uart::UARTComponent *parent);

  // PollingComponent implementations
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  void control(const media_player::MediaPlayerCall &call) override;
  bool is_muted() const override { return this->muted_; }
  bool is_on() const { return this->state_ >= State::IDLE; }

  void set_software_version_sensor(text_sensor::TextSensor *sensor) { this->software_version_sensor_ = sensor; }
  void set_max_volume_sensor(sensor::Sensor *sensor) { this->max_volume_sensor_ = sensor; }
  void set_max_streaming_volume_sensor(sensor::Sensor *sensor) { this->max_streaming_volume_sensor_ = sensor; }

protected:
  State state_ = State::UNDEFINED;
  uint8_t max_volume_ = 99;
  bool muted_ = false;
  uint32_t standby_timeout_ms_ = 20 * 60 * 1000; // 20 minutes
  uint32_t last_active_time_ = 0;
  uint32_t init_time = 6 * 1000;

  text_sensor::TextSensor *software_version_sensor_{nullptr};

  sensor::Sensor *max_volume_sensor_{nullptr};
  sensor::Sensor *max_streaming_volume_sensor_{nullptr};

  void handle_frame(const ResponseFrame& frame);

  void on_turn_on();
  void on_turn_off();
};

const char* state_to_string(State state);

}  // namespace amplifier_serial
}  // namespace esphome
