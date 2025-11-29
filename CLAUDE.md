# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Fast Ruby Language Server (frls) is a Language Server Protocol (LSP) implementation for Ruby written in C. It uses the Prism parser library for Ruby AST parsing and provides language features like go-to-definition.

## Build & Development Commands

**Build the project:**
```bash
make          # or 'make all'
```

**Build and start the server:**
```bash
make start
```

**Run tests:**
```bash
make test
```

**Build experimental main executable:**
```bash
make main
```

**Clean build artifacts:**
```bash
make clean
```

**Build Prism static library (required dependency, auto-run by make):**
```bash
cd prism && make static
```

**Run the executable directly:**
```bash
build/frls --help      # Show help
build/frls --version   # Show version
build/frls             # Start server with default settings
```

All build artifacts (object files and binaries) are placed in the `build/` directory to keep the project root clean.

## Architecture

### Core Components

**Server (src/server.c, include/server.h)**
- TCP server using select() for handling multiple client connections
- Implements LSP lifecycle: UNINITIALIZED → INITIALIZED → SHUTDOWN
- Non-blocking socket I/O with master/working file descriptor sets
- Maximum 8 concurrent client connections (MAX_CONNECTIONS)

**Parser (src/parser.c, include/parser.h)**
- Wraps Prism parser (prism/include/prism.h) for Ruby AST traversal
- `parse()`: Parses source files and builds constant map (ConstHM)
- `traverse_ast()`: Generic visitor pattern for AST traversal
- `build_const_map()`: Builds hashmap of constants to locations for go-to-definition
- Uses stb_ds.h for dynamic arrays and hashmaps

**Commands (src/commands.c, include/commands.h)**
- LSP command handlers: initialize, shutdown, exit, textDocument/*
- `process_file_tree()`: Recursively walks directory tree to parse Ruby files
- `text_document_did_open/did_change/did_close`: Synchronizes file state
- `go_to_definition()`: Finds constant definitions by position (currently PM_CONSTANT_READ_NODE only)

**Source Management (src/source.c, include/source.h)**
- Source struct tracks file_path, content, parsed AST root, parser instance, and open_status
- Sources stored in dynamic array on Server struct
- OPENED sources are actively edited in client; CLOSED sources are workspace files

**Transport (src/transport.c, include/transport.h)**
- Parses LSP HTTP-style headers (Content-Length, Content-Type)
- Creates Request/Response objects with JSON-RPC 2.0 payloads using cJSON
- Only supports utf-8 charset

**Configuration (src/config.c, include/config.h)**
- Parses CLI arguments: --host, --port, --help, --version
- Stores client info from initialize request

**File Ignoring (src/ignore.c, include/ignore.h)**
- Filters out common directories (.git, node_modules, tmp, vendor, etc.)
- Used during workspace indexing

### Data Flow

1. Client connects → `add_client()` accepts connection
2. Client sends initialize → `initialize()` parses workspace, builds constant index
3. Server status changes UNINITIALIZED → INITIALIZED
4. Client sends textDocument/didOpen → `text_document_did_open()` syncs file, parses, updates ParsedInfo
5. Client sends textDocument/definition → `go_to_definition()` looks up constant in ParsedInfo.consts hashmap
6. Client sends shutdown → `shutdown_server()` changes status to SHUTDOWN
7. Client sends exit → `exit_server()` terminates process

### Key Data Structures

**ParsedInfo**: Global index of all parsed constants
- `consts`: ConstHM hashmap (key: const_name, value: Const with Location array)

**Source**: Represents a Ruby file
- Maintains parsed AST (`root`, `parser`) and synchronization state (`open_status`)

**Location**: File position range
- `file_path`, `start` (line, character), `end` (line, character)
- Positions are zero-based (LSP spec)

## Supported LSP Features

- textDocument/definition (only for constants - PM_CONSTANT_READ_NODE)
- textDocument/didOpen, didChange, didClose
- Full document sync (TextDocumentSyncKind = 1)

## Dependencies

- **Prism**: Ruby parser (prism/ submodule)
- **cJSON**: JSON parsing (vendor/cJSON.c)
- **stb_ds.h**: Dynamic arrays and hashmaps (include/stb_ds.h)

## Testing

Tests are in test/suite.c. Test fixtures use LSP JSON-RPC messages in test/fixtures/.

The test suite uses a sample Ruby project in test/fixtures/project/.

## Notes

- Only .rb files are supported (SUPPORTED_FILE_EXTENSIONS)
- Parser uses Prism's line numbers (1-based) but LSP uses 0-based indexing
- URI format: file:// scheme. Use `get_file_path()` to convert URI → path and `build_uri()` for path → URI
- Error codes follow LSP spec: SERVER_NOT_INITIALIZED (-32002), INVALID_REQUEST (-32600), INVALID_PARAMS (-32602)
