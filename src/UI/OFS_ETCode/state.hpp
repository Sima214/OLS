#pragma once

#include <OFS_StateHandle.h>
#include <tcode/ParserDispatcher.hpp>

struct eTCodeInteractiveState {
    static constexpr auto StateName = "eTCodeInteractive";

    std::string conn_path = "";

    bool serial_port_enabled = true;
    tcode::ConnectionConfig::FlowControl serial_port_flow_control = tcode::ConnectionConfig::FlowControl::NotSet;
    tcode::ConnectionConfig::Parity serial_port_parity = tcode::ConnectionConfig::Parity::NotSet;
    tcode::ConnectionConfig::StopBits serial_port_stop_bits = tcode::ConnectionConfig::StopBits::NotSet;
    uint32_t serial_port_data_size = 0;
    uint32_t serial_port_baud_rate = 0;

    bool enable_suggested_property_intervals = true;
    bool enable_packet_tracing = false;

    static inline eTCodeInteractiveState& State(uint32_t stateHandle) noexcept
    {
        return OFS_AppState<eTCodeInteractiveState>(stateHandle).Get();
    }

    tcode::ConnectionConfig make_connection_config() const
    {
        tcode::ConnectionConfig cfg;
        cfg.serial_port_enabled(serial_port_enabled);
        cfg.serial_port_flow_control(serial_port_flow_control);
        cfg.serial_port_parity(serial_port_parity);
        cfg.serial_port_stop_bits(serial_port_stop_bits);
        cfg.serial_port_data_size(serial_port_data_size);
        cfg.serial_port_baud_rate(serial_port_baud_rate);
        return cfg;
    }
};

REFL_TYPE(eTCodeInteractiveState)
REFL_FIELD(conn_path)
REFL_FIELD(serial_port_enabled)
REFL_FIELD(serial_port_flow_control, serializeEnum{})
REFL_FIELD(serial_port_parity, serializeEnum{})
REFL_FIELD(serial_port_stop_bits, serializeEnum{})
REFL_FIELD(serial_port_data_size)
REFL_FIELD(serial_port_baud_rate)
REFL_FIELD(enable_suggested_property_intervals)
REFL_FIELD(enable_packet_tracing)
REFL_END