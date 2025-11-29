require_relative 'test_helper'

class DefinitionTest < IntegrationTest
  def setup
    super
    initialize_server
  end

  def test_go_to_constant_definition
    # Open a file that uses a constant
    file_uri = build_file_uri('main.rb')

    @client.send_notification('textDocument/didOpen', {
      textDocument: {
        uri: file_uri,
        languageId: 'ruby',
        version: 1,
        text: File.read(File.join(WORKSPACE_PATH, 'main.rb'))
      }
    })

    # Request definition for a constant (e.g., "Post" in main.rb)
    @client.send_request('textDocument/definition', {
      textDocument: { uri: file_uri },
      position: { line: 2, character: 6 } # Position of "Post" constant
    })

    response = @client.read_response

    assert response['result'], 'Should return definition result'

    if response['result'].is_a?(Array) && !response['result'].empty?
      location = response['result'].first
      assert location['uri'], 'Should have URI'
      assert location['range'], 'Should have range'
      assert location['range']['start'], 'Should have start position'
      assert location['range']['end'], 'Should have end position'
    end
  end

  def test_definition_for_nonexistent_constant
    file_uri = build_file_uri('main.rb')

    @client.send_notification('textDocument/didOpen', {
      textDocument: {
        uri: file_uri,
        languageId: 'ruby',
        version: 1,
        text: 'NonexistentConstant'
      }
    })

    @client.send_request('textDocument/definition', {
      textDocument: { uri: file_uri },
      position: { line: 0, character: 5 }
    })

    response = @client.read_response

    # Should return null or empty array for nonexistent constant
    assert(response['result'].nil? || response['result'].empty?,
           'Should return null or empty for nonexistent constant')
  end

  def test_did_change_updates_document
    file_uri = build_file_uri('main.rb')

    # Open document
    @client.send_notification('textDocument/didOpen', {
      textDocument: {
        uri: file_uri,
        languageId: 'ruby',
        version: 1,
        text: 'OldConstant'
      }
    })

    # Change document
    @client.send_notification('textDocument/didChange', {
      textDocument: {
        uri: file_uri,
        version: 2
      },
      contentChanges: [
        { text: 'NewConstant' }
      ]
    })

    # Request definition should work with new content
    @client.send_request('textDocument/definition', {
      textDocument: { uri: file_uri },
      position: { line: 0, character: 5 }
    })

    response = @client.read_response
    assert_not_nil response, 'Should receive response after didChange'
  end

  private

  def initialize_server
    @client.send_request('initialize', {
      processId: Process.pid,
      rootUri: "file://#{WORKSPACE_PATH}",
      capabilities: {
        textDocument: {
          definition: { linkSupport: true },
          synchronization: {
            didSave: true,
            didOpen: true,
            didClose: true,
            didChange: true
          }
        }
      }
    })

    @client.read_response
    @client.send_notification('initialized', {})
  end
end
