#include <algorithm>
#include "esphome/core/log.h"
#include "device.h"


namespace esphome {
namespace amplifier_serial {

static const char *TAG = "amplifier_serial.device";

AmplifierSerial::AmplifierSerial(uart::UARTComponent *parent)
  : SerialTransport(parent), PollingComponent(POLLING_TIME), media_player::MediaPlayer() {
  set_frame_handler([this](const ResponseFrame& frame) {
    this->handle_frame(frame);
  });
}

void AmplifierSerial::setup() {
  SerialTransport::setup();
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  this->last_active_time_ = millis();

  // Register actions with the HA API
  this->register_service(&AmplifierSerial::on_turn_on, "turn_on");
  this->register_service(&AmplifierSerial::on_turn_off, "turn_off");
}

void AmplifierSerial::loop() {
  SerialTransport::loop();
}

void AmplifierSerial::dump_config() {
  ESP_LOGCONFIG(TAG, "Amplifier Serial:");
  ESP_LOGCONFIG(TAG, "  State: %s", state_to_string(this->state_));
  ESP_LOGCONFIG(TAG, "  Max Volume: %d", this->max_volume_);
  ESP_LOGCONFIG(TAG, "  Standby Timeout: %dmin", this->standby_timeout_ms_ / units::MINUTE);
  this->check_uart_settings(UART_SPEED);
}

void AmplifierSerial::update() {
  State prev_state = this->state_;
  uint32_t idle_time = millis() - this->last_active_time_;

  switch (this->state_) {
    case State::UNDEFINED:
      // Sending commands when device is powering on sometimes reboots it in service mode or sth
      if (idle_time < INIT_TIME) break;

      this->send_command(Command::POWER, STATUS_REQUEST);
      this->state_ = State::UNAVAILABLE;
      break;

    case State::UNAVAILABLE:
      // Do nothing, wait for power on frame
      // Sending commands when device is in standby sometimes causes the device to turn on
      // Also we don't know if standby communication is actually turned on in device settings
      break;

    case State::UNINITIALIZED:
      if (idle_time < INIT_TIME) break;

      this->send_command(Command::MAX_VOLUME, STATUS_REQUEST);
      this->send_command(Command::MAX_STREAMING_VOLUME, STATUS_REQUEST);
      this->send_command(Command::STANDBY_TIMEOUT, STATUS_REQUEST);
      // Only call System Status once, it takes a while to respond, and cannot be interrupted to successfully complete
      this->send_command(Command::SYSTEM_STATUS, STATUS_REQUEST);
      this->state_ = State::INITIALIZING;
      break;

    case State::INITIALIZING:
      // Do nothing, wait for system status frame to proceed
      break;

    case State::IDLE:
      if (this->standby_timeout_ms_ > 0 && idle_time > this->standby_timeout_ms_) {
        ESP_LOGI(TAG, "Turning off amplifier due to standby timeout");
        this->send_command(Command::POWER, 0x00);
        this->state_ = State::UNAVAILABLE;
        break;
      }
      else {
        ESP_LOGD(TAG, "Time till standby timeout: %.1fmin", static_cast<float>(this->standby_timeout_ms_ - idle_time) / units::MINUTE);
      }
      [[fallthrough]]; // We want the same status updates as in playing state

    case State::PLAYING:
      // Periodic updates seems to reset EuP standby timer
      this->send_command(Command::LIFTER_TEMPERATURE, STATUS_REQUEST);
      this->send_command(Command::OUTPUT_TEMPERATURE, STATUS_REQUEST);
      this->send_command(Command::INPUT_DETECT, STATUS_REQUEST);
      break;
  }

  if (prev_state != this->state_) {
    ESP_LOGD(TAG, "Device state changed: %s -> %s", state_to_string(prev_state), state_to_string(this->state_));
  }
}

void AmplifierSerial::handle_frame(const ResponseFrame& frame) {
  if (!SerialTransport::handle_frame(frame)) {
    return;
  }

  State prev_state = this->state_;

  switch (frame.command_code) {
    case Command::POWER:
      if (frame.data.size() >= 1) {
        uint8_t power_on = frame.data[0];
        if (power_on == 0x01) {
          this->last_active_time_ = millis();
          if (this->state_ <= State::UNAVAILABLE) {
            this->state_ = State::UNINITIALIZED;
          }
        } else if (power_on == 0x00) {
          this->state_ = State::UNAVAILABLE;
        }
        this->state = frame.data[0] == 0x01 ? media_player::MEDIA_PLAYER_STATE_IDLE : media_player::MEDIA_PLAYER_STATE_NONE;
      }
      break;
      
    case Command::SOFTWARE_VERSION:
      if (frame.data.size() >= 2 && software_version_sensor_ != nullptr) {
        software_version_sensor_->publish_state(std::to_string(frame.data[0]) + "." + std::to_string(frame.data[1]));
      }
      break;

    case Command::VOLUME:
      if (frame.data.size() >= 1) {
        this->volume = static_cast<float>(frame.data[0]) / this->max_volume_;
      }
      break;

    case Command::MUTE:
      if (frame.data.size() >= 1) {
        this->muted_ = frame.data[0] == 0x01;
      }
      break;

    case Command::INPUT_SOURCE:
      if (frame.data.size() >= 1) {
        ESP_LOGD(TAG, "Input source: %s", source_to_string(frame.data[0] & 0x0F));
        // this->source = input_sources[frame.data[0] & 0x0F];
      }
      break;

    case Command::STANDBY_TIMEOUT:
      if (frame.data.size() >= 1) {
        this->standby_timeout_ms_ = standby_timeout_to_ms(frame.data[0]);
        ESP_LOGD(TAG, "Standby timeout set to: %.1fmin", static_cast<float>(this->standby_timeout_ms_) / units::MINUTE);
      }
      break;

    case Command::INPUT_DETECT:
      if (frame.data.size() >= 1) {
        if (frame.data[0] == 0x01) {
          this->last_active_time_ = millis();
        }
        if (this->state_ > State::INITIALIZING) {
          this->state_ = frame.data[0] == 0x01 ? State::PLAYING : State::IDLE;
        }
        this->state = frame.data[0] == 0x01 ? media_player::MEDIA_PLAYER_STATE_PLAYING : media_player::MEDIA_PLAYER_STATE_IDLE;
      }
      break;

    case Command::SYSTEM_STATUS:
      if (frame.data.size() >= 1 && frame.data[0] == 0xF0) {
        this->state_ = State::IDLE; // Inilization done
      }
      break;

    case Command::SYSTEM_MODEL:
      if (frame.data.size() > 0) {
        std::string model_name(frame.data.begin(), frame.data.end());
        ESP_LOGD(TAG, "System model: %s", model_name.c_str());
        //system_model_sensor_->publish_state(model_name);
      }
      break;

    case Command::MAX_VOLUME:
      if (frame.data.size() >= 1 && max_volume_sensor_ != nullptr) {
        this->max_volume_ = std::min<uint8_t>(frame.data[0], 99);
        this->max_volume_sensor_->publish_state(this->max_volume_);
      }
      break;

    case Command::MAX_STREAMING_VOLUME:
      if (frame.data.size() >= 1 && max_streaming_volume_sensor_ != nullptr) {
        this->max_streaming_volume_sensor_->publish_state(frame.data[0]);
      }
      break;
      
    case Command::SERVICE_DATA:
      if (frame.data.size() >= 5) {
        uint8_t offset = 4;
        uint16_t f_type = frame.data[0] & frame.data[1] << 8;
        uint16_t c_type = frame.data[2] & frame.data[3] << 8;
        auto null_pos = std::find(frame.data.begin(), frame.data.end(), '\0');
        ESP_LOGD(TAG, "%04X %04X: %s", f_type, c_type, std::string(frame.data.begin() + offset, null_pos).c_str());
      }
      break;
    default:
      ESP_LOGD(TAG, "Unhandled command: %s (%02X)", command_to_string(frame.command_code), static_cast<uint8_t>(frame.command_code));
      break;
  }

  if (prev_state != this->state_) {
    ESP_LOGD(TAG, "Device state changed: %s -> %s", state_to_string(prev_state), state_to_string(this->state_));
  }

  this->publish_state(); // MediaPlayer state update
}

void AmplifierSerial::control(const media_player::MediaPlayerCall &call) {
  if (call.get_volume().has_value()) {
    float volume = *call.get_volume();
    uint8_t volume_byte = static_cast<uint8_t>(volume * this->max_volume_);
    this->send_command(Command::VOLUME, {volume_byte});
  }
  if (call.get_command().has_value()) {
    switch (*call.get_command()) {
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->send_command(Command::MUTE, {0x00});
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->send_command(Command::MUTE, {0x01});
        break;
      case media_player::MEDIA_PLAYER_COMMAND_TOGGLE:
        ESP_LOGD(TAG, "Media toggle");
        break;
      default:
        ESP_LOGD(TAG, "Unsupported command: %s", media_player::media_player_command_to_string(*call.get_command()));
        break;
    }
  }
}

media_player::MediaPlayerTraits AmplifierSerial::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(false);
  return traits;
};

void AmplifierSerial::on_turn_on() {
  if (this->state_ == State::UNAVAILABLE) {
    ESP_LOGD(TAG, "Turning amplifier on");
    this->send_command(Command::POWER, 0x01);
  }
}

void AmplifierSerial::on_turn_off() {
  if (this->state_ >= State::IDLE) {
    ESP_LOGD(TAG, "Turning amplifier off");
    this->send_command(Command::POWER, 0x00);
  }
}


const char* state_to_string(State state) {
  switch (state) {
    case State::UNDEFINED:
      return "Undefined";
    case State::UNAVAILABLE:
      return "Unavailable";
    case State::UNINITIALIZED:
      return "Uninitialized";
    case State::INITIALIZING:
      return "Initializing";
    case State::IDLE:
      return "Idle";
    case State::PLAYING:
      return "Playing";
    default:
      return "Unknown";
  }
}


}  // namespace amplifier_serial
}  // namespace esphome