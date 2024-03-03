# Fast Ruby Language Server

Yet another one impementation of Ruby Language Server. Currentrly in progress

#### Available CLI arguments

`--help, -h`: help info

`--version, -v`: print current version

`--host=<value>`: specify host

`--port=<port>`: specify port

#### How to send a request?

```bash
cat fixtures/initialize.txt | curl telnet://127.0.0.1:1488
```

### Development

Build and run: `make`

Run tests: `make test`
