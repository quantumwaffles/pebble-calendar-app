# Calendar Note-Taking App Implementation Checklist

## Confirmed Requirements

### Calendar Navigation
- Default navigation mode is **day**.
- **Select (single press)** toggles between **day** and **week** navigation modes.
- Navigation mode indicators:
  - **Day mode**: show arrows around the selected day, for example `< 29 >`
  - **Week mode**: show arrows vertically above and below the selected day
- **Up** moves backward in time according to the current navigation mode (1 day or 1 week).
- **Down** moves forward in time according to the current navigation mode (1 day or 1 week).
- **Holding Up or Down** repeats the movement continuously for fast navigation.

### Actions / Notes Menu
- **Select (long press)** opens a secondary actions menu.
- The actions menu contains:
  1. **Go To Today**: jump calendar selection to the current date
  2. **Add Note**: open voice dictation and save a new note for the selected date
  3. **Existing note entries list**: selecting an entry opens the full note view
- In the full note view:
  - **Select** deletes the note
  - Deletion requires confirmation
  - **Back** returns to the actions menu
- Pressing **Back** from the actions menu returns to the calendar view.

### Assumptions to Validate During Implementation
- New notes are associated with the **currently selected date**.
- Existing note entries shown in the menu are the entries for the **currently selected date**.
- Voice dictation uses the existing or available Pebble-compatible input flow rather than keyboard text entry.

---

## Phase 1 - Navigation State and Input Model ✅
- [x] Add navigation mode state with two modes: **day**, **week**
- [x] Make **single Select press** toggle between day and week modes
- [x] Update **Up/Down** handlers to move the selected date by 1 day or 1 week based on the current mode
- [x] Keep date transitions correct across month/year boundaries and variable month lengths
- [x] Preserve **day mode** as the default on app launch

### Checkpoint ✅
- [x] Validated that Select toggles between day and week modes
- [x] Validated that **Up** always moves backward and **Down** always moves forward using the active increment
- [x] Validated edge cases such as month-end and year rollover

---

## Phase 1.1 - Remove Month Mode and Add Hold-to-Repeat Navigation ✅
- [x] Remove `NAV_MONTH` from the `NavMode` enum; keep only `NAV_DAY` and `NAV_WEEK`
- [x] Change Select to toggle between `NAV_DAY` and `NAV_WEEK` (no cycle, just flip)
- [x] Add **repeating click** (hold) support for Up button
- [x] Add **repeating click** (hold) support for Down button
- [x] Tune repeat interval to a comfortable fast-scroll speed (100ms)

### Checkpoint ✅
- [x] Validated that Select only toggles between two modes (no third mode)
- [x] Validated that holding Up navigates backward continuously until released
- [x] Validated that holding Down navigates forward continuously until released
- [x] Validated that tap (single press) still works correctly for both Up and Down

---

## Phase 2 - Calendar UI Indicators for Navigation Mode ✅
- [x] Add a **day mode** visual indicator using small filled triangles left and right of the selected day
- [x] Add a **week mode** visual indicator using small filled triangles above and below the selected day
- [x] Ensure only the active mode indicator is shown at a time
- [x] Keep the calendar readable within Pebble display constraints (bounds-checked before drawing)
- [x] Handle case where today == selected date (filled square + arrows)

### Checkpoint ✅
- [x] Validate that day mode shows left/right arrow triangles flanking the selected day
- [x] Validate that week mode shows up/down arrow triangles above/below the selected day
- [x] Validate that switching modes updates the arrows immediately
- [x] Validate that arrows are not drawn outside display bounds (e.g. selected day in first row in week mode)

### Future UI Note
- Consider replacing arrow indicators with a **drag handle bar** style if that reads better on-device.

---

## Phase 3 - Secondary Actions Menu Flow ✅
- [x] Add **long Select press** handling from the calendar view
- [x] Open a secondary menu with sections for:
  - **Go To Today**
  - **Add Note**
  - **Existing note entries**
- [x] Implement **Back** behavior from the actions menu to return to the calendar view
- [x] Ensure the menu refreshes its note list for the currently selected date

### Checkpoint ✅
- [x] Validate that long Select reliably opens the actions menu from the calendar
- [x] Validate that **Back** exits the actions menu to the calendar without losing selection state
- [x] Validate that the actions menu structure matches the requested order and separators
- [x] Validate that **Go To Today** returns selection to today from the menu

---

## Phase 4 - Go To Today and Note Creation ✅
- [x] Implement **Go To Today** to move selection to the current date and refresh the calendar display
- [x] Implement **Add Note** entry point from the actions menu
- [x] Open the voice dictation interface for note capture
- [x] Save the captured note against the selected date
- [x] Refresh the actions menu note list after saving
- [x] Persist saved notes across app restarts using on-watch storage

### Checkpoint ✅
- [x] Validate that **Go To Today** always returns to the actual current date
- [x] Validate that voice dictation opens from **Add Note**
- [x] Validate that a saved note appears in the existing entries list for the selected date

---

## Phase 4.1 - Calendar Note Indicators
- [x] Add a calendar indicator for dates that have one or more saved notes
- [x] Render the indicator as a compact **3px bar above the day number**
- [x] Ensure the indicator coexists cleanly with the selected-day outline/fill and navigation arrows
- [x] Show the indicator for both selected and non-selected days with notes
- [x] Keep the indicator visible without making the calendar feel crowded

### Checkpoint
- [ ] Validate that days with notes show the indicator in the calendar view
- [ ] Validate that days without notes show no indicator
- [ ] Validate that the indicator remains legible for today, selected, and today+selected states
- [ ] Validate that the indicator does not clash with week/day navigation affordances
- [ ] **Pause for review before continuing to Phase 5**

---

## Phase 5 - Note Viewing and Deletion Flow
- [ ] Open a full note view when an existing note entry is selected
- [ ] Implement **Back** from note view to return to the actions menu
- [ ] Implement **Select** in note view to begin note deletion
- [ ] Add a deletion confirmation step before the note is removed
- [ ] Remove the note from storage and refresh the entries list after confirmation

### Checkpoint
- [ ] Validate that selecting a note opens the correct full note content
- [ ] Validate that **Back** returns to the actions menu
- [ ] Validate that deletion requires confirmation and only removes the intended note
- [ ] Validate that deleted notes no longer appear in the entry list
- [ ] **Pause for review before continuing to Phase 6**

---

## Phase 6 - Integration, Persistence, and Polish
- [ ] Verify note persistence across app restarts
- [ ] Verify that notes remain correctly associated with their dates
- [ ] Review interaction conflicts between short Select, long Select, Back, Up, and Down
- [ ] Polish menu labels and empty-state messaging for dates without notes
- [ ] Update any directly related project documentation or usage notes

### Checkpoint
- [ ] Validate the complete end-to-end flow: navigate -> open menu -> add note -> view note -> delete note
- [ ] Validate persistence after restart
- [ ] Validate that navigation and note flows do not interfere with one another
- [ ] **Pause for final review before implementation is considered complete**
