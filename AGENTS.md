# Notes for coding agents

## Communication and language
- Communicate with the user in their preferred natural language, including progress
  updates, questions, and final responses.

Determine the conversation language in this order:

1. Follow an explicit language request from the user.
2. When continuing or resuming a session, preserve the language established in that
   conversation, including language preferences recorded in its continuation summary.
   English repository files or an English summary do not by themselves change the
   conversation language.
3. For a new conversation, infer the language from the user's opening words and
   messages. Do not treat quoted text, code, or technical terms as a language switch.
4. If the current conversation provides no clear signal, use a known language preference
   from other sessions when that context is available. Do not assume access to
   unavailable session history.
5. If no preference can be inferred, ask briefly which language the user prefers.

The conversation language does not change the English-language requirements for tracked
documentation, code comments, Git commit messages and UI strings in the demo.

## Git
- Local commits are fine. Never push (neither commits nor tags); the user pushes.
- Suggest an annotated tag (`vX.Y.Z`) for a release commit; create it only when the user
  agrees, and remind them to push it with `git push --tags`.
- Handoff notes (`HANDOFF-*.md`) are working notes between sessions: never commit them.

## Building, installing, checking
- Build: `cmake -B build && cmake --build build`; the demo is `./build/demo/qt-extra-demo`.
- The installed copy lives in `/usr/local` (`include/qt-extra/`, `lib/libqt-extra.a`,
  `lib/cmake/qt-extra/`). Installing needs `sudo`, so the user does it: give them the
  exact commands (`cmake --build build && sudo cmake --install build`).
- Do not add tests unless asked. There is no test directory; `demo/main.cpp` exercises
  the widgets. Run only one or two targeted checks (a small throwaway program in a
  scratch directory is fine), and leave hand checks in the demo to the user.

## Code
- Keep Qt 5.15 and Qt 6 compatibility (the user builds with Qt 6); guard version-specific
  APIs with `QT_VERSION_CHECK`. C++17.
- Match the surrounding code style.
- New widget features go into the demo so they can be checked by hand, and into
  `README.md`.
- Versioning follows semver: a change that breaks clients (signals, signatures,
  behavior) bumps the major version in `project(... VERSION ...)` and gets a "Migrating"
  note in `README.md`. Clients use `find_package(qt-extra <major> REQUIRED)`.

## Clients
The installed package is used by:
- gemini-commander (`/home/andrzej/wazne/gitmy/gemini-commander`): file panels and the
  editor use `MruTabWidget`.
- agentdeskt (`/home/andrzej/wazne/gitmy/agentdeskt`): chats in `MruTabWidget` tabs
  (preview tab, tab keys, busy/attention markers, page-based close signals).

When changing a public API, check how these use it.
