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

## Phase 1.1 - Remove Month Mode and Add Hold-to-Repeat Navigation
- [ ] Remove `NAV_MONTH` from the `NavMode` enum; keep only `NAV_DAY` and `NAV_WEEK`
- [ ] Change Select to toggle between `NAV_DAY` and `NAV_WEEK` (no cycle, just flip)
- [ ] Add **repeating click** (hold) support for Up button
- [ ] Add **repeating click** (hold) support for Down button
- [ ] Tune repeat interval to a comfortable fast-scroll speed

### Checkpoint
- [ ] Validate that Select only toggles between two modes (no third mode)
- [ ] Validate that holding Up navigates backward continuously until released
- [ ] Validate that holding Down navigates forward continuously until released
- [ ] Validate that tap (single press) still works correctly for both Up and Down
- [ ] **Pause for review before continuing to Phase 2**

---

## Phase 2 - Calendar UI Indicators for Navigation Mode
- [ ] Add a **day mode** visual indicator using horizontal arrows around the selected day
- [ ] Add a **week mode** visual indicator using vertical arrows above and below the selected day
- [ ] Ensure only the active mode indicator is shown at a time
- [ ] Keep the calendar readable within Pebble display constraints

### Checkpoint
- [ ] Validate that each mode renders the correct indicator and only for the active mode
- [ ] Validate that the selected date remains visually clear in both modes
- [ ] Validate layout on device/emulator without overlap or clipped text
- [ ] **Pause for review before continuing to Phase 3**

---

## Phase 3 - Secondary Actions Menu Flow
- [ ] Add **long Select press** handling from the calendar view
- [ ] Open a secondary menu with sections for:
  - **Go To Today**
  - **Add Note**
  - **Existing note entries**
- [ ] Implement **Back** behavior from the actions menu to return to the calendar view
- [ ] Ensure the menu refreshes its note list for the currently selected date

### Checkpoint
- [ ] Validate that long Select reliably opens the actions menu from the calendar
- [ ] Validate that **Back** exits the actions menu to the calendar without losing selection state
- [ ] Validate that the actions menu structure matches the requested order and separators
- [ ] **Pause for review before continuing to Phase 4**

---

## Phase 4 - Go To Today and Note Creation
- [ ] Implement **Go To Today** to move selection to the current date and refresh the calendar display
- [ ] Implement **Add Note** entry point from the actions menu
- [ ] Open the voice dictation interface for note capture
- [ ] Save the captured note against the selected date
- [ ] Refresh the actions menu note list after saving

### Checkpoint
- [ ] Validate that **Go To Today** always returns to the actual current date
- [ ] Validate that voice dictation opens from **Add Note**
- [ ] Validate that a saved note appears in the existing entries list for the selected date
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
