require 'open3'

module Curb
  module RakeHelpers
    # A simple shell wrapper using stdlib open3. It implements very basic set of methods to
    # mimic MixLib.
    #
    # It's used for old rubies where MixLib is not available.
    class ShellWrapper
      def initialize(*args)
        @options = args.last.is_a?(Hash) ? args.pop : {}
        @cmd = args
        @live_stream = @options.delete(:live_stdout)
        @stdout = ''
        @stderr = ''
      end


      def run_command
        wait_thr = nil
        Open3.popen3(*@cmd) do |stdin, stdout, stderr, thr|
          stdin.close
          wait_thr = thr

          while line = stdout.gets do
            @stdout << line
            @live_stream.puts(line) if @live_stream
          end

          while line = stderr.gets do
            @stderr << line
            puts line
          end
        end

        @exit_code = wait_thr.value.exitstatus
        @error = !wait_thr.value.success?
        self
      end

      def stderr
        @stderr
      end

      def stdout
        @stdout
      end

      def error!
        fail "Command failed with exit-code #{@exit_code}" if @error
      end

      def error?
        !!@error
      end
    end
  end
end
