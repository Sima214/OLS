#include "interactive.hpp"

eTCodeInteractive::eTCodeInteractive() noexcept
{
    stateHandle = OFS_AppState<eTCodeInteractiveState>::Register(eTCodeInteractiveState::StateName);
    auto& state = eTCodeInteractiveState::State(stateHandle);

    // Restore state.
    _conn_path = state.conn_path;
    _conn_cfg = state.make_connection_config();
    _enable_suggested_property_intervals = state.enable_suggested_property_intervals;
    _enable_packet_tracing = state.enable_packet_tracing;
    // Save defaults.
    _save_state();
}
eTCodeInteractive::~eTCodeInteractive() = default;

void eTCodeInteractive::_save_state()
{
    auto& state = eTCodeInteractiveState::State(stateHandle);
    state.conn_path = _conn_path;
    state.serial_port_enabled = _conn_cfg.serial_port_enabled();
    state.serial_port_flow_control = _conn_cfg.serial_port_flow_control();
    state.serial_port_parity = _conn_cfg.serial_port_parity();
    state.serial_port_stop_bits = _conn_cfg.serial_port_stop_bits();
    state.serial_port_data_size = _conn_cfg.serial_port_data_size();
    state.serial_port_baud_rate = _conn_cfg.serial_port_baud_rate();
    state.enable_suggested_property_intervals = _enable_suggested_property_intervals;
    state.enable_packet_tracing = _enable_packet_tracing;
}

size_t eTCodeInteractive::_handle_axes_get_time_delta()
{
    auto current_time = std::chrono::steady_clock::now();
    auto delta_time = current_time - _handle_axes_last_time;
    size_t delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta_time).count();
    _handle_axes_last_time += std::chrono::milliseconds(delta_ms);
    return delta_ms;
}
