# Module Template

Use this template when creating or refreshing a file under `references/modules/`.

Keep it concise. Favor durable pointers over exhaustive detail.

```md
# <Module Name>

## Load When
- <task or symptom>
- <task or symptom>

## Main Paths
- `<path>` — <why this file or directory matters>
- `<path>` — <why this file or directory matters>
- `<path>` — <why this file or directory matters>

## Important Concepts
- <service, abstraction, or ownership boundary>
- <service, abstraction, or ownership boundary>
- <constraint, pattern, or invariant>

## Read Next
- `<path>` — <first follow-up read>
- `<path>` — <second follow-up read>
- `<path>` — <example, test, or consumer>

## Related Skills
- `<skill-name>` — <when to use it>
- `<skill-name>` — <when to use it>

## Gotchas
- <important caveat, invariant, or non-obvious rule>
- <important caveat, invariant, or non-obvious rule>
```

## Writing Notes
- Use exact relative paths.
- Include only a few stable, high-value files.
- Mention one nearby example/test/consumer when useful.
- Prefer short bullets over prose.
- Avoid exhaustive symbol lists or full directory dumps.
- If uncertain, say so instead of guessing.
