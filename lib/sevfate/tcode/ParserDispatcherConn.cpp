#include "ParserDispatcher.hpp"

#include <asio.hpp>
#include <utils/Trace.hpp>

#include <cassert>
#include <chrono>
#include <functional>
#include <mutex>
#include <string_view>
#include <thread>

#include <bits/chrono.h>
#include <pthread.h>

namespace tcode {

bool ParserDispatcher::is_connected() const {
    // If no IO context is running, then we are also certain no connection is active.
    if (_conn_ctx.stopped()) {
        return false;
    }
    switch (_conn_hnd.index()) {
        case 0: {
            return false;
        } break;
        case 1: {
            auto& hdl = std::get<asio::serial_port>(_conn_hnd);
            return hdl.is_open();
        } break;
        default: {
            utils::fatal("Invalid _conn_hnd type!");
        } break;
    }
}

void ConnectionConfig::configure(asio::serial_port& port) const {
    if (has_serial_port_baud_rate()) {
        port.set_option(asio::serial_port::baud_rate(serial_port_baud_rate()));
    }
    if (has_serial_port_data_size()) {
        port.set_option(asio::serial_port::character_size(serial_port_data_size()));
    }
    if (has_serial_port_flow_control()) {
        asio::serial_port::flow_control opt;
        switch (serial_port_flow_control()) {
            case FlowControl::None: {
                opt = asio::serial_port::flow_control(asio::serial_port::flow_control::none);
            } break;
            case FlowControl::Software: {
                opt =
                     asio::serial_port::flow_control(asio::serial_port::flow_control::software);
            } break;
            case FlowControl::Hardware: {
                opt =
                     asio::serial_port::flow_control(asio::serial_port::flow_control::hardware);
            } break;
            default: {
                utils::fatal("ConnectionConfig::configure(serial_port): "
                             "invalid flow control enum");
            }
        }
        port.set_option(opt);
    }
    if (has_serial_port_parity()) {
        asio::serial_port::parity opt;
        switch (serial_port_parity()) {
            case Parity::None: {
                opt = asio::serial_port::parity(asio::serial_port::parity::none);
            } break;
            case Parity::Odd: {
                opt = asio::serial_port::parity(asio::serial_port::parity::odd);
            } break;
            case Parity::Even: {
                opt = asio::serial_port::parity(asio::serial_port::parity::even);
            } break;
            default: {
                utils::fatal("ConnectionConfig::configure(serial_port): "
                             "invalid parity enum");
            }
        }
        port.set_option(opt);
    }
    if (has_serial_port_stop_bits()) {
        asio::serial_port::stop_bits opt;
        switch (serial_port_stop_bits()) {
            case StopBits::One: {
                opt = asio::serial_port::stop_bits(asio::serial_port::stop_bits::one);
            } break;
            case StopBits::OnePointFive: {
                opt = asio::serial_port::stop_bits(asio::serial_port::stop_bits::onepointfive);
            } break;
            case StopBits::Two: {
                opt = asio::serial_port::stop_bits(asio::serial_port::stop_bits::two);
            } break;
            default: {
                utils::fatal("ConnectionConfig::configure(serial_port): "
                             "invalid stop bits enum");
            }
        }
        port.set_option(opt);
    }
}

void ParserDispatcher::connect(const std::string& path, const ConnectionConfig& cfg) {
    // Ensure no previous connections remain active.
    disconnect();
    // Reset object state.
    reset();

    // Try different connection methods in order:
    _connecting = true;
    asio::error_code ec;

    // TODO: Fifo

    // 1. Serial port
    if (cfg.serial_port_enabled()) {
        asio::serial_port port(_conn_ctx);
        port.open(path, ec);
        if (port.is_open()) {
            cfg.configure(port);
            utils::trace("Connected at serial port: ", path);
            _conn_hnd = std::move(port);
            _connecting = false;
            _schedule_response();
            return;
        }
        if (ec) {
            utils::trace("Error opening serial port: ", ec.message(), " (", ec, ").");
            ec.clear();
        }
    }

    // TODO: Tcp

    // Failure to connect, return to default state.
    _connecting = false;
}
void ParserDispatcher::disconnect() {
    _conn_ctx.stop();
    asio::error_code ec;
    switch (_conn_hnd.index()) {
        case 1: {
            auto& hdl = std::get<asio::serial_port>(_conn_hnd);
            hdl.close(ec);
            if (ec) {
                utils::trace("Error closing serial port: ", ec.message(), " (", ec, ").");
            }
        } break;
        default: {
            /* noop */
        } break;
    }

    // Close io handler thread if active.
    if (_conn_thr.get_id() != std::thread::id()) {
        _conn_thr.join();
        _conn_thr = std::thread();
    }
    // Reset logic.
    _connecting = false;
}

void ParserDispatcher::_schedule_response() {
    switch (_conn_hnd.index()) {
        case 1: {
            auto& hdl = std::get<asio::serial_port>(_conn_hnd);
            asio::async_read_until(hdl, _input_adapter, '\n',
                                   std::bind(&ParserDispatcher::_handle_response, this,
                                             std::placeholders::_1, std::placeholders::_2));
        } break;
        default: {
            /* noop */
        } break;
    }
}
void ParserDispatcher::_handle_response(asio::error_code ec, size_t bytes_read) {
    if (ec) [[unlikely]] {
        utils::trace("Error for handling response: ", ec.message(), " (", ec, ").");
        if (ec == asio::error::eof) {
            utils::trace("Detected connection loss.");
            _conn_ctx.stop();
        }
        else {
            // Ignore unknown errors.
            _schedule_response();
        }
        return;
    }

    assert(_input_buffer.at(bytes_read - 1) == '\n');

    // utils::trace("Received response of #", bytes_read, " bytes.");
    {
        std::lock_guard _g(_callback_mutex);
        if (_cb_on_response_received) {
            _cb_on_response_received(*this);
        }
        if (_trace_file.is_open()) {
            auto const current_timestamp = std::chrono::steady_clock::now();
            auto const time_offset = current_timestamp - _trace_file_time_start;
            auto const timestamp =
                 std::chrono::duration_cast<std::chrono::microseconds>(time_offset);
            _trace_file << timestamp.count() << "<<<";
            std::string_view data_str(_input_buffer.data(), bytes_read - 1);
            _trace_file << data_str << std::endl;
        }
    }

    if (_input_buffer.size() == bytes_read) [[likely]] {
        // Add null terminator.
        _input_buffer.push_back('\0');
        _tokenize(_input_buffer.data(), bytes_read);
        _input_buffer.pop_back();
    }
    else {
        // Replace the past-sequence character with null terminator.
        char replaced_char = _input_buffer.at(bytes_read);
        _input_buffer.at(bytes_read) = '\0';
        _tokenize(_input_buffer.data(), bytes_read);
        _input_buffer.at(bytes_read) = replaced_char;
    }

    _input_adapter.consume(bytes_read);

    _schedule_response();
}

void ParserDispatcher::_send_data(const void* data, size_t n) {
    // TODO: make async
    asio::error_code ec;
    switch (_conn_hnd.index()) {
        case 1: {
            auto& hdl = std::get<asio::serial_port>(_conn_hnd);
            _request_sequence_length += asio::write(hdl, asio::buffer(data, n), ec);
            if (ec) [[unlikely]] {
                utils::trace("Error while sending data: ", ec.message(), " (", ec, ").");
            }
        } break;
        default: {
            utils::fatal("Trying to send data, but no connection is active!");
        } break;
    }
}

void ParserDispatcher::start_event_loop() {
    asio::error_code ec;
    while (!_conn_ctx.stopped()) {
        _conn_ctx.run(ec);
        if (ec) {
            utils::trace("Error while waiting for io events: ", ec.message(), " (", ec, ").");
        }
    }
}

void ParserDispatcher::_conn_thr_main(std::function<void(ParserDispatcher&)> prestart_callback,
                                      std::function<void(ParserDispatcher&)> stopped_callback) {
    {
        std::stringstream name("tcode_io_thread@");
        name << (const void*) this;
        pthread_setname_np(pthread_self(), name.str().c_str());
        utils::trace("I/O handler thread starting...");
        if (prestart_callback) {
            std::lock_guard _g(_callback_mutex);
            prestart_callback(*this);
        }
    }

    asio::error_code ec;
    while (!_conn_ctx.stopped()) {
        _conn_ctx.run(ec);
        if (ec) {
            utils::trace("Error while handling io events: ", ec.message(), " (", ec, ").");
        }
    }

    {
        if (stopped_callback) {
            std::lock_guard _g(_callback_mutex);
            stopped_callback(*this);
        }
        utils::trace("I/O handler thread exiting...");
    }
}
void ParserDispatcher::start_detached_event_loop(
     std::function<void(ParserDispatcher&)> prestart_callback,
     std::function<void(ParserDispatcher&)> stopped_callback) {
    if (_conn_thr.get_id() == std::thread::id()) {
        _conn_thr = std::thread(&ParserDispatcher::_conn_thr_main, this, prestart_callback,
                                stopped_callback);
    }
    else {
        utils::fatal("IO handler thread is already running!");
    }
}

bool ParserDispatcher::poll_events() {
    asio::error_code ec;
    _conn_ctx.poll(ec);
    if (ec) {
        utils::trace("Error while polling for io events: ", ec.message(), " (", ec, ").");
    }
    return !_conn_ctx.stopped();
}

}  // namespace tcode
