require_relative 'test_helper'

class BasicTest < IntegrationTest
  def test_initialize
    response = @client.send_request('initialize', {
      processId: Process.pid,
      rootUri: "file://#{WORKSPACE_PATH}",
      capabilities: {
        textDocument: {
          definition: {
            linkSupport: true
          }
        }
      }
    })

    result = @client.read_response

    assert result['result'], 'Initialize should return result'
    assert result['result']['capabilities'], 'Should have capabilities'
  end

  def test_initialize_and_shutdown
    # Initialize
    @client.send_request('initialize', {
      processId: Process.pid,
      rootUri: "file://#{WORKSPACE_PATH}",
      capabilities: {}
    })

    init_response = @client.read_response
    assert init_response['result'], 'Initialize should succeed'

    # Initialized notification
    @client.send_notification('initialized', {})

    # Shutdown
    @client.send_request('shutdown', {})
    shutdown_response = @client.read_response

    # Shutdown should either return success or null (both are valid)
    if shutdown_response
      assert_nil shutdown_response['error'], 'Shutdown should not return error'
    end
  end

  def test_initialize_with_workspace_folders
    response = @client.send_request('initialize', {
      processId: Process.pid,
      rootUri: "file://#{WORKSPACE_PATH}",
      workspaceFolders: [
        {
          uri: "file://#{WORKSPACE_PATH}",
          name: File.basename(WORKSPACE_PATH)
        }
      ],
      capabilities: {}
    })

    result = @client.read_response
    assert result['result'], 'Initialize with workspace folders should succeed'
  end

  def test_uninitialized_request_fails
    # Try to send a request before initializing
    @client.send_request('textDocument/definition', {
      textDocument: { uri: 'file:///test.rb' },
      position: { line: 0, character: 0 }
    })

    response = @client.read_response
    assert response['error'], 'Should return error for uninitialized request'
    assert_equal(-32002, response['error']['code'], 'Should be SERVER_NOT_INITIALIZED error')
  end
end
