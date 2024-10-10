#ifndef SEVFATE_TCODE_PARSERDISPATCHERREGISTRY_HPP
#define SEVFATE_TCODE_PARSERDISPATCHERREGISTRY_HPP
/**
 * @file
 * @brief Device enumeration info.
 */
#include <nlohmann/json.hpp>
#include <tcode/Common.hpp>
#include <tcode/Messages.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <type_traits>
#include <variant>

namespace tcode {

class PropertyMetadata {
   public:
    enum class Type : char {
        UInt32 = 'u',
        Int32 = 'i',
        UInt64 = 'U',
        Int64 = 'I',
        FP32 = 'F',
        FP64 = 'D',
        String = 'S',
        UBJson = 'O',
        Unknown = '\0'
    };

    enum class DataInterpretation : char {
        Normal = 0,
        Enum,
        Boolean,
        Bitfield,
        Observations,
    };

    struct EnumMetadataEntry {
        uint64_t key;
        std::string label;

        EnumMetadataEntry(uint64_t key_ = {}, const std::string& label_ = {}) :
            key(key_), label(label_) {}

        constexpr bool operator<(const EnumMetadataEntry& rhs) const noexcept {
            return key < rhs.key;
        }
    };
    struct BitfieldMetadataEntry {
        uint64_t mask;
        std::string label;

        BitfieldMetadataEntry(uint64_t mask_ = {}, const std::string& label_ = {}) :
            mask(mask_), label(label_) {}

        constexpr bool operator<(const BitfieldMetadataEntry& rhs) const noexcept {
            return mask < rhs.mask;
        }
    };
    struct ObservationMetadataEntry {
        std::string label;

        ObservationMetadataEntry(const std::string& label_ = {}) : label(label_) {}
    };

    using enum_mapping_t = std::set<EnumMetadataEntry>;
    using bitfield_mapping_t = std::set<BitfieldMetadataEntry>;
    struct observations_mapping_t {
        /** X axis is always at index/key 0. */
        ObservationMetadataEntry x_axis;
        std::vector<ObservationMetadataEntry> y_axes;
    };

    enum class DisplayType : uint8_t {
        /**
         * Allow the host to automatically decide how to present the data.
         */
        Default = 0,

        /* common */
        TextBox = 1,
        DragBox = 2,

        /* button */
        PressButton = 11,
        ToggleButton = 12,
        CheckboxButton = 13,

        /* enum and derivatives */
        RadioButton = 21,
        ComboBox = 22,
        SliderBox = 23,
        ListBox = 24,

        /* special purpose */
        Plot = 91
    };

    using types_variant_t = std::variant<std::monostate, uint32_t, int32_t, uint64_t, int64_t,
                                         float, double, std::string, nlohmann::json>;
    using numerical_types_variant_t =
         std::variant<std::monostate, uint32_t, int32_t, uint64_t, int64_t, float, double>;
    using callback_t = std::function<void(ParserDispatcher&, common::CommandIndex,
                                          std::string_view, PropertyMetadata&)>;

   protected:
    Type _type = Type::Unknown;
    bool _flag_read : 1;
    bool _flag_write : 1;
    bool _flag_event : 1;
    bool _flag_action : 1;
    DataInterpretation _data_interp = DataInterpretation::Normal;
    DisplayType _disp_type = DisplayType::Default;

    bool _pending_get = false;
    bool _pending_set = false;
    bool _pending_set_interval = false;

    std::variant<std::monostate, enum_mapping_t, bitfield_mapping_t, observations_mapping_t>
         _data_interp_metadata{};
    std::variant<std::monostate> _disp_metadata{};

    uint32_t _suggested_update_interval = 0;
    uint32_t _current_update_interval = 0;

    types_variant_t _latest_data{};
    numerical_types_variant_t _min_value{};
    numerical_types_variant_t _max_value{};
    types_variant_t _pending_data{};

    callback_t _cb_on_update{};

   public:
    PropertyMetadata() noexcept :
        _flag_read(false), _flag_write(false), _flag_event(false), _flag_action(false) {}

    /** Getters */
    Type get_type() const {
        return _type;
    }
    bool has_flag_read() const {
        return _flag_read;
    }
    bool has_flag_write() const {
        return _flag_write;
    }
    bool has_flag_event() const {
        return _flag_event;
    }
    bool has_flag_action() const {
        return _flag_action;
    }
    DataInterpretation get_data_interp() const {
        return _data_interp;
    }
    bool is_normal() const {
        return _data_interp == DataInterpretation::Normal;
    }
    bool is_enum() const {
        return _data_interp == DataInterpretation::Enum;
    }
    bool is_boolean() const {
        return _data_interp == DataInterpretation::Boolean;
    }
    bool is_bitfield() const {
        return _data_interp == DataInterpretation::Bitfield;
    }
    bool is_observation() const {
        return _data_interp == DataInterpretation::Observations;
    }
    const enum_mapping_t& get_data_interp_enum_map() const {
        return std::get<enum_mapping_t>(_data_interp_metadata);
    }
    const bitfield_mapping_t& get_data_interp_bit_map() const {
        return std::get<bitfield_mapping_t>(_data_interp_metadata);
    }
    const observations_mapping_t& get_data_interp_obs_map() const {
        return std::get<observations_mapping_t>(_data_interp_metadata);
    }
    DisplayType get_disp_type() const {
        return _disp_type;
    }
    uint32_t get_suggested_update_interval() const {
        return _suggested_update_interval;
    }
    uint32_t get_current_update_interval() const {
        return _current_update_interval;
    }

    bool has_data() const {
        return _latest_data.index() != 0;
    }
    template<typename T> const T& get() const {
        return std::get<std::remove_cv_t<std::remove_reference_t<T>>>(_latest_data);
    }
    template<typename T> std::enable_if_t<std::is_integral_v<T>, T> autocast_get() const {
        switch (_type) {
            case PropertyMetadata::Type::UInt32: {
                return get<uint32_t>();
            } break;
            case PropertyMetadata::Type::Int32: {
                return get<int32_t>();
            } break;
            case PropertyMetadata::Type::UInt64: {
                return get<uint64_t>();
            } break;
            case PropertyMetadata::Type::Int64: {
                return get<int64_t>();
            } break;
            default: {
                std::abort();
            } break;
        }
    }

    bool has_min() const {
        return _min_value.index() != 0;
    }
    bool has_max() const {
        return _max_value.index() != 0;
    }

    template<typename T> const T& get_min() const {
        return std::get<T>(_min_value);
    }
    template<typename T> const T& get_max() const {
        return std::get<T>(_max_value);
    }

    void pend_get() {
        _pending_get = true;
    }

    /** Added for dynamic axis limits. */
    MMCC_TCODE_API_EXPORT float get_normalized() const;
    MMCC_TCODE_API_EXPORT uint32_t get_ratio(int digit_count) const;

    /** Setters */
    template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> pend_set(T v) {
        _pending_data = v;
        _pending_set = true;
    }

    template<typename T> std::enable_if_t<std::is_arithmetic_v<T>> pend_autocast_set(T v) {
        switch (_type) {
            case Type::UInt32: {
                pend_set(static_cast<uint32_t>(v));
            } break;
            case Type::Int32: {
                pend_set(static_cast<int32_t>(v));
            } break;
            case Type::UInt64: {
                pend_set(static_cast<uint64_t>(v));
            } break;
            case Type::Int64: {
                pend_set(static_cast<int64_t>(v));
            } break;
            case Type::FP32: {
                pend_set(static_cast<float>(v));
            } break;
            case Type::FP64: {
                pend_set(static_cast<double>(v));
            } break;
            default: {
                std::abort();
            } break;
        }
    }

    MMCC_TCODE_API_EXPORT void pend_set(std::string_view v);
    MMCC_TCODE_API_EXPORT void pend_set(std::string&& v);
    MMCC_TCODE_API_EXPORT void pend_set(const std::string& v);
    MMCC_TCODE_API_EXPORT void pend_set(nlohmann::json&& v);
    MMCC_TCODE_API_EXPORT void pend_set(const nlohmann::json& v);

    MMCC_TCODE_API_EXPORT void pend_current_update_interval(uint32_t interval);

    /**
     * Register a callback function to handle updated data received from the device in a
     * response.
     */
    MMCC_TCODE_API_EXPORT callback_t register_callback(callback_t&& cb);

   protected: /** Internal API */
    bool parse(const nlohmann::json&);

    friend class CommandEndpoint;

    bool on_update(response::Z85Data new_data, ParserDispatcher& parent,
                   common::CommandIndex cmd_idx, std::string_view prop_name);
    bool has_pending_ops() const;
    void consume_pending_ops(ParserDispatcher& parser, request::CommandIndex cmd_idx,
                             request::PropertyData prop_name);

    friend class ParserDispatcher;
};

constexpr bool is_integral(PropertyMetadata::Type type) {
    return type == PropertyMetadata::Type::Int32 || type == PropertyMetadata::Type::UInt32 ||
           type == PropertyMetadata::Type::Int64 || type == PropertyMetadata::Type::UInt64;
}
constexpr bool is_floating(PropertyMetadata::Type type) {
    return type == PropertyMetadata::Type::FP32 || type == PropertyMetadata::Type::FP64;
}
constexpr bool is_numerical(PropertyMetadata::Type type) {
    return is_integral(type) || is_floating(type);
}
inline const char* to_string(PropertyMetadata::Type ev) {
    switch (ev) {
        case PropertyMetadata::Type::UInt32: {
            return "UInt32";
        }
        case PropertyMetadata::Type::Int32: {
            return "Int32";
        }
        case PropertyMetadata::Type::UInt64: {
            return "UInt64";
        }
        case PropertyMetadata::Type::Int64: {
            return "Int64";
        }
        case PropertyMetadata::Type::FP32: {
            return "FP32";
        }
        case PropertyMetadata::Type::FP64: {
            return "FP64";
        }
        case PropertyMetadata::Type::String: {
            return "String";
        }
        case PropertyMetadata::Type::UBJson: {
            return "UBJson";
        }
        case PropertyMetadata::Type::Unknown:
        default: {
            return "Unknown";
        }
    }
}
inline const char* to_string(PropertyMetadata::DataInterpretation ev) {
    switch (ev) {
        case PropertyMetadata::DataInterpretation::Enum: {
            return "Enum";
        }
        case PropertyMetadata::DataInterpretation::Boolean: {
            return "Boolean";
        }
        case PropertyMetadata::DataInterpretation::Bitfield: {
            return "Bitfield";
        }
        case PropertyMetadata::DataInterpretation::Observations: {
            return "Observations";
        }
        case PropertyMetadata::DataInterpretation::Normal:
        default: {
            return "Normal";
        }
    }
}
inline const char* to_string(PropertyMetadata::DisplayType ev) {
    switch (ev) {
        case PropertyMetadata::DisplayType::TextBox: {
            return "TextBox";
        }
        case PropertyMetadata::DisplayType::DragBox: {
            return "DragBox";
        }
        case PropertyMetadata::DisplayType::PressButton: {
            return "PressButton";
        }
        case PropertyMetadata::DisplayType::ToggleButton: {
            return "ToggleButton";
        }
        case PropertyMetadata::DisplayType::CheckboxButton: {
            return "CheckboxButton";
        }
        case PropertyMetadata::DisplayType::RadioButton: {
            return "RadioButton";
        }
        case PropertyMetadata::DisplayType::ComboBox: {
            return "ComboBox";
        }
        case PropertyMetadata::DisplayType::SliderBox: {
            return "SliderBox";
        }
        case PropertyMetadata::DisplayType::ListBox: {
            return "ListBox";
        }
        case PropertyMetadata::DisplayType::Plot: {
            return "Plot";
        }
        case PropertyMetadata::DisplayType::Default:
        default: {
            return "Default";
        }
    }
}

class CommandEndpoint {
   public:
    using callback_t =
         std::function<void(ParserDispatcher&, common::CommandIndex, CommandEndpoint&)>;

   protected:
    std::string _description{};
    /** Whether the endpoint can be called. */
    bool _support_direct_call = false;
    bool _support_normal_update = false;
    bool _support_interval_update = false;
    bool _support_speed_update = false;
    bool _support_stop_cmd = false;
    std::unordered_map<std::string, PropertyMetadata> _properties{};

    nlohmann::json _latest_data{};

    bool _pending_call = false;
    bool _pending_stop = false;
    bool _pending_update = false;
    bool _pending_update_speed = false;
    uint32_t _pending_update_value_extra = 0;
    fractional<uint32_t> _pending_update_value{};

    callback_t _cb_on_response{};

   public:
    const std::string& get_description() const {
        return _description;
    }
    std::string_view get_description_view() const {
        return _description;
    }
    bool supports_direct_call() const {
        return _support_direct_call;
    }
    bool supports_normal_update() const {
        return _support_normal_update;
    }
    bool supports_interval_update() const {
        return _support_interval_update;
    }
    bool supports_speed_update() const {
        return _support_speed_update;
    }
    bool supports_any_update() const {
        return supports_normal_update() || supports_interval_update() ||
               supports_speed_update();
    }
    bool supports_stop_cmd() const {
        return _support_stop_cmd;
    }
    auto& get_properties() {
        return _properties;
    }
    const auto& get_properties() const {
        return _properties;
    }
    const nlohmann::json& get_data() const {
        return _latest_data;
    }

    MMCC_TCODE_API_EXPORT PropertyMetadata* get_min_limit_prop();
    MMCC_TCODE_API_EXPORT const PropertyMetadata* get_min_limit_prop() const;
    MMCC_TCODE_API_EXPORT PropertyMetadata* get_max_limit_prop();
    MMCC_TCODE_API_EXPORT const PropertyMetadata* get_max_limit_prop() const;

    template<typename T, int digit_count = 3, T def_min = 0,
             T def_max = tcode::make_nines<T, digit_count>()>
    std::tuple<T, T, bool> extract_axis_limits() {
        auto* limit_prop_min = get_min_limit_prop();
        auto* limit_prop_max = get_max_limit_prop();
        T limit_min = def_min;
        T limit_max = def_max;
        if (limit_prop_min != nullptr) {
            if (limit_prop_min->has_data()) {
                limit_min = limit_prop_min->get_ratio(digit_count);
            }
            else {
                limit_prop_min->pend_get();
            }
        }
        if (limit_prop_max != nullptr) {
            if (limit_prop_max->has_data()) {
                limit_max = limit_prop_max->get_ratio(digit_count);
            }
            else {
                limit_prop_max->pend_get();
            }
        }
        return {limit_min, limit_max, limit_min > limit_max};
    }
    template<typename T, int digit_count = 3, T def_min = 0,
             T def_max = tcode::make_nines<T, digit_count>()>
    std::tuple<T, T, bool> extract_axis_limits() const {
        const auto* limit_prop_min = get_min_limit_prop();
        const auto* limit_prop_max = get_max_limit_prop();
        T limit_min = def_min;
        T limit_max = def_max;
        if (limit_prop_min != nullptr && limit_prop_min->has_data()) {
            limit_min = limit_prop_min->get_ratio(digit_count);
        }
        if (limit_prop_max != nullptr && limit_prop_max->has_data()) {
            limit_max = limit_prop_max->get_ratio(digit_count);
        }
        return {limit_min, limit_max, limit_min > limit_max};
    }

    /* Setters */
    void pend_direct_call() {
        _pending_call = true;
    }
    MMCC_TCODE_API_EXPORT void pend_normal_update(fractional<uint32_t> v);
    MMCC_TCODE_API_EXPORT void pend_interval_update(fractional<uint32_t> v, uint32_t interval);
    MMCC_TCODE_API_EXPORT void pend_speed_update(fractional<uint32_t> v, uint32_t speed);
    void pend_stop() {
        _pending_stop = true;
    }

    MMCC_TCODE_API_EXPORT callback_t register_callback(callback_t&& cb);

   protected:
    bool parse(common::CommandIndex cmd_idx, const nlohmann::json&);

    friend class Registry;

    void on_response(nlohmann::json&& new_data, ParserDispatcher& parent,
                     common::CommandIndex cmd_idx);
    bool has_pending_ops() const;
    void consume_pending_ops(ParserDispatcher& parser, request::CommandIndex cmd_idx);

    friend class ParserDispatcher;
};

class Registry {
   public:
    using enumeration_callback_t = std::function<void(ParserDispatcher&, Registry&)>;

   private:
    /** Device info section. */
    std::string _device_name{};
    std::string _device_version{};
    std::vector<uint8_t> _device_uuid{};
    /** Protocol info section. */
    std::string _protocol_name{};
    std::string _protocol_version{};
    /** 'Dynamic' protocol metadata and state. */
    uint32_t _min_update_interval = 0;
    uint32_t _max_update_interval = UINT32_MAX;
    std::map<common::CommandIndex, CommandEndpoint> _endpoints{};

    enumeration_callback_t _cb_on_enumeration_complete{};

   protected:
    Registry();
    ~Registry();

    friend class ParserDispatcher;

   public:
    const auto& get_device_name() const {
        return _device_name;
    }
    const auto& get_device_version() const {
        return _device_version;
    }
    const auto& get_device_uuid() const {
        return _device_uuid;
    }
    const auto& get_protocol_name() const {
        return _protocol_name;
    }
    const auto& get_protocol_version() const {
        return _protocol_version;
    }
    const auto& get_min_update_interval() const {
        return _min_update_interval;
    }
    const auto& get_max_update_interval() const {
        return _max_update_interval;
    }

    auto& get_endpoints() {
        return _endpoints;
    }

    MMCC_TCODE_API_EXPORT void pend_suggested_property_intervals();

    MMCC_TCODE_API_EXPORT enumeration_callback_t
    register_enumeration_complete_callback(enumeration_callback_t&& cb);

   protected:
    bool parse(const nlohmann::json&);

    void on_enumeration_complete(ParserDispatcher& parent);
};

}  // namespace tcode

#endif /*SEVFATE_TCODE_PARSERDISPATCHERREGISTRY_HPP*/
