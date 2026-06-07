require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlNetworkPolicy < Test::Unit::TestCase
  include TestServerMethods

  def setup
    server_setup
  end

  def teardown
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
    super
  end

  def test_network_policy_defaults_to_none
    easy = Curl::Easy.new

    assert_equal :none, easy.network_policy
  end

  def test_network_policy_rejects_unknown_policy
    easy = Curl::Easy.new

    assert_raise(ArgumentError) do
      easy.network_policy = :private
    end
  end

  def test_safe_bang_allowed_hosts_rejects_unlisted_initial_host
    Curl.safe! do |config|
      config.allowed_hosts = ['allowed.example']
    end

    easy = Curl::Easy.new('http://blocked.example/')

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end

    assert_match(/host allowlist/, error.message)
    assert_match(/blocked\.example/, error.message)
  end

  def test_safe_bang_allowed_hosts_accepts_matching_initial_host
    Curl.safe! do |config|
      config.allowed_hosts = ['ALLOWED.example.']
    end

    easy = Curl::Easy.new('http://allowed.example/path')

    Curl.__send__(:apply_safety!, easy)
  end

  def test_safe_bang_allowed_hosts_strips_ports_without_scheme
    Curl.safe! do |config|
      config.allowed_hosts = ['api.example:443', '[2001:db8::1]:443']
    end

    easy = Curl::Easy.new('http://api.example/path')
    Curl.__send__(:apply_safety!, easy)

    assert_equal ['api.example', '2001:db8::1'], easy.allowed_hosts
  end

  def test_safety_allowlists_are_deduplicated_after_normalization
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_hosts = ['API.example.', 'api.example', 'https://api.example/path']
      config.allowed_cidrs = ['1.1.1.0/24', '1.1.1.0/24']
    end

    easy = Curl::Easy.new('http://api.example/')
    Curl.__send__(:apply_safety!, easy)

    assert_equal ['api.example'], easy.allowed_hosts
    assert_equal ['1.1.1.0/24'], easy.allowed_cidrs
  end

  def test_easy_allowlists_are_deduplicated_after_normalization
    easy = Curl::Easy.new

    easy.allowed_hosts = ['API.example.', 'api.example', 'https://api.example/path']
    easy.allowed_cidrs = ['1.1.1.0/24', '1.1.1.0/24']

    assert_equal ['api.example'], easy.allowed_hosts
    assert_equal ['1.1.1.0/24'], easy.allowed_cidrs
  end

  def test_safe_bang_allowed_hosts_allows_same_host_redirect
    omit('redirect-aware host allowlists require CURLOPT_PREREQFUNCTION') unless Curl.const_defined?(:CURLOPT_PREREQFUNCTION)

    allowed_host = 'curb-allowed.test'

    with_host_redirect_server(allowed_host, allowed_host) do |port, hits|
      Curl.safe! do |config|
        config.allowed_hosts = [allowed_host]
      end

      easy = Curl::Easy.new("http://#{allowed_host}:#{port}/redirect-to-target")
      easy.resolve = [resolve_entry(allowed_host, port, '127.0.0.1')]
      easy.follow_location = true

      easy.perform

      assert_equal 200, easy.response_code
      assert_equal 'target', easy.body_str
      assert_equal 1, hits[:redirect]
      assert_equal 1, hits[:target]
    end
  end

  def test_safe_bang_allowed_hosts_blocks_redirect_to_unlisted_host
    omit('redirect-aware host allowlists require CURLOPT_PREREQFUNCTION') unless Curl.const_defined?(:CURLOPT_PREREQFUNCTION)

    allowed_host = 'curb-allowed.test'
    blocked_host = 'curb-blocked.test'

    with_host_redirect_server(allowed_host, blocked_host) do |port, hits|
      Curl.safe! do |config|
        config.allowed_hosts = [allowed_host]
      end

      easy = Curl::Easy.new("http://#{allowed_host}:#{port}/redirect-to-target")
      easy.resolve = [
        resolve_entry(allowed_host, port, '127.0.0.1'),
        resolve_entry(blocked_host, port, '127.0.0.1')
      ]
      easy.follow_location = true

      error = assert_raise(Curl::Err::UnsafeDestinationError) do
        easy.perform
      end

      assert_match(/host allowlist/, error.message)
      assert_match(/#{Regexp.escape(blocked_host)}/, error.message)
      assert_equal 1, hits[:redirect]
      assert_equal 0, hits[:target], 'blocked redirected host should not receive a request'
    end
  end

  def test_safe_bang_allowed_cidrs_requires_public_network_policy
    Curl.safe! do |config|
      config.allowed_cidrs = ['1.1.1.0/24']
    end

    easy = Curl::Easy.new('http://1.1.1.1/')

    assert_raise(ArgumentError) do
      Curl.__send__(:apply_safety!, easy)
    end
  end

  def test_allowed_cidrs_rejects_invalid_ranges
    easy = Curl::Easy.new

    assert_raise(ArgumentError) do
      easy.allowed_cidrs = ['not-a-cidr']
    end

    assert_raise(ArgumentError) do
      Curl.safe! { |config| config.allowed_cidrs = ['1.1.1.1/33'] }
    end
  end

  def test_clone_preserves_allowed_hosts_for_native_prereq_checks
    omit('redirect-aware host allowlists require CURLOPT_PREREQFUNCTION') unless Curl.const_defined?(:CURLOPT_PREREQFUNCTION)

    allowed_host = 'curb-clone-allowed.test'
    blocked_host = 'curb-clone-blocked.test'
    original = Curl::Easy.new("http://#{allowed_host}:#{TestServlet.port}#{TestServlet.path}")
    original.allowed_hosts = [allowed_host]

    clone = original.clone
    clone.url = "http://#{blocked_host}:#{TestServlet.port}#{TestServlet.path}"
    clone.resolve = [resolve_entry(blocked_host, TestServlet.port, '127.0.0.1')]
    clone.connect_timeout_ms = 50

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      clone.perform
    end

    assert_match(/host allowlist/, error.message)
    assert_match(/#{Regexp.escape(blocked_host)}/, error.message)
  end

  def test_clone_preserves_allowed_cidrs_for_native_public_policy_checks
    require_public_network_policy!

    original = Curl::Easy.new('http://1.1.1.1:81/')
    original.network_policy = :public
    original.allowed_cidrs = ['8.8.8.0/24']
    original.connect_timeout_ms = 50
    original.timeout_ms = 100

    clone = original.clone
    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      clone.perform
    end

    assert_match(/outside allowed CIDR ranges/, error.message)
    assert_match(/1\.1\.1\.1/, error.message)
  end

  def test_allowed_cidrs_set_before_public_network_policy_are_enforced
    require_public_network_policy!

    easy = Curl::Easy.new('http://1.1.1.1:81/')
    easy.allowed_cidrs = ['8.8.8.0/24']
    easy.network_policy = :public
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/outside allowed CIDR ranges/, error.message)
    assert_match(/1\.1\.1\.1/, error.message)
  end

  def test_public_network_policy_blocks_loopback_destination
    easy = Curl::Easy.new(TestServlet.url)
    require_public_network_policy!(easy)

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/127\.0\.0\.1/, error.message)
    assert_match(/public network policy/, error.message)
    assert_match(/127\.0\.0\.1/, easy.unsafe_destination_error)
  end

  def test_public_network_policy_does_not_reuse_pre_policy_loopback_connection
    easy = Curl::Easy.new(TestServlet.url)

    easy.perform
    assert_equal 200, easy.response_code
    assert_match(/GET/, easy.body_str)

    require_public_network_policy!(easy)
    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/127\.0\.0\.1/, error.message)
  end

  def test_public_network_policy_reuse_controls_clear_when_policy_is_disabled
    omit('CURLOPT_FRESH_CONNECT is not available') unless Curl.const_defined?(:CURLOPT_FRESH_CONNECT)
    omit('CURLOPT_FORBID_REUSE is not available') unless Curl.const_defined?(:CURLOPT_FORBID_REUSE)

    previous_autoclose = Curl::Multi.autoclose
    Curl::Multi.autoclose = false

    easy = Curl::Easy.new(TestServlet.url)
    require_public_network_policy!(easy)

    assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    easy.network_policy = :none
    easy.perform
    assert_equal 1, easy.num_connects

    easy.perform
    assert_equal 0, easy.num_connects
  ensure
    easy.close if defined?(easy) && easy
    Curl::Multi.autoclose = previous_autoclose if defined?(previous_autoclose)
  end

  def test_default_network_policy_preserves_explicit_forbid_reuse
    omit('CURLOPT_FORBID_REUSE is not available') unless Curl.const_defined?(:CURLOPT_FORBID_REUSE)

    previous_autoclose = Curl::Multi.autoclose
    Curl::Multi.autoclose = false

    easy = Curl::Easy.new(TestServlet.url)
    easy.setopt(Curl::CURLOPT_FORBID_REUSE, 1)

    easy.perform
    assert_equal 1, easy.num_connects

    easy.perform
    assert_equal 1, easy.num_connects
  ensure
    easy.close if defined?(easy) && easy
    Curl::Multi.autoclose = previous_autoclose if defined?(previous_autoclose)
  end

  def test_public_network_policy_disable_preserves_explicit_forbid_reuse
    omit('CURLOPT_FORBID_REUSE is not available') unless Curl.const_defined?(:CURLOPT_FORBID_REUSE)

    previous_autoclose = Curl::Multi.autoclose
    Curl::Multi.autoclose = false

    easy = Curl::Easy.new(TestServlet.url)
    easy.setopt(Curl::CURLOPT_FORBID_REUSE, 1)
    require_public_network_policy!(easy)

    assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    easy.network_policy = :none
    easy.perform
    assert_equal 1, easy.num_connects

    easy.perform
    assert_equal 1, easy.num_connects
  ensure
    easy.close if defined?(easy) && easy
    Curl::Multi.autoclose = previous_autoclose if defined?(previous_autoclose)
  end

  def test_public_network_policy_blocks_ipv4_unsafe_matrix
    require_public_network_policy!

    unsafe_addresses = {
      'unspecified' => '0.0.0.0',
      'private_10' => '10.0.0.1',
      'private_172' => '172.16.0.1',
      'private_192' => '192.168.0.1',
      'shared_carrier' => '100.64.0.1',
      'cloud_metadata' => '100.100.100.200',
      'loopback' => '127.0.0.1',
      'link_local_metadata' => '169.254.169.254',
      'ietf_assignments' => '192.0.0.8',
      'benchmarking' => '198.18.0.1',
      'documentation_1' => '192.0.2.1',
      'documentation_2' => '198.51.100.1',
      'documentation_3' => '203.0.113.1',
      'multicast' => '224.0.0.1',
      'reserved' => '240.0.0.1'
    }

    unsafe_addresses.each do |label, address|
      error = assert_resolved_destination_blocked(address, label)

      assert_match(/#{Regexp.escape(address)}/, error.message)
    end
  end

  def test_public_network_policy_blocks_ipv6_unsafe_matrix
    require_public_network_policy!
    omit('IPv6 sockets are not available on this platform') unless Socket.const_defined?(:AF_INET6)

    unsafe_addresses = {
      'unspecified' => '::',
      'loopback' => '::1',
      'unique_local_fc' => 'fc00::1',
      'unique_local_fd' => 'fd12:3456::1',
      'link_local' => 'fe80::1',
      'site_local' => 'fec0::1',
      'multicast' => 'ff02::1',
      'documentation' => '2001:db8::1',
      'benchmarking' => '2001:2::1',
      'six_to_four' => '2002::1',
      'ipv4_mapped_loopback' => '::ffff:127.0.0.1',
      'ipv4_mapped_private' => '::ffff:10.0.0.1',
      'ipv4_mapped_metadata' => '::ffff:169.254.169.254',
      'ipv4_compatible_private' => '::192.168.0.1'
    }

    unsafe_addresses.each do |label, address|
      assert_resolved_destination_blocked(address, label)
    end
  end

  def test_public_network_policy_uses_canonical_ipv6_diagnostics
    require_public_network_policy!
    omit('IPv6 sockets are not available on this platform') unless Socket.const_defined?(:AF_INET6)

    error = assert_resolved_destination_blocked('2001:db8::1', 'documentation_canonical')

    assert_match(/2001:db8::1/, error.message)
    refute_match(/2001:0db8:0000:0000:0000:0000:0000:0001/, error.message)
  end

  def test_public_network_policy_allows_representative_ipv4_public_peer
    assert_public_destination_not_blocked('1.1.1.1')
  end

  def test_public_network_policy_allowed_cidrs_allows_matching_public_peer
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_cidrs = ['1.1.1.0/24']
    end

    easy = Curl::Easy.new('http://1.1.1.1:81/')
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    perform_without_unsafe_destination(easy, '1.1.1.1')
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_allowed_cidrs_blocks_public_peer_outside_allowlist
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_cidrs = ['8.8.8.0/24']
    end

    easy = Curl::Easy.new('http://1.1.1.1:81/')
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/outside allowed CIDR ranges/, error.message)
    assert_match(/1\.1\.1\.1/, error.message)
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_allowed_cidrs_do_not_permit_unsafe_peer
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_cidrs = ['127.0.0.0/8']
    end

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.get(TestServlet.url)
    end

    assert_match(/unsafe destination/, error.message)
    assert_match(/127\.0\.0\.1/, error.message)
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_allows_representative_ipv6_public_peer
    omit('IPv6 sockets are not available on this platform') unless Socket.const_defined?(:AF_INET6)

    assert_public_destination_not_blocked('2606:4700:4700::1111')
  end

  def test_public_network_policy_allowed_cidrs_allows_matching_ipv6_public_peer
    require_public_network_policy!
    omit('IPv6 sockets are not available on this platform') unless Socket.const_defined?(:AF_INET6)

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_cidrs = ['2606:4700:4700::/48']
    end

    easy = Curl::Easy.new('http://[2606:4700:4700::1111]:81/')
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    perform_without_unsafe_destination(easy, '2606:4700:4700::1111')
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_safe_bang_public_network_policy_blocks_loopback_destination
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.get(TestServlet.url)
    end

    assert_match(/127\.0\.0\.1/, error.message)
  end

  def test_public_network_policy_disables_explicit_proxy
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    easy = Curl::Easy.new(TestServlet.url)
    easy.proxy_url = 'http://127.0.0.1:3128'
    easy.proxy_tunnel = true

    Curl.__send__(:apply_safety!, easy)

    assert_equal '', easy.proxy_url
    assert_equal false, easy.proxy_tunnel?
  end

  def test_public_network_policy_allows_explicit_proxy_host_allowlist
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_proxy_hosts = ['proxy.example:3128']
    end

    easy = Curl::Easy.new('http://1.1.1.1/')
    easy.proxy_url = 'http://user:pass@PROXY.example.:3128'

    Curl.__send__(:apply_safety!, easy)

    assert_equal 'http://user:pass@PROXY.example.:3128', easy.proxy_url
  end

  def test_direct_public_network_policy_ignores_explicit_proxy
    proxy_server = TCPServer.new('127.0.0.1', 0)
    proxy_port = proxy_server.addr[1]
    proxy_connections = []
    proxy_thread = record_proxy_connections(proxy_server, proxy_connections)

    easy = Curl::Easy.new('http://1.1.1.1:81/')
    require_public_network_policy!(easy)
    easy.proxy_url = "http://127.0.0.1:#{proxy_port}"
    easy.proxy_tunnel = true
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    perform_without_unsafe_destination(easy, '1.1.1.1')
    sleep 0.05

    assert_nil easy.unsafe_destination_error
    assert_empty proxy_connections, 'direct public network policy should disable explicit proxies'
  ensure
    proxy_server.close if defined?(proxy_server) && proxy_server
    proxy_thread.join(1) if defined?(proxy_thread) && proxy_thread
  end

  def test_public_network_policy_rejects_explicit_proxy_outside_allowlist
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allowed_proxy_hosts = ['proxy.example']
    end

    easy = Curl::Easy.new('http://1.1.1.1/')
    easy.proxy_url = 'http://blocked.example:3128'

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end

    assert_match(/proxy allowlist/, error.message)
    assert_match(/blocked\.example/, error.message)
  end

  def test_public_network_policy_proxy_allowlist_still_ignores_environment_proxy_without_explicit_proxy
    require_public_network_policy!
    proxy_server = TCPServer.new('127.0.0.1', 0)
    proxy_port = proxy_server.addr[1]
    proxy_connections = []
    proxy_thread = record_proxy_connections(proxy_server, proxy_connections)

    with_proxy_environment("http://127.0.0.1:#{proxy_port}") do
      Curl.safe! do |config|
        config.network_policy = :public
        config.allowed_proxy_hosts = ['proxy.example']
      end

      easy = Curl::Easy.new('http://1.1.1.1:81/')
      easy.connect_timeout_ms = 50
      easy.timeout_ms = 100

      perform_without_unsafe_destination(easy, '1.1.1.1')
      sleep 0.05

      assert_equal '', easy.proxy_url
      assert_empty proxy_connections, 'proxy allowlist should not permit environment proxies without an explicit proxy'
    end
  ensure
    proxy_server.close if defined?(proxy_server) && proxy_server
    proxy_thread.join(1) if defined?(proxy_thread) && proxy_thread
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_direct_public_network_policy_ignores_proxy_environment_variables
    proxy_server = TCPServer.new('127.0.0.1', 0)
    proxy_port = proxy_server.addr[1]
    proxy_connections = []
    proxy_thread = record_proxy_connections(proxy_server, proxy_connections)

    with_proxy_environment("http://127.0.0.1:#{proxy_port}") do
      easy = Curl::Easy.new('http://1.1.1.1:81/')
      require_public_network_policy!(easy)
      easy.connect_timeout_ms = 50
      easy.timeout_ms = 100

      perform_without_unsafe_destination(easy, '1.1.1.1')
      sleep 0.05

      assert_nil easy.unsafe_destination_error
      assert_empty proxy_connections, 'direct public network policy should disable environment proxies'
    end
  ensure
    proxy_server.close if defined?(proxy_server) && proxy_server
    proxy_thread.join(1) if defined?(proxy_thread) && proxy_thread
  end

  def test_public_network_policy_ignores_proxy_environment_variables
    require_public_network_policy!
    proxy_server = TCPServer.new('127.0.0.1', 0)
    proxy_port = proxy_server.addr[1]
    proxy_connections = []
    proxy_thread = record_proxy_connections(proxy_server, proxy_connections)

    with_proxy_environment("http://127.0.0.1:#{proxy_port}") do
      Curl.safe! do |config|
        config.network_policy = :public
      end

      easy = Curl::Easy.new('http://1.1.1.1:81/')
      easy.connect_timeout_ms = 50
      easy.timeout_ms = 100

      perform_without_unsafe_destination(easy, '1.1.1.1')
      sleep 0.05

      assert_nil easy.unsafe_destination_error
      assert_empty proxy_connections, 'public network policy should disable environment proxies'
    end
  ensure
    proxy_server.close if defined?(proxy_server) && proxy_server
    proxy_thread.join(1) if defined?(proxy_thread) && proxy_thread
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_rejects_resolve_override
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    easy = Curl::Easy.new(TestServlet.url)
    easy.resolve = ["example.test:80:127.0.0.1"]

    assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end
  end

  def test_public_network_policy_can_allow_resolve_override_before_peer_check
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_resolve = true
    end

    easy = Curl::Easy.new(TestServlet.url)
    easy.resolve = ["example.test:80:127.0.0.1"]

    Curl.__send__(:apply_safety!, easy)

    assert_equal ["example.test:80:127.0.0.1"], easy.resolve
  end

  def test_public_network_policy_rejects_connect_to_override
    omit('CURLOPT_CONNECT_TO not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_CONNECT_TO)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    easy = Curl::Easy.new('http://example.test/')
    easy.connect_to = ['example.test:80:127.0.0.1:80']

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end

    assert_match(/connect_to overrides/, error.message)
  end

  def test_public_network_policy_can_allow_connect_to_override_before_peer_check
    omit('CURLOPT_CONNECT_TO not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_CONNECT_TO)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_connect_to = true
    end

    mapping = ['example.test:80:127.0.0.1:80']
    easy = Curl::Easy.new('http://example.test/')
    easy.connect_to = mapping

    Curl.__send__(:apply_safety!, easy)

    assert_equal mapping, easy.connect_to
  end

  def test_public_network_policy_blocks_connect_to_private_peer_when_override_is_allowed
    omit('CURLOPT_CONNECT_TO not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_CONNECT_TO)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_connect_to = true
    end

    host = 'curb-connect-to.invalid'
    port = TestServlet.port
    easy = Curl::Easy.new("http://#{host}:#{port}#{TestServlet.path}")
    easy.connect_to = ["#{host}:#{port}:127.0.0.1:#{port}"]
    easy.connect_timeout_ms = 50

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/127\.0\.0\.1/, error.message)
  end

  def test_public_network_policy_rejects_doh_url_override
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    easy = Curl::Easy.new('http://example.test/')
    easy.doh_url = 'https://dns.example/dns-query'

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end

    assert_match(/DoH URL overrides/, error.message)
  end

  def test_public_network_policy_rejects_dns_servers_override
    omit('CURLOPT_DNS_SERVERS not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DNS_SERVERS)
    require_public_network_policy!

    easy = Curl::Easy.new('http://example.test/')
    easy.network_policy = :public
    easy.setopt(Curl::CURLOPT_DNS_SERVERS, '127.0.0.1')

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/DNS server overrides/, error.message)
  end

  def test_public_network_policy_can_allow_doh_url_override_before_peer_check
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_doh = true
    end

    easy = Curl::Easy.new('http://example.test/')
    easy.doh_url = 'https://dns.example/dns-query'

    Curl.__send__(:apply_safety!, easy)

    assert_equal 'https://dns.example/dns-query', easy.doh_url
  end

  def test_doh_url_nil_clears_native_doh_url
    omit('CURLOPT_DOH_URL not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    omit('CURLOPT_DOH_SSL_VERIFYPEER not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_SSL_VERIFYPEER)
    omit('CURLOPT_DOH_SSL_VERIFYHOST not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_DOH_SSL_VERIFYHOST)

    host = 'curb-doh-clear.invalid'
    with_rebinding_doh_server(host, '127.0.0.1') do |doh_url, doh_state|
      easy = Curl::Easy.new("http://#{host}:81/")
      configure_live_doh_easy(easy, doh_url)
      easy.doh_url = nil

      begin
        easy.perform
      rescue Curl::Err::CurlError
        nil
      end

      assert_equal [], doh_a_answers(doh_state)
    end
  end

  def test_public_network_policy_blocks_resolved_private_peer_when_resolve_override_is_allowed
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_resolve = true
    end

    host = 'curb-dns-rebind.invalid'
    easy = Curl::Easy.new("http://#{host}:80/")
    easy.resolve = [resolve_entry(host, 80, '127.0.0.1')]
    easy.connect_timeout_ms = 50

    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/127\.0\.0\.1/, error.message)
  end

  def test_public_network_policy_rechecks_same_host_when_resolved_address_changes
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_resolve = true
    end

    host = 'curb-dns-rebind.invalid'
    easy = Curl::Easy.new("http://#{host}:81/")
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    easy.resolve = [resolve_entry(host, 81, '1.1.1.1')]
    perform_without_unsafe_destination(easy, '1.1.1.1')
    assert_nil easy.unsafe_destination_error

    easy.resolve = [resolve_entry(host, 81, '127.0.0.1')]
    error = assert_raise(Curl::Err::UnsafeDestinationError) do
      easy.perform
    end

    assert_match(/127\.0\.0\.1/, error.message)
  end

  def test_live_public_redirect_to_private_peer_is_blocked_by_resolved_peer_policy
    live_network_required!
    require_public_network_policy!

    with_private_target_server('/private-redirect-target') do |port, hits|
      private_host = 'curb-private-redirect.invalid'
      private_url = "http://#{private_host}:#{port}/private-redirect-target"

      Curl.safe! do |config|
        config.network_policy = :public
        config.allow_resolve = true
      end

      easy = Curl::Easy.new(live_redirect_url_for(private_url))
      easy.follow_location = true
      easy.resolve = [resolve_entry(private_host, port, '127.0.0.1')]
      easy.connect_timeout_ms = live_connect_timeout_ms
      easy.timeout_ms = live_timeout_ms

      error = nil
      begin
        easy.perform
      rescue Curl::Err::UnsafeDestinationError => e
        error = e
      rescue Curl::Err::CurlError => e
        if easy.respond_to?(:redirect_count) && easy.redirect_count.to_i == 0
          omit("live redirect endpoint unavailable before redirect: #{e.class}: #{e.message}")
        end

        flunk("expected redirected private peer to be reported unsafe, got #{e.class}: #{e.message}")
      end

      assert_equal 0, hits[:target], 'private redirect target should not receive a request'
      if error.nil? && easy.respond_to?(:redirect_count) && easy.redirect_count.to_i == 0
        omit("live redirect endpoint did not issue a redirect, response code #{easy.response_code}")
      end

      assert_not_nil error, 'expected redirected private peer to be blocked'
      assert_match(/public network policy/, error.message)
      assert_match(/127\.0\.0\.1/, error.message)
    end
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_blocks_private_peer_after_live_dns_answer_changes
    omit('live DNS rebinding test requires CURLOPT_DOH_URL') unless Curl.const_defined?(:CURLOPT_DOH_URL)
    omit('live DNS rebinding test requires CURLOPT_DOH_SSL_VERIFYPEER') unless Curl.const_defined?(:CURLOPT_DOH_SSL_VERIFYPEER)
    omit('live DNS rebinding test requires CURLOPT_DOH_SSL_VERIFYHOST') unless Curl.const_defined?(:CURLOPT_DOH_SSL_VERIFYHOST)
    require_public_network_policy!

    host = 'curb-doh-rebind.invalid'
    with_rebinding_doh_server(host, '1.1.1.1') do |doh_url, doh_state|
      Curl.safe! do |config|
        config.network_policy = :public
        config.allow_doh = true
        config.allowed_cidrs = ['8.8.8.0/24']
      end

      first = Curl::Easy.new("http://#{host}/dns-rebind-target")
      configure_live_doh_easy(first, doh_url)

      first_error = assert_raise(Curl::Err::UnsafeDestinationError) do
        first.perform
      end

      assert_include doh_a_answers(doh_state), '1.1.1.1'
      assert_match(/outside allowed CIDR ranges/, first_error.message)
      assert_match(/1\.1\.1\.1/, first_error.message)

      set_doh_answer(doh_state, '127.0.0.1')
      second = Curl::Easy.new("http://#{host}/dns-rebind-target")
      configure_live_doh_easy(second, doh_url)

      error = assert_raise(Curl::Err::UnsafeDestinationError) do
        second.perform
      end

      assert_include doh_a_answers(doh_state), '127.0.0.1'
      assert_match(/public network policy/, error.message)
      assert_match(/127\.0\.0\.1/, error.message)
    end
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def test_public_network_policy_rejects_unix_socket_override
    omit('CURLOPT_UNIX_SOCKET_PATH not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_UNIX_SOCKET_PATH)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    easy = Curl::Easy.new(TestServlet.url)
    easy.setopt(Curl::CURLOPT_UNIX_SOCKET_PATH, '/tmp/curb-test.sock')

    assert_raise(Curl::Err::UnsafeDestinationError) do
      Curl.__send__(:apply_safety!, easy)
    end
  end

  def test_public_network_policy_can_allow_unix_socket_override
    omit('CURLOPT_UNIX_SOCKET_PATH not supported by this libcurl') unless Curl.const_defined?(:CURLOPT_UNIX_SOCKET_PATH)
    omit('Unix socket transfers are not supported on Windows runners') if WINDOWS
    omit('Unix domain sockets are not available on this platform') unless defined?(UNIXServer)
    require_public_network_policy!
    require 'tmpdir'

    socket_path = File.join(Dir.tmpdir, "curb-test-#{$$}-#{object_id}.sock")
    FileUtils.rm_f(socket_path)
    server = UNIXServer.new(socket_path)
    server_thread = Thread.new do
      socket = server.accept
      begin
        socket.readpartial(4096)
      rescue EOFError
      end
      socket.write("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nunix socket")
      socket.close
    end

    Curl.safe! do |config|
      config.network_policy = :public
      config.allow_unix_socket = true
    end

    easy = Curl::Easy.new('http://unix.test/')
    easy.setopt(Curl::CURLOPT_UNIX_SOCKET_PATH, socket_path)
    easy.connect_timeout_ms = 500
    easy.timeout_ms = 1_000
    easy.perform

    assert_equal 200, easy.response_code
    assert_equal 'unix socket', easy.body_str
  ensure
    server.close if defined?(server) && server && !server.closed?
    if defined?(server_thread) && server_thread
      server_thread.join(1)
      server_thread.kill if server_thread.alive?
    end
    FileUtils.rm_f(socket_path) if defined?(socket_path) && socket_path
  end

  def test_multi_records_public_network_policy_block
    easy = Curl::Easy.new(TestServlet.url)
    require_public_network_policy!(easy)
    multi = Curl::Multi.new

    multi.add(easy)
    multi.perform

    assert_match(/127\.0\.0\.1/, easy.unsafe_destination_error)
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_multi_reinstalls_public_policy_callbacks_when_safe_enabled_after_add
    require_public_network_policy!
    easy = Curl::Easy.new(TestServlet.url)
    multi = Curl::Multi.new

    multi.add(easy)
    Curl.safe! do |config|
      config.network_policy = :public
    end
    multi.perform

    assert_match(/127\.0\.0\.1/, easy.unsafe_destination_error)
    assert_equal 7, easy.last_result
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_multi_reinstalls_public_policy_callbacks_when_easy_policy_enabled_after_add
    easy = Curl::Easy.new(TestServlet.url)
    multi = Curl::Multi.new

    multi.add(easy)
    require_public_network_policy!(easy)
    multi.perform

    assert_match(/127\.0\.0\.1/, easy.unsafe_destination_error)
    assert_equal 7, easy.last_result
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_multi_reinstalls_host_allowlist_callback_when_allowed_hosts_set_after_add
    omit('redirect-aware host allowlists require CURLOPT_PREREQFUNCTION') unless Curl.const_defined?(:CURLOPT_PREREQFUNCTION)

    blocked_host = 'curb-late-blocked.test'
    easy = Curl::Easy.new("http://#{blocked_host}:#{TestServlet.port}#{TestServlet.path}")
    easy.resolve = [resolve_entry(blocked_host, TestServlet.port, '127.0.0.1')]
    multi = Curl::Multi.new

    multi.add(easy)
    easy.allowed_hosts = ['curb-late-allowed.test']
    multi.perform

    assert_match(/host allowlist/, easy.unsafe_destination_error)
    assert_match(/#{Regexp.escape(blocked_host)}/, easy.unsafe_destination_error)
    assert_not_equal 0, easy.last_result
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_multi_safety_refresh_does_not_duplicate_slist_backed_options
    with_header_recording_server do |url, request_headers|
      easy = Curl::Easy.new(url)
      easy.headers['X-Test'] = '1'
      multi = Curl::Multi.new

      multi.add(easy)
      Curl.safe!
      multi.perform

      assert_equal [['1']], request_headers
    ensure
      multi.close if defined?(multi) && multi
    end
  end

  def test_multi_does_not_reuse_pre_policy_loopback_connection
    easy = Curl::Easy.new(TestServlet.url)
    multi = Curl::Multi.new

    multi.add(easy)
    multi.perform
    assert_equal 200, easy.response_code
    assert_match(/GET/, easy.body_str)

    require_public_network_policy!(easy)
    multi.add(easy)
    multi.perform

    assert_match(/127\.0\.0\.1/, easy.unsafe_destination_error)
    assert_equal 7, easy.last_result
  ensure
    multi.close if defined?(multi) && multi
  end

  def test_multi_isolates_allowed_and_blocked_public_policy_handles
    allowed = Curl::Easy.new(TestServlet.url)
    blocked = Curl::Easy.new(TestServlet.url)
    require_public_network_policy!(blocked)
    multi = Curl::Multi.new
    events = []

    allowed.on_complete { events << [:allowed, :complete] }
    allowed.on_success do |easy|
      events << [:allowed, :success, easy.response_code]
    end
    allowed.on_failure do |_easy, error|
      events << [:allowed, :failure, error.first]
    end

    blocked.on_complete { events << [:blocked, :complete] }
    blocked.on_success do |easy|
      events << [:blocked, :success, easy.response_code]
    end
    blocked.on_failure do |_easy, error|
      events << [:blocked, :failure, error.first]
    end

    multi.add(allowed)
    multi.add(blocked)
    multi.perform

    assert_equal 200, allowed.response_code
    assert_match(/GET/, allowed.body_str)
    assert_nil allowed.unsafe_destination_error
    assert_match(/127\.0\.0\.1/, blocked.unsafe_destination_error)
    assert_equal 7, blocked.last_result

    assert_equal [:allowed, :complete], events.find { |event| event[0] == :allowed && event[1] == :complete }
    assert_equal [:allowed, :success, 200], events.find { |event| event[0] == :allowed && event[1] == :success }
    assert_nil events.find { |event| event[0] == :allowed && event[1] == :failure }
    assert_equal [:blocked, :complete], events.find { |event| event[0] == :blocked && event[1] == :complete }
    assert_equal [:blocked, :failure, Curl::Err::ConnectionFailedError],
                 events.find { |event| event[0] == :blocked && event[1] == :failure }
    assert_nil events.find { |event| event[0] == :blocked && event[1] == :success }
  ensure
    multi.close if defined?(multi) && multi
  end

  private

  def require_public_network_policy!(easy = Curl::Easy.new)
    easy.network_policy = :public
    easy
  rescue NotImplementedError
    omit('network_policy=:public requires CURLOPT_OPENSOCKETFUNCTION support')
  end

  def assert_resolved_destination_blocked(address, label)
    host = "curb-#{label.tr('_', '-')}.invalid"
    easy = Curl::Easy.new("http://#{host}:80/")
    require_public_network_policy!(easy)
    easy.resolve = [resolve_entry(host, 80, address)]
    easy.connect_timeout_ms = 50

    error = assert_raise(Curl::Err::UnsafeDestinationError, "expected #{address} to be blocked") do
      easy.perform
    end

    assert_match(/public network policy/, error.message)
    assert_match(/public network policy/, easy.unsafe_destination_error)
    error
  end

  def resolve_entry(host, port, address)
    resolved_address = address.include?(':') ? "[#{address}]" : address
    "#{host}:#{port}:#{resolved_address}"
  end

  def assert_public_destination_not_blocked(address)
    require_public_network_policy!

    Curl.safe! do |config|
      config.network_policy = :public
    end

    host = address.include?(':') ? "[#{address}]" : address
    easy = Curl::Easy.new("http://#{host}:81/")
    easy.connect_timeout_ms = 50
    easy.timeout_ms = 100

    perform_without_unsafe_destination(easy, address)
  rescue Curl::Err::UnsafeDestinationError => e
    flunk("expected #{address} to be treated as public, got #{e.class}: #{e.message}")
  ensure
    Curl.__send__(:clear_safe!) if Curl.respond_to?(:clear_safe!, true)
  end

  def perform_without_unsafe_destination(easy, address)
    easy.perform
  rescue Curl::Err::UnsafeDestinationError => e
    flunk("expected #{address} to be treated as public, got #{e.class}: #{e.message}")
  rescue Curl::Err::CurlError
    assert_nil easy.unsafe_destination_error
  end

  def record_proxy_connections(server, connections)
    Thread.new do
      loop do
        readable = IO.select([server], nil, nil, 0.05)
        next unless readable

        socket = server.accept_nonblock(exception: false)
        next unless socket.is_a?(TCPSocket)

        connections << true
        socket.close
      rescue IOError, Errno::EBADF, Errno::ENOTSOCK
        break
      end
    end
  end

  def with_proxy_environment(proxy_url)
    keys = %w[http_proxy HTTP_PROXY all_proxy ALL_PROXY no_proxy NO_PROXY]
    old_values = keys.each_with_object({}) { |key, values| values[key] = ENV[key] }

    ENV['http_proxy'] = proxy_url
    ENV['HTTP_PROXY'] = proxy_url
    ENV['all_proxy'] = proxy_url
    ENV['ALL_PROXY'] = proxy_url
    ENV.delete('no_proxy')
    ENV.delete('NO_PROXY')

    yield
  ensure
    old_values.each do |key, value|
      if value.nil?
        ENV.delete(key)
      else
        ENV[key] = value
      end
    end
  end

  def live_network_required!
    omit('set CURB_LIVE_NETWORK_TESTS=1 to run live network security tests') unless ENV['CURB_LIVE_NETWORK_TESTS'] == '1'
  end

  def live_redirect_url_for(target_url)
    template = ENV['CURB_LIVE_REDIRECT_URL_TEMPLATE'] || 'https://httpbin.org/redirect-to?url={url}'
    omit('CURB_LIVE_REDIRECT_URL_TEMPLATE must include {url}') unless template.include?('{url}')

    template.gsub('{url}', URI.encode_www_form_component(target_url))
  end

  def live_connect_timeout_ms
    [(ENV['CURB_LIVE_CONNECT_TIMEOUT_MS'] || 1_000).to_i, 1].max
  end

  def live_timeout_ms
    [(ENV['CURB_LIVE_TIMEOUT_MS'] || 5_000).to_i, 1].max
  end

  def configure_live_doh_easy(easy, doh_url)
    easy.resolve_mode = :ipv4
    easy.dns_cache_timeout = 0
    easy.connect_timeout_ms = live_connect_timeout_ms
    easy.timeout_ms = live_timeout_ms
    easy.doh_url = doh_url
    easy.setopt(Curl::CURLOPT_DOH_SSL_VERIFYPEER, 0)
    easy.setopt(Curl::CURLOPT_DOH_SSL_VERIFYHOST, 0)
  end

  def with_rebinding_doh_server(host, initial_address)
    require 'webrick/https'
    require 'openssl'

    omit('DoH rebinding fixture requires OpenSSL') unless defined?(OpenSSL::PKey::RSA)

    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    expected_host = host.downcase
    state = {
      :address => initial_address,
      :answers => [],
      :queries => [],
      :mutex => Mutex.new
    }

    cert, key = doh_certificate
    server = WEBrick::HTTPServer.new(:BindAddress => '127.0.0.1',
                                     :Port => port,
                                     :Logger => WEBRICK_TEST_LOG,
                                     :AccessLog => [],
                                     :SSLEnable => true,
                                     :SSLCertificate => cert,
                                     :SSLPrivateKey => key)
    server.mount_proc('/dns-query') do |req, res|
      res['Content-Type'] = 'application/dns-message'
      res.body = dns_response(req.body.b, expected_host, state)
    end

    thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: thread)

    yield "https://127.0.0.1:#{port}/dns-query", state
  ensure
    server.shutdown if defined?(server) && server
    thread.join(server_startup_timeout) if defined?(thread) && thread
    thread.kill if defined?(thread) && thread&.alive?
  end

  def set_doh_answer(state, address)
    state[:mutex].synchronize { state[:address] = address }
  end

  def doh_a_answers(state)
    state[:mutex].synchronize { state[:answers].dup }
  end

  def doh_certificate
    key = OpenSSL::PKey::RSA.new(2048)
    cert = OpenSSL::X509::Certificate.new
    cert.version = 2
    cert.serial = 1
    cert.subject = OpenSSL::X509::Name.parse('/CN=127.0.0.1')
    cert.issuer = cert.subject
    cert.public_key = key.public_key
    cert.not_before = Time.now - 60
    cert.not_after = Time.now + 3600

    extensions = OpenSSL::X509::ExtensionFactory.new
    extensions.subject_certificate = cert
    extensions.issuer_certificate = cert
    cert.add_extension(extensions.create_extension('basicConstraints', 'CA:FALSE', true))
    cert.add_extension(extensions.create_extension('subjectAltName', 'IP:127.0.0.1', false))
    cert.sign(key, OpenSSL::Digest::SHA256.new)

    [cert, key]
  end

  def dns_response(packet, expected_host, state)
    question = dns_question(packet)
    return nil unless question

    name, qtype, qclass, question_bytes = question
    answer_address = nil
    state[:mutex].synchronize do
      state[:queries] << [name, qtype, qclass]
      if name == expected_host && qtype == 1 && qclass == 1
        answer_address = state[:address]
        state[:answers] << answer_address
      end
    end

    answer = answer_address && dns_a_answer(answer_address)
    [packet.unpack1('n'), 0x8180, 1, answer ? 1 : 0, 0, 0].pack('nnnnnn') +
      question_bytes +
      (answer || ''.b)
  end

  def dns_question(packet)
    return nil if packet.bytesize < 12

    offset = 12
    labels = []
    loop do
      length = packet.getbyte(offset)
      return nil unless length

      offset += 1
      break if length.zero?
      return nil if length & 0xc0 != 0
      return nil if offset + length > packet.bytesize

      labels << packet.byteslice(offset, length)
      offset += length
    end

    return nil if offset + 4 > packet.bytesize

    qtype, qclass = packet.byteslice(offset, 4).unpack('nn')
    [labels.join('.').downcase, qtype, qclass, packet.byteslice(12, offset + 4 - 12)]
  end

  def dns_a_answer(address)
    octets = address.split('.').map { |octet| Integer(octet, 10) }
    return nil unless octets.length == 4 && octets.all? { |octet| octet.between?(0, 255) }

    [0xc00c, 1, 1, 0, 4].pack('nnnNn') + octets.pack('C4')
  end

  def with_private_target_server(path)
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    hits = Hash.new(0)
    server = WEBrick::HTTPServer.new(:BindAddress => '127.0.0.1', :Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc(path) do |_req, res|
      hits[:target] += 1
      res['Content-Type'] = 'text/plain'
      res.body = 'private target'
    end

    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    yield port, hits
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end

  def with_host_redirect_server(_allowed_host, target_host)
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    hits = Hash.new(0)
    server = WEBrick::HTTPServer.new(:BindAddress => '127.0.0.1', :Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/redirect-to-target') do |_req, res|
      hits[:redirect] += 1
      res.status = 302
      res['Location'] = "http://#{target_host}:#{port}/target"
      res.body = 'redirect'
    end
    server.mount_proc('/target') do |_req, res|
      hits[:target] += 1
      res['Content-Type'] = 'text/plain'
      res.body = 'target'
    end

    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    yield port, hits
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end

  def with_header_recording_server
    port_socket = TCPServer.new('127.0.0.1', 0)
    port = port_socket.addr[1]
    port_socket.close

    request_headers = []
    server = WEBrick::HTTPServer.new(:BindAddress => '127.0.0.1', :Port => port, :Logger => WEBRICK_TEST_LOG, :AccessLog => [])
    server.mount_proc('/headers') do |req, res|
      request_headers << req.header.fetch('x-test', []).dup
      res['Content-Type'] = 'text/plain'
      res.body = 'ok'
    end

    server_thread = Thread.new(server) { |srv| srv.start }
    wait_for_server_ready(port, thread: server_thread)

    yield "http://127.0.0.1:#{port}/headers", request_headers
  ensure
    server.shutdown if defined?(server) && server
    server_thread.join(server_startup_timeout) if defined?(server_thread) && server_thread
    server_thread.kill if defined?(server_thread) && server_thread&.alive?
  end
end
