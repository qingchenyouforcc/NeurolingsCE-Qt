# Shijima-Qt API Documentation

The HTTP API is disabled by default. It starts only when the Qt setting
`http/enabled` is explicitly set to `true`; local IPC remains the preferred
control interface. The server still binds only to `127.0.0.1`.

Base URL: http://127.0.0.1:32456/shijima/api/v1

## GET /ping

Returns API readiness metadata.

**Sample response:**

```json
{
    "ok": true,
    "app": "NeurolingsCE",
    "api_version": "v1"
}
```

## GET /mascots

Returns a list of mascots that are on the screen.

**Sample response:**

```json
{
    "mascots": [
        {
            "active_behavior": "ClimbIEWall",
            "anchor": {
                "x": 67.2,
                "y": 225.63864462595598
            },
            "data_id": 0,
            "id": 35,
            "label": 0,
            "name": "Default",
            "version": "1.0",
            "description": "Default mascot for the application.",
            "author": "pixelomer[https://github.com/pixelomer]"
        },
        {
            "active_behavior": "Fall",
            "anchor": {
                "x": 368,
                "y": 863
            },
            "data_id": 0,
            "id": 36,
            "name": "Default Mascot"
        }
    ]
}
```

## POST /mascots

Spawns a new mascot. One of `name` or `data_id` must be specified.

**Request format:**

```json
{
    "name": "string",
    "data_id": "int",
    "anchor": {
        "x": "double",
        "y": "double"
    },
    "behavior": "string"
}
```

**Sample request:**

```json
{
    "name": "Default Mascot",
    "anchor": {
        "x": 150,
        "y": 150
    },
    "behavior": "SplitIntoTwo"
}
```

**Sample response:**

```json
{
    "mascot": {
        "active_behavior": null,
        "anchor": {
            "x": 150,
            "y": 150
        },
        "data_id": 0,
        "id": 40,
        "name": "Default Mascot"
    }
}
```

## DELETE /mascots

Dismisses all mascots.

**Request format (optional):**

```json
{
    "selector": "string"
}
```

## GET /mascots/:id

Gets data for one mascot.

**Sample response:**

```json
{
    "mascot": {
        "active_behavior": "ClimbIEWall",
        "anchor": {
            "x": 67.2,
            "y": 225.63864462595598
        },
        "data_id": 0,
        "id": 35,
        "name": "Default Mascot"
    }
}
```

**404 response:**

```json
{
    "error": "No such mascot",
    "code": "mascot_not_found",
    "status": 404,
    "mascot": null
}
```

## PUT /mascots/:id

Alters the state of an existing mascot.

**Request format:**

```json
{
    "id": "int",
    "anchor": {
        "x": "double",
        "y": "double"
    },
    "behavior": "string"
}
```

**Sample request:**

```json
{
    "id": 4,
    "anchor": {
        "x": 150,
        "y": 150
    },
    "behavior": "SplitIntoTwo"
}
```

**Sample response:**

```json
{
    "mascot": {
        "active_behavior": "SitDown",
        "anchor": {
            "x": 150,
            "y": 150
        },
        "data_id": 79,
        "id": 4,
        "name": "Jenny"
    }
}
```

## GET /loadedMascots

Returns a list of mascots that are loaded into Shijima-Qt and can be spawned.

**Sample response:**

```json
{
    "loaded_mascots": [
        {
            "id": 0,
            "name": "Default Mascot"
        },
        {
            "id": 79,
            "name": "Jenny"
        },
        {
            "id": 78,
            "name": "niko"
        }
    ]
}
```

## GET /loadedMascots/:id

Returns information about a specific loaded mascot.

**Sample response:**

```json
{
    "loaded_mascot": {
        "id": 79,
        "name": "Jenny"
    }
}
```

**404 response:**

```json
{
    "error": "No such loaded mascot",
    "code": "loaded_mascot_not_found",
    "status": 404,
    "loaded_mascot": null
}
```

## GET /loadedMascots/:id/preview.png

Returns the preview image for a loaded mascot.

## CLI

NeurolingsCE also ships a dedicated console CLI binary, `NeurolingsCE-cli`,
for automation against a running instance.

**Global options:**

- `--quiet`
- `--json`
- `--connect-timeout-ms <ms>`
- `--read-timeout-ms <ms>`

**Document commands:**

- `--help`, `-h`
- `--summon`, `-s`
- `--close`
- `--close-all`
- `--stop`
- `--mascot`, `-m`
- `--list`, `-l`
- `--version`, `-v`

**Document command forms:**

- `--summon mascot --name NAME [label]`
- `--summon mascot --data-id ID [label]`
- `--summon random [label]`
- `--close LABEL`
- `--close-all`
- `--stop`
- `--mascot list`
- `--mascot add ZIP`
- `--mascot remove MASCOT`
- `--list`
- `--version`

**Legacy commands still supported:**

- `list`
- `list-loaded`
- `spawn`
- `alter`
- `dismiss`
- `dismiss-all`

**Notes:**

- `label` is a user-facing CLI label and is separate from the runtime mascot ID.
- Labels only live for the current NeurolingsCE process and are cleared on restart.
- `GET /mascots` may include a `label` field when a mascot has been assigned a CLI label.
- `--mascot list`, `--mascot add ZIP`, and `--mascot remove MASCOT` run standalone
  and directly manage the local template directory.
- `--stop` closes all mascots and stops the NeurolingsCE runtime. It is
  idempotent and does not auto-start the runtime if it is already stopped.
- Runtime control commands auto-start NeurolingsCE when no local runtime is ready,
  then use local IPC, not HTTP, to talk to it.
- `--host` and `--port` are no longer supported.
- On Windows, prefer `NeurolingsCE-cli.exe` so command shells can observe exit codes correctly.

`--json` emits stable machine-readable output on success and failure. Runtime
control commands will start the local runtime automatically when needed.
