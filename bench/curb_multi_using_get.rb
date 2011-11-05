require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'

$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','ext'))
$:.unshift File.expand_path(File.join(File.dirname(__FILE__),'..','lib'))

N = (ARGV.shift || 50).to_i
BURL = 'http://127.0.0.1/zeros-2k'

Memory.usage("Curl::Multi.get(#{N})") do
  require 'curb'

  group = []
  N.times do|n|
    url = BURL + "?n=#{n}"

    group << url

    if group.size == 10
      Curl::Multi.get(group)
      group = []
    end

  end

  Curl::Multi.get(group) if group.any?

end
