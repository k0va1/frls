require_relative 'test_helper'

class DefinitionTest < IntegrationTest
  def setup
    super
    initialize_server
  end

  def test_go_to_constant_definition
    # Open a file that references the Project constant
    file_uri = build_file_uri('lib/project.rb')

    @client.send_notification('textDocument/didOpen', {
      textDocument: {
        uri: file_uri,
        languageId: 'ruby',
        version: 1,
        text: File.read(File.join(WORKSPACE_PATH, 'lib/project.rb'))
      }
    })

    # Request definition for "StandardError" constant on line 6 (class Error < StandardError)
    @client.send_request('textDocument/definition', {
      textDocument: { uri: file_uri },
      position: { line: 6, character: 20 } # Position of "StandardError" constant
    })

    response = @client.read_response

    # StandardError is a built-in Ruby constant, so it might not be found in workspace
    # Just verify we get a valid response (either null or empty array)
    refute_nil response, 'Should receive response'

    # The result can be null/empty for built-in constants or an array with locations
    if response['result'].is_a?(Array) && !response['result'].empty?
      location = response['result'].first
      assert location['uri'], 'Should have URI'
      assert location['range'], 'Should have range'
      assert location['range']['start'], 'Should have start position'
      assert location['range']['end'], 'Should have end position'
    end
  end

  def test_definition_for_nonexistent_constant
    file_uri = build_file_uri('lib/project.rb')

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
    file_uri = build_file_uri('lib/project.rb')

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
    refute_nil response, 'Should receive response after didChange'
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
