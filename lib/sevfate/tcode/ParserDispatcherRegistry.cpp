#include "ParserDispatcherRegistry.hpp"

#include <nlohmann/json.hpp>
#include <tcode/Messages.hpp>
#include <tcode/ParserDispatcher.hpp>
#include <tcode/Utils.hpp>
#include <utils/Trace.hpp>

#include <cstdint>
#include <string_view>
#include <variant>

namespace tcode {

Registry::Registry() = default;
Registry::~Registry() = default;

// Callback registering is protected by acquire_registry's _registry_mutex.
Registry::enumeration_callback_t Registry::register_enumeration_complete_callback(
     enumeration_callback_t&& cb) {
    return std::exchange(_cb_on_enumeration_complete, std::move(cb));
}
CommandEndpoint::callback_t CommandEndpoint::register_callback(callback_t&& cb) {
    return std::exchange(_cb_on_response, std::move(cb));
}
PropertyMetadata::callback_t PropertyMetadata::register_callback(callback_t&& cb) {
    return std::exchange(_cb_on_update, std::move(cb));
}

void PropertyMetadata::pend_set(std::string_view v) {
    _pending_data = std::string(v);
    _pending_set = true;
}
void PropertyMetadata::pend_set(std::string&& v) {
    _pending_data = std::move(v);
    _pending_set = true;
}
void PropertyMetadata::pend_set(const std::string& v) {
    _pending_data = v;
    _pending_set = true;
}
void PropertyMetadata::pend_set(nlohmann::json&& v) {
    _pending_data = std::move(v);
    _pending_set = true;
}
void PropertyMetadata::pend_set(const nlohmann::json& v) {
    _pending_data = v;
    _pending_set = true;
}

void PropertyMetadata::pend_current_update_interval(uint32_t interval) {
    _current_update_interval = interval;
    _pending_set_interval = true;
}

bool PropertyMetadata::has_pending_ops() const {
    return _pending_get || _pending_set || _pending_set_interval;
}
void PropertyMetadata::consume_pending_ops(ParserDispatcher& parser,
                                           request::CommandIndex cmd_idx,
                                           request::PropertyData prop_name) {
    if (_pending_set) {
        request::Z85Data bin;
        std::vector<uint8_t> tmp_buffer;
        uint8_t null_symbol_override = '\0';
        switch (_type) {
            case Type::UInt32: {
                const auto& v = std::get<uint32_t>(_pending_data);
                bin.n = 4;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::Int32: {
                const auto& v = std::get<int32_t>(_pending_data);
                bin.n = 4;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::UInt64: {
                const auto& v = std::get<uint64_t>(_pending_data);
                bin.n = 8;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::Int64: {
                const auto& v = std::get<int64_t>(_pending_data);
                bin.n = 8;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::FP32: {
                const auto& v = std::get<float>(_pending_data);
                bin.n = 4;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::FP64: {
                const auto& v = std::get<double>(_pending_data);
                bin.n = 8;
                bin.data = reinterpret_cast<const uint8_t*>(&v);
            } break;
            case Type::String: {
                const auto& v = std::get<std::string>(_pending_data);
                bin.n = v.size();
                bin.data = reinterpret_cast<const uint8_t*>(v.data());
            } break;
            case Type::UBJson: {
                const auto& v = std::get<nlohmann::json>(_pending_data);
                tmp_buffer = nlohmann::json::to_ubjson(v);
                bin.n = tmp_buffer.size();
                bin.data = tmp_buffer.data();
                null_symbol_override = 'N';
            } break;
            default: {
                utils::fatal("PropertyMetadata::consume_pending_ops: unknown type enum!");
            } break;
        }

        parser.send_request(cmd_idx, prop_name, bin, null_symbol_override);
        _pending_set = false;
        _pending_data = std::monostate();
    }
    // Reorder get after set, so that we don't get old data back.
    if (_pending_get) {
        parser.send_request(cmd_idx, prop_name);
        _pending_get = false;
    }
    if (_pending_set_interval) {
        parser.send_request(cmd_idx, prop_name,
                            request::IntervalData(_current_update_interval));
        _pending_set_interval = false;
    }
}

bool PropertyMetadata::parse(const nlohmann::json& obj) {
    if (!obj.is_object()) {
        return false;
    }
    {
        auto type_entry = obj.find("type");
        auto flags_entry = obj.find("flags");
        if (type_entry == obj.end() || !type_entry->is_string()) [[unlikely]] {
            utils::trace("Couldn't find valid type for property.");
            return false;
        }
        if (flags_entry == obj.end() || !flags_entry->is_string()) [[unlikely]] {
            utils::trace("Couldn't find valid flags for property.");
            return false;
        }
        std::string_view type_str = type_entry->get<std::string_view>();
        std::string_view flags_str = flags_entry->get<std::string_view>();
        if (type_str.size() != 1) [[unlikely]] {
            utils::trace("Invalid type string size for property, unable to parse.");
            return false;
        }
        char type_value = (type_str[0]);
        switch (type_value) {
            case static_cast<char>(Type::UInt32):
            case static_cast<char>(Type::Int32):
            case static_cast<char>(Type::UInt64):
            case static_cast<char>(Type::Int64):
            case static_cast<char>(Type::FP32):
            case static_cast<char>(Type::FP64):
            case static_cast<char>(Type::String):
            case static_cast<char>(Type::UBJson): {
                _type = static_cast<PropertyMetadata::Type>(type_value);
            } break;
            default: {
                utils::trace("Invalid type enum value for property, unable to parse.");
                return false;
            } break;
        }
        for (char flag_value : flags_str) {
            switch (flag_value) {
                case 'r': {
                    _flag_read = true;
                } break;
                case 'w': {
                    _flag_write = true;
                } break;
                case 'e': {
                    _flag_event = true;
                } break;
                case 'a': {
                    _flag_action = true;
                } break;
                case 'n': {
                    if (_data_interp != DataInterpretation::Normal) {
                        utils::trace("Multiple data interpretation flag chars, ignoring!");
                        return false;
                    }
                    else {
                        _data_interp = DataInterpretation::Enum;
                    }
                } break;
                case 'l': {
                    if (_data_interp != DataInterpretation::Normal) {
                        utils::trace("Multiple data interpretation flag chars, ignoring!");
                        return false;
                    }
                    else {
                        _data_interp = DataInterpretation::Boolean;
                    }
                } break;
                case 'f': {
                    if (_data_interp != DataInterpretation::Normal) {
                        utils::trace("Multiple data interpretation flag chars, ignoring!");
                        return false;
                    }
                    else {
                        _data_interp = DataInterpretation::Bitfield;
                    }
                } break;
                case 'o': {
                    if (_data_interp != DataInterpretation::Normal) {
                        utils::trace("Multiple data interpretation flag chars, ignoring!");
                        return false;
                    }
                    else {
                        _data_interp = DataInterpretation::Observations;
                    }
                } break;
                default: {
                    utils::trace("Unknown flag enum value `", static_cast<int>(flag_value),
                                 "` for property, unable to parse.");
                    return false;
                } break;
            }
        }
        if (_flag_action && !_flag_write) [[unlikely]] {
            utils::trace("Actionable properties must be writable.");
            return false;
        }
        if (_flag_event && !_flag_read) [[unlikely]] {
            utils::trace("Event properties must be readable.");
            return false;
        }
    }
    if (_data_interp == DataInterpretation::Enum) {
        auto enum_mapping_entry = obj.find("enum_mapping");
        if (enum_mapping_entry == obj.end() || !enum_mapping_entry->is_object()) [[unlikely]] {
            utils::trace("Enum property must have enum_mapping metadata!");
            return false;
        }
        enum_mapping_t dat_mt;
        auto keys_entry = enum_mapping_entry->find("keys");
        auto labels_entry = enum_mapping_entry->find("labels");
        if (keys_entry == enum_mapping_entry->end() ||
            labels_entry == enum_mapping_entry->end() ||
            keys_entry->size() != labels_entry->size() || !keys_entry->is_array() ||
            !labels_entry->is_array()) [[unlikely]] {
            utils::trace("Invalid enum_mapping metadata!");
            return false;
        }
        size_t n = keys_entry->size();
        for (size_t i = 0; i < n; i++) {
            auto& key_entry = keys_entry->at(i);
            auto& label_entry = labels_entry->at(i);
            if (!key_entry.is_number_integer() || !label_entry.is_string()) [[unlikely]] {
                utils::trace("Invalid enum_mapping metadata entry at index #", i, '!');
                return false;
            }
            auto [pos, ok] =
                 dat_mt.emplace(key_entry.get<uint64_t>(), label_entry.get<std::string>());
            if (!ok) {
                utils::trace("Duplicate entries in enum_mapping metadata!");
                return false;
            }
        }
        _data_interp_metadata = dat_mt;
    }
    else if (_data_interp == DataInterpretation::Bitfield) {
        auto bitfld_mapping_entry = obj.find("bitfield_mapping");
        if (bitfld_mapping_entry == obj.end() || !bitfld_mapping_entry->is_object())
             [[unlikely]] {
            utils::trace("Bitfield property must have bitfield_mapping metadata!");
            return false;
        }
        bitfield_mapping_t dat_mt;
        auto keys_entry = bitfld_mapping_entry->find("keys");
        auto labels_entry = bitfld_mapping_entry->find("labels");
        if (keys_entry == bitfld_mapping_entry->end() ||
            labels_entry == bitfld_mapping_entry->end() ||
            keys_entry->size() != labels_entry->size() || !keys_entry->is_array() ||
            !labels_entry->is_array()) [[unlikely]] {
            utils::trace("Invalid bitfield_mapping metadata!");
            return false;
        }
        size_t n = keys_entry->size();
        for (size_t i = 0; i < n; i++) {
            auto& key_entry = keys_entry->at(i);
            auto& label_entry = labels_entry->at(i);
            if (!key_entry.is_number_integer() || !label_entry.is_string()) [[unlikely]] {
                utils::trace("Invalid bitfield_mapping metadata entry at index #", i, '!');
                return false;
            }
            uint64_t bit_pos = key_entry.get<uint64_t>();
            if (bit_pos >= 64) {
                utils::trace("Cannot support bitfields with size >=64!");
                return false;
            }
            auto [pos, ok] = dat_mt.emplace(0x1 << bit_pos, label_entry.get<std::string>());
            if (!ok) {
                utils::trace("Duplicate entries in bitfield_mapping metadata!");
                return false;
            }
        }
        _data_interp_metadata = dat_mt;
    }
    else if (_data_interp == DataInterpretation::Observations) {
        auto axis_mapping_entry = obj.find("axis_mapping");
        if (axis_mapping_entry == obj.end() || !axis_mapping_entry->is_object()) [[unlikely]] {
            utils::trace("Observations property must have axis_mapping metadata!");
            return false;
        }
        observations_mapping_t dat_mt;
        auto labels_entry = axis_mapping_entry->find("labels");
        if (labels_entry == axis_mapping_entry->end() || !labels_entry->is_array() ||
            labels_entry->size() <= 1) [[unlikely]] {
            utils::trace("Invalid observations_mapping metadata!");
            return false;
        }
        size_t n = labels_entry->size();
        dat_mt.y_axes.reserve(n - 1);
        for (size_t i = 0; i < n; i++) {
            auto& label_entry = labels_entry->at(i);
            if (!label_entry.is_string()) [[unlikely]] {
                utils::trace("Invalid observations_mapping metadata entry at index #", i, '!');
                return false;
            }
            if (i == 0) {
                // x axis.
                dat_mt.x_axis = {label_entry.get<std::string>()};
            }
            else {
                dat_mt.y_axes.emplace_back(label_entry.get<std::string>());
            }
        }
        _data_interp_metadata = dat_mt;
    }
    {
        auto display_type_entry = obj.find("display_type");
        if (display_type_entry != obj.end() && display_type_entry->is_number_integer()) {
            _disp_type =
                 static_cast<PropertyMetadata::DisplayType>(display_type_entry->get<uint8_t>());
            switch (_disp_type) {
                case DisplayType::Default: {
                    /* pass */
                } break;
                case DisplayType::TextBox: {
                    if (_data_interp != PropertyMetadata::DataInterpretation::Normal &&
                        _data_interp != PropertyMetadata::DataInterpretation::Bitfield)
                         [[unlikely]] {
                        utils::trace("Display type of TextBox requires data "
                                     "interpretation of Normal or Bitfield.");
                        return false;
                    }
                } break;
                case DisplayType::DragBox: {
                    if (!is_numerical(_type)) {
                        utils::trace("Property with display type of "
                                     "DragBox requires numerical data type.");
                        return false;
                    }
                    switch (_data_interp) {
                        case DataInterpretation::Normal:
                        case DataInterpretation::Boolean: {
                            /* pass */
                        } break;
                        case DataInterpretation::Enum:
                        case DataInterpretation::Bitfield:
                        case DataInterpretation::Observations:
                            [[unlikely]] {
                                utils::trace("Display type of DragBox requires data "
                                             "interpretation of Normal or Boolean.");
                                return false;
                            }
                            break;
                    }
                } break;
                case DisplayType::PressButton: {
                    if (!_flag_action) [[unlikely]] {
                        utils::trace("Property of display type PressButton "
                                     "must also be actionable.");
                        return false;
                    }
                    /* event flag is ignored */
                    if (!is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "PressButton requires integral data type.");
                        return false;
                    }
                    if (_data_interp != DataInterpretation::Normal &&
                        _data_interp != DataInterpretation::Boolean) [[unlikely]] {
                        utils::trace("Display type of PressButton requires data "
                                     "interpretation of Normal or Boolean.");
                        return false;
                    }
                } break;
                case DisplayType::ToggleButton: {
                    if (!_flag_action && !_flag_event) [[unlikely]] {
                        utils::trace("Property of display type ToggleButton "
                                     "must be either actionable or an event.");
                        return false;
                    }
                    if (!is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "ToggleButton requires integral data type.");
                        return false;
                    }
                    if (_data_interp != DataInterpretation::Normal &&
                        _data_interp != DataInterpretation::Boolean) [[unlikely]] {
                        utils::trace("Display type of ToggleButton requires data "
                                     "interpretation of Normal or Boolean.");
                        return false;
                    }
                } break;
                case DisplayType::CheckboxButton: {
                    if (!is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "CheckboxButton requires integral data type.");
                        return false;
                    }
                    if (_data_interp == DataInterpretation::Bitfield) {
                        /* pass */
                    }
                    else {
                        if (!_flag_action && !_flag_event) [[unlikely]] {
                            utils::trace("Property of display type CheckboxButton "
                                         "must be either actionable or an event.");
                            return false;
                        }
                        if (_data_interp != DataInterpretation::Normal &&
                            _data_interp != DataInterpretation::Boolean) [[unlikely]] {
                            utils::trace("Display type of CheckboxButton requires data "
                                         "interpretation of Normal or Boolean.");
                            return false;
                        }
                    }
                } break;
                case DisplayType::RadioButton: {
                    if (!is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "RadioButton requires integral data type.");
                        return false;
                    }
                    if (_data_interp != DataInterpretation::Enum) [[unlikely]] {
                        utils::trace("Display type of RadioButton requires "
                                     "data interpretation of Enum.");
                        return false;
                    }
                } break;
                case DisplayType::ComboBox: {
                    if (!is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "ComboBox requires integral data type.");
                        return false;
                    }
                    if (_data_interp != DataInterpretation::Enum) [[unlikely]] {
                        utils::trace("Display type of ComboBox requires "
                                     "data interpretation of Enum.");
                        return false;
                    }
                } break;
                case DisplayType::SliderBox: {
                    if (_data_interp == DataInterpretation::Enum) {
                        if (!is_integral(_type)) [[unlikely]] {
                            utils::trace("Property with display type of SliderBox "
                                         "and data interpretation of Enum "
                                         "requires integral data type.");
                            return false;
                        }
                    }
                    else if (_data_interp == DataInterpretation::Normal) {
                        if (!is_numerical(_type)) [[unlikely]] {
                            utils::trace("Property with display type of SliderBox "
                                         "and data interpretation of Normal "
                                         "requires numerical data type.");
                            return false;
                        }
                    }
                    else [[unlikely]] {
                        utils::trace("Display type of SliderBox requires "
                                     "data interpretation of Normal or Enum.");
                        return false;
                    }
                } break;
                case DisplayType::ListBox: {
                    if (is_integral(_type)) [[unlikely]] {
                        utils::trace("Property with display type of "
                                     "ListBox requires integral data type.");
                        return false;
                    }
                    if (_data_interp != DataInterpretation::Enum &&
                        _data_interp != DataInterpretation::Bitfield) [[unlikely]] {
                        utils::trace("Display type of ListBox requires "
                                     "data interpretation of Enum or Bitfield.");
                        return false;
                    }
                } break;
                case DisplayType::Plot: {
                    if (!_flag_event || _type != Type::UBJson ||
                        _data_interp != DataInterpretation::Observations) {
                        utils::trace("Display type of Plot requires event property "
                                     "with Observations data interpretation and UBJson type.");
                        return false;
                    }
                } break;
            }
        }
        if (!_flag_read && (_data_interp != DataInterpretation::Boolean ||
                            _disp_type != DisplayType::PressButton)) {
            utils::trace("Only PressButtons can be non-readable!");
            return false;
        }
    }
    // TODO: std::string _display_tooltip;
    if (_type != Type::String && _type != Type::UBJson && _type != Type::Unknown) {
        auto min_value_obj = obj.find("min");
        auto max_value_obj = obj.find("max");
        if (min_value_obj != obj.end() && min_value_obj->is_number()) {
            switch (_type) {
                case Type::UInt32: {
                    _min_value = min_value_obj->get<uint32_t>();
                } break;
                case Type::Int32: {
                    _min_value = min_value_obj->get<int32_t>();
                } break;
                case Type::UInt64: {
                    _min_value = min_value_obj->get<uint64_t>();
                } break;
                case Type::Int64: {
                    _min_value = min_value_obj->get<int64_t>();
                } break;
                case Type::FP32: {
                    _min_value = min_value_obj->get<float>();
                } break;
                case Type::FP64: {
                    _min_value = min_value_obj->get<double>();
                } break;
                default: {
                    std::abort();
                } break;
            }
        }
        if (max_value_obj != obj.end() && max_value_obj->is_number()) {
            switch (_type) {
                case Type::UInt32: {
                    _max_value = max_value_obj->get<uint32_t>();
                } break;
                case Type::Int32: {
                    _max_value = max_value_obj->get<int32_t>();
                } break;
                case Type::UInt64: {
                    _max_value = max_value_obj->get<uint64_t>();
                } break;
                case Type::Int64: {
                    _max_value = max_value_obj->get<int64_t>();
                } break;
                case Type::FP32: {
                    _max_value = max_value_obj->get<float>();
                } break;
                case Type::FP64: {
                    _max_value = max_value_obj->get<double>();
                } break;
                default: {
                    std::abort();
                } break;
            }
        }
    }
    {
        auto value_obj = obj.find("current_update_interval");
        if (value_obj != obj.end() && value_obj->is_number()) {
            _current_update_interval = value_obj->get<uint32_t>();
        }
    }
    {
        auto value_obj = obj.find("suggested_update_interval");
        if (value_obj != obj.end() && value_obj->is_number()) {
            _suggested_update_interval = value_obj->get<uint32_t>();
        }
    }
    return true;
}

float PropertyMetadata::get_normalized() const {
    switch (get_type()) {
        case PropertyMetadata::Type::UInt32: {
            auto min = get_min<uint32_t>();
            auto max = get_max<uint32_t>();
            auto val = get<uint32_t>();
            return normalize_integral<float>(val, min, max);
        } break;
        case PropertyMetadata::Type::Int32: {
            auto min = get_min<int32_t>();
            auto max = get_max<int32_t>();
            auto val = get<int32_t>();
            return normalize_integral<float>(val, min, max);
        } break;
        case PropertyMetadata::Type::UInt64: {
            auto min = get_min<uint64_t>();
            auto max = get_max<uint64_t>();
            auto val = get<uint64_t>();
            return normalize_integral<float>(val, min, max);
        } break;
        case PropertyMetadata::Type::Int64: {
            auto min = get_min<int64_t>();
            auto max = get_max<int64_t>();
            auto val = get<int64_t>();
            return normalize_integral<float>(val, min, max);
        } break;
        case PropertyMetadata::Type::FP32: {
            auto min = get_min<float>();
            auto max = get_max<float>();
            auto val = get<float>();
            return normalize(val, min, max);
        } break;
        case PropertyMetadata::Type::FP64: {
            auto min = get_min<double>();
            auto max = get_max<double>();
            auto val = get<double>();
            return normalize(val, min, max);
        } break;
        default: {
            return 0.f;
        }
    }
}
uint32_t PropertyMetadata::get_ratio(int digit_count) const {
    float norm = get_normalized();
    // Let optional throw exception if argument is invalid.
    uint32_t denom = *make_nines<uint32_t>(digit_count);
    return static_cast<uint32_t>((norm * denom) + 0.5f);
}

PropertyMetadata* CommandEndpoint::get_min_limit_prop() {
    auto prop_it = _properties.find("axis_limit_min");
    if (prop_it != _properties.end() && is_numerical(prop_it->second.get_type())) {
        return &prop_it->second;
    }
    // Not found or invalid type.
    return nullptr;
}
const PropertyMetadata* CommandEndpoint::get_min_limit_prop() const {
    auto prop_it = _properties.find("axis_limit_min");
    if (prop_it != _properties.end() && is_numerical(prop_it->second.get_type())) {
        return &prop_it->second;
    }
    // Not found or invalid type.
    return nullptr;
}
PropertyMetadata* CommandEndpoint::get_max_limit_prop() {
    auto prop_it = _properties.find("axis_limit_max");
    if (prop_it != _properties.end() && is_numerical(prop_it->second.get_type())) {
        return &prop_it->second;
    }
    // Not found or invalid type.
    return nullptr;
}
const PropertyMetadata* CommandEndpoint::get_max_limit_prop() const {
    auto prop_it = _properties.find("axis_limit_max");
    if (prop_it != _properties.end() && is_numerical(prop_it->second.get_type())) {
        return &prop_it->second;
    }
    // Not found or invalid type.
    return nullptr;
}

void CommandEndpoint::pend_normal_update(fractional<uint32_t> v) {
    _pending_update_value = v;
    _pending_update_value_extra = 0;
    _pending_update_speed = false;
    _pending_update = true;
}
void CommandEndpoint::pend_interval_update(fractional<uint32_t> v, uint32_t interval) {
    _pending_update_value = v;
    _pending_update_value_extra = interval;
    _pending_update_speed = false;
    _pending_update = true;
}
void CommandEndpoint::pend_speed_update(fractional<uint32_t> v, uint32_t speed) {
    _pending_update_value = v;
    _pending_update_value_extra = speed;
    _pending_update_speed = true;
    _pending_update = true;
}

bool CommandEndpoint::has_pending_ops() const {
    return _pending_call || _pending_stop || _pending_update;
}
void CommandEndpoint::consume_pending_ops(ParserDispatcher& parser,
                                          request::CommandIndex cmd_idx) {
    if (_pending_call) {
        parser.send_request(cmd_idx);
        _pending_call = false;
    }
    if (_pending_update) {
        request::AxisUpdateData update_data = {cmd_idx, _pending_update_value};
        if (_pending_update_value_extra == 0) {
            parser.send_request(update_data);
        }
        else {
            if (!_pending_update_speed) {
                request::IntervalData ex(_pending_update_value_extra);
                parser.send_request(update_data, ex);
            }
            else {
                request::SpeedData ex(_pending_update_value_extra);
                parser.send_request(update_data, ex);
            }
        }
        _pending_update_value = {};
        _pending_update = false;
    }
    if (_pending_stop) {
        parser.send_stop_request(cmd_idx);
        _pending_stop = false;
    }
}

bool CommandEndpoint::parse(common::CommandIndex key, const nlohmann::json& entry) {
    {
        auto value_obj = entry.find("support_callback");
        if (value_obj != entry.end() && value_obj->is_boolean()) {
            _support_direct_call = value_obj->get<bool>();
        }
    }
    {
        auto value_obj = entry.find("description");
        if (value_obj != entry.end() && value_obj->is_string()) {
            _description = value_obj->get<std::string>();
        }
    }
    {
        auto value_obj = entry.find("support_update_callback");
        if (value_obj != entry.end() && value_obj->is_boolean()) {
            _support_normal_update = value_obj->get<bool>();
        }
    }
    {
        auto value_obj = entry.find("support_update_interval_callback");
        if (value_obj != entry.end() && value_obj->is_boolean()) {
            _support_interval_update = value_obj->get<bool>();
        }
    }
    {
        auto value_obj = entry.find("support_update_speed_callback");
        if (value_obj != entry.end() && value_obj->is_boolean()) {
            _support_speed_update = value_obj->get<bool>();
        }
    }
    {
        auto value_obj = entry.find("support_stop_callback");
        if (value_obj != entry.end() && value_obj->is_boolean()) {
            _support_stop_cmd = value_obj->get<bool>();
        }
    }
    auto props_obj = entry.find("props");
    if (props_obj != entry.end() && props_obj->is_object()) {
        for (auto prop_entry_it = props_obj->begin(); prop_entry_it != props_obj->end();
             ++prop_entry_it) {
            std::string_view prop_name = prop_entry_it.key();
            const auto& prop_entry = prop_entry_it.value();
            auto [prop_reg_entry, prop_reg_entry_new] =
                 _properties.try_emplace(std::string(prop_name));
            if (!prop_reg_entry->second.parse(prop_entry)) {
                utils::trace("Unable to parse property ", key, '.', prop_name, "!");
                return false;
            }
        }
    }
    return true;
}

void Registry::pend_suggested_property_intervals() {
    for (auto& cmd_idx_ep : get_endpoints()) {
        // common::CommandIndex cmd_idx = cmd_idx_ep.first;
        CommandEndpoint& ep = cmd_idx_ep.second;
        for (auto& prop_kv : ep.get_properties()) {
            // auto& prop_name = prop_kv.first;
            auto& prop_meta = prop_kv.second;
            const auto sug_int = prop_meta.get_suggested_update_interval();
            if (sug_int != 0) {
                prop_meta.pend_current_update_interval(sug_int);
            }
        }
    }
}

bool Registry::parse(const nlohmann::json& obj) {
    if (!obj.is_object()) {
        return false;
    }
    {
        auto value_obj = obj.find("min_update_interval");
        if (value_obj != obj.end() && value_obj->is_number()) {
            _min_update_interval = value_obj->get<uint32_t>();
        }
    }
    {
        auto value_obj = obj.find("max_update_interval");
        if (value_obj != obj.end() && value_obj->is_number()) {
            _max_update_interval = value_obj->get<uint32_t>();
        }
    }
    for (auto entry_it = obj.begin(); entry_it != obj.end(); ++entry_it) {
        const auto& key = entry_it.key();
        const auto& entry = entry_it.value();

        if (entry.is_object()) {
            if (key.size() != 2) {
                utils::trace("Invalid key size for command index.");
                return false;
            }
            response::CommandIndex cmd_idx;
            cmd_idx.parse(key.c_str());

            auto [reg_entry, reg_entry_new] = _endpoints.try_emplace(cmd_idx);
            if (!reg_entry->second.parse(cmd_idx, entry)) {
                return false;
            }
        }
    }
    return true;
}

}  // namespace tcode
