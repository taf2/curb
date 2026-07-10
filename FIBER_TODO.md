# Fiber Scheduler Review TODO

Tracker for fiber/scheduler correctness findings in the C wait loops
(`ext/curb_multi.c`) and the Ruby scheduler driver (`lib/curl.rb`).

Findings were verified empirically on ruby 4.0.5 / Async 2.37.0 against a
local HTTP server with a 0.5s response delay, plus source inspection of the
matching ruby source tree (`thread.c`, `scheduler.c`). Probe method: while one
fiber performs a request, a sibling fiber ticks every 10ms; the tick count
measures whether the wait loop blocks the thread.

Baseline probe results (0.5s transfer, 50 possible ticks):

| Path                        | Ticks | CPU     | Verdict            |
|-----------------------------|-------|---------|--------------------|
| `multi.perform` (legacy)    | 1/50  | -       | thread blocked     |
| `multi._socket_perform`     | 49/50 | 0.001s  | scheduler friendly |
| `easy.perform` (scheduler)  | 5/50  | -       | thread blocked     |

Post-fix probe results (same probe, 2026-07-02; retained after final review):

| Path                        | Ticks | CPU     | Verdict            |
|-----------------------------|-------|---------|--------------------|
| `multi.perform`             | 49/50 | 0.003s  | scheduler friendly |
| `multi.perform` (2 easies)  | 49/50 | 0.003s  | scheduler friendly |
| `multi._socket_perform`     | 49/50 | 0.006s  | scheduler friendly |
| `easy.perform` (scheduler)  | 49/50 | 0.003s  | scheduler friendly |

The io_select-less-scheduler probe (two easies, `_socket_perform`) now both
completes and advances a sibling fiber. Before the first fix it spun at 99.9%
CPU indefinitely and survived SIGTERM; the intermediate native-select fallback
completed but still starved sibling fibers.

## Progress Checklist

- [x] [Critical] Fix infinite busy-loop when scheduler lacks the optional `io_select` hook
- [x] [High] Default `Curl::Multi#perform` blocks the whole thread under a fiber scheduler
- [x] [High] `Curl::Easy#perform` scheduler driver inherits legacy-loop blocking
- [x] [Medium] "Scheduler-aware" sleep fallbacks are plain thread sleeps
- [x] [Low] `timeout_milliseconds == 0` path yields to threads, not fibers
- [x] [Low] Single-fd `io_wait` exception fallback waits twice
- [x] [Test gap] Add sibling-fiber-progress regression tests

## Details

### [Critical] Infinite busy-loop when scheduler lacks the optional `io_select` hook

- Files:
  - `ext/curb_multi.c` (`rb_curl_multi_socket_drive`, multi-fd scheduler branch)
  - ruby 4.0.5 `scheduler.c:786-794` (`rb_fiber_scheduler_io_selectv` uses `rb_check_funcall`)
- Issue: `Fiber::Scheduler#io_select` is an optional hook (Async only added it in
  2.6). For schedulers without it, `rb_fiber_scheduler_io_select` returns
  **Qundef immediately without waiting**. The drive loop treats any
  non-raising return as a completed wait (`handled_wait = 1`), skips the
  `rb_thread_fd_select` fallback, and re-enters the loop having waited zero
  time.
- Impact: With >1 tracked socket and a scheduler lacking `io_select`,
  `_socket_perform` spins at 100% CPU. If libcurl has no pending timer
  deadline the spin is infinite, and the cycle contains no interrupt
  checkpoint, so the process does not even service SIGTERM (verified: probe
  process pinned at 99.8% CPU for 5+ minutes, required SIGKILL).
- Repro: scheduler implementing `io_wait`/`kernel_sleep`/`block`/`unblock` but
  not `io_select`; two easies on one multi via `_socket_perform`.
- Fix direction: treat `Qundef` as "not handled" and perform a real wait before
  retrying. A native `rb_thread_fd_select` prevents the spin but still blocks
  sibling fibers, so a scheduler wait is preferable.
- Note: `io_wait` is a required scheduler hook. Exceptions from either hook are
  scheduler cancellation or application failures and must propagate; only the
  optional `io_select` hook uses `Qundef` to signal absence.
- Status: **Fixed.** `rb_curl_multi_socket_drive` now treats a `Qundef` return
  from `rb_fiber_scheduler_io_select` as "hook not implemented". It waits on a
  representative descriptor through `io_wait`, then performs a zero-time
  native select to collect readiness across every tracked descriptor. This
  also covers Ruby 3.1, whose public scheduler C API has `io_wait` but not
  `io_select`. Regression test:
  `test_socket_perform_progresses_when_scheduler_lacks_io_select` (cooperative
  subprocess scheduler, sibling ticker, and parent-side SIGKILL fuse).

### [High] Default `Curl::Multi#perform` blocks the whole thread under a fiber scheduler

- Files:
  - `ext/curb_multi.c` (`ruby_curl_multi_perform_impl`, `ruby_curl_multi_perform`)
  - ruby 4.0.5 `thread.c:4547` (`rb_thread_fd_select`)
- Issue: The comments claim `rb_thread_fd_select` "integrates with the fiber
  scheduler". It does not, on any CRuby: it performs a GVL-released blocking
  select with no scheduler involvement. The scheduler-aware implementation
  (`_socket_perform`) exists but is not the default because of previously
  observed "scheduler regressions for one-handle multi usage".
- Impact: A bare `multi.perform` under Async stalls every fiber on the thread
  for the duration of all transfers (probe: sibling fiber got 1 tick in 0.5s).
  With a perform block, the thread still blocks in up-to-100ms select chunks
  between yields.
- Fix direction: route `perform` to the socket-action drive loop when
  `rb_fiber_scheduler_current() != Qnil`. The regression comment may be stale
  after 54c29ef ("Fix Fiber scheduler socket handling with libcurl 8.20.0");
  re-test one-handle multi usage under Async before switching. Alternatively,
  add an `rb_fiber_scheduler_io_select`-based wait to the legacy fdset loop
  (with the cooperative missing-hook fallback described above).
- Status: **Fixed.** `ruby_curl_multi_perform` now dispatches to
  `ruby_curl_multi_socket_perform_impl` when `rb_fiber_scheduler_current() !=
  Qnil` on non-Windows builds with socket-action plus the required scheduler C
  APIs; unsupported builds keep the legacy loop. The multi-fd path remains
  cooperative when `io_select` is unavailable by using `io_wait` plus a
  zero-time all-fd poll. The incorrect scheduler comments were rewritten. The
  one-handle-regression concern proved stale: the full
  suite including `test_multi_single_request_scheduler_path`,
  `test_multi_reuse_after_scheduler_perform`, and all Easy-under-Async tests
  passes. Probe: `multi.perform` went from 1/50 to 49/50 sibling ticks.
  Regression tests: `test_multi_perform_waits_via_scheduler_hooks`, the one-
  and two-easy sibling-progress tests, the two scheduler-timeout tests, and
  `test_multi_perform_runs_work_added_from_final_idle_yield_under_scheduler`.

### [High] `Curl::Easy#perform` scheduler driver inherits legacy-loop blocking

- Files:
  - `lib/curl.rb:558-573` (`ensure_scheduler_driver` runs `state[:multi].perform`)
  - `lib/curl/easy.rb:260-261` (routes to `Curl.perform_with_scheduler`)
- Issue: The driver fiber drives the shared thread-local multi with the legacy
  `perform`, so the thread blocks in ~100ms select chunks. Sibling fibers run
  only because the perform block calls `kernel_sleep(0)` once per wakeup.
- Impact: Probe measured 5/50 ticks — sleeping fibers see up to ~100ms added
  latency per wakeup, and non-curb IO fibers effectively starve during heavy
  transfers. Existing tests pass because their timing margins exceed the 100ms
  wakeup quantum.
- Fix direction: falls out of the previous finding (make the scheduler path
  use the socket-action loop). No lib/curl.rb change should be needed beyond
  that.
- Status: **Fixed** by the `Multi#perform` routing above; no lib/curl.rb
  change was needed, as predicted. The driver fiber's `state[:multi].perform`
  now runs the socket-action loop. Probe: `easy.perform` went from 5/50 to
  49/50 sibling ticks. Regression test:
  `test_sibling_fibers_progress_during_easy_perform`.

### [Medium] "Scheduler-aware" sleep fallbacks are plain thread sleeps

- Files:
  - `ext/curb_multi.c` (`rb_curl_multi_socket_drive`, no-fd wait branches)
  - `ext/curb_multi.c` (`ruby_curl_multi_perform_impl`, legacy no-fd waits)
  - ruby 4.0.5 `thread.c:4560-4566` and `thread.c:1445-1450`
- Issue: `rb_thread_fd_select(0, NULL, NULL, NULL, &tv)` short-circuits to
  `rb_thread_wait_for` → `sleep_hrtime`, a thread-level sleep with no
  scheduler consult, despite comments calling it "scheduler-aware".
- Impact: The `count_tracked == 0` phase is exactly the DNS-resolution window
  under libcurl's threaded resolver (no sockets exposed yet), so a slow DNS
  lookup blocks every fiber on the thread in up-to-100ms chunks — even in the
  otherwise scheduler-friendly `_socket_perform` path.
- Fix direction: when `rb_fiber_scheduler_current() != Qnil`, sleep via
  `rb_fiber_scheduler_kernel_sleep(scheduler, timeout)` instead.
- Status: **Fixed.** Added a shared `curb_multi_scheduler_sleep(tv)` helper
  that sleeps via `rb_fiber_scheduler_kernel_sleep` when a scheduler is
  current and falls back to `rb_thread_fd_select(0, ...)` /
  `rb_thread_wait_for` otherwise, plus an extconf check for
  `rb_fiber_scheduler_kernel_sleep`. All four sleep sites now use it: the
  socket path's `count_tracked == 0` (DNS window) and single-fd `wait_fd < 0`
  branches, and the legacy loop's `maxfd == -1` and `curl_multi_wait`
  `numfds == 0` branches. The test-suite `RecordingScheduler` runs its plain
  sleeps in a blocking fiber (`Fiber.blocking` where available, an explicit
  blocking fiber otherwise) so the hooks cannot recursively re-enter the
  scheduler.

### [Low] `timeout_milliseconds == 0` path yields to threads, not fibers

- Files:
  - `ext/curb_multi.c` (legacy 0ms timeout and socket immediate-timer paths)
- Issue: When curl reports a 0ms timeout, the legacy loop calls
  `rb_thread_schedule()`, which yields to other OS threads but does not enter
  the fiber scheduler.
- Impact: If curl repeatedly reports 0 timeouts, the loop can spin without
  letting sibling fibers run.
- Fix direction: use `rb_fiber_scheduler_yield(scheduler)` (or
  `kernel_sleep(0)`) when a scheduler is current.
- Status: **Fixed.** The 0ms path now calls
  `rb_fiber_scheduler_kernel_sleep(scheduler, 0)` when a scheduler is current
  (`kernel_sleep(0)` chosen over `rb_fiber_scheduler_yield` for parity with
  `Curl.scheduler_yield` in lib/curl.rb), keeping `rb_thread_schedule()` as
  the fallback for builds without the kernel_sleep C API. The socket-action
  loop also yields when libcurl repeatedly rearms an immediately-due timer, so
  the path normally selected under a scheduler has the same protection.

### [Low] Single-fd `io_wait` exception fallback waits twice

- Files:
  - `ext/curb_multi.c` (`rb_curl_multi_socket_drive`, scheduler hook calls)
- Issue: If the scheduler's `io_wait` raises (caught by `rb_protect`),
  `scheduler_wait_handled` stays 0 and control falls through to
  `rb_wait_for_single_fd`, performing a second full-length wait in the same
  loop iteration.
- Impact: The apparent double wait is not harmless: scheduler hook exceptions
  include timeout and task cancellation. Catching and clearing them can make a
  cancelled transfer run to completion. Also, `rb_wait_for_single_fd` can
  consult the scheduler again, so it is not a guaranteed native fallback.
- Fix direction: propagate hook exceptions. Treat only `Qundef` from optional
  `io_select` as a request to use another wait implementation.
- Status: **Fixed.** The scheduler hooks are called directly, so exceptions
  unwind through the socket performer's existing `rb_ensure` cleanup. The
  elapsed-budget helper and exception fallback were removed. One- and
  two-handle Async timeout regressions verify `io_wait` and `io_select`
  cancellation is prompt rather than swallowed.

### [Test gap] Add sibling-fiber-progress regression tests

- Files:
  - `tests/tc_fiber_scheduler.rb`
- Issue: All concurrency assertions route N requests through one multi handle,
  so libcurl provides the parallelism and the tests pass even when the wait
  loop blocks the whole thread. The timer test's margin (0.05s timer vs 0.2s
  transfer) is wider than the 100ms select quantum, so it also passes.
- Fix direction: add a probe-style test — one fiber performs a request against
  a delayed endpoint while a sibling fiber ticks every 10ms; assert the tick
  count is a large fraction of the expected count. Add a second test with a
  minimal scheduler lacking `io_select` (guard with a subprocess + kill fuse
  until the Critical fix lands). Probe scripts to adapt:
  `fiber_block_probe.rb` and `io_select_qundef_probe.rb` (session scratchpad).
- Status: **Fixed.** Added to `tests/tc_fiber_scheduler.rb`:
  - `test_sibling_fibers_progress_during_multi_perform` and
    `test_sibling_fibers_progress_during_easy_perform` — 10ms-tick sibling
    probes (≥8 of ~25 possible ticks required; blocked loop yields 0–5).
  - `test_multi_perform_waits_via_scheduler_hooks` — deterministic check that
    `Multi#perform` routes waits through scheduler `io_wait`/`io_select`.
  - `test_socket_perform_progresses_when_scheduler_lacks_io_select` -
    cooperative subprocess probe (`tests/io_select_less_scheduler_probe.rb`)
    with a sibling ticker and 30s parent-side SIGKILL fuse.
  - A two-easy sibling-progress test covers the multi-fd wait branch.
  - Two Async timeout tests cover exception propagation from one- and
    multi-socket scheduler waits.
  - An IO-wrapper construction failure test ensures adjacent Ruby exceptions
    also unwind instead of being cleared into a native fallback.
  - A scheduler-specific final-idle-yield test ensures work queued by the
    perform block is driven before return/autoclose.
  The original four regressions were checked against the pre-fix build; the
  added tests cover issues found during the final review of the routed socket
  path.

Runtime probes directly cover routing, sibling progress, missing `io_select`,
cancellation, and final-yield behavior. The no-socket/DNS `kernel_sleep` branch
and feature-disabled legacy 0ms branch are verified by source/build inspection;
they are not claimed as deterministic pre-fix/post-fix runtime regressions.

## Known Limits

The socket-action implementation is currently excluded on Windows. Windows,
builds without libcurl socket-action support, and builds without the required
Ruby scheduler C APIs retain the legacy perform loop; the strict
sibling-progress tests omit configurations without the socket-action path.

When `io_select` is unavailable, the cooperative fallback waits on one
representative descriptor and then polls the full descriptor set. Readiness on
a different descriptor can therefore be observed up to one
`Curl::Multi.default_timeout` quantum later (100ms by default), but the wait no
longer blocks sibling fibers or spins the thread.

## Verified Correct (checked, no action)

- io_cache invalidation on interest change and `CURL_POLL_REMOVE`
  (`multi_socket_cb`), including fd-reuse safety since libcurl always issues
  REMOVE before closing its sockets.
- Collect-then-act separation in both ready-fd paths — `sock_map` is never
  mutated during `st_foreach` (socket callbacks fire only in the later
  `curl_multi_socket_action` calls over the collected array).
- `rb_ensure` cleanup in `_socket_perform` clears the socket/timer callbacks
  that point at the stack-allocated `multi_socket_ctx` on all exits.
- `io_wait` result handling covers `Integer` event masks, `true`, and
  `false`/`nil` timeouts, including the mask-maps-to-zero-flags edge.
- Timer deadline bookkeeping (`multi_timer_cb` absolute deadline, `-1` = no
  timer) and the `wait_ms` cap at `Curl::Multi.default_timeout` (100ms).
