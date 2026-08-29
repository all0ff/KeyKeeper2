#include "input/encoder.hpp"

#include "esp_log.h"

namespace input {

namespace {

constexpr char TAG[] = "input.encoder";

// PCNT counter range. Polled frequently enough (input task runs every
// few ms) that this never gets close to overflowing even at an
// unreasonably fast spin speed.
constexpr int16_t PCNT_HIGH_LIMIT = 4000;
constexpr int16_t PCNT_LOW_LIMIT = -4000;

} // namespace

bool Encoder::init(const Config& cfg)
{
    if (unit_ != nullptr) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    steps_per_notch_ = (cfg.steps_per_notch == 0) ? 1 : cfg.steps_per_notch;
    invert_ = cfg.invert;
    residual_edges_ = 0;

    pcnt_unit_config_t unit_config{};
    unit_config.high_limit = PCNT_HIGH_LIMIT;
    unit_config.low_limit = PCNT_LOW_LIMIT;
    unit_config.flags.accum_count = true;

    esp_err_t err = pcnt_new_unit(&unit_config, &unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_new_unit failed: %s", esp_err_to_name(err));
        return false;
    }

    pcnt_glitch_filter_config_t filter_config{};
    filter_config.max_glitch_ns = cfg.glitch_filter_ns;
    err = pcnt_unit_set_glitch_filter(unit_, &filter_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_unit_set_glitch_filter failed: %s", esp_err_to_name(err));
        return false;
    }

    // Channel A: counts on every A edge, direction decided by B's level.
    pcnt_chan_config_t chan_a_config{};
    chan_a_config.edge_gpio_num = cfg.pin_a;
    chan_a_config.level_gpio_num = cfg.pin_b;

    err = pcnt_new_channel(unit_, &chan_a_config, &chan_a_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_new_channel (A) failed: %s", esp_err_to_name(err));
        return false;
    }

    pcnt_channel_set_edge_action(
        chan_a_, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
    pcnt_channel_set_level_action(
        chan_a_, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    // Channel B: counts on every B edge, direction decided by A's level.
    // Mirrors channel A so that A-then-B vs B-then-A produce opposite
    // signs, giving standard x4 quadrature decoding.
    pcnt_chan_config_t chan_b_config{};
    chan_b_config.edge_gpio_num = cfg.pin_b;
    chan_b_config.level_gpio_num = cfg.pin_a;

    err = pcnt_new_channel(unit_, &chan_b_config, &chan_b_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_new_channel (B) failed: %s", esp_err_to_name(err));
        return false;
    }

    pcnt_channel_set_edge_action(
        chan_b_, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE);
    pcnt_channel_set_level_action(
        chan_b_, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);

    err = pcnt_unit_enable(unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_unit_enable failed: %s", esp_err_to_name(err));
        return false;
    }

    pcnt_unit_clear_count(unit_);
    err = pcnt_unit_start(unit_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "pcnt_unit_start failed: %s", esp_err_to_name(err));
        return false;
    }

    ESP_LOGI(TAG, "Encoder ready: A=GPIO%d B=GPIO%d, %u edges/notch, invert=%s",
             static_cast<int>(cfg.pin_a), static_cast<int>(cfg.pin_b),
             steps_per_notch_, invert_ ? "yes" : "no");

    return true;
}

Encoder::~Encoder()
{
    if (unit_ != nullptr) {
        pcnt_unit_stop(unit_);
        if (chan_a_ != nullptr) {
            pcnt_del_channel(chan_a_);
        }
        if (chan_b_ != nullptr) {
            pcnt_del_channel(chan_b_);
        }
        pcnt_unit_disable(unit_);
        pcnt_del_unit(unit_);
    }
}

int32_t Encoder::take_delta()
{
    if (unit_ == nullptr) {
        return 0;
    }

    int pulse_count = 0;
    pcnt_unit_get_count(unit_, &pulse_count);
    pcnt_unit_clear_count(unit_);

    residual_edges_ += pulse_count;

    const int32_t notches = residual_edges_ / steps_per_notch_;
    residual_edges_ -= notches * steps_per_notch_;

    return invert_ ? -notches : notches;
}

} // namespace input
