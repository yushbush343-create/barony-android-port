# Custom classes

This fork loads additional character classes from `custom/classes.json`
through PhysicsFS. Full Android game builds package the repository's default
file in the APK. Changes made in the in-game editor are stored in the writable
app-data directory at `mods/custom/classes.json` and mounted at startup.

## Version 2 format

Use a unique numeric `id` for each class. The game ID is
`NUMCLASSES + id`; keep the ID unchanged after players have created saves
with that class. Current network packets require the resulting game ID to be
no greater than 127, so the current valid custom ID range is 0–101. The
loader accepts up to 64 class definitions. This fork currently includes 15
classes with IDs 0–14; use an unused ID when adding another class.

```json
{
  "version": 2,
  "classes": [
    {
      "id": 15,
      "internal_name": "dune_sentinel",
      "display_name": "Dune Sentinel",
      "description": "A sample class definition with a stable custom ID.",
      "max_hp": 0,
      "max_mp": 10,
      "STR": -1,
      "DEX": 1,
      "CON": 1,
      "INT": 2,
      "PER": 1,
      "CHR": 1,
      "proficiencies": [],
      "starting_spells": [],
      "starting_items": []
    }
  ]
}
```

## Fields

- `id`: stable unique integer from 0 to 101.
- `internal_name`: unique identifier for the class.
- `display_name`: name shown in character creation; maximum 63 characters.
- `description`: optional description shown in character creation; maximum
  512 characters.
- `allowed_races`: optional array of numeric race IDs from the source
  `RACE_*` enum. If omitted or empty, the class is available to Humans only.
- `max_hp`, `max_mp`: additive changes to maximum and current HP/MP.
- `STR`, `DEX`, `CON`, `INT`, `PER`, `CHR`: additive stat changes.
- `proficiencies`: flat pairs of proficiency ID and starting value. Values
  must be from 0 to 100.
- `starting_spells`: array of spell IDs from the source spell enum.
- `starting_items`: array of item IDs from the source item enum.

The loader rejects malformed files, duplicate IDs/names, out-of-range IDs,
invalid item/spell/proficiency references, and files larger than 1 MiB. Version
1 files are still accepted for compatibility, but their IDs are assigned by
array order; version 2 with explicit IDs is recommended.

## In-game editor

Open **Custom Class Editor** from the lower-right corner of the title menu. It supports creating, editing, saving, and deleting custom classes.
Changes are written to the game's writable PhysicsFS directory at
`mods/custom/classes.json` and are loaded on the next game start.

The list fields accept comma-separated numeric IDs:
- Allowed races: race IDs from the source `RACE_*` enum. Leave blank for Humans.
- Proficiencies: alternating proficiency ID and starting value, e.g.
  `2,25,5,40`.
- Starting spells: spell IDs from the source spell enum.
- Starting items: item IDs from the source item enum.

The editor validates the values when Save is pressed. It does not create new
spell or item definitions; those still need to exist in the game data.

## Current limitations

Custom classes currently support stat modifiers, starting proficiencies,
starting items, starting spells, and race restrictions. Their hotbar layouts
still use the built-in class system and are not yet separately configurable.
The editor does not create new spell or item definitions, and custom passive ability
definitions are a later feature.
