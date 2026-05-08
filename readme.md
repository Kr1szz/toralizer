Toralizer - Transparent Tor Routing via LD_PRELOAD

Toralizer is a small, LD_PRELOAD-based tool that intercepts a program's outgoing network connections and routes them through the Tor network via a SOCKS5 proxy. Instead of modifying or reconfiguring your software, Toralizer transparently hooks the connect() system call at runtime, sets up a SOCKS5 handshake with Tor (usually at 127.0.0.1:9050), and routes traffic through Tor without any application modifications.
Features

    LD_PRELOAD Hooking: Leverages the dynamic linker's LD_PRELOAD mechanism to intercept the connect() system call
    SOCKS5 Implementation: Communicates with Tor's SOCKS5 interface, enabling secure and anonymous outbound connections
    IPv4 & IPv6 Support: The hooking library checks the sa_family and handles both AF_INET (IPv4) and AF_INET6 (IPv6)
    Flexible Installation: Can be used locally (no special privileges) or globally (requires root)

How It Works

    LD_PRELOAD: When you set LD_PRELOAD=/path/to/toralizer.so, the dynamic loader injects our library before the standard C library
    Function Override: Our implementation of connect() overrides the system's default connect function
    Traffic Interception: Whenever an application attempts to establish a TCP connection, it calls connect()
    Proxy Connection: Our custom connect() function detects the target address and opens a new socket to your local Tor SOCKS5 proxy
    SOCKS5 Handshake: We perform the SOCKS5 handshake with Tor, instructing it to connect to the original target IP and port
    Transparent Routing: If successful, Tor returns a success code, and we dup2() the proxy socket onto the original file descriptor, so the application thinks it's directly connected to the remote server

Requirements

    gcc or another compiler
    make
    libc6-dev (standard libraries/headers)
    Tor (to actually route traffic via SOCKS5)

Installation
Quick Start
bash

# Clone or download the toralizer files
cd toralizer
# Build the library
make
# Run your first command
./scripts/toralize.sh curl http://check.torproject.org

System-wide Installation
bash

# Install system-wide (requires root)
sudo make install
# Now use the torify command anywhere
torify curl http://check.torproject.org

Local Installation
bash

# Install locally
make install-local
# Add ~/.local/bin to your PATH if not already added
export PATH="$HOME/.local/bin:$PATH"
# Use toralize command
toralize wget https://www.google.com

Usage
Basic Usage
bash

./scripts/toralize.sh <command> [arguments]
# Examples
./scripts/toralize.sh curl http://check.torproject.org
./scripts/toralize.sh wget https://www.google.com
./scripts/toralize.sh telnet example.com 80

Advanced Options
bash

# Use custom Tor proxy
./scripts/toralize.sh --tor-proxy 192.168.1.100:9050 curl http://example.com
# Verbose output
./scripts/toralize.sh --verbose wget https://example.com
# Help
./scripts/toralize.sh --help

System-wide Usage

After installing system-wide, use the torify command:
bash

# Basic usage
torify curl http://check.torproject.org
# Multiple commands
torify ping google.com
torify wget https://example.com
torify ssh user@host

Configuration
Environment Variables

    TOR_PROXY: Override default Tor proxy (format: IP:PORT)
    LOCAL_MODE: Set to "true" for local installation
    VERBOSE: Set to "true" for verbose output

Custom Tor Proxy
bash

# Use non-default Tor proxy
./scripts/toralize.sh --tor-proxy 192.168.1.100:9050 curl http://example.com

Troubleshooting
Common Issues

    Tor not running: Ensure Tor service is running on default port 9050
    Permission denied: Check library permissions and LD_PRELOAD path
    Connection timeouts: Verify Tor proxy accessibility
    Application crashes: Some applications may not work with LD_PRELOAD

Debugging
bash

# Verbose mode
./scripts/toralize.sh --verbose curl http://example.com
# Check Tor status
curl --socks5 127.0.0.1:9050 http://check.torproject.org/api/ip
# Test direct Tor connection
curl --socks5 127.0.0.1:9050 http://example.com

Security Considerations

    All traffic is routed through Tor's encrypted network
    DNS requests are also proxied through Tor
    Application data remains unencrypted at the application layer
    Ensure Tor service is properly configured and secured

License

This project is open source and available under the MIT License.
Contributing

    Fork the repository
    Create a feature branch
    Make your changes
    Add tests if applicable
    Submit a pull request

Support

For issues and questions:

    Create an issue on the repository
    Check the troubleshooting section
    Verify Tor service is running properly

Changelog
v1.0

    Initial release
    LD_PRELOAD library implementation
    SOCKS5 protocol support
    Local and system installation options
    Comprehensive wrapper script

File Structure

toralizer/
├── src/
│   ├── toralizer.c          # Main LD_PRELOAD library
│   └── Makefile            # Build configuration
├── scripts/
│   ├── toralize.sh         # Wrapper script
│   └── make_install_all.sh # Complete installation script
├── docs/
│   ├── README.md           # This documentation
│   └── INSTALL.md          # Detailed installation guide
├── bin/
│   └── toralizer.so        # The LD_PRELOAD library
└── examples/
    ├── basic_usage.sh      # Basic usage examples
    └── advanced_usage.sh   # Advanced usage examples

API Reference
LD_PRELOAD Library

The toralizer works by intercepting connect() system calls using LD_PRELOAD:
c

// The library intercepts all connect() calls
// Redirects non-Tor connections through SOCKS5 proxy
// Maintains full compatibility with existing applications

Supported Protocols

    TCP connections
    IPv4 addresses
    Standard socket APIs

Testing

Run the built-in tests to verify functionality:
bash

make test

Examples
Basic Usage Examples
bash

# Check your IP through Tor
toralize curl http://httpbin.org/ip
# Download files through Tor
toralize wget https://example.com/file.zip
# Browse websites through Tor
toralize lynx http://example.com
# Use with SSH (dynamic port forwarding)
ssh -D 1080 user@server
toralize curl http://check.torproject.org

Advanced Usage Examples
bash

# Custom proxy configuration
TOR_PROXY=192.168.1.100:9050 toralize ping google.com
# Verbose debugging
VERBOSE=1 toralize curl -v http://example.com
# Multiple commands
toralize bash -c "curl http://example.com && wget https://google.com"

Performance Notes

    LD_PRELOAD overhead is minimal for most applications
    SOCKS5 handshake adds ~100-200ms latency to new connections
    Connection reuse provides better performance for subsequent requests
    IPv6 support may have limited availability depending on Tor exit nodes

Limitations

    Only works with dynamically linked applications (not statically linked)
    May not work with applications using raw syscalls instead of libc
    UDP traffic is not supported (only TCP)
    DNS leakage can occur if applications resolve hostnames before calling connect()

Alternatives

    torsocks: More comprehensive but more complex
    proxychains: Network proxy tool (different approach)
    proxychains-ng: Modern fork of proxychains
    wirez: Rootless container-based solution

License

MIT License - see LICENSE file for details

Note: This tool is designed for legitimate privacy and security purposes. Please ensure you comply with all applicable laws and regulations in your jurisdiction.