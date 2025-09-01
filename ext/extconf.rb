require 'mkmf'
require 'tmpdir'
begin
  require 'etc'
rescue LoadError
  # Etc may not be available on all Ruby builds (very rare). Fallback later.
end

dir_config('curl')

if find_executable('curl-config')
  $CFLAGS << " #{`curl-config --cflags`.strip} -g"
  if ENV['STATIC_BUILD']
    $LIBS << " #{`curl-config --static-libs`.strip}"
  else
    $LIBS << " #{`curl-config --libs`.strip}"
  end
  ca_bundle_path=`curl-config --ca`.strip.gsub(/^"([^"]+)"$/,'\1')
  if !ca_bundle_path.nil? and ca_bundle_path != ''
    $defs.push( %{-D HAVE_CURL_CONFIG_CA} )
    $defs.push( %{-D CURL_CONFIG_CA='#{ca_bundle_path.inspect}'} )
  end
elsif !have_library('curl') or !have_header('curl/curl.h')
  fail <<-EOM
  Can't find libcurl or curl/curl.h

  Make sure development libs (ie libcurl4-openssl-dev) are installed on the system.

  Try passing --with-curl-dir or --with-curl-lib and --with-curl-include
  options to extconf.
  EOM
end

# Check arch flags
# TODO: detect mismatched arch types when libcurl mac ports is mixed with native mac ruby or vice versa
#archs = $CFLAGS.scan(/-arch\s(.*?)\s/).first # get the first arch flag
#if archs and archs.size >= 1
#  # need to reduce the number of archs...
#  # guess the first one is correct... at least the first one is probably the ruby installed arch...
#  # this could lead to compiled binaries that crash at runtime...
#  $CFLAGS.gsub!(/-arch\s(.*?)\s/,' ')
#  $CFLAGS << " -arch #{archs.first}"
#  puts "Selected arch: #{archs.first}"
#end

def define(s, v = 1)
  $defs.push(format("-D HAVE_%s=%d", s.to_s.upcase, v))
end

# Optional parallelization support for constant checks only.
# Enable automatically when job hints are present (JOBS, BUNDLE_JOBS, MAKEFLAGS -jN),
# or explicitly via EXTCONF_JOBS/EXTCONF_PARALLEL.

def parse_jobs_from_makeflags(flags)
  return nil if flags.nil? || flags.empty?
  tokens = flags.to_s.split(/\s+/)
  jobs = nil
  tokens.each_with_index do |tok, i|
    case tok
    when /\A-j(\d+)\z/
      jobs = $1.to_i
    when '-j'
      nxt = tokens[i + 1]
      jobs = nxt.to_i if nxt && nxt =~ /\A\d+\z/
    when /\A--jobs(?:=(\d+)|\s+(\d+))\z/
      jobs = ($1 || $2).to_i
    when /\Aj(\d+)\z/ # sometimes make condenses flags
      jobs = $1.to_i
    end
    break if jobs && jobs > 0
  end
  jobs
end

def detect_job_hints
  # Priority: explicit extconf hint, then common envs used by bundler/rubygems
  [
    ENV['EXTCONF_JOBS'],
    ENV['JOBS'],
    ENV['BUNDLE_JOBS'],
    parse_jobs_from_makeflags(ENV['MAKEFLAGS'])
  ].each do |v|
    n = v.to_i if v
    return n if n && n > 0
  end
  nil
end

explicit_parallel = ENV.key?('EXTCONF_PARALLEL') && ENV['EXTCONF_PARALLEL'] == '1'
job_hints = detect_job_hints

DEFAULT_PARALLEL_JOBS = begin
  n = Etc.respond_to?(:nprocessors) ? Etc.nprocessors.to_i : 1
  n = 1 if n <= 0
  # Use half the CPUs by default (rounded up), but at least 1
  jobs = [(n.to_f / 2).ceil, 1].max
  jobs
rescue
  1
end

PARALLEL_JOBS = begin
  if job_hints && job_hints > 0
    job_hints
  else
    # If no hints provided, default to half the CPUs.
    DEFAULT_PARALLEL_JOBS
  end
end

# Only enable if the platform supports fork. On Windows this stays sequential.
PARALLEL_CONSTANT_CHECKS = PARALLEL_JOBS > 1 && Process.respond_to?(:fork)

$queued_constants = []

def try_constant_compile(sname)
  src = %{
    #include <curl/curl.h>
    int main() {
      int test = (int)#{sname};
      (void)test;
      return 0;
    }
  }
  try_compile(src, "#{$CFLAGS} #{$LIBS}")
end

def have_constant(name)
  # Queue constants for optional parallel probing. Falls back to sequential.
  if PARALLEL_CONSTANT_CHECKS
    $queued_constants << name
    true
  else
    sname = name.is_a?(Symbol) ? name.to_s : name.upcase
    checking_for name do
      if try_constant_compile(sname)
        define name
        true
      else
        false
      end
    end
  end
end

def flush_constant_checks
  return if $queued_constants.empty?

  constants = $queued_constants.uniq
  $queued_constants.clear

  # On platforms without fork (e.g., Windows), run sequentially.
  unless PARALLEL_CONSTANT_CHECKS
    constants.each { |name| have_constant(name) }
    return
  end

  # Run constant probes in isolated child processes to avoid mkmf
  # scratch file and logfile contention.
  results = {}
  pending = constants.dup
  running = {}
  max_jobs = PARALLEL_JOBS

  spawn_probe = lambda do |const_name|
    r, w = IO.pipe
    pid = fork do
      begin
        r.close
        Dir.mktmpdir('curb-mkmf-') do |dir|
          Dir.chdir(dir) do
            begin
              # Avoid clobbering the main mkmf.log
              Logging::logfile = File.open(File::NULL, 'w') rescue File.open(File.join(dir, 'mkmf.log'), 'w')
            rescue
              # best-effort
            end
            sname = const_name.is_a?(Symbol) ? const_name.to_s : const_name.upcase
            ok = try_constant_compile(sname)
            w.write([const_name, ok ? 1 : 0].join("\t"))
          end
        end
      rescue
        # Treat as failure if anything unexpected happens in child
        begin
          w.write([const_name, 0].join("\t"))
        rescue
        end
      ensure
        begin w.close rescue nil end
        # Ensure the child exits without running at_exit handlers
        exit! 0
      end
    end
    w.close
    running[pid] = r
  end

  # Start initial batch
  while running.size < max_jobs && !pending.empty?
    spawn_probe.call(pending.shift)
  end

  # Collect results and keep spawning until done
  until running.empty?
    pid = Process.wait
    io = running.delete(pid)
    if io
      begin
        msg = io.read.to_s
        name_str, ok_str = msg.split("\t", 2)
        if name_str
          # Map back to the original object if symbol-like
          original = constants.find { |n| n.to_s == name_str }
          results[original || name_str] = ok_str.to_i == 1
        end
      ensure
        begin io.close rescue nil end
      end
    end
    # Fill next task slot
    spawn_probe.call(pending.shift) unless pending.empty?
  end

  # Apply results to $defs and output summary via checking_for
  results.each do |const_name, ok|
    sname = const_name.is_a?(Symbol) ? const_name.to_s : const_name.upcase
    checking_for const_name do
      if ok
        define const_name
        true
      else
        false
      end
    end
  end
end

have_constant "curlopt_tcp_keepalive"
have_constant "curlopt_tcp_keepidle"
have_constant "curlopt_tcp_keepintvl"
have_constant "curlinfo_appconnect_time"
have_constant "curlinfo_redirect_time"
have_constant "curlinfo_response_code"
have_constant "curlinfo_filetime"
have_constant "curlinfo_redirect_count"
have_constant "curlinfo_os_errno"
have_constant "curlinfo_num_connects"
have_constant "curlinfo_cookielist"
have_constant "curlinfo_ftp_entry_path"
have_constant "curl_version_ssl"
have_constant "curl_version_libz"
have_constant "curl_version_ntlm"
have_constant "curl_version_gssnegotiate"
have_constant "curl_version_debug"
have_constant "curl_version_asynchdns"
have_constant "curl_version_spnego"
have_constant "curl_version_largefile"
have_constant "curl_version_idn"
have_constant "curl_version_sspi"
have_constant "curl_version_conv"
have_constant "curl_version_http2"
have_constant "curlproxy_http"
have_constant "curlproxy_socks4"
have_constant "curlproxy_socks4a"
have_constant "curlproxy_socks5"
have_constant "curlproxy_socks5_hostname"
have_constant "curlauth_basic"
have_constant "curlauth_digest"
have_constant "curlauth_gssnegotiate"
have_constant "curlauth_ntlm"
have_constant "curlauth_anysafe"
have_constant "curlauth_any"
have_constant "curle_tftp_notfound"
have_constant "curle_tftp_perm"
have_constant "curle_tftp_diskfull"
have_constant "curle_tftp_illegal"
have_constant "curle_tftp_unknownid"
have_constant "curle_tftp_exists"
have_constant "curle_tftp_nosuchuser"
# older versions of libcurl 7.12
have_constant "curle_send_fail_rewind"
have_constant "curle_ssl_engine_initfailed"
have_constant "curle_login_denied"

# older than 7.10.0
have_constant "curlopt_nosignal"

# older than 7.16.0
have_constant "curlmopt_pipelining"

# older than 7.16.3
have_constant "curlmopt_maxconnects"

have_constant "curlopt_seekfunction"
have_constant "curlopt_seekdata"
have_constant "curlopt_sockoptfunction"
have_constant "curlopt_sockoptdata"
have_constant "curlopt_opensocketfunction"
have_constant "curlopt_opensocketdata"

# additional consts
have_constant "curle_conv_failed"
have_constant "curle_conv_reqd"
have_constant "curle_ssl_cacert_badfile"
have_constant "curle_remote_file_not_found"
have_constant "curle_ssh"
have_constant "curle_ssl_shutdown_failed"
have_constant "curle_again"
have_constant "curle_ssl_crl_badfile"
have_constant "curle_ssl_issuer_error"

# added in 7.18.2
have_constant "curlinfo_redirect_url"

# username/password added in 7.19.1
have_constant "curlopt_username"
have_constant "curlopt_password"
have_constant "curlinfo_primary_ip"

# ie quirk added in 7.19.3
have_constant "curlauth_digest_ie"

# added in 7.15.1
have_constant "curlftpmethod_multicwd"
have_constant "curlftpmethod_nocwd"
have_constant "curlftpmethod_singlecwd"

# centos 4.5 build of libcurl
have_constant "curlm_bad_socket"
have_constant "curlm_unknown_option"
have_func("curl_multi_timeout")
have_func("curl_multi_fdset")
have_func("curl_multi_perform")

have_constant "curlopt_haproxyprotocol"

# constants
have_constant "curlopt_interleavefunction"
have_constant "curlopt_interleavedata"
have_constant "curlopt_chunk_bgn_function"
have_constant "curlopt_chunk_end_function"
have_constant "curlopt_chunk_data"
have_constant "curlopt_fnmatch_function"
have_constant "curlopt_fnmatch_data"
have_constant "curlopt_errorbuffer"
have_constant "curlopt_stderr"
have_constant "curlopt_failonerror"
have_constant "curlopt_url"
have_constant "curlopt_protocols"
have_constant "curlopt_redir_protocols"
have_constant "curlopt_proxy"
have_constant "curlopt_proxyport"
have_constant "curlopt_proxytype"
have_constant "curlopt_noproxy"
have_constant "curlopt_httpproxytunnel"
have_constant "curlopt_socks5_gssapi_service"
have_constant "curlopt_socks5_gssapi_nec"
have_constant "curlopt_interface"
have_constant "curlopt_localport"
have_constant "curlopt_dns_cache_timeout"
have_constant "curlopt_dns_use_global_cache"
have_constant "curlopt_buffersize"
have_constant "curlopt_port"
have_constant "curlopt_tcp_nodelay"
have_constant "curlopt_address_scope"
have_constant "curlopt_netrc"
have_constant "curl_netrc_optional"
have_constant "curl_netrc_ignored"
have_constant "curl_netrc_required"
have_constant "curlopt_netrc_file"
have_constant "curlopt_userpwd"
have_constant "curlopt_proxyuserpwd"
have_constant "curlopt_username"
have_constant "curlopt_password"
have_constant "curlopt_password"
have_constant "curlopt_password"
have_constant "curlopt_httpauth"
have_constant "curlauth_digest_ie"
have_constant "curlauth_only"
have_constant "curlopt_tlsauth_type"
have_constant "curlopt_tlsauth_srp"
have_constant "curlopt_tlsauth_username"
have_constant "curlopt_tlsauth_password"
have_constant "curlopt_proxyauth"
have_constant "curlopt_autoreferer"
have_constant "curlopt_encoding"
have_constant "curlopt_followlocation"
have_constant "curlopt_unrestricted_auth"
have_constant "curlopt_maxredirs"
have_constant "curlopt_postredir"
have_constant "curlopt_put"
have_constant "curlopt_post"
have_constant "curlopt_postfields"
have_constant "curlopt_postfieldsize"
have_constant "curlopt_postfieldsize_large"
have_constant "curlopt_copypostfields"
have_constant "curlopt_httppost"
have_constant "curlopt_referer"
have_constant "curlopt_useragent"
have_constant "curlopt_httpheader"
have_constant "curlopt_proxyheader"
have_constant "curlopt_http200aliases"
have_constant "curlopt_cookie"
have_constant "curlopt_cookiefile"
have_constant "curlopt_cookiejar"
have_constant "curlopt_cookiesession"
have_constant "curlopt_cookielist"
have_constant "curlopt_httpget"
have_constant "curlopt_http_version"
have_constant "curl_http_version_none"
have_constant "curl_http_version_1_0"
have_constant "curl_http_version_1_1"
have_constant "curlopt_ignore_content_length"
have_constant "curlopt_http_content_decoding"
have_constant "curlopt_http_transfer_decoding"
have_constant "curlopt_mail_from"
have_constant "curlopt_mail_rcpt"
have_constant "curlopt_tftp_blksize"
have_constant "curlopt_ftpport"
have_constant "curlopt_quote"
have_constant "curlopt_postquote"
have_constant "curlopt_prequote"
have_constant "curlopt_dirlistonly"
have_constant "curlopt_append"
have_constant "curlopt_ftp_use_eprt"
have_constant "curlopt_ftp_use_epsv"
have_constant "curlopt_ftp_use_pret"
have_constant "curlopt_ftp_create_missing_dirs"
have_constant "curlopt_ftp_response_timeout"
have_constant "curlopt_ftp_alternative_to_user"
have_constant "curlopt_ftp_skip_pasv_ip"
have_constant "curlopt_ftpsslauth"
have_constant "curlftpauth_default"
have_constant "curlftpauth_ssl"
have_constant "curlftpauth_tls"
have_constant "curlopt_ftp_ssl_ccc"
have_constant "curlftpssl_ccc_none"
have_constant "curlftpssl_ccc_passive"
have_constant "curlftpssl_ccc_active"
have_constant "curlopt_ftp_account"
have_constant "curlopt_ftp_filemethod"
have_constant "curlftpmethod_multicwd"
have_constant "curlftpmethod_nocwd"
have_constant "curlftpmethod_singlecwd"
have_constant "curlopt_rtsp_request"
have_constant "curl_rtspreq_options"
have_constant "curl_rtspreq_describe"
have_constant "curl_rtspreq_announce"
have_constant "curl_rtspreq_setup"
have_constant "curl_rtspreq_play"
have_constant "curl_rtspreq_pause"
have_constant "curl_rtspreq_teardown"
have_constant "curl_rtspreq_get_parameter"
have_constant "curl_rtspreq_set_parameter"
have_constant "curl_rtspreq_record"
have_constant "curl_rtspreq_receive"
have_constant "curlopt_rtsp_session_id"
have_constant "curlopt_rtsp_stream_uri"
have_constant "curlopt_rtsp_transport"
have_constant "curlopt_rtsp_header"
have_constant "curlopt_rtsp_client_cseq"
have_constant "curlopt_rtsp_server_cseq"
have_constant "curlopt_transfertext"
have_constant "curlopt_proxy_transfer_mode"
have_constant "curlopt_crlf"
have_constant "curlopt_range"
have_constant "curlopt_resume_from"
have_constant "curlopt_resume_from_large"
have_constant "curlopt_customrequest"
have_constant "curlopt_filetime"
have_constant "curlopt_nobody"
have_constant "curlopt_infilesize"
have_constant "curlopt_infilesize_large"
have_constant "curlopt_upload"
have_constant "curlopt_maxfilesize"
have_constant "curlopt_maxfilesize_large"
have_constant "curlopt_timecondition"
have_constant "curlopt_timevalue"
have_constant "curlopt_timeout"
have_constant "curlopt_timeout_ms"
have_constant "curlopt_low_speed_limit"
have_constant "curlopt_low_speed_time"
have_constant "curlopt_max_send_speed_large"
have_constant "curlopt_max_recv_speed_large"
have_constant "curlopt_maxconnects"
have_constant "curlopt_closepolicy"
have_constant "curlopt_fresh_connect"
have_constant "curlopt_forbid_reuse"
have_constant "curlopt_connecttimeout"
have_constant "curlopt_connecttimeout_ms"
have_constant "curlopt_ipresolve"
have_constant "curl_ipresolve_whatever"
have_constant "curl_ipresolve_v4"
have_constant "curl_ipresolve_v6"
have_constant "curlopt_connect_only"
have_constant "curlopt_use_ssl"
have_constant "curlusessl_none"
have_constant "curlusessl_try"
have_constant "curlusessl_control"
have_constant "curlusessl_all"
have_constant "curlopt_resolve"
have_constant "curlopt_request_target"
have_constant "curlopt_sslcert"
have_constant "curlopt_sslcerttype"
have_constant "curlopt_sslkey"
have_constant "curlopt_sslkeytype"
have_constant "curlopt_keypasswd"
have_constant "curlopt_sslengine"
have_constant "curlopt_sslengine_default"
have_constant "curlopt_sslversion"
have_constant "curl_sslversion_default"
have_constant :CURL_SSLVERSION_TLSv1
have_constant :CURL_SSLVERSION_SSLv2
have_constant :CURL_SSLVERSION_SSLv3

# Added in 7.30.0
have_constant "curlmopt_max_host_connections"

# Added in 7.34.0
have_constant :CURL_SSLVERSION_TLSv1_0
have_constant :CURL_SSLVERSION_TLSv1_1
have_constant :CURL_SSLVERSION_TLSv1_2

# Added in 7.52.0
have_constant :CURL_SSLVERSION_TLSv1_3

have_constant "curlopt_ssl_verifypeer"
have_constant "curlopt_cainfo"
have_constant "curlopt_issuercert"
have_constant "curlopt_capath"
have_constant "curlopt_crlfile"
have_constant "curlopt_ssl_verifyhost"
have_constant "curlopt_certinfo"
have_constant "curlopt_random_file"
have_constant "curlopt_egdsocket"
have_constant "curlopt_ssl_cipher_list"
have_constant "curlopt_ssl_sessionid_cache"
have_constant "curlopt_krblevel"
have_constant "curlopt_ssh_auth_types"
have_constant "curlopt_ssh_host_public_key_md5"
have_constant "curlopt_ssh_public_keyfile"
have_constant "curlopt_ssh_private_keyfile"
have_constant "curlopt_ssh_knownhosts"
have_constant "curlopt_ssh_keyfunction"
have_constant "curlkhstat_fine_add_to_file"
have_constant "curlkhstat_fine"
have_constant "curlkhstat_reject"
have_constant "curlkhstat_defer"
have_constant "curlopt_ssh_keydata"
have_constant "curlopt_private"
have_constant "curlopt_share"
have_constant "curlopt_new_file_perms"
have_constant "curlopt_new_directory_perms"
have_constant "curlopt_telnetoptions"
# was obsoleted in August 2007 for 7.17.0, reused in April 2011 for 7.21.5
have_constant "curle_not_built_in"

have_constant "curle_obsolete" # removed in 7.24 ?

have_constant "curle_ftp_pret_failed"
have_constant "curle_rtsp_cseq_error"
have_constant "curle_rtsp_session_error"
have_constant "curle_ftp_bad_file_list"
have_constant "curle_chunk_failed"
have_constant "curle_no_connection_available"
have_constant "curle_ssl_pinnedpubkeynotmatch"
have_constant "curle_ssl_invalidcertstatus"
have_constant "curle_http2_stream"

# gssapi/spnego delegation related constants
have_constant "curlopt_gssapi_delegation"
have_constant "curlgssapi_delegation_policy_flag"
have_constant "curlgssapi_delegation_flag"

have_constant "CURLM_ADDED_ALREADY"

# added in 7.40.0
have_constant "curlopt_unix_socket_path"

# added in 7.42.0
have_constant "curlopt_path_as_is"

# added in 7.43.0
have_constant "curlopt_pipewait"

have_constant "curlopt_proxy_ssl_verifyhost"

# protocol constants
have_constant "curlproto_all"
have_constant "curlproto_dict"
have_constant "curlproto_file"
have_constant "curlproto_ftp"
have_constant "curlproto_ftps"
have_constant "curlproto_gopher"
have_constant "curlproto_http"
have_constant "curlproto_https"
have_constant "curlproto_imap"
have_constant "curlproto_imaps"
have_constant "curlproto_ldap"
have_constant "curlproto_ldaps"
have_constant "curlproto_pop3"
have_constant "curlproto_pop3s"
have_constant "curlproto_rtmp"
have_constant "curlproto_rtmpe"
have_constant "curlproto_rtmps"
have_constant "curlproto_rtmpt"
have_constant "curlproto_rtmpte"
have_constant "curlproto_rtmpts"
have_constant "curlproto_rtsp"
have_constant "curlproto_scp"
have_constant "curlproto_sftp"
have_constant "curlproto_smb"
have_constant "curlproto_smbs"
have_constant "curlproto_smtp"
have_constant "curlproto_smtps"
have_constant "curlproto_telnet"
have_constant "curlproto_tftp"

if try_compile('int main() { return 0; }','-Wall')
  $CFLAGS << ' -Wall'
end

# do some checking to detect ruby 1.8 hash.c vs ruby 1.9 hash.c
def test_for(name, const, src)
  checking_for name do
    if try_compile(src,"#{$CFLAGS} #{$LIBS}")
      define const
      true
    else
      false
    end
  end
end

test_for("curl_easy_escape", "CURL_EASY_ESCAPE", %{
  #include <curl/curl.h>
  int main() {
    CURL *easy = curl_easy_init();
    curl_easy_escape(easy,"hello",5);
    return 0;
  }
})

have_func('rb_thread_blocking_region')
have_header('ruby/thread.h') && have_func('rb_thread_call_without_gvl', 'ruby/thread.h')
have_header('ruby/io.h')
have_func('rb_thread_fd_select', 'ruby/io.h')
have_func('rb_wait_for_single_fd', 'ruby/io.h')
have_header('ruby/fiber/scheduler.h')
have_func('rb_fiber_scheduler_current', 'ruby/fiber/scheduler.h')
have_func('rb_fiber_scheduler_io_wait', 'ruby/fiber/scheduler.h')
have_func('rb_io_stdio_file')
have_func('curl_multi_wait')
have_func('curl_multi_socket_action')
have_func('curl_multi_assign')
have_func('curl_multi_wakeup')
have_func('curl_multi_poll')
have_func('curl_multi_socket')
have_func('curl_multi_timer_callback')
have_constant 'curlmopt_socketfunction'
have_constant 'curlmopt_timerfunction'
have_func('curl_easy_duphandle')

# Optional: enable verbose socket-action debug logging.
# Set CURB_SOCKET_DEBUG=1 in the environment before running extconf to enable.
if ENV['CURB_SOCKET_DEBUG'] == '1'
  $defs << '-DCURB_SOCKET_DEBUG=1'
end

# Run any queued constant checks (in parallel if enabled) before header generation.
flush_constant_checks

create_header('curb_config.h')
create_makefile('curb_core')
