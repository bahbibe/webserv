# webserv v4 - config parser hardening and real test coverage

## Execution order (reorganized after v5 Phase 1 landed)

v5 started first and its Phase 1 (the `user` directive) is already
committed on the `v5` branch, parsed and validated against the
*current* parser. This plan supersedes that code, not just tests it:
v5's remaining phases (the actual privilege drop, and everything
`install.sh`/testing built on top of a `user`/`group` config value)
are real, security-sensitive work that shouldn't be built against a
parser already known to be fragile, and redoing v4's rewrite *after*
v5 fully lands would mean reconciling a rewrite against
privilege-drop-adjacent code instead of against plain directive
parsing. So: v4 lands first (branched from `main`, independent of the
still-unmerged `v5` branch), absorbing `user`-directive recognition
into the new parser directly (see Phase 4) rather than treating it as
already-solved. `v5` resumes after v4 merges, rebased onto the new
parser - its own Phase 1 commit becomes superseded, not wasted: the
`getpwnam()`/`getgrnam()` validation logic it wrote carries over
directly (see Phase 4), only the line-scanning it was sitting in gets
replaced. Version numbers were swapped from how these two plans were
first drafted, specifically so the numbering matches actual release
order: whichever plan ships first is v4, full stop - privilege drop
was drafted and started first chronologically, but parser hardening
is what actually needs to *release* first (this plan), so it's the
one that gets called v4. The privilege-drop plan is v5 now, not
because it was renumbered arbitrarily, but because it releases
second.

## Status

- **Phase 1 (test harness) - done.** `webserv_tests` target (doctest
  via `FetchContent`, dev-only), a shared `ParserFixture` resetting
  the parser's cross-call global state before each test case, and 14
  characterization tests against real config shapes - including
  permanent regressions for the two exact v3 bugs found this session.
  14/14 test cases, 42/42 assertions. `src/Globals.cpp` split out of
  `main.cpp` to make this possible; full `tests/run_tests.sh` (33/33)
  confirms that split changed nothing about the running server.
- **Phase 2 (edge-case tests) - not started.**
- **Phase 3 (the lexer) - not started.**
- **Phase 4 (the parser) - not started.**
- **Phase 5 (regression and docs) - not started.**

## Context

The config parser is four separate functions (`Webserver::brackets()`,
`parseGlobalDirectives()`, `Server::parseServer()`,
`Server::parseLocation()`, all in `Config.cpp`) that each walk the
*same* config file text line by line, each independently tracking its
own notion of "am I inside a block right now" - a `stack<string>` in
one, a plain `int depth` in another, an early-break-on-`}` in the
other two, kept in sync across calls only by convention
(`Server::_pos`, a static member recording where the last call left
off). They share the character-level helpers
(`isComment()`/`isBrackets()`/`isWhitespace()`/`trim()`), but the
*structural* understanding of the file - what nesting level a given
line is actually at - is reconstructed independently, four times,
every time the config is parsed.

That duplication is not theoretical - it already produced two of the
real bugs found in v3 (`Server::parseServer()` misreading a leading
main-context directive as an invalid server-level one, then
double-reporting a genuinely invalid one, both because its line-
walking loop had no real concept of "content that isn't mine to
validate"). Both got caught because this project's discipline is to
verify every change against a running server and the full
`tests/run_tests.sh` suite - but that discipline finds bugs *after*
they're written, one scenario at a time, by hand or via a slow
end-to-end curl-driven suite. Nothing today can answer "does the
parser handle a `server` block with no closing brace, a directive
with zero arguments, a comment appended after a directive on the same
line, a location path containing spaces, a config file that's just
whitespace" in under a second, or even enumerate that those cases
exist. That's the gap this plan closes: real, fast, structural test
coverage for parsing specifically, and a parser built so there's one
place that understands "where am I in this file," not four.

## Rule for this effort

Same model as v2/v3/v5: one dedicated branch (`v4`), phases land as
their own commits, no merge to `main` until the whole story works end
to end. This one especially: it touches code every existing config
(`conf/default.conf`, `conf/webserv.conf.install`, every fixture
`tests/run_tests.sh` generates) already depends on working correctly,
so it does not get merged on partial confidence.

## Decisions made

- **A real unit-test framework, scoped narrowly to parsing.**
  Reverses part of v2's "no unit-test framework" decision - but only
  for config parsing specifically, not HTTP behavior. Config parsing
  is pure logic (string in, a validated structure or a config error
  out) with no sockets, no timing, no process forking - exactly the
  shape unit tests are good at, and exactly the shape the existing
  curl-driven e2e suite is slow and indirect for (spinning up a real
  process just to prove a malformed brace is rejected). HTTP-level
  behavior (keep-alive, CGI, TLS, timeouts) stays on the e2e suite -
  that's still the right tool for anything involving real sockets and
  real timing, and nothing about this plan touches it.
- **doctest**, not Catch2 or a homegrown assert harness. Single
  header, effectively instant compile, near feature parity with
  Catch2's assertions/sections. Pulled in via the same CMake
  `FetchContent` pattern already used for spdlog, scoped to a
  separate test binary target - never linked into the actual
  `webserv` executable, so it's a dev-only dependency, not a runtime
  one.
- **A real tokenizer, then a real recursive-descent parser - not
  regex.** Regex is a fine tool for validating an individual token's
  *shape* (is this a valid port number, does this path look sane) and
  the existing directive-specific validators
  (`isNumber()`, `resolveHostFamily()`, etc.) already do that job
  without needing to become regexes. It's a poor fit for the actual
  problem here, which is structural and recursive (blocks nest inside
  blocks) - regex doesn't nest, so using it for the overall grammar
  would just be pattern-matching whole lines with extra steps,
  which is close to what the current code already does informally
  and has already proven fragile. Two stages instead:
  1. **Lexer**: turns the raw file text into one flat token stream
     (`WORD`, `BRACE_OPEN`, `BRACE_CLOSE`, `EOF`) - comments stripped,
     whitespace normalized, a quoted string (`"a path with spaces"`)
     read as one token. One place decides what a token is, instead of
     four line-scanners each doing their own ad hoc splitting.
  2. **Parser**: consumes that token stream with real recursive-
     descent functions (`parseConfig` → `parseMainDirective*` /
     `parseServerBlock*` → `parseServerDirective*` /
     `parseLocationBlock*` → `parseLocationDirective*`) and builds a
     structured result - replacing the four hand-rolled line-walkers
     and the `Server::_pos` coordination hack with one real call
     stack that always knows exactly what nesting level it's at.
  Existing semantic validation (`isServerDir()`/`isLocationDir()`,
  every directive's own value-parsing and error-reporting,
  `duplicateDirective()`) is *reused*, not rewritten - this plan is
  scoped to the structural layer that decides "what directive is this
  and what block is it in," not the layer that decides "is `8080` a
  valid port."
- **Characterize before refactoring.** Every phase below is ordered
  so the safety net (tests) exists *before* the risky part (rewriting
  a parser every existing config depends on) - not the other way
  around. This is the direct answer to "we can't just keep pushing
  and break stuff": the new parser doesn't get written until there's
  a test suite that would catch it being wrong.

## Phases

### Phase 1 - test harness

- `FetchContent` doctest, a new CMake test target (`webserv_tests` or
  similar) separate from the `webserv` binary target, wired so
  `ctest`/running the test binary directly both work.
- Characterization tests: real config snippets already used
  successfully across this project's history (`conf/default.conf`,
  `conf/webserv.conf.install`, representative fixtures from
  `tests/run_tests.sh`) parsed through the *current* (pre-refactor)
  functions, asserting the structural result matches what's actually
  observed today. Proves the harness works and pins down current
  behavior as a baseline before anything changes.

### Phase 2 - edge-case tests (write the failing tests first)

Each of these gets a test now, against the *current* parser, before
Phase 3-4 touch any parsing code - some will fail immediately
(documenting real, previously-unverified gaps), some will pass
(confirming the current code already handles them, which the refactor
then has to keep true):

- Malformed structure: unclosed `server {`, unclosed `location {}`, a
  stray `}` with nothing open, brace on its own line vs. same line as
  `server`/`location`.
- Directive edge cases: zero arguments, an argument containing a
  space via quoting, a directive repeated at a nesting level that
  should reject duplicates vs. one that legitimately allows repeats
  (`error_page`, `cgi_path`), a comment appended after a directive on
  the same line (`listen 8080 # comment`) vs. a comment on its own
  line, tabs vs. spaces, trailing whitespace, `\r\n` line endings.
  Also every main-context directive: `pid`/`error_log` (shipped in
  v3) and `user` (parsed and validated on the still-unmerged `v5`
  branch, ahead of this plan) appearing before, between, and after
  `server` blocks. `pid`/`error_log` were already manually verified
  once in v3 - now pinned down as permanent, fast tests instead of
  one-off manual checks. `user`'s test cases are written against the
  behavior `v5`'s Phase 1 already established (a real account with
  and without a group, a nonexistent account, a real account with a
  nonexistent group), since Phase 4 below is what actually carries
  that directive's recognition into the new parser.
- File-level edge cases: an empty file, a file that's only comments/
  whitespace, an extremely long single line, non-ASCII bytes in a
  path value.
- Everything found as a *real bug* this session gets its own
  regression test here, permanently, not just the manual verification
  it originally got: the leading-main-context-directive
  misread-as-server-directive bug, the double-error-report bug, and
  the disk-presence-only system-tier bug (that last one isn't a
  parser bug exactly, but the same "no permanent test for an edge
  case that was only checked by hand" pattern applies, and it belongs
  in this project's regression net regardless of which file it lives
  in).

### Phase 3 - the lexer

- New `src/Server/Lexer.cpp`/`inc/Lexer.hpp`, unit-tested directly
  against the token streams it produces (not indirectly through the
  parser) - a token stream for a given input is either exactly right
  or it isn't, independent of anything about server/location
  semantics.

### Phase 4 - the parser

- New `src/Server/ConfigParser.cpp`/`inc/ConfigParser.hpp` (naming
  TBD - `Config.cpp` already exists and currently holds the code this
  replaces), built on the Phase 3 lexer, replacing
  `Webserver::brackets()`/`parseGlobalDirectives()`/
  `Server::parseServer()`/`Server::parseLocation()`'s hand-rolled
  scanning and the `Server::_pos` static-member coordination. Existing
  semantic validators are called from here, not reimplemented.
  Phase 2's edge-case tests are the acceptance bar - this phase isn't
  done until all of them pass against the *new* parser, matching
  intended behavior (including the ones that were failing on purpose
  against the old one).
- Carries all three main-context directives (`pid`, `error_log`,
  `user`) into the new architecture as one coherent piece of main-
  context handling, instead of `user` being bolted onto the old
  parser the way `pid`/`error_log` briefly were in v3. The `user`
  directive's actual validation (`getpwnam()`/`getgrnam()`) is
  ported over from `v5`'s Phase 1 essentially unchanged - that logic
  was already correct and already tested, only the surrounding
  line-scanning it was sitting in is what's being replaced.

### Phase 5 - regression and docs

- Full `tests/run_tests.sh` (e2e/HTTP-level) must stay green,
  unmodified in what it tests - proves the new parser produces
  identical *observable server behavior* for every config this
  project already relies on, not just identical parse-tree shape in
  isolation.
- Every real config file in the repo (`conf/default.conf`,
  `conf/webserv.conf.install`) re-verified against a live server,
  same as every prior phase in this project's history.
- README/docs note: config parsing now has dedicated unit coverage
  (`doctest`), separate from the HTTP-behavior e2e suite - worth
  stating plainly so a future contributor knows where a parsing fix
  belongs.

## Testing

Same discipline as every `vN-PLAN.md` before this one, with the order
inverted on purpose for this plan specifically: tests exist first,
implementation second, per "characterize before refactoring" above.
Build warning-free, full `tests/run_tests.sh` green throughout, and
the new doctest suite green before any phase is considered done -
neither suite alone is sufficient proof for this plan, both have to
agree.

## Explicitly not in scope

- **Unit tests for HTTP-level behavior** (keep-alive, CGI, TLS,
  timeouts) - stays on the e2e suite, for the reasons in "Decisions
  made" above. This plan is about the parser, not a wholesale testing
  philosophy change.
- **Changing what any directive means or accepts** - this is a
  structural rewrite of *how* the file gets parsed, not a change to
  the config language itself. Every config that's valid today stays
  valid, and means the same thing, when this plan is done.
- **A config schema/grammar file** (e.g. generating the parser from a
  grammar spec, or JSON-Schema-style declarative validation). Real
  option for a much bigger rewrite, not this one - a hand-written
  recursive-descent parser is a well-understood, directly debuggable
  match for a grammar this size (a handful of block types, a few
  dozen directives).
