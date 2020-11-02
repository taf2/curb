# Change Log
## 0.9.11
* Fixes for haproxy protocol via CURLOPT_HAPROXYPROTOCOL
* Ensure dev environments don't use older versions of rake when working on curb (not a production dependency)
* Many readmen updates - more windows support documentation
* Support added for CURLOPT_STDERR
## 0.9.8
### Fixes
* Fix build with curl `7.62.0`
* Support building against Ruby `1.8` to help people migrate to the future and beyond
### Enhancements
* Improve `timeout=` to allow Floating point values which will automatically switch to `timeout_ms`
* Add `SOCKS5_HOSTNAME` support
* Added `Curl::Multi.autoclose` and `Curl::Multi#close` to improve connection handling. In past releases connection
clean up only ever happens when GC runs. Now in this release you can explicitly control connections via
`multi.close` or have it always close your connections with `Curl::Multi.autoclose=true`
### Breaking Changes
* Request handling reduces C (lines of code), and might resolve segfault when mixing curb with ruby timeout
* Timeout change for anyone who expected `0.9` to equal `0` or `infinity`

## 0.9.7
### Breaking Changes
### Fixes
* Guard use of `CURLOPT_RESOLVE` in `#if HAVE_CURLOPT_RESOLVE`
* Prevent `NoMethodError` when no HTTP Status
* [`Curl.urlalize` will miss raw url's query params](https://github.com/taf2/curb/issues/330)
* [Fix SSL versions constant names](https://github.com/taf2/curb/pull/333)
### Enhancements
* Add `Curl::Easy#resolve` method to implement static IP overrides, bypassing DNS
* [Add new curl features](https://github.com/taf2/curb/pull/331)
* [Allow extending `Curl::Easy` and having a different number of constructor arguments
than `Curl::Easy` allows](https://github.com/taf2/curb/pull/329)

## 0.9.4
### Fixes
* [Fix `multi.pipeline`: `HAVE_CURLMOPT_PIPELINING` was never defined](https://github.com/taf2/curb/issues/298)
* Eliminate warnings on Ruby `2.4.1`
* [Hang in `select()`](https://github.com/taf2/curb/issues/323)
* Revert 'Experimenting with adding multi handle cleanup after every request, normally would be cleaned up with GC
(problem addressed by fix hang in `select()`)
* Prevent errant callbacks once the handle is cleaned up
### Enhancements
* Add `PATCH` support?
* Add support for `CURLOPT_PATH_AS_IS`
* Add `.cookielist` method to `Curb::Easy`
* Improved Windows compatibility

## 0.9.3
### Fixes
* Correctly check for constants that may not be present in older versions of Curl
 
## 0.9.2
* **Breaking change** - Change base class for exceptions from `Exception` to `StandardError`
### Fixes
* [Segmentation fault (introduced with commit 50ab567)](https://github.com/taf2/curb/issues/240)
* Use 64-bit int for key in hash of active easy handles, to avoid clobbering on 32-bit systems
* [Fix `http_auth_types` for `:any` and `:anysafe`](https://github.com/taf2/curb/issues/291)
* [Use `LONG2NUM` and `NUM2LONG` to support all values of the `long` type](https://github.com/taf2/curb/issues/295)
* [Support empty reason phrases in the HTTP status line](https://github.com/taf2/curb/issues/292)
### Enhancements
* Implement setting of `CURLOPT_MAXCONNECTS`
* Implement setting of `CURLOPT_FORBID_REUSE`
* Ability to specify specific allowed TLS versions when connecting
* Add support for `HTTP/2.0` and multiplexing
* Experimenting with adding multi handle cleanup after every request, normally would be cleaned up with GC

## 0.9.1
### Fixes
* Fix regression in `timeout`/`timeout_ms` handling

## 0.9.0
### Fixes
* Remove `CURLAUTH_ANY` as the default for authentication setting, as too many services in the world don't
respond correctly with a `401` but instead `403` so the option rarely works as we'd hope
* Use the memory address as the key to correctly keep a hash of active easy handles alive in the multi handle -- should
resolve existing reports of memory leaks as well as crashes under some conditions

## 0.8.9
### Fixes
* Only append query string if it exists
* Setting any of `CURLOPT_HEADER`, `CURLOPT_NOPROGRESS`, `CURLOPT_NOSIGNAL`, `CURLOPT_HTTPGET` did not work
(introduced in commits `4358d04` and `14e9b53`)
* Compilation for curl versions before `7.40.0`
* [`Curl::Multi.http` call of the `on_complete` callback expects unsupplied
parameter](https://github.com/taf2/curb/issues/270)
### Enhancements
* Add `Curl::HTTP_2_0`, and `Curl.http2?`
* Support `CURLOPT_UNIX_SOCKET_PATH` in `Curl::Easy`
* Add `timeout_ms`/`connect_timeout_ms` options
* Add `on_redirect` callback handling to `.http`

## 0.8.8
### Fixes
* [`Curl::Easy.http_get` broken on macOS](https://github.com/taf2/curb/pull/242)
 
## 0.8.7
### Breaking Changes
### Fixes
* Restore support for older, `pre-7.17` versions of curl (fixes CentOS 5 compatibility)
* Improve Ruby `2.2.x` support (use `rb_thread_fd_select` instead of `rb_thread_select`)
### Enhancements
* Add SOCKS4A support
* Allow custom ssl cipher lists
* Add install instructions for Windows
* Make it possible easily reset the Thread current handle (`Curl.reset`)

## 0.8.6
### Breaking Changes
### Fixes
* Add support for the new error `CURLM_ADDED_ALREADY` introduced at curl `7.33.0`
* Add conditionals for new `URLM_ADDED_ALREADY` error
* `Curl#urlalize` - `CGI.escape` for param values
* Use correct HTTP verb for DELETE requests (was `delete`, and some servers are apparently case sensitive)
* Typo in `ext/curb_errors.c`: `BadBasswordError` -> `BadPasswordError`
* Status method now returns the last status in the response header
### Enhancements
* Support Ruby `2.2.0`
* Support `CURLOPT_TCP_NODELAY` in `Curl::Easy`
* Raise an exception if `curb` doesn't know how to handle a `CURLOPT`
* Expose license via gemspec
* Implement seek function for multi-pass authentication uploads

## 0.8.5
### Fixes
* Guard against a curl without GSSAPI delegation compiled in
* Added `extconf.rb` entry for `appconnect`
* Reset handle before each call when using "super simple API"
### Enhancements
* Added support for `app_connect_time` from `libcurl`
* Alias `body_str` to `body` and `header_str` to `head`

## 0.8.4
### Fixes
* [Setting `post_body` to `nil` does not reset data posted in `http(:GET)`](https://github.com/taf2/curb/issues/133)
* Various spurious/inconsequential warnings
* `Curl.head` uses `OPTIONS` method instead of `HEAD` method
### Enhancements
* Make "super simple API" more efficient by storing `Curl::Easy` instance in `Thread.current`
* GSSAPI: Add required flags for GSSAPI delegation support
* GSSAPI: Allow setting of the new delegation flag
* GSSAPI: Implement support for libcurl SPNEGO flags in curb

## 0.8.3
### Fixes
* [Exception raised when process is signaled](https://github.com/taf2/curb/issues/117)
* [Curb always appends a query to request uris](https://github.com/taf2/curb/issues/137)

## 0.8.2
### Fixes
* Clang compatibility
### Enhancements
* Allow string data with simpler interface e.g. `Curl.post('...', 'data')`
 
## 0.8.1
### Fixes
* [Error installing on Ubuntu](https://github.com/taf2/curb/issues/106)
* Protect callbacks from reset/close
* `Curl::Multi` not actually performing HTTP requests in cases when the `:max_connects >= number of requests`
* Build error when compiled with `-Werror=format-security`
* `postalize` parameter escaping
### Enhancements
* Support `CURLINFO_REDIRECT_URL`
* Support `CURLOPT_RESUME_FROM` in `Curl::Easy`
* Add `.status` to `Curl::Easy` instances
* Support `CURLOPT_FAILONERROR`
* Add even easier interface: `Curl.get` / `Curl.post` / e.t.c

## 0.8.0
* **Breaking change** - `on_failure` only fires for 5xx responses 
### Fixes
* Error in README section for HTTP POST file upload.
### Enhancements
* Add a default for `resolve_mode` to `CURL_IPRESOLVE_WHATEVER` e.g. `:auto`
* Add `on_redirect` and `on_missing` callbacks for 3xx/4xx responses
* Allow `http_post(array_of_fields)` when using multipart form posts

## 0.7.18
### Fixes
* Check for obsolete, since it appears to be gone from later versions of libcurl per issue #100
* Regression in PUT requests with NULL bytes (caused by `RSTRING_PTR` => `StringValueCStr` change)

## 0.7.17
### Fixes
* Restore compatibility with 0.7.16 (e.g. suport for building without `CURLOPT_SEEKDATA`, etc)
* Use `rb_thread_blocking_region` when available in place of `rb_thread_select` for ruby 1.9.x
* Copy-paste mistake which made it impossible to set `CURLOPT_SSL_VERIFYHOST` to `2`
* Copy-paste mistake that caused `CURLOPT_SSL_VERIFYPEER` to be overwritten by the value of `use_netrc` setting
### Enhancements
* Warn on exceptions in success/failure/complete callbacks
* Use `StringValueCStr` more instead of the less safe `RSTRING_PTR`
* Defaulting `CURLOPT_SSL_VERIFYHOST` to `2` to make it consistent with cURLs default value

## 0.7.16
* Add better support for timeouts - issue 48
* Improve stability, avoid bus errors when raising in callbacks
* Allow Content-Length header when sending a PUT request
* Better handling of cacert
* Support `CURLOPT_IPRESOLVE`
* Allow `PostField.file#to_s`
* Minor refactoring to how `PostField.file` handles strings
* Accept symbol and any object that `responds_to?(:to_s)` for `easy.http(:verb)`
* Free memory sooner after a request finishes instead of waiting for garbage collection
* Fix crash when sending nil for the `http_put` body
* Fix inspect for `Curl::Easy`, thanks Hongli Lai
* Fix unit test assertions for `Curl::Multi`, thanks Hongli Lai

## 0.7.7
* `Curl::Multi` perform is more robust process callbacks before exiting the loop - issue 24.
Thanks to igrigorik and ramsingla
* improve `Curl::Multi` idle callback robustness, trigger callbacks more often - issue 24.
Thanks to igrigorik and ramsingla

## 0.7.6
* fix bug in http_* when http_delete or http("verb") raises an exception
* add support for autoreferrer e.g. if follow_location=true it will pass that along

## 0.7.5
* minor fix for no signal instead of INT2FIX Check boolean value
 
## 0.7.4
* add support to set the http version e.g. `easy.version = Curl::HTTP_1_1`
* add support to disable libcurls internal use of signals.

## 0.7.3
* Support rubinius
* allow Multi.download to be called without a block
* add additional `Multi.download` examples

## 0.7.2
* Start tracking high level changes
* Add `Curl::Easy#close` - hard close an easy handle
* Add `Curl::Easy#reset` - reset all settings but keep connections alive
* Maintain persistent connections for single easy handles
