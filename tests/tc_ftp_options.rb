require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbFtpOptions < Test::Unit::TestCase
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

  # Setting ftp_commands remains supported for control-connection commands.
  def test_can_assign_ftp_commands
    c = Curl::Easy.new('ftp://example.com/')
    c.ftp_commands = ["PWD", "CWD /"]
    assert_kind_of(Array, c.ftp_commands)
    assert_equal ["PWD", "CWD /"], c.ftp_commands
  end
end

