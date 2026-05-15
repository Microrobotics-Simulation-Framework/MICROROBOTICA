# Downstream Integration Guide — MICROBOTICA

> **Status**: Skeleton — full content in Phase 3.

## Four-Layer Dependency Chain

```
MADDENING (Layer 1, LGPL-3.0)
    ↓
MIME (Layer 2, LGPL-3.0)
    ↓
MICROBOTICA (Layer 3, AGPL-3.0)  ← this project
    ↓
Commercial Product (Layer 4, proprietary)
```

## Commercial Responsibility Statement

The commercial product manufacturer (Layer 4) bears all {term}`EU MDR`
obligations. MICROBOTICA provides documentation artifacts to support
the manufacturer's regulatory submissions, but does not itself make
any clinical claims or bear regulatory obligations.

## SOUP Assessment Support

MICROBOTICA provides the following artifacts to support downstream
{term}`SOUP` assessment under {term}`IEC 62304`:

- `docs/validation/known_anomalies.yaml` — complete anomaly registry
- `docs/validation/soup_package.md` — SOUP package document
- `CHANGELOG.md` — version history with verification status
- `CITATION.cff` — version identification

## Full Content (Phase 3)

The complete downstream integration guide, including:

- Detailed SOUP assessment procedure
- Risk management integration guidance
- Verification evidence mapping
- Configuration management requirements

will be developed in Phase 3 when commercial integration becomes relevant.
