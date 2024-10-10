#include "Funscript.h"
#include "OpenFunscripter.h"
#include "axis_control.hpp"
#include "interactive.hpp"

#include <memory>
#include <tcode/Utils.hpp>
#include <utils/Trace.hpp>

#include <algorithm>
#include <cstdint>
#include <sstream>

#include <imgui.h>

namespace sevfate {

    void AxisPatternElement::build_ui(size_t i, tcode::CommandEndpoint& ep, build_ui_state_t& state, uint32_t duration_allowance) const
    {
        { // Index
            ImGui::TableNextColumn();
            bool active = state.current_time >= _start_time && state.current_time < get_end_time();
            std::stringstream index_str_builder;
            index_str_builder << std::setfill(' ') << std::setw(3) << i << '.';
            std::string index_str = index_str_builder.str();
            // constexpr ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns;
            constexpr ImGuiSelectableFlags flags = ImGuiSelectableFlags_None;
            ImGui::Selectable(index_str.data(), active, flags);
        }
        { // Start
            ImGui::TableNextColumn();
            auto tmp_start_time = _start_time;
            ImGui::InputScalar("##start", ImGuiDataType_U16, &tmp_start_time, &TIME_STEP,
                &TIME_STEP_FAST, "%d ms", ImGuiInputTextFlags_ReadOnly);
        }
        { // Duration
            ImGui::TableNextColumn();
            state.updated_duration = _duration;
            state.update_duration =
                ImGui::InputScalar("##duration", ImGuiDataType_U16, &state.updated_duration,
                    &TIME_STEP, &TIME_STEP_FAST, "%d ms", ImGuiInputTextFlags_None);
            if (state.update_duration) {
                // Enforce time limit.
                state.updated_duration =
                    std::min<uint16_t>(state.updated_duration, _duration + duration_allowance);
            }
        }
        { // Type
            ImGui::TableNextColumn();
            state.updated_type = _type;
            const char* prv_str = TYPE_METADATA_TABLE[static_cast<uint8_t>(_type)];
            constexpr ImGuiComboFlags flags =
                ImGuiComboFlags_HeightSmall;
            if (ImGui::BeginCombo("##type", prv_str, flags)) {
                for (size_t i = 0; i < TYPE_METADATA_TABLE.size(); i++) {
                    // Check for and skip unsupported options.
                    if (i == static_cast<uint8_t>(Type::Normal) && !ep.supports_normal_update()) {
                        continue;
                    }
                    if (i == static_cast<uint8_t>(Type::Interval) && !ep.supports_interval_update()) {
                        continue;
                    }
                    if (i == static_cast<uint8_t>(Type::Speed) && !ep.supports_speed_update()) {
                        continue;
                    }
                    // Speed mode requires a previous element for its target value.
                    if (i == static_cast<uint8_t>(Type::Speed) && state.first) {
                        continue;
                    }

                    const bool is_selected = (static_cast<uint8_t>(state.updated_type) == i);
                    if (ImGui::Selectable(TYPE_METADATA_TABLE[i], is_selected)) {
                        state.update_type = true;
                        state.updated_type = static_cast<Type>(i);
                    }
                    if (is_selected) {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
        }
        { // Target
            ImGui::TableNextColumn();
            auto [limit_min, limit_max, reversal] = ep.extract_axis_limits<uint16_t>();
            if (reversal) {
                limit_min = TARGET_DEFAULT;
                limit_max = TARGET_DEFAULT;
            }
            state.updated_target = _target;
            state.update_target =
                ImGui::SliderScalar("##target", ImGuiDataType_U16, &state.updated_target,
                    &limit_min, &limit_max, NULL, ImGuiSliderFlags_None);
        }
        { // Control
            ImGui::TableNextColumn();
            state.move_up = ImGui::ArrowButton("##up", ImGuiDir_Up);
            ImGui::SameLine();
            state.move_down = ImGui::ArrowButton("##dn", ImGuiDir_Down);
            ImGui::SameLine();
            state.append = ImGui::Button("+");
            ImGui::SameLine();
            state.remove = ImGui::Button("x");
        }
    }
    void AxisPatternList::build_ui(tcode::CommandEndpoint& ep)
    {
        size_t pattern_count = get_pattern_count();
        constexpr ImGuiTableFlags flags = ImGuiTableFlags_Resizable | ImGuiTableFlags_Hideable;
        if (ImGui::BeginTable("axis_pattern_table", 6, flags)) {
            ImGui::TableSetupColumn("Index");
            ImGui::TableSetupColumn("Start");
            ImGui::TableSetupColumn("Duration");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Target");
            ImGui::TableSetupColumn("Control");
            ImGui::TableHeadersRow();
            for (size_t i = 0; i < pattern_count; i++) {
                ImGui::TableNextRow();
                AxisPatternElement::build_ui_state_t state = { _current_time, i == 0,
                    i == (pattern_count - 1) };
                uint32_t duration_allowance = TIME_LIMIT - get_total_time();
                ImGui::PushID(i);
                (*this)[i].build_ui(i, ep, state, duration_allowance);
                ImGui::PopID();
                // Apply UI logic.
                if (state.update_type) {
                    set_pattern_type(i, state.updated_type);
                }
                if (state.update_duration) {
                    set_pattern_duration(i, state.updated_duration);
                }
                if (state.update_target) {
                    set_pattern_target(i, state.updated_target);
                }
                // Only one list modify operation can be selected per frame.
                if (state.append) {
                    new_pattern(i + 1);
                    pattern_count++;
                }
                else if (state.remove) {
                    del_pattern(i);
                    pattern_count--;
                }
                else if (state.move_up) {
                    swap_patterns(i, i - 1);
                }
                else if (state.move_down) {
                    swap_patterns(i, i + 1);
                }
            }
            ImGui::EndTable();
        }
        ImGui::Separator();
        ImGui::Text("Current time: %d", _current_time);
        // TODO: One-shot
        if (!_active && ImGui::Button("Play")) {
            _active = true;
        }
        else if (_active && ImGui::Button("Pause")) {
            // TODO: Send stop command - last sent pattern will continue otherwise?
            _active = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            if (_active) {
                // Reverse time to start.
                apply(ep, -_current_time);
            }
            else {
                _current_time = 0;
                _current_pattern_idx = 0;
            }
        }
        // TODO: save/load support.
    }

    void AxisScriptLink::build_ui(tcode::CommandEndpoint& ep)
    {
        std::shared_ptr<Funscript> linked_funscript = _script.lock();
        const char* prv_str = "";
        if (linked_funscript) {
            prv_str = linked_funscript->Title().c_str();
        }
        constexpr ImGuiComboFlags flags =
            ImGuiComboFlags_None;
        if (ImGui::BeginCombo("Scripts", prv_str, flags)) {
            auto loaded_scripts = OpenFunscripter::ptr->LoadedFunscripts();
            for (auto& loaded_script : loaded_scripts) {
                const bool is_selected = loaded_script == linked_funscript;
                if (ImGui::Selectable(loaded_script->Title().c_str(), is_selected)) {
                    _script = loaded_script;
                    _ms_until_next_update = 0;
                }
                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }
        if (ImGui::Checkbox("Invert", &_invert)) {
            _ms_until_next_update = 0;
        }
    }

} // namespace sevfate

void eTCodeInteractive::_build_axis_control(tcode::common::CommandIndex cmd_idx,
    tcode::CommandEndpoint& ep)
{
    if (ImGui::BeginTabBar(("##" + cmd_idx.to_null_string() + "#axis_control").c_str())) {
        // Retrieve the axis state for the current endpoint.
        auto ep_ctl_state_it =
            std::lower_bound(_axis_control_state.begin(), _axis_control_state.end(), cmd_idx);
        if (ep_ctl_state_it == _axis_control_state.end() || !(*ep_ctl_state_it == cmd_idx))
            [[unlikely]] {
            // Register new default state for endpoint.
            ep_ctl_state_it = _axis_control_state.emplace(ep_ctl_state_it, cmd_idx);
        }
        // NOTE: As long as the field `_axis_idx` does not get mutated, the following is safe.
        auto& ep_ctl_state = const_cast<sevfate::AxisControlElement&>(*ep_ctl_state_it);
        if (ImGui::BeginTabItem("Manual")) {
            auto& manual_state = ep_ctl_state.select_ctl_manual(AXIS_OUTPUT_DEFAULT);
            uint32_t* slider_state = &manual_state;
            auto [limit_min, limit_max, reversal] = ep.extract_axis_limits<uint32_t>();
            if (reversal) {
                limit_min = AXIS_OUTPUT_DEFAULT;
                limit_max = AXIS_OUTPUT_DEFAULT;
                ImGui::BeginDisabled();
            }
            if (ImGui::SliderScalar("Output", ImGuiDataType_U32, slider_state, &limit_min,
                    &limit_max)) {
                if (ep.supports_normal_update()) {
                    ep.pend_normal_update({ manual_state, AXIS_OUTPUT_MAX });
                }
                else if (ep.supports_interval_update()) {
                    ep.pend_interval_update({ manual_state, AXIS_OUTPUT_MAX },
                        AXIS_IMPULSE_INTERVAL);
                }
                else if (ep.supports_speed_update()) {
                    ep.pend_speed_update({ manual_state, AXIS_OUTPUT_MAX }, AXIS_IMPULSE_SPEED);
                }
            }
            if (reversal) {
                ImGui::EndDisabled();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pattern")) {
            auto& pattern_list = ep_ctl_state.select_ctl_pattern();
            pattern_list.build_ui(ep);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Script")) {
            auto& script_link = ep_ctl_state.select_ctl_script();
            script_link.build_ui(ep);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

void eTCodeInteractive::_handle_axes()
{
    // TODO: Take into account _stop_on_pause
    auto delta_ms = _handle_axes_get_time_delta();
    auto [reg_lck, reg] = _state.acquire_registry();
    for (auto& cmd_idx_ep : reg.get_endpoints()) {
        tcode::common::CommandIndex cmd_idx = cmd_idx_ep.first;
        tcode::CommandEndpoint& ep = cmd_idx_ep.second;
        // Retrieve the axis state for the current endpoint.
        auto ep_ctl_state_it =
            std::lower_bound(_axis_control_state.begin(), _axis_control_state.end(), cmd_idx);
        if (ep_ctl_state_it != _axis_control_state.end() && *ep_ctl_state_it == cmd_idx) {
            sevfate::AxisControlElement& ep_ctl_state = *ep_ctl_state_it;
            switch (ep_ctl_state.get_ctl_state()) {
                case sevfate::AxisControlState::Unknown:
                case sevfate::AxisControlState::Manual: { /* NOP */
                } break;
                case sevfate::AxisControlState::Pattern: {
                    ep_ctl_state.get_ctl_pattern().apply(ep, delta_ms);
                } break;
                case sevfate::AxisControlState::Script: {
                    ep_ctl_state.get_ctl_script().apply(ep, delta_ms);
                } break;
            }
        }
    }
}
