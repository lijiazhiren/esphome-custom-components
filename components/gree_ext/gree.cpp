#include "gree.h"
#include "esphome/components/remote_base/remote_base.h"

namespace esphome {
namespace gree_ext {

static const char *const TAG = "gree.climate";

uint8_t calculate_checksum(const uint8_t (&data)[8]) {
  return ((((data[0] & 0x0F) + (data[1] & 0x0F) + (data[2] & 0x0F) + (data[3] & 0x0F) + ((data[4] & 0xF0) >> 4) +
            ((data[5] & 0xF0) >> 4) + ((data[6] & 0xF0) >> 4) + 0x0A) &
           0x0F)
          << 4);
}

void GreeClimate::set_transmit_data_(remote_base::RemoteTransmitData *data, const uint8_t (&remote_state)[8],
                                     const uint32_t space) {
  data->mark(GREE_HEADER_MARK);
  data->space(GREE_HEADER_SPACE);

  for (int i = 0; i < 4; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(GREE_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->mark(GREE_BIT_MARK);
  data->space(GREE_ZERO_SPACE);
  data->mark(GREE_BIT_MARK);
  data->space(GREE_ONE_SPACE);
  data->mark(GREE_BIT_MARK);
  data->space(GREE_ZERO_SPACE);

  data->mark(GREE_BIT_MARK);
  data->space(GREE_MESSAGE_SPACE);

  for (int i = 4; i < 8; i++) {
    for (uint8_t mask = 1; mask > 0; mask <<= 1) {  // iterate through bit mask
      data->mark(GREE_BIT_MARK);
      bool bit = remote_state[i] & mask;
      data->space(bit ? GREE_ONE_SPACE : GREE_ZERO_SPACE);
    }
  }

  data->mark(GREE_BIT_MARK);
  data->space(space);
}

void GreeClimate::set_model(Model model) {
  this->presets_.insert(climate::CLIMATE_PRESET_NONE);
  this->presets_.insert(climate::CLIMATE_PRESET_SLEEP);
  this->presets_.insert(climate::CLIMATE_PRESET_BOOST);
  this->presets_.insert(climate::CLIMATE_PRESET_ECO);

  if (this->supports_horizontal_swing_ || this->supports_vertical_swing_) {
    this->swing_modes_.insert(climate::CLIMATE_SWING_OFF);
  }
  if (this->supports_horizontal_swing_) {
    this->swing_modes_.insert(climate::CLIMATE_SWING_HORIZONTAL);
  }
  if (this->supports_vertical_swing_) {
    this->swing_modes_.insert(climate::CLIMATE_SWING_VERTICAL);
  }
  if (this->supports_horizontal_swing_ && this->supports_vertical_swing_) {
    this->swing_modes_.insert(climate::CLIMATE_SWING_BOTH);
  }

  this->model_ = model;
}

void GreeClimate::set_mode_bit(uint8_t bit_mask, bool enabled) {
  if (enabled) {
    this->mode_bits_ |= bit_mask;
  } else {
    this->mode_bits_ &= ~bit_mask;
  }
  this->transmit_state();
}

void GreeClimate::transmit_state() {
  uint8_t remote_state_a[8] = {0x00, 0x00, 0x00, GREE_MESSAGE_A, 0x00, 0x00, 0x00, 0x00};
  uint8_t remote_state_b[8] = {0x00, 0x00, 0x00, GREE_MESSAGE_B, 0x00, 0x00, 0x00, 0x00};
  // remote_state_b[4] bits 7 echo
  // remote_state_b[6] 0x00 ~ 0x50: Fan auto - Fan 5

  remote_state_a[0] = remote_state_b[0] = this->fan_speed_() | this->operation_mode_();
  remote_state_a[1] = remote_state_b[1] = this->temperature_() & 0x0F;
  // only LIGHT, XFAN (0xF0 for all),  bits 4..7 TURBO, LIGHT, HEALTH,  XFAN
  remote_state_a[2] = remote_state_b[2] = this->mode_bits_ & 0xA0;
  remote_state_b[6] |= this->fan_speed_();

  remote_state_a[4] = this->vertical_swing_() | (this->horizontal_swing_() << 4);
  if (this->vertical_swing_() == GREE_VDIR_SWING || this->horizontal_swing_() == GREE_HDIR_SWING) {
    remote_state_a[0] = remote_state_b[0] |= (1 << 6);
  }

  switch (this->preset.value()) {
    case climate::CLIMATE_PRESET_SLEEP:
      remote_state_a[0] = remote_state_b[0] |= GREE_PRESET_SLEEP_BIT;
      break;
    case climate::CLIMATE_PRESET_BOOST:
      remote_state_a[2] = remote_state_b[2] |= GREE_FAN_TURBO_BIT;
      break;
    case climate::CLIMATE_PRESET_ECO:
      remote_state_b[4] |= GREE_PRESET_ECO_BIT;
      break;
  }

  remote_state_a[7] = calculate_checksum(remote_state_a);
  remote_state_b[7] = calculate_checksum(remote_state_b);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();
  data->set_carrier_frequency(GREE_IR_FREQUENCY);

  this->set_transmit_data_(data, remote_state_a, GREE_MESSAGE_SPLIT);
  this->set_transmit_data_(data, remote_state_b, 0);

  transmit.perform();
}

uint8_t GreeClimate::operation_mode_() {
  uint8_t operating_mode = GREE_MODE_ON;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      operating_mode |= GREE_MODE_COOL;
      break;
    case climate::CLIMATE_MODE_DRY:
      operating_mode |= GREE_MODE_DRY;
      break;
    case climate::CLIMATE_MODE_HEAT:
      operating_mode |= GREE_MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      operating_mode |= GREE_MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      operating_mode |= GREE_MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      operating_mode = GREE_MODE_OFF;
      break;
  }

  return operating_mode;
}

uint8_t GreeClimate::fan_speed_() {
  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_LOW:
      return GREE_FAN_1;
    case climate::CLIMATE_FAN_MEDIUM:
      return GREE_FAN_2;
    case climate::CLIMATE_FAN_HIGH:
      return GREE_FAN_3;
    case climate::CLIMATE_FAN_AUTO:
    default:
      return GREE_FAN_AUTO;
  }
}

uint8_t GreeClimate::horizontal_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_HORIZONTAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_HDIR_SWING;
    default:
      return GREE_HDIR_MANUAL;
  }
}

uint8_t GreeClimate::vertical_swing_() {
  switch (this->swing_mode) {
    case climate::CLIMATE_SWING_VERTICAL:
    case climate::CLIMATE_SWING_BOTH:
      return GREE_VDIR_SWING;
    default:
      return GREE_VDIR_MANUAL;
  }
}

uint8_t GreeClimate::temperature_() {
  return (uint8_t) roundf(clamp<float>(this->target_temperature, GREE_TEMP_MIN, GREE_TEMP_MAX));
}

bool GreeClimate::decode_status_(remote_base::RemoteReceiveData &data) {
  uint8_t remote_state[8] = {0};

  if (!data.expect_item(GREE_HEADER_MARK, GREE_HEADER_SPACE)) {
    return false;
  }

  for (uint8_t byte_idx = 0; byte_idx < 4; byte_idx++) {
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
        byte |= (1 << bit);
      } else if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
        ESP_LOGD(TAG, "decode - byte[%d]bit[%d]error", byte_idx, bit);
        return false;
      }
    }
    remote_state[byte_idx] = byte;
  }

  if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE) ||
      !data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE) || !data.expect_item(GREE_BIT_MARK, GREE_MESSAGE_SPACE)) {
    return false;
  }

  for (uint8_t byte_idx = 4; byte_idx < 8; byte_idx++) {
    uint8_t byte = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      if (data.expect_item(GREE_BIT_MARK, GREE_ONE_SPACE)) {
        byte |= (1 << bit);
      } else if (!data.expect_item(GREE_BIT_MARK, GREE_ZERO_SPACE)) {
        ESP_LOGD(TAG, "decode - byte[%d]bit[%d]error", byte_idx + 4, bit);
        return false;
      }
    }
    remote_state[byte_idx] = byte;
  }

  // 打印接收到的原始数据用于调试
  ESP_LOGD(TAG, "received: %02X %02X %02X %02X %02X %02X %02X %02X", remote_state[0], remote_state[1], remote_state[2],
           remote_state[3], remote_state[4], remote_state[5], remote_state[6], remote_state[7]);

  // 验证校验和
  uint8_t received_checksum = remote_state[7] & 0xF0;
  uint8_t calculated_checksum = calculate_checksum(remote_state);

  if (received_checksum != calculated_checksum) {
    ESP_LOGD(TAG, "checksum error - recv:0x%02X calc:0x%02X", received_checksum, calculated_checksum);
    return false;
  }

  this->mode_bits_ = remote_state[2] & 0xA0;  // only light, xfan (0xF0 for all)

  // 解析工作模式
  uint8_t mode_byte = remote_state[0];
  if ((mode_byte & GREE_MODE_ON) == 0) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else {
    // 解析运行模式
    uint8_t mode = mode_byte & 0x07;  // 提取位1-3
    switch (mode) {
      case GREE_MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case GREE_MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case GREE_MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case GREE_MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      case GREE_MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      default:
        ESP_LOGW(TAG, "unkown mode: 0x%02X", mode);
        break;
    }
  }

  // 解析目标温度
  uint8_t temp_value = remote_state[1] & 0x0F | 0x10;
  if (temp_value < GREE_TEMP_MIN || temp_value > GREE_TEMP_MAX) {
    ESP_LOGW(TAG, "temperature out of range: %d", temp_value);
  } else {
    this->target_temperature = temp_value;
  }

  uint8_t fan_speed = remote_state[0] & 0x30;
  switch (fan_speed) {
    case GREE_FAN_AUTO:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
    case GREE_FAN_1:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case GREE_FAN_2:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case GREE_FAN_3:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    default:
      ESP_LOGW(TAG, "unkown fan speed: 0x%02X", fan_speed);
      break;
  }

  if (remote_state[3] == GREE_MESSAGE_A) {
    // 解析导风板模式
    bool vertical_swing = ((remote_state[4] & 0x01) != 0);
    bool horizontal_swing = ((remote_state[4] & 0x10) != 0);

    // 根据摆动状态设置swing_mode
    if (vertical_swing && horizontal_swing) {
      this->swing_mode = climate::CLIMATE_SWING_BOTH;
    } else if (vertical_swing) {
      this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
    } else if (horizontal_swing) {
      this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
    } else {
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    }
  }

  // 解析特殊预设模式（仅适用于支持这些功能的型号）
  if (remote_state[0] & GREE_PRESET_SLEEP_BIT) {
    this->preset = climate::CLIMATE_PRESET_SLEEP;
  } else if (remote_state[2] & GREE_FAN_TURBO_BIT) {
    this->preset = climate::CLIMATE_PRESET_BOOST;
  } else if (remote_state[3] == GREE_MESSAGE_B && (remote_state[4] & GREE_PRESET_ECO_BIT)) {
    this->preset = climate::CLIMATE_PRESET_ECO;
  } else {
    this->preset = climate::CLIMATE_PRESET_NONE;
  }

  return true;
}

bool GreeClimate::on_receive(remote_base::RemoteReceiveData data) {
  bool success = false;

  while (data.is_valid()) {
    if (this->decode_status_(data)) {
      success = true;
    }
    data.advance(2);
  }

  if (!success) {
    return success;
  }

  this->publish_state();

  for (auto *listener : this->listeners_) {
    listener->on_update(this->mode_bits_);
  }

  return true;
}

}  // namespace gree_ext
}  // namespace esphome
