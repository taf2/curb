require 'fileutils'
require 'open3'
require 'shellwords'
include FileUtils

def makefile_variables(path)
  variables = {}

  File.foreach(path) do |line|
    line = line.chomp
    variables[$1] = $2.strip if line =~ /\A([A-Za-z_]\w*)\s*=\s*(.*)\z/
  end

  variables
end

def expand_make_variables(value, variables, seen = [])
  value.gsub(/\$\(([^)]+)\)/) do
    key = Regexp.last_match(1)
    raise ArgumentError, "unsupported Makefile variable in INCFLAGS: #{key}" unless variables.key?(key)
    raise ArgumentError, "recursive Makefile variable in INCFLAGS: #{key}" if seen.include?(key)

    expand_make_variables(variables[key], variables, seen + [key])
  end
end

def makefile_incflags
  variables = makefile_variables('ext/Makefile')
  Shellwords.split(expand_make_variables(variables.fetch('INCFLAGS', ''), variables))
rescue Errno::ENOENT
  $stderr.puts("No makefile found; run `rake ext/Makefile' first.")
  []
end

pp_srcdir = 'ext'

rm_rf(tmpdir = '.doc-tmp')
mkdir(tmpdir)

begin
  if ARGV.include?('--cpp') 
    begin
      cpp_version, status = Open3.capture2e('cpp', '--version')

      if status.success? && cpp_version =~ /\(GCC\)/
        # gnu cpp
        $stderr.puts "Running GNU cpp over source"
        incflags = makefile_incflags
        
        Dir['ext/*.c'].sort.each do |fn|
          out = File.join(tmpdir, File.basename(fn))
          abort "cpp failed for #{fn}" unless system('cpp', '-DRDOC_NEVER_DEFINED', '-C', *incflags, '-o', out, fn)
        end
        
        pp_srcdir = tmpdir
      else
        $stderr.puts "Not running cpp (non-GNU)"
      end
    rescue Errno::ENOENT
      # no cpp          
      $stderr.puts "No cpp found"
    rescue ArgumentError => e
      abort e.message
    end
  end
 
  main = File.exist?('README.md') ? 'README.md' : 'README'
  sources = Dir[File.join(pp_srcdir, '*.c')].sort
  abort 'rdoc failed' unless system(
    'rdoc',
    '--title', 'Curb - libcurl bindings for ruby',
    '--main', main,
    *sources,
    main,
    'LICENSE',
    'lib/curb.rb'
  )
ensure
  rm_rf(tmpdir)
end

 
