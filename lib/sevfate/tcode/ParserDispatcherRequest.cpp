#include "ParserDispatcher.hpp"

#include <tcode/Utils.hpp>
#include <utils/Trace.hpp>

#include <algorithm>
#include <mutex>
#include <string>
#include <string_view>

namespace tcode {

namespace {
static std::string __build_fractional_string(tcode::fractional<uint32_t> value) {
    int denum_digit_count;
    auto denum = value.denominator();
    switch (denum) {
        case 9: {
            denum_digit_count = 1;
        } break;
        case 99: {
            denum_digit_count = 2;
        } break;
        case 999: {
            denum_digit_count = 3;
        } break;
        case 9999: {
            denum_digit_count = 4;
        } break;
        case 99999: {
            denum_digit_count = 5;
        } break;
        case 999999: {
            denum_digit_count = 6;
        } break;
        case 9999999: {
            denum_digit_count = 7;
        } break;
        case 99999999: {
            denum_digit_count = 8;
        } break;
        case 999999999: {
            denum_digit_count = 9;
        } break;
        default: {
            utils::fatal("ParserDispatcher::send_request: invalid denominator `", denum,
                         "` for fractional.");
        } break;
    }

    std::stringstream number_builder;
    number_builder << std::setfill('0') << std::setw(denum_digit_count) << value.numerator();
    return number_builder.str();
}
}  // namespace

void ParserDispatcher::begin_request() {
    if (_pending_response) {
        utils::fatal("Cannot begin request while pending for response!");
    }
    _request_code_count = 0;
    _request_sequence_length = 0;
    _output_buffer_usage = 0;
    _building_request = true;
}
void ParserDispatcher::end_request() {
    if (!_building_request) {
        utils::fatal("Tried to end request while currently not building one!");
    }
    /**
     * The following flags must be set appropriately and the aftereffects be
     * visible to other threads, before the end-of-sequence marker ('\n') is sent.
     */
    _pending_response = true;
    _building_request = false;
    _send_raw_byte('\n');
    _flush_output();
    utils::trace("Sent request of #", _request_sequence_length,
                 " bytes for a total sequence of #", _request_code_count, " codes.");
}
void ParserDispatcher::_on_new_request() {
    if (_request_code_count != 0) {
        _send_raw_byte(' ');
    }
    _request_code_count++;
}

void ParserDispatcher::send_request(request::AxisUpdateData axis_updt) {
    _on_new_request();
    auto data = axis_updt.cmd.to_string();
    _send_raw_data(data.data(), data.size());
    auto fract_str = __build_fractional_string(axis_updt.value);
    _send_raw_data(fract_str.data(), fract_str.size());
}
void ParserDispatcher::send_request(request::AxisUpdateData axis_updt,
                                    request::IntervalData interval) {
    _on_new_request();
    auto data = axis_updt.cmd.to_string();
    _send_raw_data(data.data(), data.size());
    auto fract_str = __build_fractional_string(axis_updt.value);
    _send_raw_data(fract_str.data(), fract_str.size());
    _send_raw_byte('I');
    std::string interval_str = std::to_string(interval.interval);
    _send_raw_data(interval_str.data(), interval_str.size());
}
void ParserDispatcher::send_request(request::AxisUpdateData axis_updt,
                                    request::SpeedData speed) {
    _on_new_request();
    auto data = axis_updt.cmd.to_string();
    _send_raw_data(data.data(), data.size());
    auto fract_str = __build_fractional_string(axis_updt.value);
    _send_raw_data(fract_str.data(), fract_str.size());
    _send_raw_byte('S');
    std::string speed_str = std::to_string(speed.speed);
    _send_raw_data(speed_str.data(), speed_str.size());
}
void ParserDispatcher::send_request(request::CommandIndex cmd_idx) {
    _on_new_request();
    auto data = cmd_idx.to_string();
    _send_raw_data(data.data(), data.size());
}
void ParserDispatcher::send_request(request::CommandIndex cmd_idx, request::PropertyData prop) {
    _on_new_request();
    auto data = cmd_idx.to_string();
    _send_raw_data(data.data(), data.size());
    _send_raw_byte('P');
    _send_raw_data(prop.name.data(), prop.name.size());
}
void ParserDispatcher::send_request(request::CommandIndex cmd_idx, request::PropertyData prop,
                                    request::IntervalData interval) {
    _on_new_request();
    auto data = cmd_idx.to_string();
    _send_raw_data(data.data(), data.size());
    _send_raw_byte('P');
    _send_raw_data(prop.name.data(), prop.name.size());
    _send_raw_byte('I');
    std::string interval_str = std::to_string(interval.interval);
    _send_raw_data(interval_str.data(), interval_str.size());
}
void ParserDispatcher::send_request(request::CommandIndex cmd_idx, request::PropertyData prop,
                                    request::Z85Data bin, uint8_t null_symbol) {
    _on_new_request();
    auto data = cmd_idx.to_string();
    _send_raw_data(data.data(), data.size());
    _send_raw_byte('P');
    _send_raw_data(prop.name.data(), prop.name.size());
    _send_raw_byte('Z');
    _send_z85_data(bin.data, bin.n, null_symbol);
}

void ParserDispatcher::send_stop_request(request::CommandIndex axis) {
    _on_new_request();
    auto data = axis.to_string();
    _send_raw_data(data.data(), data.size());
    using namespace std::string_view_literals;
    static constexpr std::string_view STOP_TOKEN = "stop"sv;
    _send_raw_data(STOP_TOKEN.data(), STOP_TOKEN.size());
}
void ParserDispatcher::send_stop_request() {
    _on_new_request();
    using namespace std::string_view_literals;
    static constexpr std::string_view STOP_TOKEN = "dstop"sv;
    _send_raw_data(STOP_TOKEN.data(), STOP_TOKEN.size());
}

bool ParserDispatcher::send_registry_pending_requests() {
    bool sent_any = _building_request;
    std::lock_guard reg_lck(_registry_mutex);
    for (auto& cmd_idx_ep : _registry.get_endpoints()) {
        common::CommandIndex cmd_idx = cmd_idx_ep.first;
        CommandEndpoint& ep = cmd_idx_ep.second;

        if (ep.has_pending_ops()) {
            if (!sent_any) {
                begin_request();
                sent_any = true;
            }
            ep.consume_pending_ops(*this, cmd_idx);
        }

        for (auto& prop_kv : ep.get_properties()) {
            auto& prop_name = prop_kv.first;
            auto& prop_meta = prop_kv.second;

            if (prop_meta.has_pending_ops()) {
                if (!sent_any) {
                    begin_request();
                    sent_any = true;
                }
                prop_meta.consume_pending_ops(*this, cmd_idx, prop_name);
            }
        }
    }
    return sent_any;
}

}  // namespace tcode
