#include "Funscript.h"

#include "FunscriptAction.h"
#include "OFS_FileLogging.h"
#include "OFS_Util.h"
#include "OFS_Profiling.h"

#include "OFS_EventSystem.h"
#include "OFS_Serialization.h"
#include "FunscriptUndoSystem.h"

#include "state/states/ChapterState.h"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdlib>
#include <limits>

std::array<const char*, 9> Funscript::AxisNames = {
    "surge",
    "sway",
    "suck",
    "twist",
    "roll",
    "pitch",
    "vib",
    "pump",
    "raw"
};

Funscript::Funscript() noexcept
{
    notifyActionsChanged(false);
    undoSystem = std::make_unique<FunscriptUndoSystem>(this);
    editTime = std::chrono::system_clock::now();
}

Funscript::~Funscript() noexcept
{
}

void Funscript::loadMetadata(const nlohmann::json& metadataObj, Funscript::Metadata& outMetadata) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    OFS::Serializer<false>::Deserialize(outMetadata, metadataObj);
}

void Funscript::saveMetadata(nlohmann::json& outMetadataObj, const Funscript::Metadata& inMetadata) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    OFS::Serializer<false>::Serialize(inMetadata, outMetadataObj);
}

void Funscript::notifyActionsChanged(bool isEdit) noexcept
{
    funscriptChanged = true;
    if (isEdit && !unsavedEdits) {
        unsavedEdits = true;
        editTime = std::chrono::system_clock::now();
    }
}

void Funscript::Update() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (funscriptChanged) {
        funscriptChanged = false;
        EV::Enqueue<FunscriptActionsChangedEvent>(this);
    }
    if (selectionChanged) {
        selectionChanged = false;
        EV::Enqueue<FunscriptSelectionChangedEvent>(this);
    }
}

FunscriptAction* Funscript::getAction(FunscriptAction action) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Actions.empty()) return nullptr;
    auto it = data.Actions.find(action);
    if (it != data.Actions.end()) {
        return &*it;
    }
    return nullptr;
}

FunscriptAction* Funscript::getActionAtTime(FunscriptArray& actions, float time, float maxErrorTime) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (actions.empty()) return nullptr;
    // gets an action at a time with a margin of error
    float smallestError = std::numeric_limits<float>::max();
    FunscriptAction* smallestErrorAction = nullptr;

    int i = 0;
    auto it = actions.lower_bound(FunscriptAction(time - maxErrorTime, 0));
    if (it != actions.end()) {
        i = std::distance(actions.begin(), it);
        if (i > 0) --i;
    }

    for (; i < actions.size(); i++) {
        auto& action = actions[i];

        if (action.atS > (time + (maxErrorTime / 2)))
            break;

        auto error = std::abs(time - action.atS);
        if (error <= maxErrorTime) {
            if (error <= smallestError) {
                smallestError = error;
                smallestErrorAction = &action;
            }
            else {
                break;
            }
        }
    }
    return smallestErrorAction;
}

FunscriptAction* Funscript::getNextActionAhead(float time) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Actions.empty()) return nullptr;
    auto it = data.Actions.upper_bound(FunscriptAction(time, 0));
    return it != data.Actions.end() ? &*it : nullptr;
}

FunscriptAction* Funscript::getPreviousActionBehind(float time) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Actions.empty()) return nullptr;
    auto it = data.Actions.lower_bound(FunscriptAction(time, 0));
    if (it != data.Actions.begin()) {
        return &*(--it);
    }
    return nullptr;
}

float Funscript::GetPositionAtTime(float time) const noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Actions.size() == 0) {
        return 0;
    }
    else if (data.Actions.size() == 1)
        return data.Actions[0].pos;

    int i = 0;
    auto it = data.Actions.lower_bound(FunscriptAction(time, 0));
    if (it != data.Actions.end()) {
        i = std::distance(data.Actions.begin(), it);
        if (i > 0) --i;
    }

    for (; i < data.Actions.size() - 1; i++) {
        auto& action = data.Actions[i];
        auto& next = data.Actions[i + 1];

        if (time > action.atS && time < next.atS) {
            if (!(action.flags & FunscriptAction::ModeFlagBits::Step)) {
                // interpolate position
                int32_t lastPos = action.pos;
                float diff = next.pos - action.pos;
                float progress = (float)(time - action.atS) / (next.atS - action.atS);

                float interp = lastPos + (progress * (float)diff);
                return interp;
            }
            else {
                return action.pos;
            }
        }
        else if (action.atS == time) {
            return action.pos;
        }
    }

    return data.Actions.back().pos;
}

std::tuple<float, float, float> Funscript::getInterpolatedAction(float time) const noexcept
{
    if (data.Actions.size() == 0) {
        return { 0, 0, std::numeric_limits<float>::infinity() };
    }
    else if (data.Actions.size() == 1) {
        float pos_norm = data.Actions[0].pos / 100.f;
        return { pos_norm, pos_norm, std::numeric_limits<float>::infinity() };
    }

    // TODO: Add support for step actions.

    // Boundary conditions.
    if (time <= data.Actions.front().atS) {
        float pos_norm = data.Actions.front().pos / 100.f;
        return { pos_norm, pos_norm, std::numeric_limits<float>::infinity() };
    }
    else if (time >= data.Actions.back().atS) {
        float pos_norm = data.Actions.back().pos / 100.f;
        return { pos_norm, pos_norm, std::numeric_limits<float>::infinity() };
    }

    size_t index = 0;
    auto it = data.Actions.lower_bound(FunscriptAction(time, 0));
    if (it != data.Actions.end()) {
        index = std::distance(data.Actions.begin(), it);
        if (index > 0) {
            index -= 1;
        }
    }
    else /* it == _actions.end() */ {
        LOG_ERROR("Funscript::get_interpolated_action: "
                  "lower_bound returned end.");
        std::abort();
    }

    auto& curr_action = data.Actions.at(index);
    auto& next_action = data.Actions.at(index + 1);

    if (time > curr_action.atS && time < next_action.atS) [[likely]] {
        if (!(curr_action.flags & FunscriptAction::ModeFlagBits::Step)) [[likely]] {
            // Interpolate position.
            float pos_start = curr_action.pos / 100.f;
            float pos_end = next_action.pos / 100.f;
            float pos_diff = pos_end - pos_start;
            float time_diff = next_action.atS - curr_action.atS;
            float factor = (time - curr_action.atS) / time_diff;

            float interp = pos_start + (factor * pos_diff);
            return { interp, pos_end, next_action.atS - time };
        }
        else /* curr_action.flags & FunscriptAction::ModeFlagBits::Step */ {
            // No interpolation for step mode action, just calculate the interval.
            float pos_norm = curr_action.pos / 100.f;
            return { pos_norm, pos_norm, next_action.atS - time };
        }
    }
    else if (curr_action.atS == time) [[unlikely]] {
        LOG_ERROR("Funscript::get_interpolated_action: "
                  "exact match with curr_action shouldn't be possible.");
        std::abort();
        // float pos_norm = curr_action.pos / 100.f;
        // return {pos_norm, pos_norm, 0.};
    }
    else if (next_action.atS == time) {
        float pos_norm = next_action.pos / 100.f;
        return { pos_norm, pos_norm, 0. };
    }
    else {
        LOG_ERROR("Funscript::get_interpolated_action: "
                  "lower_bound didn't return a valid index.");
        std::abort();
    }
}

void Funscript::AddMultipleActions(const FunscriptArray& actions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    for (auto& action : actions) {
        data.Actions.emplace(action);
    }
    sortActions(data.Actions);
    notifyActionsChanged(true);
}


bool Funscript::EditAction(FunscriptAction oldAction, FunscriptAction newAction) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // update action
    auto act = getAction(oldAction);
    if (act != nullptr) {
        act->atS = newAction.atS;
        act->pos = newAction.pos;
        checkForInvalidatedActions();
        notifyActionsChanged(true);
        sortActions(data.Actions);
        return true;
    }
    return false;
}

void Funscript::AddEditAction(FunscriptAction action, float frameTime) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto close = getActionAtTime(data.Actions, action.atS, frameTime);
    if (close != nullptr) {
        *close = action;
        notifyActionsChanged(true);
        checkForInvalidatedActions();
    }
    else {
        AddAction(action);
    }
}

void Funscript::checkForInvalidatedActions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = std::remove_if(data.Selection.begin(), data.Selection.end(),
        [this](auto selected) {
            auto found = getAction(selected);
            return !found;
        });

    if (it != data.Selection.end()) {
        data.Selection.erase(it, data.Selection.end());
        notifySelectionChanged();
    }
}

void Funscript::RemoveAction(FunscriptAction action, bool checkInvalidSelection) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = data.Actions.find(action);
    if (it != data.Actions.end()) {
        data.Actions.erase(it);
        notifyActionsChanged(true);

        if (checkInvalidSelection) {
            checkForInvalidatedActions();
        }
    }
}

void Funscript::RemoveActions(const FunscriptArray& removeActions) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = std::remove_if(data.Actions.begin(), data.Actions.end(),
        [&removeActions, end = removeActions.end()](auto action) {
            if (removeActions.find(action) != end) {
                return true;
            }
            return false;
        });
    data.Actions.erase(it, data.Actions.end());

    notifyActionsChanged(true);
    checkForInvalidatedActions();
}

std::vector<FunscriptAction> Funscript::GetLastStroke(float time) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // TODO: refactor...
    // assuming "*it" is a peak bottom or peak top
    // if you went up it would return a down stroke and if you went down it would return a up stroke
    auto it = std::min_element(data.Actions.begin(), data.Actions.end(),
        [time](auto a, auto b) {
            return std::abs(a.atS - time) < std::abs(b.atS - time);
        });
    if (it == data.Actions.end() || it - 1 == data.Actions.begin()) return std::vector<FunscriptAction>(0);

    std::vector<FunscriptAction> stroke;
    stroke.reserve(5);

    // search previous stroke
    bool goingUp = (it - 1)->pos > it->pos;
    int32_t prevPos = (it - 1)->pos;
    for (auto searchIt = it - 1; searchIt != data.Actions.begin(); searchIt--) {
        if ((searchIt - 1)->pos > prevPos != goingUp) {
            break;
        }
        else if ((searchIt - 1)->pos == prevPos && (searchIt - 1)->pos != searchIt->pos) {
            break;
        }
        prevPos = (searchIt - 1)->pos;
        it = searchIt;
    }

    it--;
    if (it == data.Actions.begin()) return std::vector<FunscriptAction>(0);
    goingUp = !goingUp;
    prevPos = it->pos;
    stroke.emplace_back(*it);
    it--;
    for (;; it--) {
        bool up = it->pos > prevPos;
        if (up != goingUp) {
            break;
        }
        else if (it->pos == prevPos) {
            break;
        }
        stroke.emplace_back(*it);
        prevPos = it->pos;
        if (it == data.Actions.begin()) break;
    }
    return stroke;
}

void Funscript::SetActions(const FunscriptArray& override_with) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // data.Actions.clear();
    // data.Actions.assign(override_with.begin(), override_with.end());
    // sortActions(data.Actions);
    data.Actions = override_with;
    notifyActionsChanged(true);
}

void Funscript::RemoveActionsInInterval(float fromTime, float toTime) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    data.Actions.erase(
        std::remove_if(data.Actions.begin(), data.Actions.end(),
            [fromTime, toTime](auto action) {
                return action.atS >= fromTime && action.atS <= toTime;
            }),
        data.Actions.end());
    checkForInvalidatedActions();
    notifyActionsChanged(true);
}

void Funscript::RangeExtendSelection(int32_t rangeExtend) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto ExtendRange = [](std::vector<FunscriptAction*>& actions, int32_t rangeExtend) -> void {
        if (rangeExtend == 0) {
            return;
        }
        if (actions.size() < 0) {
            return;
        }

        auto StretchPosition = [](int32_t position, int32_t lowest, int32_t highest, int extension) -> int32_t {
            int32_t newHigh = Util::Clamp<int32_t>(highest + extension, 0, 100);
            int32_t newLow = Util::Clamp<int32_t>(lowest - extension, 0, 100);

            double relativePosition = (position - lowest) / (double)(highest - lowest);
            double newposition = relativePosition * (newHigh - newLow) + newLow;

            return Util::Clamp<int32_t>(newposition, 0, 100);
        };

        int lastExtremeIndex = 0;
        int32_t lastValue = (*actions[0]).pos;
        int32_t lastExtremeValue = lastValue;

        int32_t lowest = lastValue;
        int32_t highest = lastValue;

        enum class direction {
            NONE,
            UP,
            DOWN
        };
        direction strokeDir = direction::NONE;

        for (int index = 0; index < actions.size(); index++) {
            // Direction unknown
            if (strokeDir == direction::NONE) {
                if ((*actions[index]).pos < lastExtremeValue) {
                    strokeDir = direction::DOWN;
                }
                else if ((*actions[index]).pos > lastExtremeValue) {
                    strokeDir = direction::UP;
                }
            }
            else {
                if (((*actions[index]).pos < lastValue && strokeDir == direction::UP) // previous was highpoint
                    || ((*actions[index]).pos > lastValue && strokeDir == direction::DOWN) // previous was lowpoint
                    || (index == actions.size() - 1)) // last action
                {
                    for (int i = lastExtremeIndex + 1; i < index; i++) {
                        FunscriptAction& action = *actions[i];
                        action.pos = StretchPosition(action.pos, lowest, highest, rangeExtend);
                    }

                    lastExtremeValue = (*actions[index - 1]).pos;
                    lastExtremeIndex = index - 1;

                    highest = lastExtremeValue;
                    lowest = lastExtremeValue;

                    strokeDir = (strokeDir == direction::UP) ? direction::DOWN : direction::UP;
                }
            }
            lastValue = (*actions[index]).pos;
            if (lastValue > highest)
                highest = lastValue;
            if (lastValue < lowest)
                lowest = lastValue;
        }
    };
    std::vector<FunscriptAction*> rangeExtendSelection;
    rangeExtendSelection.reserve(SelectionSize());
    int selectionOffset = 0;
    for (auto&& act : data.Actions) {
        for (int i = selectionOffset; i < data.Selection.size(); i++) {
            if (data.Selection[i] == act) {
                rangeExtendSelection.push_back(&act);
                selectionOffset = i;
                break;
            }
        }
    }
    if (rangeExtendSelection.size() == 0) {
        return;
    }
    ClearSelection();
    ExtendRange(rangeExtendSelection, rangeExtend);
}

bool Funscript::ToggleSelection(FunscriptAction action) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = data.Selection.find(action);
    bool isSelected = it != data.Selection.end();
    if (isSelected) {
        data.Selection.erase(it);
    }
    else {
        data.Selection.emplace(action);
    }
    notifySelectionChanged();
    return !isSelected;
}

void Funscript::SetSelected(FunscriptAction action, bool selected) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = data.Selection.find(action);
    bool isSelected = it != data.Selection.end();
    if (isSelected && !selected) {
        data.Selection.erase(it);
    }
    else if (!isSelected && selected) {
        data.Selection.emplace(action);
    }
    notifySelectionChanged();
}

void Funscript::SelectTopActions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.size() < 3) return;
    std::vector<FunscriptAction> deselect;
    for (int i = 1; i < data.Selection.size() - 1; i++) {
        auto& prev = data.Selection[i - 1];
        auto& current = data.Selection[i];
        auto& next = data.Selection[i + 1];

        auto& min1 = prev.pos < current.pos ? prev : current;
        auto& min2 = min1.pos < next.pos ? min1 : next;
        deselect.emplace_back(min1);
        if (min1.atS != min2.atS) deselect.emplace_back(min2);
    }
    for (auto& act : deselect) SetSelected(act, false);
    notifySelectionChanged();
}

void Funscript::SelectBottomActions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.size() < 3) return;
    std::vector<FunscriptAction> deselect;
    for (int i = 1; i < data.Selection.size() - 1; i++) {
        auto& prev = data.Selection[i - 1];
        auto& current = data.Selection[i];
        auto& next = data.Selection[i + 1];

        auto& max1 = prev.pos > current.pos ? prev : current;
        auto& max2 = max1.pos > next.pos ? max1 : next;
        deselect.emplace_back(max1);
        if (max1.atS != max2.atS) deselect.emplace_back(max2);
    }
    for (auto& act : deselect) SetSelected(act, false);
    notifySelectionChanged();
}

void Funscript::SelectMidActions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.size() < 3) return;
    auto selectionCopy = data.Selection;
    SelectTopActions();
    auto topPoints = data.Selection;
    data.Selection = selectionCopy;
    SelectBottomActions();
    auto bottomPoints = data.Selection;

    selectionCopy.erase(std::remove_if(selectionCopy.begin(), selectionCopy.end(),
                            [&topPoints, &bottomPoints](auto val) {
                                return std::any_of(topPoints.begin(), topPoints.end(), [val](auto a) { return a == val; })
                                    || std::any_of(bottomPoints.begin(), bottomPoints.end(), [val](auto a) { return a == val; });
                            }),
        selectionCopy.end());
    data.Selection = selectionCopy;
    sortSelection();
    notifySelectionChanged();
}

void Funscript::SelectTime(float fromTime, float toTime, bool clear) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (clear)
        ClearSelection();

    for (auto& action : data.Actions) {
        if (action.atS >= fromTime && action.atS <= toTime) {
            ToggleSelection(action);
        }
        else if (action.atS > toTime)
            break;
    }

    if (!clear)
        sortSelection();
    notifySelectionChanged();
}

FunscriptArray Funscript::GetSelection(float fromTime, float toTime) noexcept
{
    FunscriptArray selection;
    if (!data.Actions.empty()) {
        auto start = data.Actions.lower_bound(FunscriptAction(fromTime, 0));
        auto end = data.Actions.upper_bound(FunscriptAction(toTime, 0));
        for (; start != end; ++start) {
            auto action = *start;
            if (action.atS >= fromTime && action.atS <= toTime) {
                selection.emplace_back_unsorted(action);
            }
        }
    }
    return selection;
}

void Funscript::SelectAction(FunscriptAction select) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto action = GetAction(select);
    if (action != nullptr) {
        if (ToggleSelection(select)) {
            // keep selection ordered for rendering purposes
            sortSelection();
        }
        notifySelectionChanged();
    }
}

void Funscript::DeselectAction(FunscriptAction deselect) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto action = GetAction(deselect);
    if (action != nullptr)
        SetSelected(*action, false);
    notifySelectionChanged();
}

void Funscript::SelectAll() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ClearSelection();
    data.Selection.assign(data.Actions.begin(), data.Actions.end());
    notifySelectionChanged();
}

void Funscript::RemoveSelectedActions() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.size() == data.Actions.size()) {
        // assume data.selection == data.Actions
        // aslong as we don't fuck up the selection this is safe
        data.Actions.clear();
    }
    else {
        RemoveActions(data.Selection);
    }

    ClearSelection();
    notifyActionsChanged(true);
    notifySelectionChanged();
}

void Funscript::moveAllActionsTime(float timeOffset)
{
    OFS_PROFILE(__FUNCTION__);
    ClearSelection();
    for (auto& move : data.Actions) {
        move.atS += timeOffset;
    }
    notifyActionsChanged(true);
}

void Funscript::moveActionsPosition(std::vector<FunscriptAction*> moving, int32_t posOffset)
{
    OFS_PROFILE(__FUNCTION__);
    ClearSelection();
    for (auto move : moving) {
        move->pos += posOffset;
        move->pos = Util::Clamp<int16_t>(move->pos, 0, 100);
    }
    notifyActionsChanged(true);
}

void Funscript::sortActions(FunscriptArray& actions) noexcept
{
    std::sort(actions.begin(), actions.end());
}

void Funscript::MoveSelectionTime(float timeOffset, float frameTime) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!HasSelection()) return;

    // faster path when everything is selected
    if (data.Selection.size() == data.Actions.size()) {
        moveAllActionsTime(timeOffset);
        SelectAll();
        return;
    }

    auto prev = GetPreviousActionBehind(data.Selection.front().atS);
    auto next = GetNextActionAhead(data.Selection.back().atS);

    auto min_bound = 0.f;
    auto max_bound = std::numeric_limits<float>::max();

    if (timeOffset > 0) {
        if (next != nullptr) {
            max_bound = next->atS - frameTime;
            timeOffset = std::min(timeOffset, max_bound - data.Selection.back().atS);
        }
    }
    else {
        if (prev != nullptr) {
            min_bound = prev->atS + frameTime;
            timeOffset = std::max(timeOffset, min_bound - data.Selection.front().atS);
        }
    }

    FunscriptArray newSelection;
    newSelection.reserve(data.Selection.size());
    for (auto selected : data.Selection) {
        auto move = getAction(selected);
        if (move) {
            FunscriptAction newAction = *move;
            newAction.atS += timeOffset;
            newSelection.emplace(newAction);
            RemoveAction(*move, false);
            AddAction(newAction);
        }
    }
    ClearSelection();
    data.Selection = std::move(newSelection);
    notifyActionsChanged(true);
}

void Funscript::MoveSelectionPosition(int32_t pos_offset) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (!HasSelection()) return;
    std::vector<FunscriptAction*> moving;

    // faster path when everything is selected
    if (data.Selection.size() == data.Actions.size()) {
        for (auto& action : data.Actions)
            moving.push_back(&action);
        moveActionsPosition(moving, pos_offset);
        SelectAll();
        return;
    }

    for (auto& find : data.Selection) {
        auto m = getAction(find);
        if (m != nullptr)
            moving.push_back(m);
    }

    ClearSelection();
    for (auto move : moving) {
        move->pos += pos_offset;
        move->pos = Util::Clamp<int16_t>(move->pos, 0, 100);
        data.Selection.emplace_back_unsorted(*move);
    }
    sortSelection();
    notifyActionsChanged(true);
}

void Funscript::SetSelection(const FunscriptArray& actionsToSelect) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    ClearSelection();
    for (auto& action : actionsToSelect) {
        data.Selection.emplace(action);
    }
    notifySelectionChanged();
}

bool Funscript::IsSelected(FunscriptAction action) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    auto it = data.Selection.find(action);
    return it != data.Selection.end();
}

void Funscript::EqualizeSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.size() < 3) return;
    sortSelection(); // might be unnecessary
    auto first = data.Selection.front();
    auto last = data.Selection.back();
    float duration = last.atS - first.atS;
    float stepTime = duration / (float)(data.Selection.size() - 1);

    auto copySelection = data.Selection;
    RemoveSelectedActions(); // clears selection

    for (int i = 1; i < copySelection.size() - 1; i++) {
        auto& newAction = copySelection[i];
        newAction.atS = first.atS + i * stepTime;
    }

    for (auto& action : copySelection)
        AddAction(action);

    data.Selection = std::move(copySelection);
}

void Funscript::InvertSelection() noexcept
{
    OFS_PROFILE(__FUNCTION__);
    if (data.Selection.empty()) return;
    auto copySelection = data.Selection;
    RemoveSelectedActions();
    for (auto& act : copySelection) {
        act.pos = std::abs(act.pos - 100);
        AddAction(act);
    }
    data.Selection = copySelection;
}

void Funscript::UpdateRelativePath(const std::string& path) noexcept
{
    currentPathRelative = path;

    if (!title.empty()) {
        EV::Enqueue<FunscriptNameChangedEvent>(this, title);
    }
    title = Util::PathFromString(currentPathRelative)
                .replace_extension("")
                .filename()
                .u8string();
}

static std::string::const_iterator
__funscript_parse_from_csv_skip_to_newline(
    const std::string& str, std::string::const_iterator start) noexcept
{
    while (start != str.end()) {
        // Always skip one.
        ++start;
        if (*start == '\n') {
            break;
        }
    }
    return start;
}
static std::string::const_iterator
__funscript_parse_from_csv_skip_after_comma(
    const std::string& str, std::string::const_iterator start) noexcept
{
    while (start != str.end()) {
        if (*start == ',') {
            ++start;
            break;
        }
        ++start;
    }
    return start;
}
static std::optional<std::pair<std::string::const_iterator, std::string::const_iterator>>
__funscript_parse_from_csv_find_number_bounds(
    const std::string& str, const std::string::const_iterator begin) noexcept
{
    if (begin == str.end()) [[unlikely]] {
        // Invalid program state!
        return {};
    }
    std::string::const_iterator cur = begin;
    std::string::const_iterator start, end;
    while (cur != str.end()) {
        if (std::isdigit(*cur)) {
            start = cur;
            break;
        }
        ++cur;
    }
    while (cur != str.end() && std::isdigit(*cur)) {
        ++cur;
    }
    end = cur;
    return std::make_pair(start, end);
}
bool Funscript::ParseFromCsv(const std::string& csvText) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    // An extremely dumb and single-purpose csv parser - TODO: testing.
    auto row_start_mark = csvText.begin();
    auto row_end_mark = __funscript_parse_from_csv_skip_to_newline(csvText, row_start_mark);

    while (row_start_mark != csvText.end()) {
        // Skip rows with no records.
        size_t comma_count = std::count(row_start_mark, row_end_mark, ',');
        if (comma_count >= 2) {
            // Find the records we care about.
            const auto time_bounds = __funscript_parse_from_csv_find_number_bounds(csvText, row_start_mark);
            if (!time_bounds.has_value()) [[unlikely]] {
                return false;
            }
            row_start_mark = __funscript_parse_from_csv_skip_after_comma(csvText, time_bounds->second);
            const auto dir_bounds = __funscript_parse_from_csv_find_number_bounds(csvText, row_start_mark);
            if (!dir_bounds.has_value()) [[unlikely]] {
                return false;
            }
            row_start_mark = __funscript_parse_from_csv_skip_after_comma(csvText, dir_bounds->second);
            const auto spd_bounds = __funscript_parse_from_csv_find_number_bounds(csvText, row_start_mark);
            if (!spd_bounds.has_value()) [[unlikely]] {
                return false;
            }

            // Verify that we haven't crossed row boundaries.
            if (row_start_mark > row_end_mark) [[unlikely]] {
                return false;
            }

            // Parse action from current row.
            auto csv_time = std::numeric_limits<uint32_t>::max();
            auto csv_dir = std::numeric_limits<uint16_t>::max();
            auto csv_spd = std::numeric_limits<uint16_t>::max();

            auto csv_time_r = std::from_chars(&*time_bounds->first, &*time_bounds->second, csv_time);
            if (csv_time_r.ec != std::errc{} || csv_time_r.ptr != &*time_bounds->second) [[unlikely]] {
                return false;
            }
            auto csv_dir_r = std::from_chars(&*dir_bounds->first, &*dir_bounds->second, csv_dir);
            if (csv_dir_r.ec != std::errc{} || csv_dir_r.ptr != &*dir_bounds->second) [[unlikely]] {
                return false;
            }
            auto csv_spd_r = std::from_chars(&*spd_bounds->first, &*spd_bounds->second, csv_spd);
            if (csv_spd_r.ec != std::errc{} || csv_spd_r.ptr != &*spd_bounds->second) [[unlikely]] {
                return false;
            }

            int16_t centered_pos = csv_dir ? -csv_spd : csv_spd;
            centered_pos = (centered_pos / 2) + 50;

            data.Actions.emplace(static_cast<float>(csv_time) * 0.1f, centered_pos, FunscriptAction::ModeFlagBits::Step);
        }

        // Quick exit when we've reached the last row.
        if (row_end_mark == csvText.end()) {
            return true;
        }
        // Find next row.
        row_start_mark = row_end_mark + 1;
        row_end_mark = __funscript_parse_from_csv_skip_to_newline(csvText, row_start_mark);
    }

    return true;
}

bool Funscript::Deserialize(const nlohmann::json& json, Funscript::Metadata* outMetadata, bool loadChapters) noexcept
{
    OFS_PROFILE(__FUNCTION__);

    if (!json.is_object() || !json["actions"].is_array()) {
        LOG_ERROR("Failed to load Funscript. No action array found.");
        return false;
    }

    auto& jsonActions = json["actions"];
    data.Actions.clear();

    for (auto& action : jsonActions) {
        float time = action["at"].get<double>() / 1000.0;
        int32_t pos = action["pos"];
        if (time >= 0.f) {
            data.Actions.emplace(time, Util::Clamp(pos, 0, 100));
        }
    }

    if (outMetadata) {
        if (json.contains("metadata")) {
            loadMetadata(json["metadata"], *outMetadata);
        }
        else {
            *outMetadata = Funscript::Metadata();
        }
    }

    if (loadChapters && json.contains("metadata")) {
        auto& chapterState = ChapterState::StaticStateSlow();
        auto& jsonMetadata = json["metadata"];

        if (jsonMetadata.contains("bookmarks")) {
            auto& jsonBookmarks = jsonMetadata["bookmarks"];
            if (jsonBookmarks.is_array()) {
                for (auto& jsonBookmark : jsonBookmarks) {
                    if (!jsonBookmark.contains("name") || !jsonBookmark.contains("time"))
                        continue;
                    if (!jsonBookmark["name"].is_string() || !jsonBookmark["time"].is_string())
                        continue;

                    auto name = std::move(jsonBookmark["name"].get<std::string>());
                    auto timeStr = std::move(jsonBookmark["time"].get<std::string>());

                    bool succ = false;
                    float time = Util::ParseTime(timeStr.c_str(), &succ);
                    if (!succ) {
                        LOGF_ERROR("Failed to parse \"%s\" to time", timeStr.c_str());
                        continue;
                    }

                    if (auto bookmark = chapterState.AddBookmark(time)) {
                        bookmark->name = std::move(name);
                    }
                }
            }
        }

        if (jsonMetadata.contains("chapters")) {
            auto& jsonChapters = jsonMetadata["chapters"];
            if (jsonChapters.is_array()) {
                for (auto& jsonChapter : jsonChapters) {
                    if (!jsonChapter.contains("name") || !jsonChapter.contains("startTime") || !jsonChapter.contains("endTime"))
                        continue;
                    if (!jsonChapter["name"].is_string() || !jsonChapter["startTime"].is_string() || !jsonChapter["endTime"].is_string())
                        continue;

                    auto name = std::move(jsonChapter["name"].get<std::string>());
                    auto startTimeStr = std::move(jsonChapter["startTime"].get<std::string>());
                    auto endTimeStr = std::move(jsonChapter["endTime"].get<std::string>());

                    bool succ = false;
                    float startTime = Util::ParseTime(startTimeStr.c_str(), &succ);
                    if (!succ) {
                        LOGF_ERROR("Failed to parse \"%s\" to time", startTimeStr.c_str());
                        continue;
                    }
                    float endTime = Util::ParseTime(endTimeStr.c_str(), &succ);
                    if (!succ) {
                        LOGF_ERROR("Failed to parse \"%s\" to time", endTimeStr.c_str());
                        continue;
                    }

                    if (startTime > endTime)
                        continue;

                    // Insert chapter at the middle point
                    float middlePoint = startTime + ((endTime - startTime) / 2.f);
                    if (auto chapter = chapterState.AddChapter(middlePoint, 1.f)) {
                        chapter->name = std::move(name);
                        // Set size is used to safely resize the chapter to the correct size
                        chapterState.SetChapterSize(*chapter, startTime);
                        chapterState.SetChapterSize(*chapter, endTime);
                    }
                }
            }
        }
    }

    notifyActionsChanged(false);
    return true;
}

void Funscript::Serialize(nlohmann::json& json, const FunscriptData& funscriptData, const Funscript::Metadata& metadata, bool includeChapters) noexcept
{
    OFS_PROFILE(__FUNCTION__);
    json = nlohmann::json::object();
    json["actions"] = nlohmann::json::array();
    json["metadata"] = nlohmann::json::object();
    json["version"] = "1.0";
    json["inverted"] = false;
    json["range"] = 100;

    auto& jsonMetadata = json["metadata"];
    OFS::Serializer<false>::Serialize(metadata, jsonMetadata);
    if (includeChapters) {
        auto& chapters = ChapterState::StaticStateSlow();
        {
            auto jsonBookmarks = nlohmann::json::array();
            for (auto& bookmark : chapters.bookmarks) {
                nlohmann::json jsonBookmark = {
                    { "name", bookmark.name },
                    { "time", std::move(bookmark.TimeToString()) }
                };
                jsonBookmarks.emplace_back(std::move(jsonBookmark));
            }
            jsonMetadata["bookmarks"] = std::move(jsonBookmarks);
        }
        {
            auto jsonChapters = nlohmann::json::array();
            for (auto& chapter : chapters.chapters) {
                nlohmann::json jsonChapter = {
                    { "name", chapter.name },
                    { "startTime", std::move(chapter.StartTimeToString()) },
                    { "endTime", std::move(chapter.EndTimeToString()) },
                };
                jsonChapters.emplace_back(std::move(jsonChapter));
            }
            jsonMetadata["chapters"] = std::move(jsonChapters);
        }
    }

    auto& jsonActions = json["actions"];
    jsonActions.clear();

    int64_t lastTimestamp = -1;
    for (auto action : funscriptData.Actions) {
        // a little validation just in case
        if (action.atS < 0.f)
            continue;

        int64_t ts = (int64_t)std::round(action.atS * 1000.0);
        // make sure timestamps are unique
        if (ts != lastTimestamp) {
            nlohmann::json actionObj = {
                { "at", ts },
                { "pos", Util::Clamp<int32_t>(action.pos, 0, 100) }
            };
            jsonActions.emplace_back(std::move(actionObj));
            lastTimestamp = ts;
        }
        else {
            LOG_WARN("Action was ignored since it had the same millisecond timestamp as the previous one.");
        }
    }
}