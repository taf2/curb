require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class TestCurbCurlEasyCookielist < Test::Unit::TestCase
  def test_setopt_cookielist
    easy = Curl::Easy.new
    # DateTime handles time zone correctly
    expires = (Date.today + 2).to_datetime
    easy.setopt(Curl::CURLOPT_COOKIELIST, "Set-Cookie: c1=v1; domain=localhost; expires=#{expires.httpdate};")
    easy.setopt(Curl::CURLOPT_COOKIELIST, 'Set-Cookie: c2=v2; domain=localhost')
    easy.setopt(Curl::CURLOPT_COOKIELIST, "Set-Cookie: c3=v3; expires=#{expires.httpdate};")
    easy.setopt(Curl::CURLOPT_COOKIELIST, 'Set-Cookie: c4=v4;')
    easy.setopt(Curl::CURLOPT_COOKIELIST, "Set-Cookie: c5=v5; domain=127.0.0.1; expires=#{expires.httpdate};")
    easy.setopt(Curl::CURLOPT_COOKIELIST, 'Set-Cookie: c6=v6; domain=127.0.0.1;;')

    # Since 7.43.0 cookies that were imported in the Set-Cookie format without a domain name are not exported by this option.
    # So, before 7.43.0, c3 and c4 will be exported too; but that version is far too old for current curb version, so it's not handled here.
    expected_cookielist = [
      "127.0.0.1\tFALSE\t/\tFALSE\t#{expires.to_time.to_i}\tc5\tv5",
      "127.0.0.1\tFALSE\t/\tFALSE\t0\tc6\tv6",
      ".localhost\tTRUE\t/\tFALSE\t#{expires.to_time.to_i}\tc1\tv1",
      ".localhost\tTRUE\t/\tFALSE\t0\tc2\tv2",
    ]
    assert_equal expected_cookielist, easy.cookielist

    easy.url = "#{TestServlet.url}/get_cookies"
    easy.perform
    assert_equal 'c6=v6; c5=v5; c4=v4; c3=v3', easy.body_str
    easy.url = "http://localhost:#{TestServlet.port}#{TestServlet.path}/get_cookies"
    easy.perform
    assert_equal 'c2=v2; c1=v1', easy.body_str

    # libcurl documentation says: "This option also enables the cookie engine", but it's not tracked on the curb level
    assert !easy.enable_cookies?
  end

  def test_setopt_cookielist_invalid_format
    easy = Curl::Easy.new
    easy.url = "http://localhost:#{TestServlet.port}#{TestServlet.path}/get_cookies"
    easy.setopt(Curl::CURLOPT_COOKIELIST, 'Not a cookie')
    assert_nil easy.cookielist
    easy.perform
    assert_equal '', easy.body_str
  end

  def test_setopt_cookielist_netscape_format
    easy = Curl::Easy.new
    expires = (Date.today + 2).to_datetime
    easy.url = "http://localhost:#{TestServlet.port}#{TestServlet.path}/get_cookies"
    # Note domain changes for include subdomains
    [
      ['localhost', 'FALSE', '/', 'TRUE', 0, 'session_http_only', '42'].join("\t"),
      ['.localhost', 'TRUE', '/', 'FALSE', 0, 'session', '43'].join("\t"),
      ['localhost', 'TRUE', '/', 'FALSE', expires.to_time.to_i, 'permanent', '44'].join("\t"),
      ['.localhost', 'FALSE', '/', 'TRUE', expires.to_time.to_i, 'permanent_http_only', '45'].join("\t"),
    ].each { |cookie| easy.setopt(Curl::CURLOPT_COOKIELIST, cookie) }

    expected_cookielist = [
      ['localhost', 'FALSE', '/', 'TRUE', 0, 'session_http_only', '42'].join("\t"),
      ['.localhost', 'TRUE', '/', 'FALSE', 0, 'session', '43'].join("\t"),
      ['.localhost', 'TRUE', '/', 'FALSE', expires.to_time.to_i, 'permanent', '44'].join("\t"),
      ['localhost', 'FALSE', '/', 'TRUE', expires.to_time.to_i, 'permanent_http_only', '45'].join("\t"),
    ]
    assert_equal expected_cookielist, easy.cookielist
    easy.perform
    assert_equal 'permanent_http_only=45; session_http_only=42; permanent=44; session=43', easy.body_str
  end

  # Multiple cookies and comments are not supported
  def test_setopt_cookielist_netscape_format_mutliline
    easy = Curl::Easy.new
    easy.url = "http://localhost:#{TestServlet.port}#{TestServlet.path}/get_cookies"
    easy.setopt(
      Curl::CURLOPT_COOKIELIST,
      [
        '# Netscape HTTP Cookie File',
        ['.localhost', 'TRUE', '/', 'FALSE', 0, 'session', '42'].join("\t"),
        '',
      ].join("\n"),
    )
    assert_nil easy.cookielist
    easy.perform
    assert_equal '', easy.body_str

    easy.setopt(
      Curl::CURLOPT_COOKIELIST,
      [
        ['.localhost', 'TRUE', '/', 'FALSE', 0, 'session', '42'].join("\t"),
        ['.localhost', 'TRUE', '/', 'FALSE', 0, 'session2', '84'].join("\t"),
        '',
      ].join("\n"),
    )
    # Only first cookie is set
    assert_equal [".localhost\tTRUE\t/\tFALSE\t0\tsession\t42"], easy.cookielist
    easy.perform
    assert_equal 'session=42', easy.body_str
  end

  # ALL erases all cookies held in memory
  # ALL was added in 7.14.1
  def test_setopt_cookielist_command_all
    expires = (Date.today + 2).to_datetime
    with_permanent_and_session_cookies(expires) do |easy|
      easy.setopt(Curl::CURLOPT_COOKIELIST, 'ALL')
      assert_nil easy.cookielist
      easy.perform
      assert_equal '', easy.body_str
    end
  end

  # SESS erases all session cookies held in memory
  # SESS was added in 7.15.4
  def test_setopt_cookielist_command_sess
    expires = (Date.today + 2).to_datetime
    with_permanent_and_session_cookies(expires) do |easy|
      easy.setopt(Curl::CURLOPT_COOKIELIST, 'SESS')
      assert_equal [".localhost\tTRUE\t/\tFALSE\t#{expires.to_time.to_i}\tpermanent\t42"], easy.cookielist
      easy.perform
      assert_equal 'permanent=42', easy.body_str
    end
  end

  # FLUSH writes all known cookies to the file specified by CURLOPT_COOKIEJAR
  # FLUSH was added in 7.17.1
  def test_setopt_cookielist_command_flush
    expires = (Date.today + 2).to_datetime
    with_permanent_and_session_cookies(expires) do |easy|
      cookiejar = File.join(Dir.tmpdir, 'curl_test_cookiejar')
      assert !File.exist?(cookiejar)
      begin
        easy.cookiejar = cookiejar
        # trick to actually set CURLOPT_COOKIEJAR
        easy.enable_cookies = true
        easy.perform
        easy.setopt(Curl::CURLOPT_COOKIELIST, 'FLUSH')
        expected_cookiejar = <<~COOKIEJAR
          # Netscape HTTP Cookie File
          # https://curl.se/docs/http-cookies.html
          # This file was generated by libcurl! Edit at your own risk.
          
          .localhost	TRUE	/	FALSE	0	session	420
          .localhost	TRUE	/	FALSE	#{expires.to_time.to_i}	permanent	42
        COOKIEJAR
        assert_equal expected_cookiejar, File.read(cookiejar)
      ensure
        # Otherwise it'll create this file again
        easy.close
        File.unlink(cookiejar) if File.exist?(cookiejar)
      end
    end
  end

  # RELOAD loads all cookies from the files specified by CURLOPT_COOKIEFILE
  # RELOAD was added in 7.39.0
  def test_setopt_cookielist_command_reload
    expires = (Date.today + 2).to_datetime
    expires_file = (Date.today + 4).to_datetime
    with_permanent_and_session_cookies(expires) do |easy|
      cookiefile = File.join(Dir.tmpdir, 'curl_test_cookiefile')
      assert !File.exist?(cookiefile)
      begin
        cookielist = [
          # Won't be updated, added intead
          ".localhost\tTRUE\t/\tFALSE\t#{expires_file.to_time.to_i}\tpermanent\t84",
          ".localhost\tTRUE\t/\tFALSE\t#{expires_file.to_time.to_i}\tpermanent_file\t84",
          # Won't be updated, added intead
          ".localhost\tTRUE\t/\tFALSE\t0\tsession\t840",
          ".localhost\tTRUE\t/\tFALSE\t0\tsession_file\t840",
          '',
        ]
        File.write(cookiefile, cookielist.join("\n"))
        easy.cookiefile = cookiefile
        # trick to actually set CURLOPT_COOKIEFILE
        easy.enable_cookies = true
        easy.perform
        easy.setopt(Curl::CURLOPT_COOKIELIST, 'RELOAD')
        expected_cookielist = [
          ".localhost\tTRUE\t/\tFALSE\t#{expires.to_time.to_i}\tpermanent\t42",
          ".localhost\tTRUE\t/\tFALSE\t0\tsession\t420",
          ".localhost\tTRUE\t/\tFALSE\t#{expires_file.to_time.to_i}\tpermanent\t84",
          ".localhost\tTRUE\t/\tFALSE\t#{expires_file.to_time.to_i}\tpermanent_file\t84",
          ".localhost\tTRUE\t/\tFALSE\t0\tsession\t840",
          ".localhost\tTRUE\t/\tFALSE\t0\tsession_file\t840",
        ]
        assert_equal expected_cookielist, easy.cookielist
        easy.perform
        # Duplicates are not removed
        assert_equal 'permanent_file=84; session_file=840; permanent=84; session=840; permanent=42; session=420', easy.body_str
      ensure
        File.unlink(cookiefile) if File.exist?(cookiefile)
      end
    end
  end

  def with_permanent_and_session_cookies(expires)
    easy = Curl::Easy.new
    easy.url = "http://localhost:#{TestServlet.port}#{TestServlet.path}/get_cookies"
    easy.setopt(Curl::CURLOPT_COOKIELIST, "Set-Cookie: permanent=42; domain=localhost; expires=#{expires.httpdate};")
    easy.setopt(Curl::CURLOPT_COOKIELIST, 'Set-Cookie: session=420; domain=localhost;')
    assert_equal [".localhost\tTRUE\t/\tFALSE\t#{expires.to_time.to_i}\tpermanent\t42", ".localhost\tTRUE\t/\tFALSE\t0\tsession\t420"], easy.cookielist
    easy.perform
    assert_equal 'permanent=42; session=420', easy.body_str

    yield easy
  end

  include TestServerMethods

  def setup
    server_setup
  end
end
