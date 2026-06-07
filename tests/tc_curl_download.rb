require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))
require 'tmpdir'

class TestCurbCurlDownload < Test::Unit::TestCase
  include TestServerMethods 

  def setup
    server_setup
  end

  def test_download_url_to_file_via_string
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"
    dl_path = File.join(Dir::tmpdir, "dl_url_test.file")

    Curl::Easy.download(dl_url, dl_path)
    assert File.exist?(dl_path)
    assert_equal File.read(File.join(File.dirname(__FILE__), '..','ext','curb_easy.c')), File.read(dl_path)
  ensure
    File.unlink(dl_path) if File.exist?(dl_path)
  end

  def test_download_default_path_does_not_overwrite_existing_file
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"

    Dir.mktmpdir('curb-download-') do |dir|
      Dir.chdir(dir) do
        File.write('curb_easy.c', 'sentinel')

        assert_raise(Curl::DownloadTargetExistsError) do
          Curl::Easy.download(dl_url)
        end

        assert_equal 'sentinel', File.read('curb_easy.c')
      end
    end
  end

  def test_download_default_path_strips_query_from_url_basename
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c?cache=1"
    source_path = File.expand_path(File.join(File.dirname(__FILE__), '..','ext','curb_easy.c'))

    Dir.mktmpdir('curb-download-') do |dir|
      Dir.chdir(dir) do
        Curl::Easy.download(dl_url)

        assert File.exist?('curb_easy.c')
        assert !File.exist?('curb_easy.c?cache=1')
        assert_equal File.read(source_path), File.read('curb_easy.c')
      end
    end
  end

  def test_download_safe_directory_rejects_unsafe_names
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"

    Dir.mktmpdir('curb-download-') do |dir|
      ['../escape.c', '.env', 'nested/file', '/tmp/escape.c', "bad\0name"].each do |name|
        assert_raise(ArgumentError, "expected #{name.inspect} to be rejected") do
          Curl::Easy.download(dl_url, name, :download_dir => dir)
        end
      end
    end
  end

  def test_download_safe_directory_rejects_dotfile_from_url
    dl_url = "http://127.0.0.1:9129/ext/.env"

    Dir.mktmpdir('curb-download-') do |dir|
      assert_raise(ArgumentError) do
        Curl::Easy.download(dl_url, :download_dir => dir)
      end

      assert !File.exist?(File.join(dir, '.env'))
    end
  end

  def test_download_overwrite_true_replaces_existing_file
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"

    Dir.mktmpdir('curb-download-') do |dir|
      dl_path = File.join(dir, 'curb_easy.c')
      File.write(dl_path, 'sentinel')

      Curl::Easy.download(dl_url, dl_path, :overwrite => true)

      assert_equal File.read(File.join(File.dirname(__FILE__), '..','ext','curb_easy.c')), File.read(dl_path)
    end
  end

  def test_download_callback_abort_does_not_replace_existing_file
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"

    Dir.mktmpdir('curb-download-') do |dir|
      dl_path = File.join(dir, 'curb_easy.c')
      File.write(dl_path, 'sentinel')

      assert_raise(Curl::Err::WriteError, Curl::Err::RecvError) do
        Curl::Easy.download(dl_url, dl_path, :overwrite => true) do |curl|
          curl.on_body { |_data| 0 }
        end
      end

      assert_equal 'sentinel', File.read(dl_path)
    end
  end

  def test_download_url_to_file_via_file_io
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"
    dl_path = File.join(Dir::tmpdir, "dl_url_test.file")
    io = File.open(dl_path, 'wb')

    Curl::Easy.download(dl_url, io)
    assert io.closed?
    assert File.exist?(dl_path)
    assert_equal File.read(File.join(File.dirname(__FILE__), '..','ext','curb_easy.c')), File.read(dl_path)
  ensure
    File.unlink(dl_path) if File.exist?(dl_path)
  end

  def test_download_url_to_file_via_io
    omit('fork not available on this platform') if NO_FORK || WINDOWS
    dl_url = "http://127.0.0.1:9129/ext/curb_easy.c"
    dl_path = File.join(Dir::tmpdir, "dl_url_test.file")
    reader, writer = IO.pipe

    # Write to local file
    child_pid = fork do
      begin
        writer.close
        File.open(dl_path, 'wb') { |file| file << reader.read }
        exit! 0
      rescue StandardError
        exit! 1
      ensure
        reader.close rescue IOError # if the stream has already been closed
      end
    end

    # Download remote source
    begin
      reader.close
      Curl::Easy.download(dl_url, writer)
      _pid, status = Process.wait2(child_pid)
      assert_predicate status, :success?
    ensure
      writer.close rescue IOError # if the stream has already been closed, which occurs in Easy::download
    end

    assert File.exist?(dl_path)
    assert_equal File.read(File.join(File.dirname(__FILE__), '..','ext','curb_easy.c')), File.read(dl_path)
  ensure
    File.unlink(dl_path) if dl_path && File.exist?(dl_path)
  end

  def test_download_bad_url_gives_404
    dl_url = "http://127.0.0.1:9129/this_file_does_not_exist.html"
    dl_path = File.join(Dir::tmpdir, "dl_url_test.file")

    curb = Curl::Easy.download(dl_url, dl_path)
    assert_equal Curl::Easy, curb.class
    assert_equal 404, curb.response_code
  ensure
    File.unlink(dl_path) if File.exist?(dl_path)
  end

end
