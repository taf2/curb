# Special extconf for coverage builds
# This ensures coverage flags are properly applied

# First, load the original extconf
load File.join(File.dirname(__FILE__), 'extconf.rb')

# After the Makefile is created, modify it to add coverage flags
if File.exist?('Makefile')
  makefile = File.read('Makefile')
  
  # Add coverage flags to CFLAGS
  makefile.gsub!(/^CFLAGS\s*=(.*)$/) do |match|
    "CFLAGS = -fprofile-arcs -ftest-coverage -g -O0 #{$1}"
  end
  
  # Add coverage flags to LDFLAGS
  makefile.gsub!(/^LDFLAGS\s*=(.*)$/) do |match|
    "LDFLAGS = -fprofile-arcs -ftest-coverage #{$1}"
  end
  
  # Also add to dldflags for shared library
  makefile.gsub!(/^dldflags\s*=(.*)$/) do |match|
    "dldflags = -fprofile-arcs -ftest-coverage #{$1}"
  end
  
  File.write('Makefile', makefile)
  puts "Modified Makefile for coverage support"
end