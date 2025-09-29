#include "protocol.h"
#include "esphome/core/log.h"

namespace esphome {
namespace amplifier_serial {

static const char *TAG = "amplifier_serial.protocol";

const char* command_to_string(Command command_code) {
  switch (command_code) {
    case Command::POWER:
      return "Power";
    case Command::DISPLAY_BRIGHTNESS:
      return "Display Brightness";
    case Command::HEADPHONES:
      return "Headphones";
    case Command::SOFTWARE_VERSION:
      return "Software Version";
    case Command::FACTORY_RESET:
      return "Factory Reset";
    case Command::IR_COMMAND:
      return "RC5 IR Command";
    case Command::VOLUME:
      return "Volume";
    case Command::MUTE:
      return "Mute";
    case Command::DIRECT_MODE:
      return "Direct Mode";
    case Command::NETWORK_PLAYBACK:
      return "Network Playback";
    case Command::INPUT_SOURCE:
      return "Input Source";
    case Command::HEADPHONE_OVERRIDE:
      return "Headphone Override";
    case Command::HEARTBEAT:
      return "Heartbeat";
    case Command::REBOOT:
      return "Reboot";
    case Command::NETWORK_INFO:
      return "Network Info";
    case Command::ROOM_EQ_NAMES:
      return "Room EQ Names";
    case Command::ROOM_EQ:
      return "Room EQ";
    case Command::BALANCE:
      return "Balance";
    case Command::AUDIO_SAMPLE_RATE:
      return "Audio Sample Rate";
    case Command::DC_OFFSET:
      return "DC Offset";
    case Command::SHORT_CIRCUIT_STATUS:
      return "Short Circuit Status";
    case Command::FRIENDLY_NAME:
      return "Friendly Name";
    case Command::IP_ADDRESS:
      return "IP Address";
    case Command::STANDBY_TIMEOUT_STATUS:
      return "Standby Timeout Status";
    case Command::LIFTER_TEMPERATURE:
      return "Lifter Temperature";
    case Command::OUTPUT_TEMPERATURE:
      return "Output Temperature";
    case Command::STANDBY_TIMEOUT:
      return "Standby Timeout";
    case Command::PHONO_INPUT_TYPE:
      return "Phono Input Type";
    case Command::INPUT_DETECT:
      return "Input Detect";
    case Command::PROCESSOR_MODE_INPUT:
      return "Processor Mode Input";
    case Command::PROCESSOR_MODE_VOLUME:
      return "Processor Mode Volume";
    case Command::SYSTEM_STATUS:
      return "System Status";
    case Command::SYSTEM_MODEL:
      return "System Model";
    case Command::DAC_FILTER:
      return "DAC Filter";
    case Command::NOW_PLAYING_INFO:
      return "Now Playing Info";
    case Command::MAX_TURN_ON_VOLUME:
      return "Max Turn On Volume";
    case Command::MAX_VOLUME:
      return "Max Volume";
    case Command::MAX_STREAMING_VOLUME:
      return "Max Streaming Volume";
    case Command::DARK_MODE:
      return "Dark Mode";
    case Command::SERVICE_DATA:
      return "Service Data";
    default:
      return "Unknown command code";
  }
}

const char* answer_to_string(Answer answer_code) {
  switch (answer_code) {
    case Answer::STATUS_UPDATE:
      return "Status update";
    case Answer::ZONE_INVALID:
      return "Zone invalid";
    case Answer::COMMAND_INVALID:
      return "Command not recognized";
    case Answer::PARAMETER_UNRECOGNIZED:
      return "Parameter not recognized";
    case Answer::COMMAND_INVALID_TMP:
      return "Command invalid at this time";
    case Answer::DATA_LENGTH_INVALID:
      return "Invalid data length";
    default:
      return "Unknown answer code";
  }
}

const char* source_to_string(uint8_t source) {
  switch (source) {
    case 0x01:
      return "Phono";
    case 0x02:
      return "AUX";
    case 0x03:
      return "PVR";
    case 0x04:
      return "AV";
    case 0x05:
      return "STB";
    case 0x06:
      return "CD";
    case 0x07:
      return "BD";
    case 0x08:
      return "SAT";
    case 0x09:
      return "GAME";
    case 0x0B:
      return "NET/USB";
    default:
      return "Unknown source";
  }
}

uint32_t standby_timeout_to_ms(uint8_t timeout_value) {
  switch (timeout_value) {
    case 0x00:
      return 0;  // Off
    case 0x01:
      return 20 * 60 * 1000;  // 20 minutes
    case 0x02:
      return 60 * 60 * 1000;  // 1 hour
    case 0x03:
      return 2 * 60 * 60 * 1000;  // 2 hours
    case 0x04:
      return 4 * 60 * 60 * 1000;  // 4 hours
    default:
      return 0;  // Unknown value, return 0 (Off)
  }
}

FrameHandler::FrameHandler(std::function<void(const ResponseFrame&)> frame_handler)
  : frame_handler_(frame_handler) {}

void FrameHandler::deserialize_frame_byte(uint8_t byte) {
  ESP_LOGVV(TAG, "State: %d, Byte: %02X", static_cast<int>(this->state_), byte);
  switch (this->state_) {
    case State::READ_START:
      if (byte == START_CHAR) {
        this->current_frame_ = ResponseFrame();
        this->state_ = State::READ_ZONE;
      }
      break;

    case State::READ_ZONE:
      this->current_frame_.zone = byte;
      this->state_ = State::READ_COMMAND;
      break;

    case State::READ_COMMAND:
      this->current_frame_.command_code = static_cast<Command>(byte);
      this->state_ = State::READ_ANSWER;
      break;

    case State::READ_ANSWER:
      this->current_frame_.answer_code = static_cast<Answer>(byte);
      this->state_ = State::READ_LENGTH;
      break;

    case State::READ_LENGTH:
      this->current_frame_.data_length = byte;
      this->current_frame_.data.clear();
      if (this->current_frame_.data_length == 0) {
        this->state_ = State::READ_END;
      } else {
        this->current_frame_.data.reserve(this->current_frame_.data_length);
        this->state_ = State::READ_DATA;
      }
      break;

    case State::READ_DATA:
      this->current_frame_.data.push_back(byte);
      if (this->current_frame_.data.size() >= this->current_frame_.data_length) {
        this->state_ = State::READ_END;
      }
      break;

    case State::READ_END:
      if (byte == END_CHAR) {
        if (this->frame_handler_) {
          this->frame_handler_(this->current_frame_);
        }
      } else {
        ESP_LOGW(TAG, "Invalid frame received");
      }
      this->state_ = State::READ_START;
      break;
  }
}

void FrameHandler::reset_state() {
  this->state_ = State::READ_START;
  this->current_frame_ = ResponseFrame();
}

std::vector<uint8_t> FrameHandler::serialize_frame(const RequestFrame& frame) {
  std::vector<uint8_t> data;
  data.reserve(5 + frame.data.size());
  data.push_back(START_CHAR);
  data.push_back(frame.zone);
  data.push_back(static_cast<uint8_t>(frame.command_code));
  data.push_back(frame.data.size());
  data.insert(data.end(), frame.data.begin(), frame.data.end());
  data.push_back(END_CHAR);
  ESP_LOGVV(TAG, "Serialized frame: %s", to_hex_string(data).c_str());
  return data;
}

const std::string to_hex_string(const std::vector<uint8_t> &data) {
  std::string result;
  result.reserve(data.size() * 2);
  static const char hex_chars[] = "0123456789ABCDEF";
  for (auto byte : data) {
    result.push_back(hex_chars[byte >> 4]);
    result.push_back(hex_chars[byte & 0xF]);
  }
  return result;
}

}  // namespace amplifier_serial
}  // namespace esphome
