module Curb
  module RakeHelpers

    # This module is being required in the top level Rakefile and it provides all-around helpers.
    module DSL

      # Invoke a Rake task by name and enable it again after completion.
      #
      # Rake tries to be smarter than Make when it comes to dependency graph resolution and it will
      # invoke any task only once, even if it's called multiple times with different parameters.
      #
      # It's not a bug, it's how Rake is designed to work.
      #
      # To workaround this limitation a wrapper method will invoke and then re-enable the task
      # afterwards. No exception handling is necessary as the Rake will just abort everything.
      def invoke_and_reenable(task_name, *args)
        rake_task = Rake::Task[task_name]
        rake_task.invoke(*args)
        rake_task.reenable # allow the task to be executed again
        rake_task
      end

      # Returns the current ruby major version.
      #
      # Ruby uses the following version schema:
      #
      #   <MAJOR>.<MINOR>.<TEENY>
      #
      # Ruby maintains API compatibility on <TEENY> level and change to <MINOR> means breaking
      # changes.
      #
      # When compiling sources we will persist the information about ruby version used and when it
      # changes it's a signal to recompile.
      def current_ruby_major
        RUBY_VERSION.match(/(\d).(\d).\d/)[1..2].join('_') # => 2_5
      end

      # Helper method for shelling out.
      #
      # It's easier than Rake's 'sh' and it's backed by Mixlib::Shellout if available with fallback
      # to pure ruby implementation compatible down to Ruby 1.8.
      def shell(args, mix_options = {})
        puts "> #{args.join(' ')}"

        _shell_wrapper(args, mix_options).run_command
      end

      def run_in_docker(*args)
        Docker.run_in_docker(*args)
      end

      private

      # Mixlib won't be available on every ruby we support. This handles selecting appropriate
      # backed for the command.
      def _shell_wrapper(args, shell_options)
        if defined?(Mixlib::ShellOut) # we're on "modern" ruby that supports mixlib
          Mixlib::ShellOut.new(args, shell_options)
        else # ruby 1.8
          ShellWrapper.new(args, shell_options)
        end
      end
    end

    # This module encapsulates configuration and helper methods to interact with docker.
    module Docker
      DEFAULT_DOCKER_IMAGE = 'ruby:2.5'
      BUILD_DIRECTORIES = %w(build/docker)

      DOCKER_VOLUMES = {
        'ruby:1.8' => [ { :name => 'curb_ruby_1_8', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_1_8.json' } ],
        'ruby:2.0' => [ { :name => 'curb_ruby_2_0', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_0.json' } ],
        'ruby:2.1' => [ { :name => 'curb_ruby_2_1', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_1.json' } ],
        'ruby:2.2' => [ { :name => 'curb_ruby_2_2', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_2.json' } ],
        'ruby:2.3' => [ { :name => 'curb_ruby_2_3', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_3.json' } ],
        'ruby:2.4' => [ { :name => 'curb_ruby_2_4', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_4.json' } ],
        'ruby:2.5' => [ { :name => 'curb_ruby_2_5', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_5.json' } ],
        'ruby:2.6' => [ { :name => 'curb_ruby_2_6', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_6.json' } ],
        'ruby:2.7' => [ { :name => 'curb_ruby_2_7', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_2_7.json' } ],
        'ruby:3.0' => [ { :name => 'curb_ruby_3_0', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_3_0.json' } ],
        'ruby:3.1' => [ { :name => 'curb_ruby_3_1', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_3_1.json' } ],
        'ruby:3.2' => [ { :name => 'curb_ruby_3_2', :mount_path => '/usr/local/bundle', :filepath => 'build/docker/volume_ruby_3_2.json' } ],
      }

      DOCKER_IMAGES = {
        'ruby:1.8'     => { :name => 'phusion/passenger-ruby18',  :tag => 'latest',
                            :filepath => 'build/docker/image_ruby_1_8.json',
                            :gemfile => 'Gemfile.ruby-1.8',
                            :entrypoint => '/code/build/docker/entrypoint_ruby1.8.sh',
                            :curl_filepath => 'build/docker/curl_ruby_1_8.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_1_8.md',
                            :volumes => DOCKER_VOLUMES['ruby:1.8'] },
        'ruby:2.0'     => { :name => 'ruby',  :tag => '2.0',
                            :filepath => 'build/docker/image_ruby_2_0.json',
                            :gemfile => 'Gemfile.ruby-2.1',
                            :curl_filepath => 'build/docker/curl_ruby_2_0.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_0.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.0'] },
        'ruby:2.1'     => { :name => 'ruby',  :tag => '2.1',
                            :gemfile => 'Gemfile.ruby-2.1',
                            :filepath => 'build/docker/image_ruby_2_1.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_1.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_1.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.1'] },
        'ruby:2.2'     => { :name => 'ruby',  :tag => '2.2',
                            :filepath => 'build/docker/image_ruby_2_2.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_2.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_2.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.2'] },
        'ruby:2.3'     => { :name => 'ruby',  :tag => '2.3',
                            :filepath => 'build/docker/image_ruby_2_3.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_3.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_3.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.3'] },
        'ruby:2.4'     => { :name => 'ruby',  :tag => '2.4',
                            :filepath => 'build/docker/image_ruby_2_4.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_4.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_4.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.4'] },
        'ruby:2.5'     => { :name => 'ruby',  :tag => '2.5',
                            :filepath => 'build/docker/image_ruby_2_5.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_5.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_5.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.5'] },
        'ruby:2.6'     => { :name => 'ruby',  :tag => '2.6',
                            :filepath => 'build/docker/image_ruby_2_6.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_6.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_6.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.6'] },
        'ruby:2.7'     => { :name => 'ruby',  :tag => '2.7',
                            :filepath => 'build/docker/image_ruby_2_7.json',
                            :curl_filepath => 'build/docker/curl_ruby_2_7.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_2_7.md',
                            :volumes => DOCKER_VOLUMES['ruby:2.7'] },
        'ruby:3.0'     => { :name => 'ruby',  :tag => '3.0',
                            :filepath => 'build/docker/image_ruby_3_0.json',
                            :curl_filepath => 'build/docker/curl_ruby_3_0.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_3_0.md',
                            :volumes => DOCKER_VOLUMES['ruby:3.0'] },
        'ruby:3.1'     => { :name => 'ruby',  :tag => '3.1',
                            :filepath => 'build/docker/image_ruby_3_1.json',
                            :curl_filepath => 'build/docker/curl_ruby_3_1.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_3_1.md',
                            :volumes => DOCKER_VOLUMES['ruby:3.1'] },
        'ruby:3.2'     => { :name => 'ruby',  :tag => '3.2',
                            :filepath => 'build/docker/image_ruby_3_2.json',
                            :curl_filepath => 'build/docker/curl_ruby_3_2.txt',
                            :bundle_env_filepath => 'build/docker/bundle_env_ruby_3_2.md',
                            :volumes => DOCKER_VOLUMES['ruby:3.2'] },
      }

      # Returns current docker image name with tag for other Rake tasks to use.
      #
      # Task 'docker:test_all' overwrites the ENV to run a test suite for every configuration, but
      # otherwise it's not overridden anywhere.
      #
      # 'docker:test' uses this to select appropriate image to test on.
      #
      # Example:
      #
      #   DOCKER_RUBY_IMAGE=ruby:2.2 rake docker:test # run a test suite on Ruby 2.2
      #
      def self.ruby_image
        ENV['DOCKER_RUBY_IMAGE'] || DEFAULT_DOCKER_IMAGE
      end

      # Execute a command in docker container.
      #
      # If the last argument is a Hash it will be used as options for the Ruby shell and not to
      # passed to docker as part of the command.
      #
      # See Mixlib::ShellOut documentation for supported options.
      # See Curb::RakeHelpers::ShellWrapper (defined in 'tasks/utils.rb') for supported options.
      #
      # The command will be passed to the Mixlib library if available, otherwise (old rubies) it
      # will be passed through ShellWrapper.
      #
      # If this method is invoked from inside the docker container it will raise an exception. We
      # will not support running docker in docker.
      #
      # Given an example call:
      #
      #   Curb::Docker.run_in_docker('ruby:2.5', 'rake', 'recompile')
      #
      # It's being translated to the following shell command:
      #
      #   docker run --rm -t -w /code
      #              --mount type=bind,source=$PWD,target=/code
      #              --mount type=volume,source=curb_ruby_2_5,target=/usr/local/bundle
      #              --network curb
      #              ruby:2.5 rake compile
      #
      def self.run_in_docker(config_name, *cmd)
        conf = DOCKER_IMAGES[config_name]

        # The docker-in-docker detection is as simple as checking existence of '/.dockerenv' file.
        # '.dockerenv' and '.dockerinit' have been historically used with the LXC execution driver,
        # but it has been completely removed from docker in version 1.10.0 and the files are no
        # longer used for anything, yet remain widely used.
        #
        # Official ruby images ship with an empty '.dockerenv' file.
        docker_in_docker = File.exists?('/.dockerenv')
        abort("Can't run docker inside docker") if docker_in_docker

        # If last argument of cmd is a hash do not pass it to docker, but pass it to shell.
        shell_options =  cmd.last.is_a?(Hash) ? cmd.pop : {}

        # Setup mount points for named volumes.
        #
        # These volumes are used to persist gems installed with 'bundle install' across runs.
        #
        # As of version 17.06 standalone containers support '--mount' flag that is almost
        # equivalent to '-v'.
        #
        # When '-v' flag is used to mount a host volume and the local path does not exist it will
        # create an empty directory and then mount it.
        #
        # When '--mount' flag is used with a non-existent path it will raise an error.
        #
        # '--mount' is more verbose than '-v', but it's more explicit, easier to understand,
        # and it's recommended over '-v' in the documentation:
        #
        #   New users should use the --mount syntax. Experienced users may be more familiar
        #   with the -v or --volume syntax, but are encouraged to use --mount, because research has
        #   shown it to be easier to use.
        #
        # Source: https://docs.docker.com/storage/bind-mounts/
        volumes = conf[:volumes].map do |conf|
          ['--mount', "type=volume,source=#{conf[:name]},target=#{conf[:mount_path]}"]
        end

        # Inject custom gemfiles over the original ones.
        #
        # Bundler supports BUNDLE_GEMFILE environment variable that points to non-standard Gemfile,
        # but it doesn't work reliably across rubies and compilation crashes on 2.1.
        #
        # This will +shadow+ the original Gemfile and Gemfile.lock with a custom one if defined in
        # the configuration.
        #
        # It will crash if custom Gemfile or Gemfile.lock does not exist.
        if conf[:gemfile]
          volumes << ['--mount', "type=bind,source=#{Dir.pwd}/#{conf[:gemfile]},target=/code/Gemfile"]
          volumes << ['--mount', "type=bind,source=#{Dir.pwd}/#{conf[:gemfile]}.lock,target=/code/Gemfile.lock"]
        end

        # Mount project directory to '/code'. Any changes made the local files will be reflected
        # in the container.
        #
        # It allows 'rake docker:test' to see the changes instantly and enables real-time feedback
        # loop for developer's convenience.
        volumes << ['--mount', "type=bind,source=#{Dir.pwd},target=/code"]

        # The docker entrypoint will be executed instead of command and the command will be passed
        # as arguments. This allows us to hook into the container and install/reconfigure
        # dependencies as needed, etc.
        entrypoint = [ '--entrypoint', conf[:entrypoint]] if conf[:entrypoint]

        image_name = "#{conf[:name]}:#{conf[:tag]}"

        args = [ 'docker',
                 'run',
                 '--rm',
                 '-t', # make docker color the output :)
                 '-w', '/code',
                 '--network', 'curb',
                 volumes,
                 entrypoint,
                 image_name,
                 cmd].
                 flatten.  # volumes
                 compact   # entrypoint

         shell(args, shell_options)
      end
    end
  end
end

# Because the file is loaded rather than required this will be evaluated by Ruby
# and the DSL will be extended automatically.
include Curb::RakeHelpers::DSL

# Handy shortcut
abort("FATAL: Docker constant already loaded.") if defined?(Docker)
Docker = Curb::RakeHelpers::Docker
