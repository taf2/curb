require File.join(File.dirname(__FILE__), 'helper')

class TestCurbCurlDownload < Test::Unit::TestCase
  
  def test_download_url_to_file
    dl_url = "http://127.0.0.1:9130/ext/curb_easy.c"
    dl_path = File.join("/tmp/dl_url_test.file")

    curb = Curl::Easy.download(dl_url, dl_path)
    assert File.exist?(dl_path)
  end

  def test_download_bad_url_gives_404
    dl_url = "http://127.0.0.1:9130/this_file_does_not_exist.html"
    dl_path = File.join("/tmp/dl_url_test.file")

    curb = Curl::Easy.download(dl_url, dl_path)
    puts curb.class
    assert_equal 404, curb.response_code
  end

    
  def self.locked_file
    File.join(File.dirname(__FILE__),'server_lock')
  end

  def setup
    if @server.nil? and !File.exist?(TestCurbCurlDownload.locked_file)
      #puts "starting"
      File.open(TestCurbCurlDownload.locked_file,'w') {|f| f << 'locked' }

      # start up a webrick server for testing delete 
      @server = WEBrick::HTTPServer.new :Port => 9130, :DocumentRoot => File.expand_path(File.dirname(File.dirname(__FILE__)))

      @server.mount(TestServlet.path, TestServlet)

      @test_thread = Thread.new { @server.start }

      exit_code = lambda do
        begin
          #puts "stopping"
          File.unlink TestCurbCurlDownload.locked_file if File.exist?(TestCurbCurlDownload.locked_file)
          @server.shutdown unless @server.nil?
        rescue Object => e
          puts "Error #{__FILE__}:#{__LINE__}\n#{e.message}"
        end
      end

      trap("INT"){exit_code.call}
      at_exit{exit_code.call}

    end
  end

  def teardown
  end

end
