require 'rubygems'
$:.unshift File.expand_path(File.dirname(__FILE__))
require '_usage'
N = (ARGV.shift || 50).to_i

Memory.usage("Patron(#{N})") do

  require 'patron'

  sess = Patron::Session.new
  sess.base_url = 'http://127.0.0.1'

  N.times do |n|
    sess.get('/zeros-2k')
  end

end
