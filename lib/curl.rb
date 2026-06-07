# frozen_string_literal: true
require 'curb_core'
require 'curl/download'
require 'curl/easy'
require 'curl/multi'
require 'ipaddr'
require 'uri'

# expose shortcut methods
module Curl
  class SafetyConfig
    DEFAULT_PROTOCOLS = [:http, :https].freeze

    attr_reader :max_body_bytes, :network_policy
    attr_accessor :allow_proxies, :allow_resolve, :allow_connect_to, :allow_doh, :allow_unix_socket

    def initialize
      @protocols = DEFAULT_PROTOCOLS
      @redirect_protocols = nil
      @max_body_bytes = nil
      @network_policy = nil
      @allowed_hosts = nil
      @allowed_proxy_hosts = nil
      @allowed_cidrs = nil
      @allow_proxies = false
      @allow_resolve = false
      @allow_connect_to = false
      @allow_doh = false
      @allow_unix_socket = false
    end

    def protocols
      @protocols.dup
    end

    def protocols=(protocols)
      @protocols = normalize_protocols(protocols)
    end

    def redirect_protocols
      @redirect_protocols&.dup
    end

    def redirect_protocols=(protocols)
      @redirect_protocols = protocols.nil? ? nil : normalize_protocols(protocols)
    end

    def allowed_hosts
      @allowed_hosts&.map(&:dup)
    end

    def allowed_proxy_hosts
      @allowed_proxy_hosts&.map(&:dup)
    end

    def allowed_cidrs
      @allowed_cidrs&.map(&:dup)
    end

    def max_body_bytes=(bytes)
      if bytes.nil?
        @max_body_bytes = nil
        return
      end

      bytes = Integer(bytes)
      raise ArgumentError, "max_body_bytes must be greater than or equal to zero" if bytes < 0

      @max_body_bytes = bytes.zero? ? nil : bytes
    end

    def network_policy=(policy)
      @network_policy = normalize_network_policy(policy)
    end

    def allowed_hosts=(hosts)
      @allowed_hosts = normalize_allowed_hosts(hosts)
    end

    def allowed_proxy_hosts=(hosts)
      @allowed_proxy_hosts = normalize_allowed_hosts(hosts)
    end

    def allowed_cidrs=(cidrs)
      @allowed_cidrs = normalize_allowed_cidrs(cidrs)
    end

    private

    def normalize_protocols(protocols)
      protocol_names = Array(protocols).map { |protocol| protocol.to_s.downcase.to_sym }
      raise ArgumentError, "at least one protocol is required" if protocol_names.empty?

      protocol_names
    end

    def normalize_network_policy(policy)
      return nil if policy.nil?

      policy_name = policy.to_s.downcase.to_sym
      return policy_name if [:none, :public].include?(policy_name)

      raise ArgumentError, "network_policy must be one of :none, :public"
    end

    def normalize_allowed_hosts(hosts)
      list = normalize_optional_list(hosts)
      return nil unless list

      list.map { |host| normalize_allowed_host(host) }.uniq
    end

    def normalize_allowed_cidrs(cidrs)
      list = normalize_optional_list(cidrs)
      return nil unless list

      list.map do |cidr|
        cidr = cidr.to_s.strip
        raise ArgumentError, "allowed_cidrs cannot include blank entries" if cidr.empty?

        IPAddr.new(cidr)
        cidr
      end.uniq
    rescue IPAddr::InvalidAddressError => e
      raise ArgumentError, "invalid CIDR range: #{e.message}"
    end

    def normalize_optional_list(values)
      return nil if values.nil?

      list = Array(values)
      return nil if list.empty?

      list
    end

    def normalize_allowed_host(host)
      host = host.to_s.strip.downcase
      raise ArgumentError, "allowed_hosts cannot include blank entries" if host.empty?

      parsed_host = begin
        URI.parse(host).host if host.include?("://")
      rescue URI::InvalidURIError
        nil
      end

      host = parsed_host.downcase if parsed_host
      host = normalize_host_authority(host) unless parsed_host
      host = host[1...-1] if host.start_with?("[") && host.end_with?("]")
      host = host.chomp(".")

      raise ArgumentError, "allowed_hosts cannot include blank entries" if host.empty?

      host
    end

    def normalize_host_authority(host)
      authority = host.split(/[\/?#]/, 2).first
      authority = authority[(authority.rindex("@") + 1)..-1] if authority.include?("@")

      if authority.start_with?("[")
        closing = authority.index("]")
        raise ArgumentError, "invalid allowed host: #{host}" unless closing

        return authority[1...closing]
      end

      if authority.count(":") <= 1
        authority = authority.split(":", 2).first
      end

      authority
    end
  end

  def self.safe!
    config = SafetyConfig.new
    yield config if block_given?
    @safety_config = config
    bump_safety_generation!
  end

  def self.apply_safety!(easy)
    config = @safety_config
    override = safety_override_for(easy)
    return easy unless config || override

    protocols = config&.protocols
    redirect_protocols = config && (config.redirect_protocols || protocols)

    if override
      override_protocols = override[:protocols]
      protocols = safety_protocol_intersection(protocols, override_protocols) if override_protocols

      override_redirect_protocols = override[:redirect_protocols] || override_protocols
      redirect_protocols = safety_protocol_intersection(redirect_protocols, override_redirect_protocols) if override_redirect_protocols
    end

    if protocols
      easy.allowed_protocols = protocols
      easy.allowed_redirect_protocols = redirect_protocols || protocols
    end

    apply_allowed_hosts!(easy, config.allowed_hosts) if config&.allowed_hosts
    apply_allowed_cidrs!(easy, config) if config&.allowed_cidrs

    if config&.network_policy
      easy.network_policy = config.network_policy
      apply_public_network_policy_controls!(easy, config) if config.network_policy == :public
    end

    apply_max_body_bytes!(easy, config.max_body_bytes) if config&.max_body_bytes
    apply_max_body_bytes!(easy, override[:max_body_bytes]) if override && override.key?(:max_body_bytes)
    easy
  end

  def self.clear_safe!
    @safety_config = nil
    bump_safety_generation!
  end

  def self.safety_active_for?(easy)
    !!(@safety_config || safety_override_for(easy))
  end

  def self.safety_signature_for(easy)
    return nil unless safety_active_for?(easy)

    override_generation = if easy.respond_to?(:__curb_safety_override_generation, true)
      easy.__send__(:__curb_safety_override_generation)
    end

    [safety_generation, override_generation.to_i]
  end

  def self.safety_override_for(easy)
    if easy.respond_to?(:__curb_safety_override, true)
      easy.__send__(:__curb_safety_override)
    end
  end

  def self.safety_generation
    @safety_generation ||= 0
  end

  def self.bump_safety_generation!
    @safety_generation = safety_generation + 1
  end

  def self.safety_protocol_intersection(base_protocols, override_protocols)
    return override_protocols unless base_protocols

    protocols = override_protocols & base_protocols
    raise ArgumentError, "safety policies allow no protocols" if protocols.empty?

    protocols
  end

  def self.apply_max_body_bytes!(easy, max_body_bytes)
    return unless max_body_bytes

    current_max_body_bytes = easy.max_body_bytes
    easy.max_body_bytes = max_body_bytes if current_max_body_bytes.nil? || current_max_body_bytes > max_body_bytes
  end

  def self.apply_public_network_policy_controls!(easy, config)
    reject_resolve_override!(easy) unless config.allow_resolve
    reject_connect_to_override!(easy) unless config.allow_connect_to
    reject_doh_override!(easy) unless config.allow_doh
    reject_dns_servers_override!(easy)
    allow_proxy = config.allow_proxies || !!config.allowed_proxy_hosts
    easy.__send__(:__curb_allow_proxy=, allow_proxy) if easy.respond_to?(:__curb_allow_proxy=, true)
    easy.__send__(:__curb_allow_unix_socket=, config.allow_unix_socket) if easy.respond_to?(:__curb_allow_unix_socket=, true)
    reject_unix_socket_override!(easy) unless config.allow_unix_socket
    if config.allowed_proxy_hosts
      apply_allowed_proxy!(easy, config.allowed_proxy_hosts)
    elsif !config.allow_proxies
      disable_proxy!(easy)
    end
  end

  def self.apply_allowed_hosts!(easy, allowed_hosts)
    if easy.respond_to?(:follow_location?) && easy.follow_location? &&
       !Curl.const_defined?(:CURLOPT_PREREQFUNCTION)
      raise NotImplementedError, "redirect-aware host allowlists require CURLOPT_PREREQFUNCTION support"
    end

    host = begin
      URI.parse(easy.url.to_s).host
    rescue URI::InvalidURIError
      nil
    end

    normalized_host = host.to_s.downcase.chomp(".")
    normalized_host = normalized_host[1...-1] if normalized_host.start_with?("[") && normalized_host.end_with?("]")

    unless allowed_hosts.include?(normalized_host)
      raise Curl::Err::UnsafeDestinationError,
            "URL host #{host.inspect} is not allowed by safe mode host allowlist"
    end

    easy.allowed_hosts = allowed_hosts if easy.respond_to?(:allowed_hosts=)
  end

  def self.apply_allowed_cidrs!(easy, config)
    unless config.network_policy == :public
      raise ArgumentError, "allowed_cidrs require network_policy = :public"
    end

    easy.allowed_cidrs = config.allowed_cidrs if easy.respond_to?(:allowed_cidrs=)
  end

  def self.apply_allowed_proxy!(easy, allowed_proxy_hosts)
    proxy_url = easy.proxy_url if easy.respond_to?(:proxy_url)
    if proxy_url.nil? || proxy_url.to_s.empty?
      disable_proxy!(easy)
      return
    end

    proxy_host = normalize_proxy_host(proxy_url)
    unless allowed_proxy_hosts.include?(proxy_host)
      raise Curl::Err::UnsafeDestinationError,
            "proxy host #{proxy_host.inspect} is not allowed by safe mode proxy allowlist"
    end

    easy.set(Curl::CURLOPT_NOPROXY, "") if Curl.const_defined?(:CURLOPT_NOPROXY)
  end

  def self.normalize_proxy_host(proxy_url)
    value = proxy_url.to_s.strip
    raise Curl::Err::UnsafeDestinationError, "proxy URL is empty" if value.empty?

    uri = begin
      URI.parse(value.include?("://") ? value : "http://#{value}")
    rescue URI::InvalidURIError
      nil
    end

    host = uri&.host
    raise Curl::Err::UnsafeDestinationError, "proxy URL host is invalid" if host.nil? || host.empty?

    host = host.downcase.chomp(".")
    host = host[1...-1] if host.start_with?("[") && host.end_with?("]")
    host
  end

  def self.reject_resolve_override!(easy)
    return unless easy.respond_to?(:resolve)

    resolve = easy.resolve
    return if resolve.nil? || (resolve.respond_to?(:empty?) && resolve.empty?)

    raise Curl::Err::UnsafeDestinationError, "resolve overrides are disabled by public network policy"
  end

  def self.reject_connect_to_override!(easy)
    return unless easy.respond_to?(:connect_to)

    connect_to = easy.connect_to
    return if connect_to.nil? || (connect_to.respond_to?(:empty?) && connect_to.empty?)

    raise Curl::Err::UnsafeDestinationError, "connect_to overrides are disabled by public network policy"
  end

  def self.reject_doh_override!(easy)
    return unless easy.respond_to?(:doh_url)

    doh_url = easy.doh_url
    return if doh_url.nil? || (doh_url.respond_to?(:empty?) && doh_url.empty?)

    raise Curl::Err::UnsafeDestinationError, "DoH URL overrides are disabled by public network policy"
  end

  def self.reject_dns_servers_override!(easy)
    return unless easy.respond_to?(:dns_servers)

    dns_servers = easy.dns_servers
    return if dns_servers.nil? || (dns_servers.respond_to?(:empty?) && dns_servers.empty?)

    raise Curl::Err::UnsafeDestinationError, "DNS server overrides are disabled by public network policy"
  end

  def self.reject_unix_socket_override!(easy)
    return unless easy.respond_to?(:unix_socket_path)

    unix_socket_path = easy.unix_socket_path
    return if unix_socket_path.nil? || (unix_socket_path.respond_to?(:empty?) && unix_socket_path.empty?)

    raise Curl::Err::UnsafeDestinationError, "Unix socket paths are disabled by public network policy"
  end

  def self.disable_proxy!(easy)
    easy.proxy_url = "" if easy.respond_to?(:proxy_url=)
    easy.proxy_tunnel = false if easy.respond_to?(:proxy_tunnel=)
    easy.set(Curl::CURLOPT_NOPROXY, "*") if Curl.const_defined?(:CURLOPT_NOPROXY)
  end

  private_class_method :apply_safety!, :clear_safe!, :safety_active_for?,
                       :safety_signature_for, :safety_override_for,
                       :safety_generation, :bump_safety_generation!,
                       :safety_protocol_intersection,
                       :apply_max_body_bytes!, :apply_public_network_policy_controls!,
                       :apply_allowed_hosts!, :apply_allowed_cidrs!,
                       :apply_allowed_proxy!, :normalize_proxy_host,
                       :reject_resolve_override!, :reject_connect_to_override!,
                       :reject_doh_override!, :reject_dns_servers_override!,
                       :reject_unix_socket_override!,
                       :disable_proxy!

  def self.scheduler_active?
    Fiber.respond_to?(:scheduler) && !Fiber.scheduler.nil?
  end

  def self.deferred_exception_source_id(state)
    return unless state[:multi].instance_variable_defined?(:@__curb_deferred_exception_source_id)

    state[:multi].instance_variable_get(:@__curb_deferred_exception_source_id)
  end

  def self.scheduler_waiter_blocking_supported?
    scheduler = Fiber.scheduler
    scheduler && scheduler.respond_to?(:block) && scheduler.respond_to?(:unblock)
  end

  def self.wake_scheduler_waiter(waiter)
    fiber = waiter[:fiber]
    scheduler = waiter[:scheduler]
    return unless fiber&.alive? && scheduler&.respond_to?(:unblock)

    scheduler.unblock(waiter, fiber)
  end

  def self.complete_scheduler_waiter(waiter)
    return if waiter[:done]

    waiter[:done] = true
    wake_scheduler_waiter(waiter)
  end

  def self.fail_scheduler_waiter(waiter, error)
    return if waiter[:error]

    waiter[:error] = error
    wake_scheduler_waiter(waiter)
  end

  def self.release_scheduler_error(state, error)
    source_waiter = state[:waiters][deferred_exception_source_id(state)]

    if source_waiter
      fail_scheduler_waiter(source_waiter, error)
    else
      state[:error] = error
      state[:waiters].each_value { |waiter| wake_scheduler_waiter(waiter) }
    end
  end

  def self.block_scheduler_waiter(waiter)
    unless scheduler_waiter_blocking_supported?
      sleep 0
      return
    end

    waiter[:fiber] = Fiber.current
    waiter[:scheduler] ||= Fiber.scheduler
    return if waiter[:done] || waiter[:error]

    waiter[:scheduler].block(waiter, nil)
  ensure
    waiter[:fiber] = nil if waiter[:fiber].equal?(Fiber.current)
  end

  def self.scheduler_yield
    scheduler = Fiber.scheduler

    if scheduler&.respond_to?(:kernel_sleep)
      scheduler.kernel_sleep(0)
    else
      sleep 0
    end
  end

  def self.release_scheduler_waiters(state)
    source_id = deferred_exception_source_id(state)

    state[:waiters].each do |easy_id, waiter|
      next if source_id == easy_id

      complete_scheduler_waiter(waiter) if waiter[:completed]
    end
  end

  def self.perform_with_scheduler(easy)
    state = scheduler_state
    waiter = {completed: false, done: false, error: nil, fiber: nil, scheduler: Fiber.scheduler}
    state[:waiters][easy.object_id] = waiter
    previous_complete = easy.on_complete do |completed_easy|
      previous_complete.call(completed_easy) if previous_complete
      waiter[:completed] = true
    end

    state[:pending] << easy
    ensure_scheduler_driver(state)

    until waiter[:done]
      raise waiter[:error] if waiter[:error]
      raise state[:error] if state[:error]
      block_scheduler_waiter(waiter)
    end

    while state[:driver_running] && state[:pending].empty? &&
          state[:waiters].length == 1 && state[:waiters].key?(easy.object_id)
      scheduler_yield
    end

    true
  ensure
    state[:waiters].delete(easy.object_id) if defined?(state) && state[:waiters]
    if defined?(previous_complete)
      if previous_complete
        easy.on_complete(&previous_complete)
      else
        easy.on_complete
      end
    end
  end

  def self.scheduler_state
    Thread.current.thread_variable_get(:curb_scheduler_state) || begin
      state = {
        multi: Curl::Multi.new,
        pending: [],
        driver_running: false,
        error: nil,
        waiters: {},
      }
      Thread.current.thread_variable_set(:curb_scheduler_state, state)
      state
    end
  end

  def self.ensure_scheduler_driver(state)
    return if state[:driver_running]

    state[:driver_running] = true
    state[:error] = nil

    runner = proc do
      begin
        # Give sibling fibers a chance to enqueue work so the shared multi can
        # batch scheduler-driven Easy#perform calls together.
        pending_count = -1
        until pending_count == state[:pending].size
          pending_count = state[:pending].size
          scheduler_yield
        end

        loop do
          drain_scheduler_pending(state)
          break if state[:multi].idle?

          begin
            state[:multi].perform do
              drain_scheduler_pending(state)
              release_scheduler_waiters(state)
              scheduler_yield
            end
          ensure
            # Release any siblings that completed just before a deferred
            # callback exception is re-raised.
            release_scheduler_waiters(state)
          end
        end
      rescue => e
        release_scheduler_waiters(state)
        release_scheduler_error(state, e)
      ensure
        state[:driver_running] = false
        ensure_scheduler_driver(state) if state[:error].nil? && !state[:pending].empty?
      end
    end

    if Fiber.respond_to?(:schedule)
      Fiber.schedule(&runner)
    else
      Fiber.new(blocking: false, &runner).resume
    end
  end

  def self.drain_scheduler_pending(state)
    pending = state[:pending]
    until pending.empty?
      easy = pending.first

      break if state[:multi].instance_variable_defined?(:@__curb_deferred_exception)

      state[:multi].add(easy)
      break unless state[:multi].requests.key?(easy.object_id)

      pending.shift
    end
  end

  def self.http(verb, url, post_body=nil, put_data=nil, &block)
    if Thread.current[:curb_curl_yielding]
      handle = Curl::Easy.new # we can't reuse this
    else
      handle = Thread.current[:curb_curl] ||= Curl::Easy.new
      handle.reset
    end
    handle.url = url
    handle.post_body = post_body if post_body
    handle.put_data = put_data if put_data
    if block_given?
      begin
        Thread.current[:curb_curl_yielding] = true
        yield handle
      ensure
        Thread.current[:curb_curl_yielding] = false
      end
    end
    handle.http(verb)
    handle
  end

  def self.safe_http(verb, url, post_body=nil, put_data=nil, options={}, &block)
    options = safe_http_options(options)

    http(verb, url, post_body, put_data) do |handle|
      yield handle if block
      handle.safe_http!
      handle.max_body_bytes = options[:max_body_bytes] if options.key?(:max_body_bytes)
    end
  end

  def self.get(url, params={}, &block)
    http :GET, urlalize(url, params), nil, nil, &block
  end

  def self.safe_get(url, params={}, options={}, &block)
    params, options = split_safe_http_params_options(params, options)
    safe_http :GET, urlalize(url, params), nil, nil, options, &block
  end

  def self.post(url, params={}, &block)
    http :POST, url, postalize(params), nil, &block
  end

  def self.safe_post(url, params={}, options={}, &block)
    safe_http :POST, url, postalize(params), nil, options, &block
  end

  def self.put(url, params={}, &block)
    http :PUT, url, nil, postalize(params), &block
  end

  def self.safe_put(url, params={}, options={}, &block)
    safe_http :PUT, url, nil, postalize(params), options, &block
  end

  def self.delete(url, params={}, &block)
    http :DELETE, url, postalize(params), nil, &block
  end

  def self.safe_delete(url, params={}, options={}, &block)
    safe_http :DELETE, url, postalize(params), nil, options, &block
  end

  def self.patch(url, params={}, &block)
    http :PATCH, url, postalize(params), nil, &block
  end

  def self.safe_patch(url, params={}, options={}, &block)
    safe_http :PATCH, url, postalize(params), nil, options, &block
  end

  def self.head(url, params={}, &block)
    http :HEAD, urlalize(url, params), nil, nil, &block
  end

  def self.safe_head(url, params={}, options={}, &block)
    params, options = split_safe_http_params_options(params, options)
    safe_http :HEAD, urlalize(url, params), nil, nil, options, &block
  end

  def self.options(url, params={}, &block)
    http :OPTIONS, urlalize(url, params), nil, nil, &block
  end

  def self.safe_options(url, params={}, options={}, &block)
    params, options = split_safe_http_params_options(params, options)
    safe_http :OPTIONS, urlalize(url, params), nil, nil, options, &block
  end

  def self.urlalize(url, params={})
    uri = URI(url)
    # early return if we didn't specify any extra params
    return uri.to_s if (params || {}).empty?

    params_query = URI.encode_www_form(params || {})
    uri.query = [uri.query.to_s, params_query].reject(&:empty?).join('&')
    uri.to_s
  end

  def self.postalize(params={})
    params.respond_to?(:map) ? URI.encode_www_form(params) : (params.respond_to?(:to_s) ? params.to_s : params)
  end

  def self.safe_http_options(options)
    options ||= {}
    raise ArgumentError, "safe HTTP options must be a Hash" unless options.is_a?(Hash)

    options = options.dup
    unsupported = options.keys - safe_http_option_keys
    raise ArgumentError, "unsupported safe HTTP option(s): #{unsupported.join(', ')}" unless unsupported.empty?

    options
  end

  def self.split_safe_http_params_options(params, options)
    if options == {} && safe_http_option_hash?(params)
      [{}, params]
    else
      [params, options]
    end
  end

  def self.safe_http_option_hash?(value)
    value.is_a?(Hash) && !value.empty? && (value.keys - safe_http_option_keys).empty?
  end

  def self.safe_http_option_keys
    [:max_body_bytes]
  end

  def self.reset
    Thread.current[:curb_curl] = Curl::Easy.new
  end

end
