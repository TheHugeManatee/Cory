# Subagent tool

A Pi extension tool that launches a separate `pi` process with a model selected from a size-to-model mapping.

## Config

Supported locations, merged in order:

- `~/.pi/agent/subagent.json`
- `~/.pi/agent/subagent.config.json`
- `./.pi/subagent.json`
- `./.pi/subagent.config.json`
- `PI_SUBAGENT_CONFIG=/path/to/file.json`

## Example

See `config.example.json`.

Each size entry can be either:

```json
"small": "anthropic/claude-haiku-4-5"
```

or:

```json
"small": {
  "model": "anthropic/claude-haiku-4-5",
  "tools": ["read", "grep"],
  "systemPrompt": "You are a scout."
}
```

## Tool params

- `modelSize` - selects the mapped model size
- `taskName` - short label shown while the sub-agent is running and included in the sub-agent prompt
  - live display format: `subagent: <taskName> · ttfb <seconds> · <tokens/sec>`
  - next line shows the task summary
  - final lines show `running...` until output streams, then the last two non-empty streamed lines
- `task` - task for the sub-agent
- `cwd` - optional working directory
