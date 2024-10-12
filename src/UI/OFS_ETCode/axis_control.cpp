#include "axis_control.hpp"
#include "OpenFunscripter.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>

#include <imgui.h>
#include <variant>

namespace sevfate {

    bool AxisControlElement::_change_ctl_state(AxisControlState new_ctl_state)
    {
        if (_ctl_state != new_ctl_state) {
            _ctl_state = new_ctl_state;
            return true;
        }
        else [[likely]] {
            return false;
        }
    }

    size_t AxisPatternList::find_pattern_index(uint16_t time) const
    {
        // Assumes that there are no time gaps between patterns (ensured by pattern/list mod funcs).
        auto it = std::partition_point(
            _patterns.begin(), _patterns.end(),
            [time](const AxisPatternElement& vt) { return (vt.get_end_time() - 1) < time; });
        size_t idx = std::distance(_patterns.begin(), it);
        // Confirm that we are in-bounds before continuing.
        if (idx == 0 && it->get_start_time() > time) [[unlikely]] {
            return _patterns.size();
        }
        assert(it == _patterns.end() || (it->get_start_time() <= time && time < it->get_end_time()));
        return idx;
    }
    const AxisPatternElement* AxisPatternList::find_pattern(uint16_t time) const
    {
        size_t idx = find_pattern_index(time);
        if (idx >= _patterns.size()) [[unlikely]] {
            return nullptr;
        }
        return &_patterns[idx];
    }

    void AxisPatternList::_sort()
    {
        const size_t n = _patterns.size();
        for (size_t i = 1; i < n; i++) {
            auto x = _patterns[i];
            size_t j;
            for (j = i; j > 0 && x < _patterns[j - 1]; j--) {
                _patterns[j] = _patterns[j - 1];
            }
            _patterns[j] = x;
        }
        // Balance durations.
        for (size_t i = 0; i < n - 1; i++) {
            _patterns[i]._duration =
                std::max(_patterns[i + 1]._start_time - _patterns[i]._start_time, 1);
            _patterns[i + 1]._start_time = _patterns[i].get_end_time();
        }
    }

    const AxisPatternElement* AxisPatternList::set_pattern_duration(size_t i,
        uint16_t new_duration)
    {
        const size_t n = _patterns.size();
        if (i >= n) [[unlikely]] {
            return nullptr;
        }
        _patterns[i]._duration = new_duration;
        // Update start times for elements (i, n).
        for (size_t j = i + 1; j < n; j++) {
            _patterns.at(j)._start_time = _patterns.at(j - 1).get_end_time();
        }
        return &_patterns[i];
    }
    bool AxisPatternList::set_pattern_start_time(size_t i, uint16_t new_start_time)
    {
        const size_t n = _patterns.size();
        if (i >= n) [[unlikely]] {
            return false;
        }

        _patterns[i]._start_time = new_start_time;
        _sort();

        return true;
    }
    const AxisPatternElement* AxisPatternList::set_pattern_type(size_t i,
        AxisPatternElement::Type new_type)
    {
        const size_t n = _patterns.size();
        if (i >= n) [[unlikely]] {
            return nullptr;
        }
        _patterns[i]._type = new_type;
        return &_patterns[i];
    }
    const AxisPatternElement* AxisPatternList::set_pattern_target(size_t i, uint16_t new_target)
    {
        const size_t n = _patterns.size();
        if (i >= n) [[unlikely]] {
            return nullptr;
        }
        _patterns[i]._target = new_target;
        return &_patterns[i];
    }

    const AxisPatternElement* AxisPatternList::new_pattern(size_t i)
    {
        decltype(_patterns)::iterator pos;
        if (i >= _patterns.size()) {
            pos = _patterns.end();
        }
        else {
            pos = _patterns.begin();
            std::advance(pos, i);
        }
        // Set start time of new element based on the end time of the previous one.
        uint16_t new_start = 0;
        if (pos != _patterns.begin()) {
            new_start = std::prev(pos)->get_end_time();
        }
        // Adds a new default element which have a duration of 1.
        _patterns.emplace(pos, new_start);
        //
        // Update all the displaced elements.
        const size_t n = _patterns.size();
        for (size_t j = i + 1; j < n; j++) {
            _patterns[j]._start_time += 1;
        }

        return &*pos;
    }
    bool AxisPatternList::swap_patterns(size_t a, size_t b)
    {
        const size_t n = _patterns.size();
        if (a == b) [[unlikely]] {
            // Swapping the same element always succeeds.
            return true;
        }
        if (a >= n || b >= n) [[unlikely]] {
            // Out-of-bounds.
            return false;
        }
        // Swap every other field except the start times.
        AxisPatternElement& a_ref = _patterns[a];
        AxisPatternElement& b_ref = _patterns[b];
        AxisPatternElement b_obj = a_ref; /* swap at load */
        AxisPatternElement a_obj = b_ref;
        a_obj._start_time = a_ref.get_start_time();
        b_obj._start_time = b_ref.get_start_time();
        a_ref = a_obj;
        b_ref = b_obj;

        // Update everyone's start times due to the swapped durations.
        for (size_t j = std::min(a, b) + 1; j < n; j++) {
            _patterns.at(j)._start_time = _patterns.at(j - 1).get_end_time();
        }

        return true;
    }
    bool AxisPatternList::del_pattern(size_t i)
    {
        // Bounds checking.
        if (i >= _patterns.size()) [[unlikely]] {
            return false;
        }
        if (_patterns.size() == 1) [[unlikely]] {
            // Always leave one element in list.
            _patterns.front() = AxisPatternElement();
            return true;
        }
        auto old_elem = _patterns[i];
        { // Removal
            auto pos = _patterns.begin();
            std::advance(pos, i);
            _patterns.erase(pos);
        }
        // Update all the displaced elements.
        const size_t n = _patterns.size();
        for (size_t j = i + 1; j < n; j++) {
            _patterns[j]._start_time -= old_elem._duration;
        }
        return true;
    }

    void AxisPatternElement::apply(tcode::CommandEndpoint& ep, uint16_t previous_target) const
    {
        switch (_type) {
            case Type::NoAction: {
                /* NOP */
            } break;
            case Type::Normal: {
                if (ep.supports_normal_update()) {
                    ep.pend_normal_update({ _target, TARGET_MAX });
                }
            } break;
            case Type::Interval: {
                if (ep.supports_interval_update()) {
                    ep.pend_interval_update({ _target, TARGET_MAX }, _duration);
                }
            } break;
            case Type::Speed: {
                if (ep.supports_speed_update()) {
                    auto target_diff = std::abs(previous_target - _target);
                    double target_diff_norm = static_cast<double>(target_diff) * (1. / TARGET_MAX);
                    double duration_secs = static_cast<double>(_duration) * (1. / 1000.);
                    // Calculate the rate first in 'normalized units' / second.
                    double rate_norm = target_diff_norm / duration_secs;
                    auto rate_scaled = std::lround(rate_norm * 100.);
                    auto rate = std::max(static_cast<uint32_t>(rate_scaled), 1U);
                    ep.pend_speed_update({ _target, TARGET_MAX }, rate);
                }
            } break;
        }
    }

    void AxisPatternList::apply(tcode::CommandEndpoint& ep, int tick_delta)
    {
        if (_active) {
            const size_t n = _patterns.size();
            auto old_time = _current_time;
            auto new_time = old_time + tick_delta;
            // Clamp to duration.
            if (n == 0 && new_time >= get_total_time()) {
                // Force an update even if there is only one pattern.
                _current_pattern_idx = n;
            }
            new_time %= get_total_time();
            // Test if we are currently at another pattern index.
            auto new_pattern_index = find_pattern_index(new_time);
            if (new_pattern_index >= n) [[unlikely]] {
                utils::fatal("AxisPatternList::apply: "
                             "new time results into out-of-bounds pattern index!");
            }
            if (new_pattern_index != _current_pattern_idx) [[unlikely]] {
                // Do not skip over any patterns.
                new_pattern_index = (_current_pattern_idx + 1) % n;
                const auto& old_pattern = _patterns.at(_current_pattern_idx % n);
                const auto& new_pattern = _patterns.at(new_pattern_index);
                // Align with start time.
                /* The below assertion might trigger when the last pattern is modified.
                   As it's not critical and also corrected by the next line of code, it is disabled.
                if (new_time < new_pattern.get_start_time()) {
                    logger.loge("AxisPatternList::apply: new_time < new_pattern.get_start_time (",
                                new_time, " < ", new_pattern.get_start_time(), ")");
                    logger.logw("n = ", n, ", old_time = ", old_time, ", new_time = ", new_time,
                                ", get_total_time() = ", get_total_time(),
                                ", _current_pattern_idx = ", _current_pattern_idx,
                                ", new_pattern_index = ", new_pattern_index);
                }
                */
                new_time = new_pattern.get_start_time();
                // Send command.
                new_pattern.apply(ep, old_pattern._target);
                _current_pattern_idx = new_pattern_index;
            }
            _current_time = new_time;
        }
    }

    bool AxisScriptLink::_send_normal_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v)
    {
        if (ep.supports_normal_update()) {
            ep.pend_normal_update(v);
            _last_command.emplace<normal_cmd_t>(v);
            return true;
        }
        return false;
    }
    bool AxisScriptLink::_send_interval_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v, uint32_t interval)
    {
        if (ep.supports_interval_update()) {
            ep.pend_interval_update(v, interval);
            _last_command.emplace<interval_cmd_t>(v, interval);
            return true;
        }
        return false;
    }
    bool AxisScriptLink::_send_speed_cmd(tcode::CommandEndpoint& ep, tcode::fractional<uint32_t> v, uint32_t speed)
    {
        if (ep.supports_speed_update()) {
            ep.pend_speed_update(v, speed);
            _last_command.emplace<speed_cmd_t>(v, speed);
            return true;
        }
        return false;
    }
    bool AxisScriptLink::_send_stop_cmd(tcode::CommandEndpoint& ep)
    {
        if (ep.supports_stop_cmd()) {
            ep.pend_stop();
            _last_command.emplace<std::monostate>();
            return true;
        }
        return false;
    }

    void AxisScriptLink::apply(tcode::CommandEndpoint& ep, size_t delta_ms)
    {
        std::shared_ptr<Funscript> linked_funscript = _script.lock();
        if (linked_funscript) {
            auto& player = OpenFunscripter::ptr->player;
            // TODO: Replace with event-driven logic.
            if (player->IsPaused() && !_paused_update_state) {
                _paused_update_state = true;
                _ms_until_next_update = 0; // Triggers a normal update once when paused.
            }
            else if (!player->IsPaused() && _paused_update_state) {
                _paused_update_state = false;
                _ms_until_next_update = 0; // Returns to normal operation when un-paused.
            }
            assert(_ms_until_next_update >= 0);
            if (_ms_until_next_update <= delta_ms) {
                auto current_playback_time = player->CurrentTime();
                auto [pos, target, interval] = linked_funscript->getInterpolatedAction(current_playback_time);
                auto [limit_min, limit_max, reversal] = ep.extract_axis_limits<uint16_t>();
                if (reversal) {
                    limit_min = TARGET_DEFAULT;
                    limit_max = TARGET_DEFAULT;
                }
                if (_ms_until_next_update == 0) {
                    /**
                     * Link has been reset.
                     * Send immediate normal update command to sync with device.
                     */
                    uint32_t scaled_pos;
                    if (_invert) {
                        scaled_pos =
                            tcode::map(1.f - pos, 0.f, 1.f, (float)limit_min, (float)limit_max);
                    }
                    else {
                        scaled_pos =
                            tcode::map(pos, 0.f, 1.f, (float)limit_min, (float)limit_max);
                    }

                    _send_normal_cmd(ep, { scaled_pos, TARGET_MAX });
                    _ms_until_next_update = _paused_update_state ? 60 * 1000 : 1;
                }
                else if (!_paused_update_state) {
                    uint32_t scaled_tgt;
                    if (_invert) {
                        scaled_tgt =
                            tcode::map(1.f - target, 0.f, 1.f, (float)limit_min, (float)limit_max);
                    }
                    else {
                        scaled_tgt =
                            tcode::map(target, 0.f, 1.f, (float)limit_min, (float)limit_max);
                    }

                    // Limit minimum speed of sent commands.
                    float ms_interval = std::min(interval, 60.f) * 1000.f;
                    if (ep.supports_interval_update()) {
                        _send_interval_cmd(ep, { scaled_tgt, TARGET_MAX }, ms_interval);
                    }
                    else {
                        // Fallback. TODO: Adapt based on axis supported commands.
                        _send_normal_cmd(ep, { scaled_tgt, TARGET_MAX });
                    }
                    // Limit to 60 second max interval between sent commands.
                    _ms_until_next_update = std::min(static_cast<int32_t>(ms_interval), MAX_UPDATE_PERIOD_MS);
                }
            }
            else {
                _ms_until_next_update -= delta_ms;
                // Saturate.
                if (_ms_until_next_update < 0) {
                    _ms_until_next_update = 0;
                }
            }
        }
    }

} // namespace sevfate
