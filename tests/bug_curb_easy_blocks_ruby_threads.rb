require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugTestInstancePostDiffersFromClassPost < Test::Unit::TestCase
  include BugTestServerSetupTeardown

  def setup
    @port = 9999
    @response_proc = lambda do|res|
      sleep 0.5
      res.body = "hi"
      res['Content-Type'] = "text/html"
    end
    super
  end

  def test_bug
    threads = []
    timer = Time.now

    5.times do |i|
      t = Thread.new do
        c = Curl::Easy.perform('http://127.0.0.1:9999/test')
        c.header_str
      end
      threads << t
    end

    multi_responses = threads.collect do|t|
      t.value
    end

    multi_time = (Time.now - timer)
    puts "requested in #{multi_time}"

    timer = Time.now
    single_responses = []
    5.times do |i|
      c = Curl::Easy.perform('http://127.0.0.1:9999/test')
      single_responses << c.header_str
    end
    
    single_time = (Time.now - timer)
    puts "requested in #{single_time}"

    assert single_time > multi_time
  end
end
