# Custom classes

This fork loads additional character classes from `custom/classes.json`.
The file is read through PhysicsFS, so a mod ZIP can provide its own file at
that path. A full Android game build also packages the repository's default
file in the APK.

## Version 2 format

Use a unique numeric `id` for each class. The game ID is
`NUMCLASSES + id`; keep the ID unchanged after players have created saves
with that class. Current network packets require the resulting game ID to be
no greater than 127, so the current valid custom ID range is 0–101.

```json
{
  "version": 2,
  "classes": [
    {
      "id": 0,
      "internal_name": "field_medic",
      "display_name": "Field Medic",
      "description": "A support class trained to keep a party alive.",
      "max_hp": 0,
      "max_mp": 10,
      "STR": -1,
      "DEX": 1,
      "CON": 1,
      "INT": 2,
      "PER": 1,
      "CHR": 1,
      "proficiencies": [12, 25, 18, 15],
      "starting_spells": [4],
      "starting_items": [0]
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

## Current limitations

Custom classes currently support stat modifiers, starting proficiencies,
starting items, starting spells, and race restrictions. Their hotbar layouts
still use the built-in class system and are not yet separately configurable.
This JSON loader is not yet an in-game class editor, and custom passive ability
definitions are a later feature.
