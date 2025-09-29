#pragma once

#include <string>
#include <vector>
#include <queue>
#include <unordered_set>

#include "esphome.h"
#include "esphome/core/component.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "protocol.h"
#include "transport.h"

namespace esphome {
namespace amplifier_serial {

class AmplifierSerial : public SerialTransport, public PollingComponent, public media_player::MediaPlayer {
public:
  AmplifierSerial(uart::UARTComponent *parent);

  // PollingComponent implementations
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void update() override;
  void loop() override;

  // MediaPlayer implementations
  media_player::MediaPlayerTraits get_traits() override;
  void control(const media_player::MediaPlayerCall &call) override;
  bool is_muted() const override { return this->muted_; }

  void set_software_version_sensor(text_sensor::TextSensor *sensor) { this->software_version_sensor_ = sensor; }
  void set_max_volume_sensor(sensor::Sensor *sensor) { this->max_volume_sensor_ = sensor; }
  void set_max_streaming_volume_sensor(sensor::Sensor *sensor) { this->max_streaming_volume_sensor_ = sensor; }

protected:
  enum class State {
	POWERED_ON,
    UNDEFINED,
    UNAVAILABLE,
    UNINITIALIZED,
    INITIALIZING,
    IDLE,
    PLAYING,
  };

  State state_ = State::POWERED_ON;
  uint8_t max_volume_ = 99;
  bool muted_ = false;
  uint32_t standby_timeout_ms_ = 20 * 60 * 1000; // 20 minutes
  uint32_t last_active_time_ = 0;

  text_sensor::TextSensor *software_version_sensor_{nullptr};

  sensor::Sensor *max_volume_sensor_{nullptr};
  sensor::Sensor *max_streaming_volume_sensor_{nullptr};

  void handle_frame(const ResponseFrame& frame);
};

}  // namespace amplifier_serial
}  // namespace esphome
