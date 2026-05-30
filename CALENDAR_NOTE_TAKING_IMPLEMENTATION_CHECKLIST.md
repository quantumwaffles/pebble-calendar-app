# Calendar Note-Taking App Implementation Checklist

## Confirmed Requirements

### Calendar Navigation
- Default navigation mode is **day**.
- **Select (single press)** cycles the navigation orientation used by the directional buttons:
  - **Day mode**: move selected date by **1 day**
  - **Week mode**: move selected date by **1 week**
  - **Month mode**: move selected date by **1 month**
- Navigation mode indicators:
  - **Day mode**: show arrows around the selected day, for example `< 29 >`
  - **Week mode**: show arrows vertically above and below the selected day
  - **Month mode**: show arrows around the month label, for example `< May 2026 >`
- **Up** moves backward in time according to the current navigation mode.
- **Down** moves forward in time according to the current navigation mode.

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

## Phase 1 - Navigation State and Input Model
- [ ] Add navigation mode state with three modes: **day**, **week**, **month**
- [ ] Make **single Select press** cycle modes in order: day -> week -> month -> day
- [ ] Update **Up/Down** handlers to move the selected date by 1 day, 1 week, or 1 month based on the current mode
- [ ] Keep date transitions correct across month/year boundaries and variable month lengths
- [ ] Preserve **day mode** as the default on app launch

### Checkpoint
- [ ] Validate that repeated **Select** presses cycle through all three modes and wrap correctly
- [ ] Validate that **Up** always moves backward and **Down** always moves forward using the active increment
- [ ] Validate edge cases such as month-end and year rollover
- [ ] **Pause for review before continuing to Phase 2**

---

## Phase 2 - Calendar UI Indicators for Navigation Mode
- [ ] Add a **day mode** visual indicator using horizontal arrows around the selected day
- [ ] Add a **week mode** visual indicator using vertical arrows above and below the selected day
- [ ] Add a **month mode** visual indicator using horizontal arrows around the month label
- [ ] Ensure only the active mode indicator is shown at a time
- [ ] Keep the calendar readable within Pebble display constraints

### Checkpoint
- [ ] Validate that each mode renders the correct indicator and only for the active mode
- [ ] Validate that the selected date remains visually clear in all modes
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
