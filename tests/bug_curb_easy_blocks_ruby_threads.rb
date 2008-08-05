require File.join(File.dirname(__FILE__), 'helper')

class BugTestInstancePostDiffersFromClassPost < Test::Unit::TestCase
  def test_bug
    threads = []
    timer = Time.now

    5.times do |i|
      t = Thread.new do
        c = Curl::Easy.perform('http://www.google.com/')
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
      c = Curl::Easy.perform('http://www.google.com/')
      single_responses << c.header_str
    end
    
    single_time = (Time.now - timer)
    puts "requested in #{single_time}"

    assert single_time > multi_time
  end
end
