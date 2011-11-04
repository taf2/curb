require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

Memory.usage("EM::HTTPRequest(#{N})") do

  require 'em-http-request'
  EM.run do
    multi = EM::MultiRequest.new

    N.times do|n|
      # add multiple requests to the multi-handler
      multi.add(EM::HttpRequest.new(BURL).get)
    end

    multi.callback do
      EM.stop
    end

  end

end
