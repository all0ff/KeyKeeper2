#include "settings/settings.hpp"

#include "event_bus/event_bus.hpp"
#include "storage/nvs_storage.hpp"
#include "storage/storage.hpp"

#include "esp_log.h"

namespace settings {

namespace {

constexpr char TAG[] = "settings";
constexpr char NVS_NAMESPACE[] = "settings";

bool initialized = false;
AllSettings current{};

template <typename T>
bool load_section(const char* key, T& out)
{
    T temp{};
    size_t actual_len = sizeof(temp);

    if (!storage::nvs::get_blob(NVS_NAMESPACE, key, &temp, actual_len)) {
        return false; // not present yet (first boot) -- caller keeps the default
    }

    if (actual_len != sizeof(T)) {
        // A firmware update changed this section's layout since it was
        // last saved. Falling back to defaults is safer than
        // reinterpreting a differently-shaped blob as T.
        ESP_LOGW(TAG, "Section '%s' size mismatch (stored %u, expected %u) -- using defaults",
                 key, static_cast<unsigned>(actual_len), static_cast<unsigned>(sizeof(T)));
        return false;
    }

    out = temp;
    return true;
}

template <typename T>
bool save_section(const char* key, const T& value)
{
    return storage::nvs::set_blob(NVS_NAMESPACE, key, &value, sizeof(value));
}

void publish_changed(Section section)
{
    if (!event_bus::is_initialized()) {
        return; // fine to skip -- nothing has subscribed yet this early in boot
    }

    event_bus::Payload payload{};
    payload.u32 = static_cast<uint32_t>(section);
    event_bus::publish(event_bus::Category::System,
                        static_cast<uint32_t>(event_bus::SystemEventId::SettingsChanged),
                        payload);
}

} // namespace

bool init()
{
    if (initialized) {
        ESP_LOGW(TAG, "init() called more than once, ignoring");
        return true;
    }

    if (!storage::is_initialized()) {
        ESP_LOGE(TAG, "storage::init() must succeed before settings::init()");
        return false;
    }

    if (!load_section("general", current.general)) {
        ESP_LOGI(TAG, "Using default General settings");
    }
    if (!load_section("usb", current.usb)) {
        ESP_LOGI(TAG, "Using default USB settings");
    }
    if (!load_section("security", current.security)) {
        ESP_LOGI(TAG, "Using default Security settings");
    }
    if (!load_section("gui", current.gui)) {
        ESP_LOGI(TAG, "Using default GUI settings");
    }

    initialized = true;
    ESP_LOGI(TAG, "Settings initialized (brightness=%u%%, pin_length=%u, language=%d)",
             static_cast<unsigned>(current.general.display_brightness),
             static_cast<unsigned>(current.security.pin_length),
             static_cast<int>(current.general.language));

    return true;
}

bool is_initialized()
{
    return initialized;
}

const AllSettings& all()
{
    return current;
}

bool set_general(const GeneralSettings& value)
{
    if (!initialized) {
        return false;
    }
    if (!save_section("general", value)) {
        ESP_LOGE(TAG, "Failed to persist General settings");
        return false;
    }
    current.general = value;
    publish_changed(Section::General);
    return true;
}

bool set_usb(const UsbSettings& value)
{
    if (!initialized) {
        return false;
    }
    if (!save_section("usb", value)) {
        ESP_LOGE(TAG, "Failed to persist USB settings");
        return false;
    }
    current.usb = value;
    publish_changed(Section::Usb);
    return true;
}

bool set_security(const SecuritySettings& value)
{
    if (!initialized) {
        return false;
    }
    if (!save_section("security", value)) {
        ESP_LOGE(TAG, "Failed to persist Security settings");
        return false;
    }
    current.security = value;
    publish_changed(Section::Security);
    return true;
}

bool set_gui(const GuiSettings& value)
{
    if (!initialized) {
        return false;
    }
    if (!save_section("gui", value)) {
        ESP_LOGE(TAG, "Failed to persist GUI settings");
        return false;
    }
    current.gui = value;
    publish_changed(Section::Gui);
    return true;
}

bool reset_to_defaults()
{
    if (!initialized) {
        return false;
    }

    bool ok = true;
    if (!set_general(GeneralSettings{})) ok = false;
    if (!set_usb(UsbSettings{})) ok = false;
    if (!set_security(SecuritySettings{})) ok = false;
    if (!set_gui(GuiSettings{})) ok = false;

    return ok;
}

} // namespace settings
