#pragma once

#include <stdint.h>
#include <vector>
#include <functional>

// Protocol documentation:
// https://www.jbl.com/on/demandware.static/-/Sites-masterCatalog_Harman/default/dwa1e10b13/pdfs/SH320E_RS232_SA750_iss1.pdf
// https://support.jbl.com/on/demandware.static/-/Sites-masterCatalog_Harman/default/dw201f6294/pdfs/RS232_SDA7120_SDA2200_SH307E_2.pdf
// https://www.arcam.co.uk/ugc/tor/SA20/Custom%20Installation%20Notes/SH277E_RS232_SA10_SA20_B.pdf
// https://www.arcam.co.uk/ugc/tor/ST60/Custom%20Installation%20Notes/SH309_RS232_ST60_C.pdf

namespace esphome {
namespace amplifier_serial {

enum class Command : uint8_t {
  POWER                  = 0x00, // Set or request the stand-by state of a zone
  DISPLAY_BRIGHTNESS     = 0x01, // Set or request the brightness of the front panel display
  HEADPHONES             = 0x02, // Determine whether headphones are connected
  SOFTWARE_VERSION       = 0x04, // Request the firmware version
  FACTORY_RESET          = 0x05, // Reset the unit to factory defaults
  IR_COMMAND             = 0x08, // Simulate RC5 IR command
  VOLUME                 = 0x0D, // Set or request the volume of a zone
  MUTE                   = 0x0E, // Set or request the mute status of the output
  DIRECT_MODE            = 0x0F, // Set or request the analogue input direct mode of an input
  NETWORK_PLAYBACK       = 0x1C, // Request network playback status
  INPUT_SOURCE           = 0x1D, // Set or request the current input source
  HEADPHONE_OVERRIDE     = 0x1F, // Set or request the headphones mute relays (does not zero the volume)
  HEARTBEAT              = 0x25, // Heartbeat command to check unit is still connected and communicating (also resets the EuP standby timer)
  REBOOT                 = 0x26, // Forces a reboot of the unit
  NETWORK_INFO           = 0x30, // Request network info
  ROOM_EQ_NAMES          = 0x34, // Request Room EQ names
  ROOM_EQ                = 0x37, // Turn the room equalization system on/off
  BALANCE                = 0x3B, // Adjust the balance control
  AUDIO_SAMPLE_RATE      = 0x44, // Request the incoming audio sample rate
  DC_OFFSET              = 0x51, // Request the output DC offset status
  SHORT_CIRCUIT_STATUS   = 0x52, // Request the output short circuit status
  FRIENDLY_NAME          = 0x53, // Set or request the friendly name of the unit
  IP_ADDRESS             = 0x54, // Set or request the IP address of the unit
  STANDBY_TIMEOUT_STATUS = 0x55, // Request the time left (in minutes) until unit enters auto standby
  LIFTER_TEMPERATURE     = 0x56, // Request the temperature of the lifter
  OUTPUT_TEMPERATURE     = 0x57, // Request the temperature of the output stage
  STANDBY_TIMEOUT        = 0x58, // Set or request the time after witch unit will go into standby state due to no input signal
  PHONO_INPUT_TYPE       = 0x59, // Set or request Phono input type
  INPUT_DETECT           = 0x5A, // Request the status of the active input
  PROCESSOR_MODE_INPUT   = 0x5B, // Set or request processor mode on a certain inputs
  PROCESSOR_MODE_VOLUME  = 0x5C, // Set the processor mode volume
  SYSTEM_STATUS          = 0x5D, // Request the system status
  SYSTEM_MODEL           = 0x5E, // Request the system model
  DAC_FILTER             = 0x61, // Set or request the DAC filter, or Amplifier mode (SDA-2200 only)
  NOW_PLAYING_INFO       = 0x64, // Request the various now playing track details
  MAX_TURN_ON_VOLUME     = 0x65, // Set or request the maximum volume level at power up
  MAX_VOLUME             = 0x66, // Set or request the maximum volume level to prevent volume being accidentally set to full
  MAX_STREAMING_VOLUME   = 0x67, // Set or request the maximum volume level when playing back streamed content
  DARK_MODE              = 0x68, // Set or request the status of the dark mode function
  SERVICE_DATA           = 0xF3, // Shows the service data
};

enum class Answer : uint8_t {
  STATUS_UPDATE          = 0x00, // Status update
  ZONE_INVALID           = 0x82, // Zone invalid
  COMMAND_INVALID        = 0x83, // Command not recognized
  PARAMETER_UNRECOGNIZED = 0x84, // Parameter not recognized
  COMMAND_INVALID_TMP    = 0x85, // Command invalid at this time
  DATA_LENGTH_INVALID    = 0x86, // Invalid data length
};

const uint8_t STATUS_REQUEST = 0xF0;
const uint8_t START_CHAR = 0x21;
const uint8_t END_CHAR = 0x0D;

struct RequestFrame {
  uint8_t zone;
  Command command_code;
  std::vector<uint8_t> data;
};

struct ResponseFrame {
  uint8_t zone;
  Command command_code;
  Answer answer_code;
  uint8_t data_length;
  std::vector<uint8_t> data;
};

class FrameHandler {
public:
  FrameHandler() = default;
  FrameHandler(std::function<void(const ResponseFrame&)> frame_handler);
  void deserialize_frame_byte(uint8_t byte);
  std::vector<uint8_t> serialize_frame(const RequestFrame& frame);
  inline void set_frame_handler(std::function<void(const ResponseFrame&)> frame_handler) { frame_handler_ = frame_handler; }
  bool is_idle() const { return state_ == State::READ_START; }
  void reset_state();

private:
  enum class State {
    READ_START,
    READ_ZONE,
    READ_COMMAND,
    READ_ANSWER,
    READ_LENGTH,
    READ_DATA,
    READ_END
  };

  State state_ = State::READ_START;
  ResponseFrame current_frame_;
  std::function<void(const ResponseFrame&)> frame_handler_ = nullptr;
};

const char* command_to_string(Command command_code);
const char* answer_to_string(Answer answer_code);
const char* source_to_string(uint8_t source);
uint32_t standby_timeout_to_ms(uint8_t timeout_value);

const std::string to_hex_string(const std::vector<uint8_t> &data);

}  // namespace amplifier_serial
}  // namespace esphome
