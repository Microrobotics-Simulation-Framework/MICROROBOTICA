# Security Policy

## Supported Versions

| Version | Supported |
|---------|-----------|
| 0.1.x   | Yes       |

## Reporting a Vulnerability

If you discover a security vulnerability in MICROBOTICA, please report it
responsibly:

1. **Do not** open a public GitHub issue for security vulnerabilities.
2. Email the maintainers with a description of the vulnerability, steps
   to reproduce, and any potential impact assessment.
3. You will receive an acknowledgement within 72 hours.
4. A fix will be developed and a security advisory published within 30 days
   of confirmation.

## Scope

MICROBOTICA is research software and does not implement security controls
for clinical deployment. See `docs/regulatory/intended_use.md` for the
cybersecurity boundary statement.

Security vulnerabilities in MICROBOTICA's context include:
- Memory safety issues (buffer overflows, use-after-free, etc.)
- Arbitrary code execution via crafted USD files or Python scripts
- Denial of service via resource exhaustion
- Information disclosure via audit logs or session provenance

## Security-Relevant Changes

All security-relevant changes are documented in `CHANGELOG.md` under the
`### Security` section, as required by MDCG 2019-16 cybersecurity guidance.
