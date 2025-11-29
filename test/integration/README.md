# Integration Tests

Ruby-based integration tests for the FRLS (Fast Ruby Language Server).

## Prerequisites

- Ruby installed (2.7 or later recommended)
- FRLS server built (`make` or `make all`)
- Minitest gem (`gem install minitest`)
- Rake gem (`gem install rake`)

## Running Tests

Run all integration tests:
```bash
make test           # Builds frls and runs all tests
rake test           # Run tests directly with Rake
```

Run specific tests:
```bash
rake test N=test_initialize       # Run test by name
rake test N=/definition/          # Run tests matching regex
rake test X=/basic/               # Exclude tests matching pattern
```

Or run individual test files:
```bash
rake test test/integration/basic_test.rb
ruby test/integration/basic_test.rb -n test_initialize
```

## Test Structure

- `lsp_client.rb` - LSP client helper for communicating with the server
- `test_helper.rb` - Shared test setup and utilities
- `basic_test.rb` - Basic LSP lifecycle tests (initialize, shutdown, etc.)
- `definition_test.rb` - Go-to-definition functionality tests

## How It Works

1. Each test starts a fresh FRLS server instance on a test port (7778)
2. Tests connect via TCP and send LSP JSON-RPC messages
3. Responses are parsed and validated
4. Server is shut down after each test

## Adding New Tests

Extend `IntegrationTest` class from `test_helper.rb`:

```ruby
require_relative 'test_helper'

class MyTest < IntegrationTest
  def test_something
    # Test code here
    # @client is available for sending LSP messages
  end
end
```
