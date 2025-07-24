# Coverage tasks for both Ruby and C code

desc "Run tests with full coverage (Ruby + C)"
task :coverage => [:coverage_clean, :compile_coverage, :test_coverage, :coverage_report]

desc "Clean coverage files"
task :coverage_clean do
  # Clean C coverage files
  sh "rm -f ext/*.gcda ext/*.gcno ext/*.gcov"
  sh "rm -rf coverage/"
  sh "rm -rf coverage_c/"
end

desc "Compile extension with coverage flags"
task :compile_coverage => :coverage_clean do
  # Clean previous build
  sh "cd ext && make clean" rescue nil
  sh "rm -f ext/Makefile"
  
  # Get existing flags
  existing_cflags = ENV['CFLAGS'] || ""
  existing_ldflags = ENV['LDFLAGS'] || ""
  
  # Set coverage flags for compilation - prepend to existing flags
  ENV['CFLAGS'] = "-fprofile-arcs -ftest-coverage -g -O0 #{existing_cflags}"
  ENV['LDFLAGS'] = "-fprofile-arcs -ftest-coverage #{existing_ldflags}"
  
  # Regenerate Makefile with coverage flags using special extconf
  Dir.chdir("ext") do
    sh "ruby extconf_coverage.rb"
    sh "make"
  end
  
  # Verify .gcno files were created
  gcno_count = Dir.glob("ext/*.gcno").size
  if gcno_count == 0
    puts "WARNING: No .gcno files generated. C coverage may not work."
  else
    puts "SUCCESS: Generated #{gcno_count} .gcno files for coverage."
  end
end

desc "Run tests with coverage enabled"
task :test_coverage do
  ENV['COVERAGE'] = '1'
  Rake::Task['test'].invoke
end

desc "Generate C coverage report"
task :coverage_report do
  puts "\n=== Generating C Coverage Report ==="
  
  # Create coverage directory
  sh "mkdir -p coverage_c"
  
  # Generate coverage info
  sh "cd ext && gcov *.c"
  
  # Check if lcov is available
  has_lcov = system("which lcov > /dev/null 2>&1")
  
  if has_lcov
    # Generate detailed HTML report with lcov
    sh "lcov --capture --directory ext --output-file coverage_c/coverage.info"
    
    # Check what patterns exist in the coverage data
    coverage_data = File.read("coverage_c/coverage.info") rescue ""
    
    # Build exclude patterns based on what's actually in the data
    exclude_patterns = []
    exclude_patterns << "'/usr/*'" if coverage_data.include?("/usr/")
    exclude_patterns << "'*/ruby/*'" if coverage_data.match?(%r{/ruby/})
    exclude_patterns << "'/Applications/*'" if coverage_data.include?("/Applications/")
    exclude_patterns << "'/opt/homebrew/*'" if coverage_data.include?("/opt/homebrew/")
    
    if exclude_patterns.any?
      sh "lcov --remove coverage_c/coverage.info #{exclude_patterns.join(' ')} --output-file coverage_c/coverage_filtered.info"
    else
      # No patterns to exclude, just copy
      sh "cp coverage_c/coverage.info coverage_c/coverage_filtered.info"
    end
    
    sh "genhtml coverage_c/coverage_filtered.info --output-directory coverage_c/html"
    
    puts "\n=== Coverage Reports Generated ==="
    puts "Ruby coverage: coverage/index.html"
    puts "C coverage: coverage_c/html/index.html"
  else
    # Basic text report
    puts "\n=== C Coverage Summary ==="
    Dir.glob("ext/*.gcov").each do |file|
      puts "\n#{File.basename(file)}:"
      coverage_data = File.read(file)
      total_lines = 0
      covered_lines = 0
      
      coverage_data.each_line do |line|
        if line =~ /^\s*(-|\d+):\s*\d+:/
          total_lines += 1
          covered_lines += 1 if line =~ /^\s*\d+:/
        end
      end
      
      percentage = (covered_lines.to_f / total_lines * 100).round(2)
      puts "  Coverage: #{percentage}% (#{covered_lines}/#{total_lines} lines)"
    end
    
    puts "\nInstall lcov for detailed HTML reports: brew install lcov (macOS) or apt-get install lcov (Linux)"
  end
end

desc "Run tests with memory checking and coverage"
task :test_memcheck_coverage => [:compile_coverage] do
  ENV['COVERAGE'] = '1'
  Rake::Task['test:valgrind'].invoke
  Rake::Task['coverage_report'].invoke
end