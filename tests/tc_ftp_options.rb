require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbFtpOptions < Test::Unit::TestCase
  SCALAR_AND_STRING_OPTIONS = {
    :ftpport => "-",
    :dirlistonly => true,
    :append => true,
    :ftp_use_eprt => false,
    :ftp_use_epsv => false,
    :ftp_use_pret => true,
    :ftp_create_missing_dirs => true,
    :ftp_response_timeout => 30,
    :ftp_alternative_to_user => "anonymous",
    :ftp_skip_pasv_ip => true,
    :ftpsslauth => 0,
    :ftp_ssl_ccc => 0,
    :ftp_account => "account",
    :ftp_filemethod => 0
  }.freeze

  # Ensure FTP-related set(:option, ...) mappings are accepted and do not raise
  # a TypeError (they used to be unsupported in setopt dispatch).
  def test_can_set_ftp_listing_related_flags
    c = Curl::Easy.new('ftp://example.com/')

    assert_nothing_raised do
      c.set(:dirlistonly, true) if Curl.const_defined?(:CURLOPT_DIRLISTONLY)
      c.set(:ftp_use_epsv, 0)   if Curl.const_defined?(:CURLOPT_FTP_USE_EPSV)
      # These may not be present on all libcurl builds; guard by constant
      c.set(:ftp_use_eprt, 0)   if Curl.const_defined?(:CURLOPT_FTP_USE_EPRT)
      c.set(:ftp_skip_pasv_ip, 1) if Curl.const_defined?(:CURLOPT_FTP_SKIP_PASV_IP)
    end
  end

  def test_can_set_detected_scalar_and_string_ftp_options
    c = Curl::Easy.new('ftp://example.com/')

    SCALAR_AND_STRING_OPTIONS.each do |name, value|
      constant = "CURLOPT_#{name.to_s.upcase}"
      next unless Curl.const_defined?(constant)

      assert_nothing_raised("setting #{name}") { c.set(name, value) }
    end
  end

  def test_issue_481_ftp_create_missing_dirs_accessor_and_set
    omit('CURLOPT_FTP_CREATE_MISSING_DIRS is not available in this build') unless
      Curl.const_defined?(:CURLOPT_FTP_CREATE_MISSING_DIRS)

    c = Curl::Easy.new('ftp://example.com/')

    assert_respond_to(c, :ftp_create_missing_dirs=)
    assert_respond_to(c, :ftp_create_missing_dirs)
    assert_nil(c.ftp_create_missing_dirs)

    c.ftp_create_missing_dirs = true
    assert_equal(true, c.ftp_create_missing_dirs)

    assert_equal(1, c.set(:ftp_create_missing_dirs, 1))
    assert_equal(1, c.ftp_create_missing_dirs)

    retry_mode = if Curl.const_defined?(:CURLFTP_CREATE_DIR_RETRY)
      Curl::CURLFTP_CREATE_DIR_RETRY
    else
      2
    end
    c.set(:ftp_create_missing_dirs, retry_mode)
    assert_equal(retry_mode, c.ftp_create_missing_dirs)
    assert_equal(retry_mode, c.clone.ftp_create_missing_dirs)

    settings = c.reset
    assert_equal(retry_mode, settings[:ftp_create_missing_dirs])
    assert_nil(c.ftp_create_missing_dirs)
  end

  # Setting ftp_commands remains supported for control-connection commands.
  def test_can_assign_ftp_commands
    c = Curl::Easy.new('ftp://example.com/')
    c.ftp_commands = ["PWD", "CWD /"]
    assert_kind_of(Array, c.ftp_commands)
    assert_equal ["PWD", "CWD /"], c.ftp_commands
  end

  def test_ftp_command_entries_can_be_objects_that_convert_to_string
    command = Object.new
    command.define_singleton_method(:to_s) { "PWD" }

    c = Curl::Easy.new($TEST_URL)
    c.ftp_commands = [command]
    m = Curl::Multi.new

    assert_nothing_raised { m.add(c) }
  ensure
    m.remove(c) if defined?(m) && m && m.requests[c.object_id]
    m.close if defined?(m) && m
  end
end
