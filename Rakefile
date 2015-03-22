# $Id$
# 
require 'rake/clean'
require 'rake/testtask'
begin
  require 'rdoc/task'
rescue LoadError => e
  require 'rake/rdoctask'
end

CLEAN.include '**/*.o'
CLEAN.include "**/*.#{(defined?(RbConfig) ? RbConfig : Config)::MAKEFILE_CONFIG['DLEXT']}"
CLOBBER.include 'doc'
CLOBBER.include '**/*.log'
CLOBBER.include '**/Makefile'
CLOBBER.include '**/extconf.h'

def announce(msg='')
  $stderr.puts msg
end

desc "Default Task (Test project)"
task :default => :test

# Determine the current version of the software
if File.read('ext/curb.h') =~ /\s*CURB_VERSION\s*['"](\d.+)['"]/
  CURRENT_VERSION = $1
else
  CURRENT_VERSION = "0.0.0"
end

if ENV['REL']
  PKG_VERSION = ENV['REL']
else
  PKG_VERSION = CURRENT_VERSION
end

task :test_ver do
  puts PKG_VERSION
end

# Make tasks -----------------------------------------------------
make_program = (/mswin/ =~ RUBY_PLATFORM) ? 'nmake' : 'make'
MAKECMD = ENV['MAKE_CMD'] || make_program
MAKEOPTS = ENV['MAKE_OPTS'] || ''

CURB_SO = "ext/curb_core.#{(defined?(RbConfig) ? RbConfig : Config)::MAKEFILE_CONFIG['DLEXT']}"

file 'ext/Makefile' => 'ext/extconf.rb' do
  Dir.chdir('ext') do
    ruby "extconf.rb #{ENV['EXTCONF_OPTS']}"
  end
end

def make(target = '')
  Dir.chdir('ext') do 
    pid = system("#{MAKECMD} #{MAKEOPTS} #{target}")
    $?.exitstatus
  end    
end

# Let make handle dependencies between c/o/so - we'll just run it. 
file CURB_SO => (['ext/Makefile'] + Dir['ext/*.c'] + Dir['ext/*.h']) do
  m = make
  fail "Make failed (status #{m})" unless m == 0
end

desc "Compile the shared object"
task :compile => [CURB_SO]

desc "Install to your site_ruby directory"
task :install => :alltests do
  m = make 'install' 
  fail "Make install failed (status #{m})" unless m == 0
end

# Test Tasks ---------------------------------------------------------
task :ta => :alltests
task :tu => :unittests
task :test => [:rmpid,:unittests]

task :rmpid do
  FileUtils.rm_rf Dir.glob("tests/server_lock-*")
end

if ENV['RELTEST']
  announce "Release task testing - not running regression tests on alltests"
  task :alltests => [:unittests]
else
  task :alltests => [:unittests, :bugtests]
end
                    
Rake::TestTask.new(:unittests) do |t|
  t.test_files = FileList['tests/tc_*.rb']
  t.verbose = false
end
                          
Rake::TestTask.new(:bugtests) do |t|
  t.test_files = FileList['tests/bug_*.rb']
  t.verbose = false
end
 
#Rake::TestTask.new(:funtests) do |t|
  #  t.test_files = FileList['test/func_*.rb']
  #t.warning = true
  #t.warning = true
#end

task :unittests => :compile
task :bugtests => :compile

def has_gem?(file,name)
  begin
    require file
    has_http_persistent = true
  rescue LoadError => e
    puts "Skipping #{name}"
  end
end

desc "Benchmark curl against http://127.0.0.1/zeros-2k - will fail if /zeros-2k or 127.0.0.1 are missing"
task :bench do
  sh "ruby bench/curb_easy.rb"
  sh "ruby bench/curb_multi.rb"
  sh "ruby bench/nethttp_test.rb" if has_gem?("net/http/persistent","net-http-persistent")
  sh "ruby bench/patron_test.rb" if has_gem?("patron","patron")
  sh "ruby bench/typhoeus_test.rb" if has_gem?("typhoeus","typhoeus")
  sh "ruby bench/typhoeus_hydra_test.rb" if has_gem?("typhoeus","typhoeus")
end

# RDoc Tasks ---------------------------------------------------------
desc "Create the RDOC documentation"
task :doc do
  ruby "doc.rb #{ENV['DOC_OPTS']}"
end

desc "Publish the RDoc documentation to project web site"
task :doc_upload => [ :doc ] do
  if ENV['RELTEST']
    announce "Release Task Testing, skipping doc upload"
  else    
    unless ENV['RUBYFORGE_ACCT']
      raise "Need to set RUBYFORGE_ACCT to your rubyforge.org user name (e.g. 'fred')"
    end
  
    require 'rake/contrib/sshpublisher'
    Rake::SshDirPublisher.new(
      "#{ENV['RUBYFORGE_ACCT']}@rubyforge.org",
      "/var/www/gforge-projects/curb",
      "doc"
    ).upload
  end
end

if ! defined?(Gem)
  warn "Package Target requires RubyGEMs"
else
  desc 'Generate gem specification'
  task :gemspec do
    require 'erb'
    tspec = ERB.new(File.read(File.join(File.dirname(__FILE__),'lib','curb.gemspec.erb')))
    File.open(File.join(File.dirname(__FILE__),'curb.gemspec'),'wb') do|f|
      f << tspec.result
    end
  end

  desc 'Build gem'
  task :package => :gemspec do
    require 'rubygems/package'
    spec_source = File.read File.join(File.dirname(__FILE__),'curb.gemspec')
    spec = nil
    # see: http://gist.github.com/16215
    Thread.new { spec = eval("$SAFE = 3\n#{spec_source}") }.join
    spec.validate
    Gem::Package.build(spec)
  end

  task :static do
    ENV['STATIC_BUILD'] = '1'
  end

  task :binary_gemspec => [:static, :compile] do
    require 'erb'
    ENV['BINARY_PACKAGE'] = '1'
    tspec = ERB.new(File.read(File.join(File.dirname(__FILE__),'lib','curb.gemspec.erb')))

    File.open(File.join(File.dirname(__FILE__),'curb-binary.gemspec'),'wb') do|f|
      f << tspec.result
    end
  end

  desc 'Strip extra strings from Binary'
  task :binary_strip do
    strip = '/usr/bin/strip'
    if File.exist?(strip) and `#{strip} -h 2>&1`.match(/GNU/)
      sh "#{strip} #{CURB_SO}"
    end
  end

  desc 'Build gem'
  task :binary_package => [:binary_gemspec, :binary_strip] do
    require 'rubygems/specification'
    spec_source = File.read File.join(File.dirname(__FILE__),'curb-binary.gemspec')
    spec = nil
    # see: http://gist.github.com/16215
    Thread.new { spec = eval("$SAFE = 3\n#{spec_source}") }.join
    spec.validate
    Gem::Builder.new(spec).build
  end
end

# --------------------------------------------------------------------
# Creating a release
desc "Make a new release (Requires SVN commit / webspace access)"
task :release => [
  :prerelease,
  :clobber,
  :alltests,
  :update_version,
  :package,
  :tag,
  :doc_upload] do
  
  announce 
  announce "**************************************************************"
  announce "* Release #{PKG_VERSION} Complete."
  announce "* Packages ready to upload."
  announce "**************************************************************"
  announce 
end

# Validate that everything is ready to go for a release.
task :prerelease do
  announce 
  announce "**************************************************************"
  announce "* Making RubyGem Release #{PKG_VERSION}"
  announce "* (current version #{CURRENT_VERSION})"
  announce "**************************************************************"
  announce  

  # Is a release number supplied?
  unless ENV['REL']
    fail "Usage: rake release REL=x.y.z [REUSE=tag_suffix]"
  end

  # Is the release different than the current release.
  # (or is REUSE set?)
  if PKG_VERSION == CURRENT_VERSION && ! ENV['REUSE']
    fail "Current version is #{PKG_VERSION}, must specify REUSE=tag_suffix to reuse version"
  end

  # Are all source files checked in?
  if ENV['RELTEST']
    announce "Release Task Testing, skipping checked-in file test"
  else
    announce "Checking for unchecked-in files..."
    data = `svn status`
    unless data =~ /^$/
      fail "SVN status is not clean ... do you have unchecked-in files?"
    end
    announce "No outstanding checkins found ... OK"
  end
  
  announce "Doc will try to use GNU cpp if available"
  ENV['DOC_OPTS'] = "--cpp"
end

# Used during release packaging if a REL is supplied
task :update_version do
  unless PKG_VERSION == CURRENT_VERSION
    pkg_vernum = PKG_VERSION.tr('.','').sub(/^0*/,'')
    pkg_vernum << '0' until pkg_vernum.length > 2
    
    File.open('ext/curb.h.new','w+') do |f|      
    maj, min, mic, patch = /(\d+)\.(\d+)(?:\.(\d+))?(?:\.(\d+))?/.match(PKG_VERSION).captures
      f << File.read('ext/curb.h').
           gsub(/CURB_VERSION\s+"(\d.+)"/) { "CURB_VERSION   \"#{PKG_VERSION}\"" }.
           gsub(/CURB_VER_NUM\s+\d+/) { "CURB_VER_NUM   #{pkg_vernum}" }.
           gsub(/CURB_VER_MAJ\s+\d+/) { "CURB_VER_MAJ   #{maj}" }.
           gsub(/CURB_VER_MIN\s+\d+/) { "CURB_VER_MIN   #{min}" }.
           gsub(/CURB_VER_MIC\s+\d+/) { "CURB_VER_MIC   #{mic || 0}" }.
           gsub(/CURB_VER_PATCH\s+\d+/) { "CURB_VER_PATCH #{patch || 0}" }           
    end
    mv('ext/curb.h.new', 'ext/curb.h')     
    if ENV['RELTEST']
      announce "Release Task Testing, skipping commiting of new version"
    else
      sh %{svn commit -m "Updated to version #{PKG_VERSION}" ext/curb.h}
    end
  end
end

# "Create a new SVN tag with the latest release number (REL=x.y.z)"
task :tag => [:prerelease] do
  reltag = "curb-#{PKG_VERSION}"
  reltag << ENV['REUSE'] if ENV['REUSE']
  announce "Tagging SVN with [#{reltag}]"
  if ENV['RELTEST']
    announce "Release Task Testing, skipping SVN tagging"
  else
    # need to get current base URL
    s = `svn info`
    if s =~ /URL:\s*([^\n]*)\n/
      svnroot = $1
      if svnroot =~ /^(.*)\/trunk/i
        svnbase = $1
        sh %{svn cp #{svnroot} #{svnbase}/TAGS/#{reltag} -m "Release #{PKG_VERSION}"}
      else
        fail "Please merge to trunk before making a release"
      end
    else 
      fail "Unable to determine repository URL from 'svn info' - is this a working copy?"
    end  
  end
end
