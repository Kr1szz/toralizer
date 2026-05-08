# Toralizer — Transparent Tor Routing via `LD_PRELOAD`

Toralizer is a lightweight `LD_PRELOAD`-based networking library that transparently routes outbound TCP connections through the Tor network using a SOCKS5 proxy.

Instead of modifying application code or manually configuring proxy settings, Toralizer intercepts the `connect()` system call at runtime and redirects traffic through Tor automatically.

This allows existing dynamically linked applications such as `curl`, `wget`, `ssh`, `telnet`, and many others to use Tor without native proxy support.

---

# Table of Contents

* [Overview](#overview)
* [Features](#features)
* [Architecture](#architecture)
* [How It Works](#how-it-works)
* [Requirements](#requirements)
* [Installation](#installation)

  * [Quick Start](#quick-start)
  * [System-wide Installation](#system-wide-installation)
  * [Local Installation](#local-installation)
* [Usage](#usage)

  * [Basic Usage](#basic-usage)
  * [Advanced Usage](#advanced-usage)
  * [System-wide Usage](#system-wide-usage)
* [Configuration](#configuration)
* [Environment Variables](#environment-variables)
* [SOCKS5 Workflow](#socks5-workflow)
* [Testing](#testing)
* [Debugging](#debugging)
* [Security Considerations](#security-considerations)
* [Performance Notes](#performance-notes)
* [Limitations](#limitations)
* [Compatibility](#compatibility)
* [Project Structure](#project-structure)
* [Alternatives](#alternatives)
* [Contributing](#contributing)
* [License](#license)

---

# Overview

Toralizer works by injecting a shared object (`toralizer.so`) into a target process using the Linux dynamic linker’s `LD_PRELOAD` mechanism.

Once injected, the library overrides the standard libc `connect()` function and transparently redirects outgoing TCP connections through a Tor SOCKS5 proxy, typically running on:

```text
127.0.0.1:9050
```

The application itself remains completely unaware that its traffic is being proxied.

---

# Features

## Transparent Proxying

Applications do not require proxy-aware configuration or code changes.

## `LD_PRELOAD` Hooking

Overrides libc networking calls dynamically at runtime.

## SOCKS5 Support

Implements the SOCKS5 protocol used by Tor.

## IPv4 and IPv6 Compatibility

Supports both:

* `AF_INET`
* `AF_INET6`

## Minimal Overhead

Lightweight implementation with very small runtime impact.

## Rootless Operation

Can run entirely in user space without elevated privileges.

## Flexible Deployment

Supports:

* Per-command usage
* Local user installation
* Global system-wide installation

## Dynamic Target Routing

Routes arbitrary TCP destinations through Tor automatically.

---

# Architecture

```text
Application
     │
     ▼
LD_PRELOAD Injection
     │
     ▼
Toralizer Library
     │
     ▼
Intercepted connect()
     │
     ▼
SOCKS5 Handshake
     │
     ▼
Tor Proxy (127.0.0.1:9050)
     │
     ▼
Tor Network
     │
     ▼
Destination Server
```

---

# How It Works

## 1. Library Injection

The dynamic linker loads `toralizer.so` before libc:

```bash
LD_PRELOAD=./bin/toralizer.so curl http://example.com
```

## 2. Function Interception

Toralizer overrides:

```c
int connect(int sockfd,
            const struct sockaddr *addr,
            socklen_t addrlen);
```

## 3. Original Destination Capture

The intercepted function extracts:

* Destination IP
* Port
* Address family

## 4. Proxy Socket Creation

A new socket is opened to the Tor SOCKS5 proxy:

```text
127.0.0.1:9050
```

## 5. SOCKS5 Negotiation

Toralizer performs:

1. Authentication negotiation
2. CONNECT request
3. Response validation

## 6. Descriptor Replacement

The proxy socket replaces the original socket using:

```c
dup2()
```

The application continues execution believing it is directly connected to the target host.

---

# Requirements

## Build Dependencies

```bash
gcc
make
libc6-dev
```

## Runtime Dependencies

```bash
tor
```

---

# Installation

# Quick Start

```bash
git clone <repository>
cd toralizer

make

./scripts/toralize.sh curl http://check.torproject.org
```

---

# System-wide Installation

Requires root privileges.

```bash
sudo make install
```

After installation:

```bash
torify curl http://check.torproject.org
```

---

# Local Installation

Install only for the current user:

```bash
make install-local
```

Add local binaries to your `PATH`:

```bash
export PATH="$HOME/.local/bin:$PATH"
```

Use normally:

```bash
toralize wget https://example.com
```

---

# Usage

# Basic Usage

```bash
./scripts/toralize.sh <command> [arguments]
```

Examples:

```bash
./scripts/toralize.sh curl http://check.torproject.org

./scripts/toralize.sh wget https://example.com

./scripts/toralize.sh telnet example.com 80
```

---

# Advanced Usage

## Custom Tor Proxy

```bash
./scripts/toralize.sh \
  --tor-proxy 192.168.1.100:9050 \
  curl http://example.com
```

## Verbose Mode

```bash
./scripts/toralize.sh --verbose wget https://example.com
```

## Display Help

```bash
./scripts/toralize.sh --help
```

---

# System-wide Usage

After global installation:

```bash
torify curl http://check.torproject.org

torify wget https://example.com

torify ssh user@host
```

---

# Configuration

## Environment Variables

### `TOR_PROXY`

Override the default SOCKS5 proxy:

```bash
export TOR_PROXY=127.0.0.1:9050
```

### `LOCAL_MODE`

Enable local installation mode:

```bash
export LOCAL_MODE=true
```

### `VERBOSE`

Enable verbose logging:

```bash
export VERBOSE=true
```

---

# SOCKS5 Workflow

The SOCKS5 negotiation process follows:

## Client Greeting

```text
Client -> Proxy:
SOCKS Version
Authentication Methods
```

## Server Selection

```text
Proxy -> Client:
Selected Authentication Method
```

## Connection Request

```text
Client -> Proxy:
CONNECT <target-ip>:<target-port>
```

## Connection Response

```text
Proxy -> Client:
Success / Failure
```

If successful, all subsequent traffic flows through Tor.

---

# Testing

## Run Test Suite

```bash
make test
```

## Verify Tor Routing

```bash
toralize curl http://httpbin.org/ip
```

## Direct SOCKS5 Validation

```bash
curl --socks5 127.0.0.1:9050 \
     http://check.torproject.org/api/ip
```

---

# Debugging

## Verbose Output

```bash
VERBOSE=1 toralize curl -v http://example.com
```

## Verify Tor Service

```bash
systemctl status tor
```

## Check Listening Port

```bash
ss -ltnp | grep 9050
```

## Trace Dynamic Linking

```bash
LD_DEBUG=libs \
LD_PRELOAD=./bin/toralizer.so \
curl http://example.com
```

## Inspect Network Activity

```bash
strace -e connect \
LD_PRELOAD=./bin/toralizer.so \
curl http://example.com
```

---

# Security Considerations

## Traffic Encryption

Traffic inside the Tor network is encrypted between relay nodes.

However:

* Exit-node traffic may be plaintext
* HTTPS is still strongly recommended

## DNS Leakage

Applications that resolve DNS before calling `connect()` may leak DNS queries outside Tor.

Prefer applications that support remote DNS resolution.

## Unsupported Protocols

Only TCP is supported.

The following are not proxied:

* UDP
* ICMP
* Raw sockets

## Localhost Exclusions

Connections to localhost should generally bypass Tor to avoid routing loops.

## Trust Model

Tor improves anonymity but does not guarantee complete privacy against advanced adversaries.

---

# Performance Notes

## Expected Latency

Tor introduces additional latency due to multi-hop routing.

Typical connection overhead:

```text
100ms–500ms
```

## Connection Reuse

Applications using persistent connections experience better performance.

## Throughput

Bandwidth depends on:

* Tor relay capacity
* Exit node performance
* Geographic distance

---

# Limitations

## Dynamically Linked Applications Only

Statically linked binaries cannot be intercepted using `LD_PRELOAD`.

## Raw Syscalls

Applications bypassing libc networking functions may evade interception.

## TCP Only

UDP traffic is unsupported.

## Partial DNS Protection

Hostname resolution may occur before interception.

## Application Compatibility

Some sandboxed or hardened applications may block `LD_PRELOAD`.

---

# Compatibility

## Supported Platforms

Primarily tested on:

* Linux
* GNU libc environments

## Supported Architectures

* x86_64
* ARM64
* ARMv7 (with recompilation)

## Supported Applications

Generally compatible with:

* `curl`
* `wget`
* `ssh`
* `telnet`
* `lynx`
* `git`
* `apt`

---

# Project Structure

```text
toralizer/
├── src/
│   ├── toralizer.c
│   └── Makefile
│
├── scripts/
│   ├── toralize.sh
```

---

# Example Workflows

## Check External IP

```bash
toralize curl http://httpbin.org/ip
```

## Download Files Through Tor

```bash
toralize wget https://example.com/file.zip
```

## Proxy Git Traffic

```bash
toralize git clone https://github.com/user/repo.git
```

## Route SSH Connections

```bash
toralize ssh user@server
```

## Chain Multiple Commands

```bash
toralize bash -c "
  curl http://example.com &&
  wget https://example.com/file
"
```

---

# Alternatives

| Tool           | Description                                     |
| -------------- | ----------------------------------------------- |
| torsocks       | Mature Tor wrapper with extensive compatibility |
| proxychains-ng | Generic proxy chaining framework                |
| tsocks         | Older SOCKS interception utility                |
| wirez          | Container-based transparent networking          |
| redsocks       | Transparent TCP-to-proxy redirector             |

---

# Contributing

Contributions are welcome.

## Development Workflow

```bash
git checkout -b feature/my-feature
```

## Suggested Areas

* UDP support
* Improved DNS handling
* Better IPv6 coverage
* Enhanced logging
* Unit testing
* macOS compatibility

## Contribution Process

1. Fork the repository
2. Create a feature branch
3. Commit changes
4. Add tests where appropriate
5. Submit a pull request

---

# License

MIT License

See the `LICENSE` file for full details.

---

# Disclaimer

This software is intended for:

* Privacy research
* Security testing
* Educational purposes
* Anonymous networking

Users are responsible for complying with all applicable laws and regulations.

Improper or unlawful use is solely the responsibility of the end user.
