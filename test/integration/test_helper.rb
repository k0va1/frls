require 'minitest/autorun'
require 'fileutils'
require_relative 'lsp_client'

class IntegrationTest < Minitest::Test
  SERVER_HOST = 'localhost'
  SERVER_PORT = 7778
  WORKSPACE_PATH = File.expand_path('../fixtures/project', __dir__)

  def setup
    start_server
    @client = LSPClient.new(host: SERVER_HOST, port: SERVER_PORT)

    # Give server time to start
    max_attempts = 10
    attempts = 0
    loop do
      begin
        @client.connect
        break
      rescue Errno::ECONNREFUSED
        attempts += 1
        if attempts >= max_attempts
          stop_server
          raise "Server failed to start after #{max_attempts} attempts"
        end
        sleep 0.5
      end
    end
  end

  def teardown
    @client.disconnect if @client
    stop_server
  end

  private

  def start_server
    server_path = File.expand_path('../../build/frls', __dir__)

    unless File.exist?(server_path)
      raise "Server binary not found at #{server_path}. Run 'make' first."
    end

    @server_pid = spawn(
      server_path,
      "--host=#{SERVER_HOST}",
      "--port=#{SERVER_PORT}",
      out: '/dev/null',
      err: '/dev/null'
    )

    Process.detach(@server_pid)
  end

  def stop_server
    if @server_pid
      Process.kill('TERM', @server_pid) rescue nil
      Process.wait(@server_pid) rescue nil
      @server_pid = nil
    end
  end

  def build_file_uri(file_path)
    "file://#{File.expand_path(file_path, WORKSPACE_PATH)}"
  end
end
