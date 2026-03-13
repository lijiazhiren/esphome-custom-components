#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome::gree_ext {

// Values for GREE IR Controllers
// Temperature
static constexpr uint8_t GREE_TEMP_MIN = 16;  // Celsius
static constexpr uint8_t GREE_TEMP_MAX = 30;  // Celsius

// Modes
static constexpr uint8_t GREE_MODE_AUTO = 0x00;
static constexpr uint8_t GREE_MODE_COOL = 0x01;
static constexpr uint8_t GREE_MODE_HEAT = 0x04;
static constexpr uint8_t GREE_MODE_DRY = 0x02;
static constexpr uint8_t GREE_MODE_FAN = 0x03;

static constexpr uint8_t GREE_MODE_OFF = 0x00;
static constexpr uint8_t GREE_MODE_ON = 0x08;

// Fan Speed
static constexpr uint8_t GREE_FAN_AUTO = 0x00;
static constexpr uint8_t GREE_FAN_1 = 0x10;
static constexpr uint8_t GREE_FAN_2 = 0x20;
static constexpr uint8_t GREE_FAN_3 = 0x30;

// IR Transmission
static constexpr uint32_t GREE_IR_FREQUENCY = 38000;
static constexpr uint32_t GREE_HEADER_MARK = 9000;
static constexpr uint32_t GREE_HEADER_SPACE = 4500;
static constexpr uint32_t GREE_BIT_MARK = 620;
static constexpr uint32_t GREE_ONE_SPACE = 1600;
static constexpr uint32_t GREE_ZERO_SPACE = 540;
static constexpr uint32_t GREE_MESSAGE_SPACE = 19980;
static constexpr uint32_t GREE_MESSAGE_SPLIT = 7300;

// State Frame size
static constexpr uint8_t GREE_STATE_FRAME_SIZE = 8;

// Vertical air directions. Note that these cannot be set on all heat pumps
static constexpr uint8_t GREE_VDIR_MANUAL = 0x00;
static constexpr uint8_t GREE_VDIR_SWING = 0x01;

// Horizontal air directions. Note that these cannot be set on all heat pumps
static constexpr uint8_t GREE_HDIR_MANUAL = 0x00;
static constexpr uint8_t GREE_HDIR_SWING = 0x01;


static constexpr uint8_t GREE_FAN_TURBO_BIT = 0x10;
static constexpr uint8_t GREE_PRESET_SLEEP_BIT = 0x80;
static constexpr uint8_t GREE_PRESET_ECO_BIT = 0x80;

// Message type
static constexpr uint8_t GREE_MESSAGE_A = 0x50;
static constexpr uint8_t GREE_MESSAGE_B = 0x70;

// Model codes
enum Model { GREE_YAP0F };

class GreeModeBitListener {
 public:
  virtual void on_update(uint8_t mode_bits) = 0;
};


class GreeClimate : public climate_ir::ClimateIR {
 public:
  GreeClimate()
      : climate_ir::ClimateIR(GREE_TEMP_MIN, GREE_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {}) {}

  void set_model(Model model);
  void set_mode_bit(uint8_t bit_mask, bool enabled);
  void set_supports_horizontal_swing(bool supports_horizontal_swing) { this->supports_horizontal_swing_ = supports_horizontal_swing; }
  void set_supports_vertical_swing(bool supports_vertical_swing) { this->supports_vertical_swing_ = supports_vertical_swing; }
  void register_listener(GreeModeBitListener *listener) { this->listeners_.push_back(listener); }

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  uint8_t operation_mode_();
  uint8_t fan_speed_();
  uint8_t horizontal_swing_();
  uint8_t vertical_swing_();
  uint8_t temperature_();
  bool decode_status_(remote_base::RemoteReceiveData &data);
  void set_transmit_data_(remote_base::RemoteTransmitData *data, const uint8_t (&remote_state)[8], const uint32_t space);

  Model model_{};
  uint8_t mode_bits_{0};  // Combined mode bits for remote_state[2]
  std::vector<GreeModeBitListener *> listeners_;

  bool supports_horizontal_swing_{true};
  bool supports_vertical_swing_{true};
};

}  // namespace esphome::gree
