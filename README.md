# pebble-calendar-app

Monthly calendar watchapp for Pebble/Rebble.

## App description metadata

This project already sets the Pebble bundle fields that are carried in the
`.pbw`, such as `pebble.displayName`, `pebble.uuid`, and `version`.

The blank description in the Pebble/Rebble phone app is expected if the app has
not been given an App Store listing description for its UUID.

Pebble's project metadata does **not** include a bundle description field read
from `package.json`, so the top-level npm `description` does not populate the
phone app entry by itself.

To show a description, create or update the App Store listing for UUID
`02905d6b-a10b-4525-a111-35ce09a7deba` and set the listing description there.

Suggested listing description:

> A simple monthly calendar for Pebble and Rebble. Browse months with Up and
> Down, hold Select to jump back to today, and customize foreground and
> background colors from the phone app.
