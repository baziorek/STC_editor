# Security Policy

## Supported Versions

STC_editor is a desktop application; only the latest release on the `master`
branch is actively maintained. There are no long-term-support (LTS) versions.

| Version | Supported          |
| ------- | ------------------ |
| master  | :white_check_mark: |
| < 0.2   | :x:                |

## Reporting a Vulnerability

Please **do not** open a public issue for security vulnerabilities.

Report vulnerabilities privately via **GitHub Security Advisories**:

1. Open the repository on GitHub: <https://github.com/baziorek/STC_editor>
2. Go to the **Security** tab → **Advisories** → **Report a vulnerability**
   (direct link: <https://github.com/baziorek/STC_editor/security/advisories/new>).
3. If private vulnerability reporting is not enabled, open a regular issue
   asking for a private contact channel instead — see
   <https://github.com/baziorek/STC_editor/issues>.

Please include:

- a description of the vulnerability and its impact,
- steps to reproduce or a proof of concept,
- affected versions, if known.

You can expect an acknowledgement within a few days. If the issue is
confirmed, a fix will be prepared and released as soon as possible; you will
be credited in the release notes unless you prefer to stay anonymous.

## Scope

This policy covers the STC_editor code in this repository. Vulnerabilities in
bundled third-party components (Qt, QtWebEngine, Nuspell, uchardet, etc.)
should be reported to the respective upstream projects.
