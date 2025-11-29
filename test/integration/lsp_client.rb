require 'socket'
require 'json'

class LSPClient
  attr_reader :socket

  def initialize(host: 'localhost', port: 7777)
    @host = host
    @port = port
    @socket = nil
    @message_id = 0
  end

  def connect
    @socket = TCPSocket.new(@host, @port)
  end

  def disconnect
    @socket.close if @socket
    @socket = nil
  end

  def send_request(method, params = {})
    @message_id += 1
    message = {
      jsonrpc: '2.0',
      id: @message_id,
      method: method,
      params: params
    }
    send_message(message)
  end

  def send_notification(method, params = {})
    message = {
      jsonrpc: '2.0',
      method: method,
      params: params
    }
    send_message(message)
  end

  def send_message(message)
    content = message.to_json
    header = "Content-Length: #{content.bytesize}\r\n\r\n"
    @socket.write(header + content)
    @socket.flush
  end

  def read_response
    # Read headers
    headers = {}
    loop do
      line = @socket.gets
      break if line.nil? || line.strip.empty?

      key, value = line.split(':', 2)
      headers[key.strip] = value.strip if key && value
    end

    # Read content
    content_length = headers['Content-Length'].to_i
    return nil if content_length == 0

    content = @socket.read(content_length)
    JSON.parse(content)
  end

  def next_message_id
    @message_id + 1
  end
end
