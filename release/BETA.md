# Beta acceptance record

Status: NOT RELEASED. Windows 11 and Ubuntu 24.04 x64 are validation targets,
not a certification claim. No Windows hardware acceptance has been performed
by the Linux build process.

Recruit 10–20 visualists. Publish stable only after at least five complete this
sequence without direct assistance: install → first visual → save → update →
reopen project. Send invitations only after the release owner chooses recipients.

For each tester record (voluntary): OS/version, GPU/driver, audio device,
MIDI device, output route, package version, steps attempted and result.
Do not collect projects or media by default.

| Acceptance | Windows 11 | Ubuntu 24.04 |
|---|---|---|
| Clean machine, no development SDK | Pending | Pending |
| Two-hour performance session | Pending | Pending |
| Audio + MIDI + output route | Pending | Pending |
| A → B keeps personal data | Pending | Pending |
| Interrupted download / offline | Pending | Pending |
| Invalid signature rejected | Pending | Pending |
| Full disk / permissions failure | Pending | Pending |
| Crash recovery with nested groups | Pending | Pending |
| Previous-version recovery | Pending | Pending |

Do not promote a draft or update a channel feed until the required rows pass.
Keep each published artifact immutable; release a new version to fix a defect.

Local Linux functional run: [2026-09-14 report](../mds/PRUEBA_LOCAL_2026-09-14.md). Automated suites passed; manual-equivalent UI walkthrough failed on examples and Save As. This does not satisfy clean-machine or hardware acceptance.

Follow-up: the same report now records fixes and successful local retests for the four functional failures, with a new artifact SHA256. Hardware/clean-machine acceptance remains pending.
