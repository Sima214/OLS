#include "ParserDispatcher.hpp"

#include <nlohmann/json.hpp>
#include <tcode/Messages.hpp>
#include <utils/Trace.hpp>

#include <mutex>
#include <ostream>
#include <string_view>
#include <variant>

namespace tcode {

namespace response {

std::ostream& operator<<(std::ostream& o, ErrorCode v) {
    switch (v) {
        case ErrorCode::Success: {
            o << "Success";
        } break;
        case ErrorCode::Tokenization: {
            o << "Tokenization";
        } break;
        case ErrorCode::Parsing: {
            o << "Parsing";
        } break;
        case ErrorCode::Allocation: {
            o << "Allocation";
        } break;
        case ErrorCode::InvalidCommandIndex: {
            o << "InvalidCommandIndex";
        } break;
        case ErrorCode::UnknownProperty: {
            o << "UnknownProperty";
        } break;
        case ErrorCode::InvalidOperation: {
            o << "InvalidOperation";
        } break;
        case ErrorCode::Generic: {
            o << "Generic";
        } break;
    }
    return o;
}

Error Error::decode(ErrorCode code, response::Z85Data data) {
    if (data.n == 0) {
        return Error(code);
    }
    else if (data.n == 4) {
        uint32_t info_bytes = *reinterpret_cast<const uint32_t*>(data.data);
        uint16_t stream_idx = info_bytes & 0xffff;
        uint16_t extra_data = (info_bytes >> 16) & 0xffff;
        return Error(code, stream_idx, extra_data);
    }
    else if (data.n > 4) {
        uint32_t info_bytes = *reinterpret_cast<const uint32_t*>(data.data);
        uint16_t stream_idx = info_bytes & 0xffff;
        uint16_t extra_data = (info_bytes >> 16) & 0xffff;
        const char* str_begin = reinterpret_cast<const char*>(data.data) + 4;
        const char* str_end = reinterpret_cast<const char*>(data.data) + data.n;
        while (str_end > str_begin && *(str_end - 1) == '\0') {
            str_end--;
        }
        return Error(code, stream_idx, extra_data, std::string(str_begin, str_end));
    }
    else {
        utils::fatal("Error::decode: Invalid data size #", data.n);
    }
}

}  // namespace response

namespace parser {

std::ostream& operator<<(std::ostream& o, yyParser::State v) {
    switch (v) {
        case yyParser::State::Continue: {
            o << "Continue";
        } break;
        case yyParser::State::Ok: {
            o << "Ok";
        } break;
        case yyParser::State::Failure: {
            o << "Failure";
        } break;
        case yyParser::State::SyntaxError: {
            o << "SyntaxError";
        } break;
        case yyParser::State::StackOverflow: {
            o << "StackOverflow";
        } break;
        case yyParser::State::UserError: {
            o << "UserError";
        } break;
    }
    return o;
}

}  // namespace parser

bool PropertyMetadata::on_update(response::Z85Data bin, ParserDispatcher& parent,
                                 common::CommandIndex cmd_idx, std::string_view prop_name) {
    switch (_type) {
        case Type::UInt32: {
            if (bin.n != 4) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected uint32_t, got #",
                             bin.n, ")!");
                return false;
            }
            const uint32_t* v = reinterpret_cast<const uint32_t*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::Int32: {
            if (bin.n != 4) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected int32_t, got #",
                             bin.n, ")!");
                return false;
            }
            const int32_t* v = reinterpret_cast<const int32_t*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::UInt64: {
            if (bin.n != 8) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected uint64_t, got #",
                             bin.n, ")!");
                return false;
            }
            const uint64_t* v = reinterpret_cast<const uint64_t*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::Int64: {
            if (bin.n != 8) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected int64_t, got #",
                             bin.n, ")!");
                return false;
            }
            const int64_t* v = reinterpret_cast<const int64_t*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::FP32: {
            if (bin.n != 4) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected float, got #",
                             bin.n, ")!");
                return false;
            }
            const float* v = reinterpret_cast<const float*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::FP64: {
            if (bin.n != 8) {
                utils::trace("Received property callback response "
                             "with invalid data size(expected double, got #",
                             bin.n, ")!");
                return false;
            }
            const double* v = reinterpret_cast<const double*>(bin.data);
            _latest_data = *v;
        } break;
        case Type::String: {
            if (bin.n == 0) {
                _latest_data = std::string();
            }
            const char* str_begin = (const char*) bin.data;
            const char* str_end = str_begin + bin.n;
            while (*str_end == '\0') {
                str_end--;
            };
            _latest_data.emplace<std::string>(str_begin, str_end);
        } break;
        case Type::UBJson: {
            nlohmann::json parsed_data =
                 nlohmann::json::from_ubjson(bin.data, bin.data + bin.n, true, false);
            if (parsed_data.is_discarded()) {
                utils::trace("Received property callback response "
                             "with invalid ubjson data!");
                return false;
            }
            _latest_data = std::move(parsed_data);
        } break;
        default: {
            utils::fatal("Invalid property type enum in registry!");
            return false;
        } break;
    }
    if (_cb_on_update) {
        _cb_on_update(parent, cmd_idx, prop_name, *this);
    }
    return true;
}

void CommandEndpoint::on_response(nlohmann::json&& new_data, ParserDispatcher& parent,
                                  common::CommandIndex cmd_idx) {
    _latest_data = std::move(new_data);
    if (_cb_on_response) {
        _cb_on_response(parent, cmd_idx, *this);
    }
}

void Registry::on_enumeration_complete(ParserDispatcher& parent) {
    if (_cb_on_enumeration_complete) {
        _cb_on_enumeration_complete(parent, *this);
    }
}

bool ParserDispatcher::_on_response(response::CommandIndex cmd_idx, response::Z85Data bin) {
    utils::trace("received@", cmd_idx, " #", bin.n, " bytes.");

    nlohmann::json parsed_data =
         nlohmann::json::from_ubjson(bin.data, bin.data + bin.n, true, false);
    if (parsed_data.is_discarded()) {
        utils::trace("Unable to parse command response as ubjson data!");
        return false;
    }

    {
        std::lock_guard _g(_registry_mutex);
        // Handle endpoints with special meaning.
        if (cmd_idx == common::CommandIndex(response::CommandType::Device, 0)) {
            if (parsed_data.is_object()) {
                auto name_obj = parsed_data.find("name");
                auto version_obj = parsed_data.find("version");
                auto uuid_obj = parsed_data.find("uuid");
                if (name_obj != parsed_data.end() && name_obj->is_string() &&
                    version_obj != parsed_data.end() && version_obj->is_string() &&
                    uuid_obj != parsed_data.end()) {

                    _registry._device_name = name_obj->get<std::string>();
                    _registry._device_version = version_obj->get<std::string>();

                    if (uuid_obj->is_array()) {
                        _registry._device_uuid = uuid_obj->get<std::vector<uint8_t>>();
                    }
                    else {
                        utils::trace("Unable to parse device uuid! Skipping...");
                    }
                    return true;
                }
            }
            /* fallthrough */
            utils::trace("Unable to parse device info!");
            return false;
        }
        else if (cmd_idx == common::CommandIndex(response::CommandType::Device, 1)) {
            if (parsed_data.is_object()) {
                auto name_obj = parsed_data.find("name");
                auto version_obj = parsed_data.find("version");
                if (name_obj != parsed_data.end() && name_obj->is_string() &&
                    version_obj != parsed_data.end() && version_obj->is_string()) {

                    _registry._protocol_name = name_obj->get<std::string>();
                    _registry._protocol_version = version_obj->get<std::string>();

                    return true;
                }
            }
            /* fallthrough */
            utils::trace("Unable to parse protocol info!");
            return false;
        }
        else if (cmd_idx == common::CommandIndex(response::CommandType::Device, 2)) {
            if (!_registry.parse(parsed_data)) {
                utils::trace("Unable to parse enumeration info!");
                return false;
            }
            _registry.on_enumeration_complete(*this);
            return true;
        }
        else {
            auto endpt_it = _registry._endpoints.find(cmd_idx);
            if (endpt_it == _registry._endpoints.end()) {
                utils::trace("Received endpoint/command callback response, "
                             "but command index is not in registry!");
                return false;
            }
            endpt_it->second.on_response(std::move(parsed_data), *this, cmd_idx);
            return true;
        }
    }
}
bool ParserDispatcher::_on_response(response::CommandIndex cmd_idx, response::PropertyData prop,
                                    response::Z85Data bin) {
    // utils::trace("received@", cmd_idx, ".", prop.name, " #", bin.n, " bytes.");

    {
        std::lock_guard _g(_registry_mutex);
        auto endpt_it = _registry._endpoints.find(cmd_idx);
        if (endpt_it == _registry._endpoints.end()) {
            utils::trace("Received property callback response, "
                         "but command index is not in registry!");
            return false;
        }
        // TODO: Avoid costly(?) string construction with heterogeneous lookup.
        auto& props = endpt_it->second.get_properties();
        auto prop_it = props.find(std::string(prop.name));
        if (prop_it == props.end()) {
            utils::trace("Received property callback response, "
                         "but property is not in registry!");
            return false;
        }
        // Update property data and inform application.
        auto& prop_name = prop_it->first;
        auto& prop_entry = prop_it->second;
        if (!prop_entry.on_update(bin, *this, cmd_idx, prop_name)) {
            return false;
        }
    }

    return true;
}
bool ParserDispatcher::_on_response(response::ErrorCode ec) {
    return _on_response(ec, {});
}
bool ParserDispatcher::_on_response(response::ErrorCode ec, response::Z85Data dat) {
    if (!_pending_response) {
        utils::trace("Received end of request code, "
                     "but no response was currently pending.");
    }
    // Decode error data.
    auto error_data = response::Error::decode(ec, dat);
    // utils::trace("Received response error '", ec, "' with #", dat.n, " payload.");
    {
        std::lock_guard _g(_callback_mutex);
        if (!error_data.has_error()) [[likely]] {
            if (_cb_on_request_success) {
                _cb_on_request_success(*this);
            }
        }
        else {
            if (_cb_on_request_error) {
                _cb_on_request_error(*this, error_data);
            }
        }
    }
    _pending_response = false;
    _notify_pending_response();
    return true;
}

void ParserDispatcher::_on_response_tokenizer_error(size_t stream_idx) {
    utils::trace("ParserDispatcher::_on_response_tokenizer_error(", stream_idx, ");");
    {
        std::lock_guard _g(_callback_mutex);
        if (_cb_on_response_error) {
            _cb_on_response_error(*this);
        }
    }
}
void ParserDispatcher::_on_response_syntax_error(size_t stream_idx,
                                                 parser::yyParser::State err_typ) {
    utils::trace("ParserDispatcher::_on_response_syntax_error(", stream_idx, ", ", err_typ,
                 ");");
    {
        std::lock_guard _g(_callback_mutex);
        if (_cb_on_response_error) {
            _cb_on_response_error(*this);
        }
    }
}
bool ParserDispatcher::_on_response_end() {
    {
        std::lock_guard _g(_callback_mutex);
        if (_cb_on_response_end) {
            if (!_cb_on_response_end(*this)) {
                return false;
            }
        }
    }
    return true;
}

ParserDispatcher::response_received_callback_t
ParserDispatcher::register_on_response_received_callback(response_received_callback_t&& cb) {
    std::lock_guard _g(_callback_mutex);
    return std::exchange(_cb_on_response_received, std::move(cb));
}
ParserDispatcher::response_end_callback_t ParserDispatcher::register_on_response_end_callback(
     response_end_callback_t&& cb) {
    std::lock_guard _g(_callback_mutex);
    return std::exchange(_cb_on_response_end, std::move(cb));
}
ParserDispatcher::response_error_callback_t
ParserDispatcher::register_on_response_error_callback(response_error_callback_t&& cb) {
    std::lock_guard _g(_callback_mutex);
    return std::exchange(_cb_on_response_error, std::move(cb));
}

ParserDispatcher::request_success_callback_t
ParserDispatcher::register_on_request_success_callback(request_success_callback_t&& cb) {
    std::lock_guard _g(_callback_mutex);
    return std::exchange(_cb_on_request_success, std::move(cb));
}
ParserDispatcher::request_error_callback_t ParserDispatcher::register_on_request_error_callback(
     request_error_callback_t&& cb) {
    std::lock_guard _g(_callback_mutex);
    return std::exchange(_cb_on_request_error, std::move(cb));
}

}  // namespace tcode
