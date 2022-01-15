# ==== About Docker
#
# Docker needs to be installed and support mounting host volumes for tasks to work as expected.
#
# macOS and Windows may require additonal configuration that is not covered here. Refer to the
# official documentation:
#
# * https://docs.docker.com/docker-for-mac/#file-sharing
# * https://docs.docker.com/docker-for-windows/#shared-drives
#
# Additionally, on Linux, having enabled and configured user namespaces is recommended to maintain
# file permissions.  Not having user-ns will make all new files belonging to root and some tasks may
# stop working outside of docker environment.
#
# See the oficial docummentation how to enable user-ns:
#
# https://docs.docker.com/engine/security/userns-remap/
#
# WARNING: Enabling user namespaces will have global impact and some containers/images will not work
#          in this configuration.  Do read the documentation carefully.
#
#          Disabling user namespaces will restore the original docker behavior and fix any errors
#          related to user-ns.
#
# ==== Summary
#
# This namespace provides Rake tasks to manage development environment in docker.
#
# With minimal requirements it allows to run all tests across all supported rubies and provides a
# real time feedback.
#
# Start with "docker:environment" task (can be ran multiple times) and then run "docker:test".
#
# Use 'docker:list_rubies' task to list supported ruby images.
#
# Use 'docker:test' task to run a test suite in docker.
#
# To run tests on different ruby set DOCKER_RUBY_IMAGE environment variable to a supported ruby.
#
# This has been tested in Linux with Docker v18.06, with user namespaces enabed and running in
# non-privileged mode.
namespace :docker do
  desc "Run the test suite in docker environment (#{Docker.ruby_image})"
  task :test => :recompile do
    run_in_docker(Docker.ruby_image, 'bundle', 'exec', 'rake', 'tu', { :live_stdout => STDOUT }).error!
  end

  desc "Run the test suite against all rubies"
  task :test_all do
    Docker::DOCKER_IMAGES.keys.each do |image_name|
      ENV['DOCKER_RUBY_IMAGE'] = image_name
      Rake::Task['docker:recompile'].reenable
      invoke_and_reenable('docker:test')
    end
  end

  desc 'List supported ruby images (for use as DOCKER_RUBY_IMAGE env)'
  task :list_rubies do
    puts "Supported images:"
    Docker::DOCKER_IMAGES.keys.each do |image_name|
      puts "  #{image_name}"
    end
  end

  # This task automates the environment configuration for developing Curb in docker.
  #
  # It pulls necessary images, configures volumes, networks etc...
  #
  # It also collects the following diagnostic information:
  #
  # * docker binary client & server version (docker system info)
  # * docker network the containers will use (docker network inspect <network>)
  # * all named docker volumes (docker volume inspect <volume>)
  # * all pulled docker images (docker image inspect <image>)
  # * ruby environment of each container (bundle env)
  # * curl version (curl -V)
  # * last 3 commits info (without diffs)
  #
  # All the collected data is stored in 'build/docker/*'.
  desc 'Docker docker environment for developing Curb.'
  task :environment => Docker::BUILD_DIRECTORIES do
    Rake::Task['build/docker/entrypoint_ruby1.8.sh'].invoke
    Rake::Task['build/docker/docker.json'].invoke
    Rake::Task['build/docker/network.json'].invoke
    Rake::Task['build/docker/git_curb_info.txt'].invoke

    Docker::DOCKER_IMAGES.each do |name, conf|
      # create all named volumes
      conf[:volumes].each { |vol| Rake::Task[vol[:filepath]].invoke }

      Rake::Task[conf[:filepath]].invoke
      Rake::Task[conf[:bundle_env_filepath]].invoke if conf[:bundle_env_filepath]
      Rake::Task[conf[:curl_filepath]].invoke       if conf[:curl_filepath]
    end
  end

  # Tasks listed below should be considered private and not invoked directly. These tasks do not
  # have "desc" to make Rake not list them.
  #
  # Rake will ignore the "private" statement, it's added here to show the intention.
  private

  file 'build/docker/_compiled_to_ruby.txt' do
    Rake::Task['docker:recompile'].invoke
  end

  file 'build/docker/git_curb_info.txt' do
    Rake::Task['docker:docker_git_curb_info'].invoke
  end

  file 'build/docker/docker.json' do
    Rake::Task['docker:docker_binary'].invoke
  end

  file 'build/docker/network.json' do
    Rake::Task['docker:docker_network'].invoke
  end

  file 'build/docker/entrypoint_ruby1.8.sh' do
    entrypoint = 'build/docker/entrypoint_ruby1.8.sh'
    File.write(entrypoint, <<-ENTRYPOINT)
#!/bin/bash -e

# Ruby 1.8 docker image does not include libcurl development libraries
apt-get update -qq || true
apt-get install -y -q --no-install-recommends libcurl4-openssl-dev

exec "$@"
    ENTRYPOINT
    File.chmod(0775, entrypoint)
  end
  # This section generates file tasks based on the configuration (see 'tasks/rake_helpers.rb').
  #
  # This code is being evaluated (loaded) early in the Rakefile, before any task is executed.
  # By the time Rake executes a task the "file" resources defined here are available.
  Docker::BUILD_DIRECTORIES.each { |dir| directory(dir) }
  Docker::DOCKER_IMAGES.each do |full_name, conf|
    file conf[:filepath] do
      invoke_and_reenable('docker:docker_pull', full_name)
    end

    file conf[:bundle_env_filepath] do
      invoke_and_reenable('docker:bundle_install', full_name)
    end if conf[:bundle_env_filepath]

    (conf[:volumes] || []).each do |vol_conf|
      file vol_conf[:filepath] do
        invoke_and_reenable('docker:docker_volume', vol_conf[:name], vol_conf[:filepath])
      end
    end

    file conf[:curl_filepath] do
      invoke_and_reenable('docker:docker_curl', full_name)
    end if conf[:curl_filepath]
  end

  #################
  #
  # Tasks defined below perform actual work and should not be invoked individually.
  #
  # These tasks are implementation detail and can change at any time without warning.
  #
  # This is the heart of docker.rake
  ##################

  # This task is a direct dependency of 'docker:test'. It has 2 purposes:
  #
  # * detect if curb has been compiled against current ruby
  # * recompile only if needed
  #
  # To successfuly recompile we need to clobber first, otherwise the built-in 'compile' task will
  # do nothing because it can't detect changes to the system libraries.
  #
  # This task triggers full recompilation on ruby version change, but when only source files are
  # changed it does nothing. In such case the default 'compile' target will run recompilation on
  # changed files only.
  task :recompile do
    # Find out what ruby version are we running in docker
    cmd_ruby_v_docker = run_in_docker(Docker.ruby_image, 'rake', 'ruby_version')
    cmd_ruby_v_docker.error!
    ruby_v_docker = cmd_ruby_v_docker.stdout

    # Check what Ruby version was Curb compiled against and set it to UNKNOWN if the file doesn't
    # exist to trigger recompilation (we can't know if it was compiled with correct Ruby).
    ruby_v_filepath = 'build/docker/_compiled_to_ruby.txt'
    ruby_v_file = File.exists?(ruby_v_filepath) ? File.read(ruby_v_filepath) : 'UNKNOWN'

    if ruby_v_file != ruby_v_docker
      run_in_docker(Docker.ruby_image, 'rake', 'clobber').error!
      run_in_docker(Docker.ruby_image, 'rake', 'compile', { :live_stdout => nil }).error!
    end

    File.write(ruby_v_filepath, ruby_v_docker)
  end

  # Creates persistent named volumes for containers to store gems installed by bundler.
  #
  # Docker containers have ephermal file system and can't persist anything. Any changes to the disk
  # will be deleted when the container stops. As a result we'd need to do a fresh 'bundle install'
  # each time we want to run tests.
  #
  # To avoid 'bundle install' completely we will mount a persistent volume to '/usr/local/bundle'.
  task :docker_volume, [:volume_name, :filepath] do |_, args|
    abort('volume_name argument is required.') unless args.volume_name
    abort('filepath argument is required.') unless args.filepath

    # This should never fail, even if the volume exists docker return success, but if it does for
    # whatever reason we can't do anything about it so we will just explode.
    create_cmd = shell(['docker', 'volume', 'create', args.volume_name])
    abort('failed to create docker volume') if create_cmd.error?

    # This could fail if the volume (for any reason) does not exist.
    inspect_cmd = shell(['docker', 'volume', 'inspect', args.volume_name])
    inspect_cmd.error!

    File.write(args.filepath, inspect_cmd.stdout)
  end

  # Run bundle install on the container and dump bundle env to file. The gems will be installed to
  # the shared volume for later reuse.
  #
  # Each image has it's own volume and different ruby versions are completely isolated.
  task :bundle_install, [:name] do |_, args|
    config = Docker::DOCKER_IMAGES[args.name]

    bundle_install = run_in_docker(args.name, 'bundle', 'install')
    bundle_install.error!

    bundle_env = run_in_docker(args.name, 'bundle', 'env')
    bundle_env.error!

    File.write(config[:bundle_env_filepath], bundle_env.stdout)
  end

  # Pull a docker image from Dockerhub.
  #
  # This will pull up-to-date images, it's common otherwise to have local outdated images in the
  # cache. We always want to work on the most recent versions.
  task :docker_pull, [:name] do |_, args|
    conf = Docker::DOCKER_IMAGES[args.name]

    pull_cmd = shell(['docker', 'pull', "#{conf[:name]}:#{conf[:tag]}"])
    abort('docker pull failed.') if pull_cmd.error?

    inspect_cmd = shell(['docker', 'image', 'inspect', "#{conf[:name]}:#{conf[:tag]}"])
    inspect_cmd.error!

    File.write(conf[:filepath], inspect_cmd.stdout)
  end

  # Make sure the docker binary is available.
  task :docker_binary do
    cmd = shell(['docker', 'system', 'info', '--format', '{{json .}}'])
    if cmd.error?
      abort('docker binary not found on the system. Make sure docker is installed.')
    end

    File.write('build/docker/docker.json', cmd.stdout)
  end

  # Create docker network `curb`.
  #
  # Using a named network will allow us to run supporting services in docker. It's not in active
  # use currently and it's intended for the future.
  #
  # The intention is to allow easy testing against different configurations with little manual
  # setup.
  #
  # Some possible examples:
  #
  # * test with nginx or apache
  # * test different protocols CURL supports (ie SMTP)
  # * test against backend application written in any language
  #
  # With initial setup in place, and even if the feature is not used in the codebase it's handy for
  # local development.
  task :docker_network do
    shell(['docker', 'network', 'create', 'curb'])

    inspect_cmd = shell(['docker', 'network', 'inspect', 'curb'])
    inspect_cmd.error!

    File.write('build/docker/network.json', inspect_cmd.stdout)
  end

  # Make sure curl is available
  task :docker_curl, [:full_name] do |_, args|
    conf = Docker::DOCKER_IMAGES[args.full_name]

    curl_info = run_in_docker(args.full_name, 'curl', '-V')
    curl_info.error!

    File.write(conf[:curl_filepath], curl_info.stdout)
  end

  # Grab last 3 commits and save info to the file.
  #
  # '--no-pager' is required, otherwise git would block waiting on user input when the commit is
  # too long to display on one screen. Since we don't connect stdin it would hang indefinitely.
  task :docker_git_curb_info do
    git_cmd = run_in_docker(Docker.ruby_image, 'git', '--no-pager', 'log', '--graph', '-n', '3', '--no-color')
    git_cmd.error!

    File.write('build/docker/git_curb_info.txt', git_cmd.stdout)
  end
end
