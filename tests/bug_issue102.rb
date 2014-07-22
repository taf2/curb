require File.expand_path(File.join(File.dirname(__FILE__), 'helper'))

class BugIssue102 < Test::Unit::TestCase

  def test_interface
    test = "https://api.twitter.com/1/users/show.json?screen_name=TwitterAPI&include_entities=true"
    ip = "0.0.0.0"

    c = Curl::Easy.new do |curl|
      curl.url = test
      curl.interface = ip
    end  

    c.perform
  end

end
