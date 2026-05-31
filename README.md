# pebble-calendar-app

Monthly calendar and quick note watchapp for Pebble/Rebble.

## Features

- Browse the calendar by **day** or **week**
- Toggle navigation mode with a short **Select** press
- Hold **Up** or **Down** for fast repeated navigation
- Open an actions menu with a long **Select** press
- Jump back to today from the actions menu
- Add notes for the currently selected date using Pebble dictation
- Show note indicators directly in the calendar
- View full notes in a dedicated scrollable note screen
- Delete notes with a confirmation screen
- Customize foreground and background colors from the phone app

## Controls

### Calendar view

- **Up**: move backward by 1 day or 1 week
- **Down**: move forward by 1 day or 1 week
- **Hold Up / Down**: repeat navigation
- **Select**: toggle day/week navigation mode
- **Hold Select**: open the actions menu

### Actions menu

- **Select**: activate the highlighted item
- **Back**: return to the calendar view

Menu items:

1. **Go To Today**
2. **Add Note**
3. Existing notes for the selected date

### Note view

- **Up / Down**: scroll note text
- **Select**: open delete confirmation
- **Back**: return to the actions menu

### Delete confirmation

- **Up**: confirm delete
- **Down**: cancel
- **Back**: cancel

## Local build and install

This repo includes simple Windows helper scripts that run the Pebble SDK through
Docker using the `rebble/pebble-sdk` image.

### Prerequisites

- Docker Desktop or Rancher Desktop running
- Phone and PC on the same Wi-Fi network for LAN install
- Pebble/Rebble phone app developer connection enabled

### Build

```bat
build
```

Equivalent command:

```bat
docker run --rm -i -w /app -v "%CD%:/app" rebble/pebble-sdk pebble build
```

### Install to phone/watch over LAN

```bat
install 192.168.1.42
```

If you omit the IP, `install.cmd` reuses the last IP saved in `.pebble-ip`.
That file is local-only and ignored by git.

Equivalent command:

```bat
docker run --rm -i -w /app -v "%CD%:/app" rebble/pebble-sdk pebble install --phone <phone-ip>
```

## App description metadata

This project already sets the Pebble bundle fields carried in the `.pbw`, such
as `pebble.displayName`, `pebble.uuid`, and `version`.

The blank description in the Pebble/Rebble phone app is expected if the app has
not been given an App Store listing description for its UUID.

Pebble project metadata does **not** include a bundle description field read
from `package.json`, so the top-level npm `description` does not populate the
phone app entry by itself.

To show a description, create or update the App Store listing for UUID
`02905d6b-a10b-4525-a111-35ce09a7deba` and set the listing description there.

Suggested listing description:

> A monthly calendar for Pebble and Rebble with fast day/week navigation,
> voice notes, note indicators, and customizable colors.
