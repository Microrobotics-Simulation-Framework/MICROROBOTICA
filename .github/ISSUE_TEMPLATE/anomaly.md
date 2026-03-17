---
name: Known Anomaly
about: Report a known anomaly for the MICROBOTICA anomaly registry
title: "[ANOMALY] "
labels: known-anomaly
assignees: ''
---

## Anomaly Details

**Anomaly ID**: MBCA-ANO-XXX (assigned by maintainer)

**Title**: <!-- Short, specific, searchable title -->

**Severity**:
- [ ] Critical — incorrect results, no workaround
- [ ] Major — incorrect results, workaround exists
- [ ] Minor — cosmetic or inconvenience, no correctness impact

**Safety Relevance**:
- [ ] Safety-relevant — affects safety-critical output regardless of context
- [ ] Context-dependent — safety relevance depends on deployment context
- [ ] Not safety-relevant — provably cannot affect safety-critical output

## Description

<!-- Detailed description: which component, under what conditions, what the
     incorrect behaviour is, what the expected behaviour would be -->

## Affected Components

<!-- C++ class name(s), e.g. ResultsApplicator, SceneManager -->

## Safety Relevance Rationale

<!-- Three-part structure required:
     1. In a research context, [impact]
     2. In a clinical context, [what could go wrong]
     3. The downstream manufacturer must [assessment/control needed] -->

## Workaround

<!-- Specific actionable workaround, or "None available" if critical -->

## Detection Method

- [ ] ASan
- [ ] UBSan
- [ ] TSan
- [ ] Valgrind
- [ ] Manual review
- [ ] CI test failure
- [ ] User report

## Memory Safety Relevant

- [ ] Yes — involves undefined behaviour, memory corruption, or data races
- [ ] No
