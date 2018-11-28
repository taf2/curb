require 'open3'

module Curb
  module RakeHelpers
    # A simple shell wrapper using stdlib open3. It implements very basic set of methods to
    # mimic MixLib.
    #
    # It's used for old rubies where MixLib is not available.
    class ShellWrapper
      def initialize(args, opts = {})
        @cmd, @live_stream, @cwd = args, opts[:live_stdout], opts[:cwd]
        @stdout, @stderr = '', ''
      end


      def run_command
        # Ruby 1.8 Open3.popen3 does not support changing directory, we
        # need to do it before shelling out.
        if @cwd
          Dir.chdir(@cwd) { execute! }
        else
          execute!
        end
        self
      end

      def execute!
        wait_thr = nil

        Open3.popen3(*@cmd) do |stdin, stdout, stderr, thr|
          stdin.close
          wait_thr = thr # Ruby 1.8 will not yield thr, this will be nil

          while line = stdout.gets do
            @stdout << line
            @live_stream.puts(line) if @live_stream
          end

          while line = stderr.gets do
            @stderr << line
            puts line
          end
        end

        # prefer process handle directly from popen3, but if not available
        # fallback to global.
        p_status = wait_thr ? wait_thr.value : $?
        @exit_code = p_status.exitstatus
        @error = (@exit_code != 0)
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
