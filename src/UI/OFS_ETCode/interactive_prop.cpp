#include "interactive.hpp"

#include <UI/OFS_ETCode/logger.hpp>

#include <type_traits>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

template<typename T, ImGuiDataType imgui_dtype>
static void __build_property_textbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta,
    T& tmp)
{
    ImGuiInputTextFlags txtinp_flags = ImGuiInputTextFlags_EnterReturnsTrue;
    if (!prop_meta.has_flag_write()) {
        txtinp_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    std::string format{};
    // Special handling for bitfields.
    if (prop_meta.is_bitfield()) {
        format = "%x";
    }
    // Required for step buttons.
    T step = std::is_floating_point_v<T> ? (T)0.1 : (T)1;
    T step_fast = std::is_floating_point_v<T> ? (T)1 : (T)10;

    if (ImGui::InputScalar(prop_name.c_str(), imgui_dtype, &tmp, &step, &step_fast,
            format.empty() ? nullptr : format.c_str(), txtinp_flags)) {
        if (prop_meta.has_flag_write()) {
            if ((!prop_meta.has_min() || tmp >= prop_meta.get_min<T>()) && (!prop_meta.has_max() || tmp <= prop_meta.get_max<T>())) {
                prop_meta.pend_set(tmp);
            }
        }
        prop_meta.pend_get();
    }
}
static void _build_property_textbox_enum(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta,
    eTCodeInteractive::text_input_t& tmp)
{
    if (!std::holds_alternative<std::string>(tmp)) {
        uint64_t state;
        switch (prop_meta.get_type()) {
            case tcode::PropertyMetadata::Type::UInt32: {
                state = std::get<uint32_t>(tmp);
            } break;
            case tcode::PropertyMetadata::Type::Int32: {
                state = std::get<int32_t>(tmp);
            } break;
            case tcode::PropertyMetadata::Type::UInt64: {
                state = std::get<uint64_t>(tmp);
            } break;
            case tcode::PropertyMetadata::Type::Int64: {
                state = std::get<int64_t>(tmp);
            } break;
            default: {
                std::abort();
            } break;
        }
        auto& enum_mt = prop_meta.get_data_interp_enum_map();
        auto e = enum_mt.find({ state, {} });
        if (e != enum_mt.end()) {
            tmp = e->label;
        }
        else {
            tmp = std::string();
        }
    }
    ImGuiInputTextFlags txtinp_flags = ImGuiInputTextFlags_EnterReturnsTrue;
    // NOTE: Getting the cursor position in CharFilter callback is not possible.
    // TODO: Tab autocomplete.
    if (!prop_meta.has_flag_write()) {
        txtinp_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    std::string* buffer = &std::get<std::string>(tmp);
    if (ImGui::InputText(prop_name.c_str(), buffer, txtinp_flags)) {
        if (prop_meta.has_flag_write()) {
            // Try to match with an enum label.
            auto& enum_mt = prop_meta.get_data_interp_enum_map();
            bool match = false;
            for (auto& e : enum_mt) {
                if (e.label == *buffer) {
                    match = true;
                    prop_meta.pend_autocast_set(e.key);
                    break;
                }
            }
            // TODO: Better user feedback on mismatch!
            if (!match) {
                SEVFATE_INTERACTIVE_LOGGER_ERROR("No matching enum label found!");
            }
        }
        prop_meta.pend_get();
    }
}
static void _build_property_textbox_string(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta,
    std::string& buf)
{
    ImGuiInputTextFlags txtinp_flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
    if (!prop_meta.has_flag_write()) {
        txtinp_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    // TODO: {multi/single}line, hint, filters.
    if (ImGui::InputTextMultiline(prop_name.c_str(), &buf, ImVec2(0, 0), txtinp_flags)) {
        if (prop_meta.has_flag_write()) {
            prop_meta.pend_set(buf);
        }
        prop_meta.pend_get();
    }
}
static void _build_property_textbox_object(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta,
    std::string& buf)
{
    ImGuiInputTextFlags txtinp_flags =
        ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine;
    if (!prop_meta.has_flag_write()) {
        txtinp_flags |= ImGuiInputTextFlags_ReadOnly;
    }
    if (ImGui::InputTextMultiline(prop_name.c_str(), &buf, ImVec2(0, 0), txtinp_flags)) {
        if (prop_meta.has_flag_write()) {
            auto nv = nlohmann::json::parse(buf, nullptr, false);
            if (!nv.is_discarded()) {
                prop_meta.pend_set(std::move(nv));
            }
            else {
                SEVFATE_INTERACTIVE_LOGGER_WARN("JSON parser error!");
            }
        }
        prop_meta.pend_get();
    }
}
static void _build_property_textbox_update_tmp(eTCodeInteractive::text_input_t& tmp,
    tcode::PropertyMetadata& prop_meta)
{
    switch (prop_meta.get_type()) {
        case tcode::PropertyMetadata::Type::UInt32: {
            tmp = prop_meta.get<uint32_t>();
        } break;
        case tcode::PropertyMetadata::Type::Int32: {
            tmp = prop_meta.get<int32_t>();
        } break;
        case tcode::PropertyMetadata::Type::UInt64: {
            tmp = prop_meta.get<uint64_t>();
        } break;
        case tcode::PropertyMetadata::Type::Int64: {
            tmp = prop_meta.get<int64_t>();
        } break;
        case tcode::PropertyMetadata::Type::FP32: {
            tmp = prop_meta.get<float>();
        } break;
        case tcode::PropertyMetadata::Type::FP64: {
            tmp = prop_meta.get<double>();
        } break;
        case tcode::PropertyMetadata::Type::String: {
            tmp = prop_meta.get<std::string>();
        } break;
        case tcode::PropertyMetadata::Type::UBJson: {
            auto& obj = prop_meta.get<nlohmann::json>();
            tmp = obj.dump(1, ' ', true);
        } break;
        default: {
            std::abort();
        } break;
    }
}
void eTCodeInteractive::_build_property_textbox(tcode::common::CommandIndex cmd_idx,
    const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    auto tmp_state = _text_input_tmp.find(std::pair(cmd_idx, prop_name));
    if (tmp_state == _text_input_tmp.end()) [[unlikely]] {
        bool ok;
        std::tie(tmp_state, ok) =
            _text_input_tmp.emplace(std::pair(cmd_idx, prop_name), text_input_t{});
        if (!ok) {
            SEVFATE_INTERACTIVE_LOGGER_FATAL("Unable to allocate for _text_input_tmp!");
        }
        // Copy current value.
        _build_property_textbox_update_tmp(tmp_state->second, prop_meta);
        // Register update callback.
        prop_meta.register_callback(
            [this](tcode::ParserDispatcher&, tcode::common::CommandIndex cmd_idx,
                std::string_view prop_name, tcode::PropertyMetadata& prop_meta) {
                auto tmp_state =
                    _text_input_tmp.find(std::pair(cmd_idx, std::string(prop_name)));
                if (tmp_state == _text_input_tmp.end()) [[unlikely]] {
                    std::abort();
                }
                _build_property_textbox_update_tmp(tmp_state->second, prop_meta);
            });
    }
    // Special handling for enums.
    if (prop_meta.is_enum()) {
        _build_property_textbox_enum(prop_name, prop_meta, tmp_state->second);
    }
    switch (prop_meta.get_type()) {
        case tcode::PropertyMetadata::Type::UInt32: {
            __build_property_textbox<uint32_t, ImGuiDataType_U32>(
                prop_name, prop_meta, std::get<uint32_t>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::Int32: {
            __build_property_textbox<int32_t, ImGuiDataType_S32>(
                prop_name, prop_meta, std::get<int32_t>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::UInt64: {
            __build_property_textbox<uint64_t, ImGuiDataType_U64>(
                prop_name, prop_meta, std::get<uint64_t>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::Int64: {
            __build_property_textbox<int64_t, ImGuiDataType_S64>(
                prop_name, prop_meta, std::get<int64_t>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::FP32: {
            __build_property_textbox<float, ImGuiDataType_Float>(
                prop_name, prop_meta, std::get<float>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::FP64: {
            __build_property_textbox<double, ImGuiDataType_Double>(
                prop_name, prop_meta, std::get<double>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::String: {
            _build_property_textbox_string(prop_name, prop_meta,
                std::get<std::string>(tmp_state->second));
        } break;
        case tcode::PropertyMetadata::Type::UBJson: {
            _build_property_textbox_object(prop_name, prop_meta,
                std::get<std::string>(tmp_state->second));
        } break;
        default: {
            std::abort();
        } break;
    }
}
template<typename T, ImGuiDataType imgui_dtype>
static void _build_property_dragbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    T v = prop_meta.get<T>();
    T min = std::numeric_limits<T>::min();
    if (prop_meta.has_min()) {
        min = prop_meta.get_min<T>();
    }
    T max = std::numeric_limits<T>::max();
    if (prop_meta.has_max()) {
        max = prop_meta.get_max<T>();
    }
    float step = std::is_floating_point_v<T> ? 0.1f : 1.0f;
    std::string format{};
    // TODO: step.
    // Special handling for bitfields.
    if (prop_meta.is_bitfield()) {
        format = "%x";
    }
    // TODO: units.
    if (ImGui::DragScalar(prop_name.c_str(), imgui_dtype, &v, step, &min, &max,
            format.empty() ? nullptr : format.c_str(),
            ImGuiSliderFlags_AlwaysClamp)) {
        if (prop_meta.has_flag_write()) {
            prop_meta.pend_set(v);
            prop_meta.pend_get();
        }
    }
}
static void _build_property_dragbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert((prop_meta.is_bitfield() && tcode::is_integral(prop_meta.get_type())) || (prop_meta.is_normal() && tcode::is_numerical(prop_meta.get_type())));
    switch (prop_meta.get_type()) {
        case tcode::PropertyMetadata::Type::UInt32: {
            _build_property_dragbox<uint32_t, ImGuiDataType_U32>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::Int32: {
            _build_property_dragbox<int32_t, ImGuiDataType_S32>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::UInt64: {
            _build_property_dragbox<uint64_t, ImGuiDataType_U64>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::Int64: {
            _build_property_dragbox<int64_t, ImGuiDataType_S64>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::FP32: {
            _build_property_dragbox<float, ImGuiDataType_Float>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::FP64: {
            _build_property_dragbox<double, ImGuiDataType_Double>(prop_name, prop_meta);
        } break;
        default: {
            std::abort();
        } break;
    }
}
static void _build_property_pressbutton(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(prop_meta.has_flag_write() && tcode::is_integral(prop_meta.get_type()) && (prop_meta.is_normal() || prop_meta.is_boolean()));
    if (ImGui::Button(prop_name.c_str())) {
        prop_meta.pend_autocast_set(true);
    }
}
static void _build_property_togglebutton(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(prop_meta.has_flag_read() && tcode::is_integral(prop_meta.get_type()) && (prop_meta.is_normal() || prop_meta.is_boolean()));
    bool state = !!prop_meta.autocast_get<uint64_t>();
    if (ImGui::Checkbox(prop_name.c_str(), &state)) {
        if (prop_meta.has_flag_write()) {
            prop_meta.pend_autocast_set(static_cast<int>(state));
            prop_meta.pend_get();
        }
    }
}
static void _build_property_checkboxbutton_bitfield(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(tcode::is_integral(prop_meta.get_type()));
    uint64_t state = prop_meta.autocast_get<uint64_t>();
    const uint64_t prev_state = state;
    if (ImGui::TreeNode(prop_name.c_str())) {
        auto& bitfld_mt = prop_meta.get_data_interp_bit_map();
        for (auto& bf_e : bitfld_mt) {
            ImGui::CheckboxFlags(bf_e.label.c_str(), reinterpret_cast<ImU64*>(&state),
                static_cast<ImU64>(bf_e.mask));
        }
        ImGui::TreePop();
        if (prop_meta.has_flag_write() && state != prev_state) {
            prop_meta.pend_autocast_set(state);
            prop_meta.pend_get();
        }
    }
}
static void _build_property_checkboxbutton_boolean(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(prop_meta.has_flag_read() && tcode::is_integral(prop_meta.get_type()) && (prop_meta.is_normal() || prop_meta.is_boolean()));
    bool state = !!prop_meta.autocast_get<uint64_t>();
    if (ImGui::Checkbox(prop_name.c_str(), &state)) {
        if (prop_meta.has_flag_write()) {
            prop_meta.pend_autocast_set(static_cast<int>(state));
            prop_meta.pend_get();
        }
    }
}
static void _build_property_checkboxbutton(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    if (prop_meta.get_data_interp() == tcode::PropertyMetadata::DataInterpretation::Bitfield) {
        _build_property_checkboxbutton_bitfield(prop_name, prop_meta);
    }
    else {
        _build_property_checkboxbutton_boolean(prop_name, prop_meta);
    }
}
static void _build_property_radiobutton(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(tcode::is_integral(prop_meta.get_type()) && prop_meta.is_enum());
    const uint64_t state = prop_meta.autocast_get<uint64_t>();
    if (ImGui::TreeNode(prop_name.c_str())) {
        auto enum_mt = prop_meta.get_data_interp_enum_map();
        for (auto& e : enum_mt) {
            bool active = e.key == state;
            if (ImGui::RadioButton(e.label.c_str(), active)) {
                if (prop_meta.has_flag_write()) {
                    prop_meta.pend_autocast_set(e.key);
                    prop_meta.pend_get();
                }
            }
        }
        ImGui::TreePop();
    }
}

static void __build_property_selectable_enum(uint64_t state,
    tcode::PropertyMetadata& prop_meta)
{
    auto& enum_mt = prop_meta.get_data_interp_enum_map();
    for (auto& e : enum_mt) {
        const bool is_selected = state == e.key;
        if (ImGui::Selectable(e.label.c_str(), is_selected)) {
            if (prop_meta.has_flag_write()) {
                prop_meta.pend_autocast_set(e.key);
                prop_meta.pend_get();
            }
        }
        if (is_selected) {
            ImGui::SetItemDefaultFocus();
        }
    }
}
static void __build_property_selectable_enum(tcode::PropertyMetadata& prop_meta)
{
    uint64_t state = prop_meta.autocast_get<uint64_t>();
    __build_property_selectable_enum(state, prop_meta);
}
static void __build_property_selectable_bitfield(tcode::PropertyMetadata& prop_meta)
{
    uint64_t state = prop_meta.autocast_get<uint64_t>();
    const uint64_t prev_state = state;
    auto& bitf_mt = prop_meta.get_data_interp_bit_map();
    for (auto& e : bitf_mt) {
        const bool is_selected = !!(state & e.mask);
        if (ImGui::Selectable(e.label.c_str(), is_selected)) {
            if (!ImGui::GetIO().KeyCtrl) {
                // Clear selection when CTRL is not held
                state = 0;
            }
            state ^= e.mask;
        }
    }
    if (prop_meta.has_flag_write() && state != prev_state) {
        prop_meta.pend_autocast_set(state);
        prop_meta.pend_get();
    }
}

static void _build_property_combobox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(tcode::is_integral(prop_meta.get_type()) && prop_meta.is_enum());
    uint64_t state = prop_meta.autocast_get<uint64_t>();
    auto& enum_mt = prop_meta.get_data_interp_enum_map();
    auto e_sel = enum_mt.find({ state, {} });
    const char* preview = nullptr;
    if (e_sel != enum_mt.end()) {
        preview = e_sel->label.c_str();
    }
    if (ImGui::BeginCombo(prop_name.c_str(), preview)) {
        __build_property_selectable_enum(state, prop_meta);
        ImGui::EndCombo();
    }
}
template<typename T, ImGuiDataType imgui_dtype>
static void _build_property_sliderbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    T v = prop_meta.get<T>();
    T min = std::numeric_limits<T>::min();
    if (prop_meta.has_min()) {
        min = prop_meta.get_min<T>();
    }
    T max = std::numeric_limits<T>::max();
    if (prop_meta.has_max()) {
        max = prop_meta.get_max<T>();
    }
    // TODO: custom units.
    if (ImGui::SliderScalar(prop_name.c_str(), imgui_dtype, &v, &min, &max, NULL,
            ImGuiSliderFlags_AlwaysClamp)) {
        if (prop_meta.has_flag_write()) {
            prop_meta.pend_set(v);
            prop_meta.pend_get();
        }
    }
}
static void _build_property_sliderbox_enum(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    uint64_t state = prop_meta.autocast_get<uint64_t>();
    auto& enum_mt = prop_meta.get_data_interp_enum_map();
    auto e_sel = enum_mt.find({ state, {} });
    const char* preview = nullptr;
    // idx can be out of bounds, do not blindly dereference with it!
    size_t idx = enum_mt.size();
    if (e_sel != enum_mt.end()) {
        preview = e_sel->label.c_str();
        idx = std::distance(enum_mt.begin(), e_sel);
    }
    constexpr size_t min = 0;
    const size_t max = enum_mt.size() - 1;
    const auto dtype = sizeof(size_t) >= 8 ? ImGuiDataType_U64 : ImGuiDataType_U32;
    if (ImGui::SliderScalar(prop_name.c_str(), dtype, &idx, &min, &max, preview,
            ImGuiSliderFlags_NoInput)) {
        if (prop_meta.has_flag_write() && idx < enum_mt.max_size()) {
            auto& e = *std::next(enum_mt.begin(), idx);
            prop_meta.pend_autocast_set(e.key);
            prop_meta.pend_get();
        }
    }
}
static void _build_property_sliderbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert((tcode::is_integral(prop_meta.get_type()) && prop_meta.is_enum()) || (tcode::is_numerical(prop_meta.get_type()) && prop_meta.is_normal()));
    if (prop_meta.is_enum()) {
        // Special handling for enums.
        _build_property_sliderbox_enum(prop_name, prop_meta);
    }
    switch (prop_meta.get_type()) {
        case tcode::PropertyMetadata::Type::UInt32: {
            _build_property_sliderbox<uint32_t, ImGuiDataType_U32>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::Int32: {
            _build_property_sliderbox<int32_t, ImGuiDataType_S32>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::UInt64: {
            _build_property_sliderbox<uint64_t, ImGuiDataType_U64>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::Int64: {
            _build_property_sliderbox<int64_t, ImGuiDataType_S64>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::FP32: {
            _build_property_sliderbox<float, ImGuiDataType_Float>(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::FP64: {
            _build_property_sliderbox<double, ImGuiDataType_Double>(prop_name, prop_meta);
        } break;
        default: {
            std::abort();
        } break;
    }
}
static void _build_property_listbox(const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    assert(tcode::is_integral(prop_meta.get_type()) && (prop_meta.is_enum() || prop_meta.is_bitfield()));
    if (ImGui::BeginListBox(prop_name.c_str())) {
        if (prop_meta.is_enum()) {
            __build_property_selectable_enum(prop_meta);
        }
        else { /* is_bitfield */
            __build_property_selectable_bitfield(prop_meta);
        }
        ImGui::EndListBox();
    }
}

void eTCodeInteractive::_build_property_default(tcode::common::CommandIndex cmd_idx,
    const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    auto dat_interp = prop_meta.get_data_interp();
    switch (prop_meta.get_type()) {
        case tcode::PropertyMetadata::Type::UInt32:
        case tcode::PropertyMetadata::Type::Int32:
        case tcode::PropertyMetadata::Type::UInt64:
        case tcode::PropertyMetadata::Type::Int64: {
            if (dat_interp == tcode::PropertyMetadata::DataInterpretation::Boolean) {
                if (!prop_meta.has_flag_read()) {
                    _build_property_pressbutton(prop_name, prop_meta);
                }
                else {
                    _build_property_togglebutton(prop_name, prop_meta);
                }
                break;
            }
            else if (dat_interp == tcode::PropertyMetadata::DataInterpretation::Enum) {
                _build_property_combobox(prop_name, prop_meta);
                break;
            }
            else if (dat_interp == tcode::PropertyMetadata::DataInterpretation::Bitfield) {
                _build_property_checkboxbutton(prop_name, prop_meta);
                break;
            }
        } /* fallthrough */
        case tcode::PropertyMetadata::Type::FP64:
        case tcode::PropertyMetadata::Type::FP32: {
            if (prop_meta.has_flag_write()) {
                if (prop_meta.has_min() && prop_meta.has_max()) {
                    _build_property_sliderbox(prop_name, prop_meta);
                }
                else {
                    _build_property_dragbox(prop_name, prop_meta);
                }
            }
            else {
                _build_property_textbox(cmd_idx, prop_name, prop_meta);
            }
        } break;
        case tcode::PropertyMetadata::Type::String: {
            _build_property_textbox(cmd_idx, prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::Type::UBJson: {
            if (dat_interp == tcode::PropertyMetadata::DataInterpretation::Observations) {
                // _build_property_plot(cmd_idx, prop_name, prop_meta);
            }
            else {
                _build_property_textbox(cmd_idx, prop_name, prop_meta);
            }
        } break;
        default: {
            std::abort();
        }
    }
}
void eTCodeInteractive::_build_property(tcode::common::CommandIndex cmd_idx,
    const std::string& prop_name,
    tcode::PropertyMetadata& prop_meta)
{
    switch (prop_meta.get_disp_type()) {
        case tcode::PropertyMetadata::DisplayType::TextBox: {
            _build_property_textbox(cmd_idx, prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::DragBox: {
            _build_property_dragbox(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::PressButton: {
            _build_property_pressbutton(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::ToggleButton: {
            _build_property_togglebutton(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::CheckboxButton: {
            _build_property_checkboxbutton(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::RadioButton: {
            _build_property_radiobutton(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::ComboBox: {
            _build_property_combobox(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::SliderBox: {
            _build_property_sliderbox(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::ListBox: {
            _build_property_listbox(prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::Plot: {
            // _build_property_plot(cmd_idx, prop_name, prop_meta);
        } break;
        case tcode::PropertyMetadata::DisplayType::Default:
        default: {
            _build_property_default(cmd_idx, prop_name, prop_meta);
        } break;
    }
}
