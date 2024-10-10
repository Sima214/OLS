#include "interactive.hpp"

#include <OFS_Localization.h>
#include <UI/OFS_ETCode/logger.hpp>
#include <tcode/ParserDispatcherRegistry.hpp>

#include <imgui.h>
#include <imgui_stdlib.h>

namespace {

    template<typename _Tp>
    constexpr int
    __countr_zero(_Tp __x) noexcept
    {
        using __gnu_cxx::__int_traits;
        constexpr auto _Nd = __int_traits<_Tp>::__digits;

        if (__x == 0)
            return _Nd;

        constexpr auto _Nd_ull = __int_traits<unsigned long long>::__digits;
        constexpr auto _Nd_ul = __int_traits<unsigned long>::__digits;
        constexpr auto _Nd_u = __int_traits<unsigned>::__digits;

        if _GLIBCXX17_CONSTEXPR (_Nd <= _Nd_u)
            return __builtin_ctz(__x);
        else if _GLIBCXX17_CONSTEXPR (_Nd <= _Nd_ul)
            return __builtin_ctzl(__x);
        else if _GLIBCXX17_CONSTEXPR (_Nd <= _Nd_ull)
            return __builtin_ctzll(__x);
        else // (_Nd > _Nd_ull)
        {
            static_assert(_Nd <= (2 * _Nd_ull),
                "Maximum supported integer size is 128-bit");

            constexpr auto __max_ull = __int_traits<unsigned long long>::__max;
            unsigned long long __low = __x & __max_ull;
            if (__low != 0)
                return __builtin_ctzll(__low);
            unsigned long long __high = __x >> _Nd_ull;
            return __builtin_ctzll(__high) + _Nd_ull;
        }
    }

}

void eTCodeInteractive::render_ui(bool* open)
{
    if (ImGui::Begin(TR_ID(WindowId, Tr::ETCODE), open, ImGuiWindowFlags_None)) {
        if (ImGui::BeginTabBar("##root#bar", ImGuiTabBarFlags_None)) {

            if (ImGui::BeginTabItem("Connectivity")) {
                _build_connection_tab();
                ImGui::EndTabItem();
            }

            if (_connection_active && _state.is_connected()) {
                auto [reg_lck, reg] = _state.acquire_registry();

                for (auto& cmd_idx_ep : reg.get_endpoints()) {
                    tcode::common::CommandIndex cmd_idx = cmd_idx_ep.first;
                    if (cmd_idx.cmd == tcode::common::CommandType::Device && (cmd_idx.idx == 0 || cmd_idx.idx == 1 || cmd_idx.idx == 2)) {
                        // Skip metadata endpoints.
                        continue;
                    }
                    tcode::CommandEndpoint& ep = cmd_idx_ep.second;
                    if (ImGui::BeginTabItem(cmd_idx.to_null_string().c_str())) {
                        _build_endpoint_tab(reg, cmd_idx, ep);
                        ImGui::EndTabItem();
                    }
                }

                if (ImGui::BeginTabItem("Metadata Info")) {
                    _build_info_tab(reg);
                    ImGui::EndTabItem();
                }
            }
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
    /**
     * Handle I/O after having handled user inputs.
     * If I ever manage to get async rendering working,
     * this should move after the queue submission.
     */
    _handle_io();
}

void eTCodeInteractive::_build_connection_tab()
{
    // TODO: Internal state reset button.
    ImGui::TextUnformatted("Common");
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::BeginDisabled(_connection_active);
    if (ImGui::InputTextWithHint("Device path", "serial port, fifo file or ip address",
            &_conn_path, ImGuiInputTextFlags_EnterReturnsTrue)) {
        _connection_active = true;
    }
    ImGui::EndDisabled();
    if (!_connection_active) {
        if (ImGui::Button("Enable")) {
            _connection_active = true;
        }
    }
    else {
        if (ImGui::Button(_state.is_connected() ? "Disconnect" : "Disable")) {
            _disconnect();
        }
        ImGui::SameLine();
        const char* status_str;
        if (_state.is_connecting()) {
            status_str = "Connecting";
        }
        else if (_state.is_connected()) {
            status_str = "Connected";
        }
        else {
            status_str = "Disconnected";
        }
        ImGui::Text("Status: %s", status_str);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save defaults")) {
        _save_state();
    }
    ImGui::BeginDisabled(_connection_active);
    ImGui::Separator();
    ImGui::Checkbox("Apply default property update intervals",
        &_enable_suggested_property_intervals);
    ImGui::Checkbox("Enable packet tracing", &_enable_packet_tracing);
    if (ImGui::CollapsingHeader("Serial port settings")) {
        bool en = _conn_cfg.serial_port_enabled();
        if (ImGui::Checkbox("Enable", &en)) {
            _conn_cfg.serial_port_enabled(en);
        }
        ImGui::BeginDisabled(en);
        {
            int fl_ct = static_cast<int>(_conn_cfg.serial_port_flow_control());
            static const char* FLOW_CTL_NAMES[] = { "NotSet", "None", "Software", "Hardware" };
            bool changed = ImGui::Combo("Flow control mode", &fl_ct, FLOW_CTL_NAMES,
                IM_ARRAYSIZE(FLOW_CTL_NAMES));
            if (changed) {
                auto fl_ct_en = static_cast<tcode::ConnectionConfig::FlowControl>(fl_ct);
                _conn_cfg.serial_port_flow_control(fl_ct_en);
            }
        }
        {
            int parity = static_cast<int>(_conn_cfg.serial_port_parity());
            static const char* PARITY_NAMES[] = { "NotSet", "None", "Odd", "Even" };
            bool changed =
                ImGui::Combo("Parity mode", &parity, PARITY_NAMES, IM_ARRAYSIZE(PARITY_NAMES));
            if (changed) {
                auto parity_en = static_cast<tcode::ConnectionConfig::Parity>(parity);
                _conn_cfg.serial_port_parity(parity_en);
            }
        }
        {
            int stb = static_cast<int>(_conn_cfg.serial_port_stop_bits());
            static const char* STOP_BIT_NAMES[] = { "-", "1", "1.5", "2" };
            bool changed =
                ImGui::Combo("Stop bits", &stb, STOP_BIT_NAMES, IM_ARRAYSIZE(STOP_BIT_NAMES));
            if (changed) {
                auto stb_en = static_cast<tcode::ConnectionConfig::StopBits>(stb);
                _conn_cfg.serial_port_stop_bits(stb_en);
            }
        }
        {
            uint32_t dat_siz = _conn_cfg.serial_port_data_size();
            static const uint32_t dat_siz_min = 0;
            static const uint32_t dat_siz_max = 16;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Leave to 0 in order to use OS' defaults.");
            }
            bool changed = ImGui::SliderScalar("Data size", ImGuiDataType_U32, &dat_siz,
                &dat_siz_min, &dat_siz_max);
            if (changed) {
                _conn_cfg.serial_port_data_size(dat_siz);
            }
        }
        {
            uint32_t baud = _conn_cfg.serial_port_baud_rate();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Leave to 0 in order to use OS' defaults.");
            }
            bool changed = ImGui::DragScalar("Baud rate", ImGuiDataType_U32, &baud);
            if (changed) {
                _conn_cfg.serial_port_baud_rate(baud);
            }
        }
        ImGui::EndDisabled();
    }
    if (ImGui::CollapsingHeader("Network settings")) {
    }
    ImGui::EndDisabled();
    ImGui::TextUnformatted("Tips");
    ImGui::SameLine();
    ImGui::Separator();
    ImGui::TextUnformatted("Middle click properties to refresh them!");
}
void eTCodeInteractive::_build_info_tab(tcode::Registry& reg)
{
    if (ImGui::CollapsingHeader("Connection info")) {
        ImGui::BulletText("Device name: %s", reg.get_device_name().c_str());
        ImGui::BulletText("Device version: %s", reg.get_device_version().c_str());
        std::stringstream device_uuid_str;
        device_uuid_str << std::hex << std::setfill('0');
        for (auto x : reg.get_device_uuid()) {
            device_uuid_str << std::setw(2) << static_cast<uint32_t>(x);
        }
        ImGui::BulletText("Device uuid: %s", device_uuid_str.str().c_str());
        ImGui::BulletText("Protocol name: %s", reg.get_protocol_name().c_str());
        ImGui::BulletText("Protocol version: %s", reg.get_protocol_version().c_str());
        ImGui::BulletText("Min update interval: %u", reg.get_min_update_interval());
        ImGui::BulletText("Max update interval: %u", reg.get_max_update_interval());
    }

    for (auto& cmd_idx_ep : reg.get_endpoints()) {
        tcode::common::CommandIndex cmd_idx = cmd_idx_ep.first;
        auto cmd_idx_str = cmd_idx.to_null_string();
        if (ImGui::CollapsingHeader(cmd_idx_str.c_str())) {
            ImGui::PushID(cmd_idx_str.c_str());
            tcode::CommandEndpoint& ep = cmd_idx_ep.second;

            ImGui::TextUnformatted("Endpoint Capabilities");
            ImGui::SameLine();
            ImGui::Separator();
            if (ep.supports_direct_call()) {
                ImGui::BulletText("callback/execute");
            }
            if (ep.supports_normal_update()) {
                ImGui::BulletText("update");
            }
            if (ep.supports_interval_update()) {
                ImGui::BulletText("update_interval");
            }
            if (ep.supports_speed_update()) {
                ImGui::BulletText("update_speed");
            }
            if (ep.supports_stop_cmd()) {
                ImGui::BulletText("stop");
            }

            ImGui::TextUnformatted("Properties");
            ImGui::SameLine();
            ImGui::Separator();
            for (auto& prop_kv : ep.get_properties()) {
                auto& prop_name = prop_kv.first;
                auto& prop_meta = prop_kv.second;

                if (ImGui::TreeNode(prop_name.c_str())) {
                    ImGui::BulletText("Type: %s", tcode::to_string(prop_meta.get_type()));
                    {
                        std::stringstream flags_str;
                        if (prop_meta.has_flag_read()) {
                            flags_str << " read";
                        }
                        if (prop_meta.has_flag_write()) {
                            flags_str << " write";
                        }
                        if (prop_meta.has_flag_event()) {
                            flags_str << " event";
                        }
                        if (prop_meta.has_flag_action()) {
                            flags_str << " action";
                        }
                        ImGui::BulletText("Flags:%s", flags_str.str().c_str());
                    }
                    ImGui::BulletText("Special data interpretation: %s",
                        tcode::to_string(prop_meta.get_data_interp()));
                    switch (prop_meta.get_data_interp()) {
                        case tcode::PropertyMetadata::DataInterpretation::Enum: {
                            if (ImGui::TreeNode("Enum metadata")) {
                                const auto& dat_mt = prop_meta.get_data_interp_enum_map();
                                for (auto& entry : dat_mt) {
                                    ImGui::BulletText("%lu. %s", entry.key,
                                        entry.label.c_str());
                                }
                                ImGui::TreePop();
                            }
                        } break;
                        case tcode::PropertyMetadata::DataInterpretation::Bitfield: {
                            if (ImGui::TreeNode("Bitfield metadata")) {
                                const auto& dat_mt = prop_meta.get_data_interp_bit_map();
                                for (auto& entry : dat_mt) {
                                    ImGui::BulletText("%d. %s(%lx)",
                                        __countr_zero(entry.mask),
                                        entry.label.c_str(), entry.mask);
                                }
                                ImGui::TreePop();
                            }
                        } break;
                        case tcode::PropertyMetadata::DataInterpretation::Observations: {
                            if (ImGui::TreeNode("Observation metadata")) {
                                const auto& dat_mt = prop_meta.get_data_interp_obs_map();
                                ImGui::BulletText("x: %s", dat_mt.x_axis.label.c_str());
                                for (size_t i = 0; i < dat_mt.y_axes.size(); i++) {
                                    auto& y = dat_mt.y_axes[i];
                                    ImGui::BulletText("y[%zu]: %s", i, y.label.c_str());
                                }
                                ImGui::TreePop();
                            }
                        } break;
                        default: {
                            /* nop */
                        } break;
                    }
                    ImGui::BulletText("Display/UI hint: %s",
                        tcode::to_string(prop_meta.get_disp_type()));
                    ImGui::BulletText("Current update inteval: %u",
                        prop_meta.get_current_update_interval());
                    if (prop_meta.get_suggested_update_interval() != 0) {
                        ImGui::BulletText("Suggested update inteval: %u",
                            prop_meta.get_suggested_update_interval());
                    }
                    ImGui::TreePop();
                }
            }
            ImGui::PopID();
        }
    }
}
void eTCodeInteractive::_build_endpoint_tab(tcode::Registry& reg,
    tcode::common::CommandIndex cmd_idx,
    tcode::CommandEndpoint& ep)
{
    // TODO: Dynamic auto-update interval.
    if (!ep.get_description().empty()) {
        ImGui::TextWrapped("Description: %s", ep.get_description().c_str());
    }
    if (ep.supports_direct_call()) {
        if (ImGui::Button("Execute")) {
            ep.pend_direct_call();
        }
    }
    if (ep.supports_stop_cmd()) {
        if (ep.supports_direct_call()) {
            ImGui::SameLine();
        }
        if (ImGui::Button("Stop")) {
            ep.pend_stop();
        }
    }
    if (ep.supports_direct_call() && ImGui::TreeNode("Last execution response")) {
        std::string id = "##" + cmd_idx.to_null_string() + "#last_data";
        std::string text = ep.get_data().dump(1, ' ', true);
        ImGui::InputTextMultiline(id.c_str(), text.data(), text.size(), ImVec2(0, 0),
            ImGuiInputTextFlags_ReadOnly);
        ImGui::TreePop();
    }
    if (ep.supports_any_update() && ImGui::CollapsingHeader("Axis control")) {
        _build_axis_control(cmd_idx, ep);
    }

    auto props = ep.get_properties();
    if (props.size() != 0 && ImGui::CollapsingHeader("Properties")) {
        for (auto& prop_kv : ep.get_properties()) {
            auto& prop_name = prop_kv.first;
            auto& prop_meta = prop_kv.second;
            assert(prop_meta.has_flag_read() || prop_meta.has_flag_write());
            if (prop_meta.has_flag_read() && !prop_meta.has_data()) {
                prop_meta.pend_get();
            }
            else {
                // TODO: Add tooltip/description.
                _build_property(cmd_idx, prop_name, prop_meta);
                if (ImGui::IsItemClicked(ImGuiMouseButton_Middle)) {
                    // Middle click to refresh.
                    prop_meta.pend_get();
                }
                if (!prop_meta.is_observation() && ImGui::BeginPopupContextItem((prop_name + "#popup").c_str())) {
                    // Build context menu.
                    uint32_t cur_updint = prop_meta.get_current_update_interval();
                    uint32_t min_updint = reg.get_min_update_interval();
                    uint32_t max_updint = reg.get_max_update_interval();
                    if (ImGui::SliderScalar("Update Interval", ImGuiDataType_U32, &cur_updint,
                            &min_updint, &max_updint)) {
                        if (cur_updint == 0 || (cur_updint >= min_updint && cur_updint <= max_updint)) {
                            prop_meta.pend_current_update_interval(cur_updint);
                        }
                    }
                    if (prop_meta.get_suggested_update_interval() != 0 && ImGui::Button("Apply suggested update interval")) {
                        prop_meta.pend_current_update_interval(
                            prop_meta.get_suggested_update_interval());
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Disable auto update")) {
                        prop_meta.pend_current_update_interval(0);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Refresh now")) {
                        prop_meta.pend_get();
                    }
                    ImGui::EndPopup();
                }
            }
        }
    }
}

void eTCodeInteractive::_handle_io()
{
    if (_connection_active) {
        if (_state.is_connected()) {
            // Calculate and apply patterns.
            _handle_axes();
            // Send any pending requests.
            if (!_state.is_response_pending() && _state.send_registry_pending_requests()) {
                _state.end_request();
            }
        }
        else if (!_state.is_connecting()) {
            // Create different packet trace files for each connection.
            _state.set_packet_tracing(false);
            // Try to (re)connect.
            _state.connect(_conn_path, _conn_cfg);
            if (_state.is_connected() || _state.is_connecting()) {
                _connection_setup();
            }
            else {
                /* SEVFATE_INTERACTIVE_LOGGER_DEBUG("Failure to connect."); */
            }
        }
    }
    else {
        // Sanity check.
        if (_state.is_connected()) {
            SEVFATE_INTERACTIVE_LOGGER_FATAL(
                "connection_active == false, but is_connected == true!");
        }
    }
}
void eTCodeInteractive::_disconnect()
{
    // Reset extra UI state.
    _text_input_tmp.clear();
    _axis_control_state.clear();
    // Clear plot history.
    _plot_history.clear();
    // Perform an immediate disconnect.
    // Blocks while waiting for the I/O handler thread to exit.
    _state.disconnect();
    _connection_active = false;
}
void eTCodeInteractive::_connection_setup()
{
    // Reset time offset for axes pattern delta time.
    _handle_axes_last_time = std::chrono::steady_clock::now();
    // Set extra options before finalizing connection.
    _state.set_packet_tracing(_enable_packet_tracing);
    _state.start_detached_event_loop([this](tcode::ParserDispatcher& state) {
        auto [reg_lck, reg] = state.acquire_registry();
        reg.register_enumeration_complete_callback(
            [this](tcode::ParserDispatcher&, tcode::Registry& reg) {
                if (_enable_suggested_property_intervals) {
                    SEVFATE_INTERACTIVE_LOGGER_INFO("Applying default property "
                                                    "update intervals...");
                    reg.pend_suggested_property_intervals();
                }
                // Reset extra UI state.
                _text_input_tmp.clear();
                _plot_history.clear();
                // _register_plot_history_callbacks(reg);
            });
        // Send enumeration request at connect.
        state.begin_request();
        state.send_request({ tcode::request::CommandType::Device, 0 });
        state.send_request({ tcode::request::CommandType::Device, 1 });
        state.send_request({ tcode::request::CommandType::Device, 2 });
        state.end_request();
    });
}
