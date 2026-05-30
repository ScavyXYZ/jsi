<div align="center">

# JSI

**A lightweight Windows remote deployment tool with ARP scanning and admin share copy**

[![Windows](https://img.shields.io/badge/Platform-Windows-blue.svg)]()
[![C++17](https://img.shields.io/badge/C%2B%2B-17-green.svg)]()
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)]()

</div>

---

**JSI** scans your local network via ARP, discovers live hosts, and deploys files or folders to selected machines using administrative shares (e.g., `C$`). It supports optional remote execution, flexible target selection, and detailed logging.

---

## Features

- **ARP‑based network scan** – fast multi‑threaded discovery of live hosts  
- **Flexible target selection** – choose all hosts, a custom list, or a range  
- **File / folder deployment** – copy to remote administrative shares  
- **Remote execution** – launch the deployed file via `SC` (service creation)  
- **Configurable via INI** – network timeouts, credentials, logging, etc.  
- **Interactive or unattended** – credentials can be read from config or prompted  
- **Results export** – save discovered hosts and deployment summary  
- **Minimal dependencies** – uses only Windows API and standard libraries  

---

## Quick Start

### 1. Compile

Use **MSVC** (Visual Studio) or **MinGW‑w64**.  
The code automatically links against required libraries via pragma comments.

```bash
# With MSVC from Developer Command Prompt
cl /EHsc /O2 JSI.cpp /link ws2_32.lib iphlpapi.lib

# With MinGW (requires -static-libgcc -static-libstdc++ for standalone exe)
g++ -O2 -static JSI.cpp -o JSI.exe -lws2_32 -luuid -liphlpapi -lnetapi32 -lmpr -lole32 -lshell32
```

### 2. Run

```bash
JSI.exe [--config myconfig.ini]
```

If `deploy.ini` does not exist, a default one is created automatically.

### 3. Follow the interactive prompts

- Select target hosts  
- Choose a file or folder to deploy  
- Provide credentials (or use preconfigured ones)  
- Optionally launch the deployed file remotely  

---

## Usage Example

```text
C:\> JSI.exe
       _______ ____
      / / ___//  _/
 __  / /\__ \ / /
/ /_/ /___/ // /
\____//____/___/
[*] Config loaded: deploy.ini


[*] Local IP  : 192.168.1.100
[*] Scanning  : 192.168.1.0/24  (64 threads)
    Progress: 254/254 (100%)

[+] Scan complete in 15.649 s.  Found 5 host(s).

[+] Hosts found: 5
    [1] 192.168.1.1      C0-25-2F-DA-8B-E0
    [2] 192.168.1.101    D4-D2-52-33-A3-BF
    [3] 192.168.1.103    62-E3-9C-ED-E1-8F
    [4] 192.168.1.106    FC-19-99-4F-3A-EE
    [5] 192.168.1.107    9A-D6-8E-54-BB-31

========================================
  Select targets for deployment
========================================
  [A] All hosts (5)
  [S] Select manually
  [R] Select range
Choice: S

Available hosts:
  [1] 192.168.1.1      C0-25-2F-DA-8B-E0
  [2] 192.168.1.101    D4-D2-52-33-A3-BF
  [3] 192.168.1.103    62-E3-9C-ED-E1-8F
  [4] 192.168.1.106    FC-19-99-4F-3A-EE
  [5] 192.168.1.107    9A-D6-8E-54-BB-31

Enter numbers (e.g. 1 3 5 or 1,3,5):
> 2

[*] Selected 1 host(s):
    192.168.1.101    D4-D2-52-33-A3-BF

[*] Deploy what?
    [1] File
    [2] Folder
Choice: 1

[+] Selected: C:\file.txt

[*] Launch after deploy?
    [1] Yes
    [2] No
Choice: 2

[*] Credentials
    Username: User1
    Password:

[*] Destination directory on target (default: C:\):
[+] Destination : C:\  (share: C$)

[*] Deploying to 1 host(s)...

[>] 192.168.1.101    [V] OK

========================================
  Done!  OK: 1  |  Failed: 0
========================================
```

---

## Configuration

The `deploy.ini` file controls every aspect of JSI. A default file is created on first run.

### Example configuration

```ini
; JSI Deploy - configuration file
; Lines starting with ; or # are comments
; Boolean values: true / false / yes / no / 1 / 0

; -------------------------------------------------------
[network]
; Maximum simultaneous ARP threads
max_scan_threads  = 64

; ARP reply timeout (milliseconds)
scan_timeout_ms   = 2000

; Default gateway / base IP (informational)
default_ip        = 192.168.1.1

; Subnet mask prefix (informational)
subnet_mask       = 24

; Last octet upper limit (1 .. max_ip_range)
max_ip_range      = 254

; -------------------------------------------------------
[deploy]
; Default admin share letter (C$ / D$ / etc.)
admin_share        = C$

; Timeout waiting for xcopy to finish (milliseconds)
copy_timeout_ms    = 120000

; Overwrite existing files on target
overwrite_existing = true

; Print each discovered host during scan
verbose_output     = false

; -------------------------------------------------------
[log]
enable        = true
file          = deploy_log.txt
path          = .\
log_success   = true
log_failures  = true

; -------------------------------------------------------
[results]
sort               = true
show_mac_addresses = true
export             = true
export_file        = scan_results.txt
export_path        = .\

; -------------------------------------------------------
[arp]
use_cache_fallback = true
temp_file          = _arp_tmp_.txt

; Filter word used to identify dynamic ARP entries
cache_filter       = dynamic

; -------------------------------------------------------
[credentials]
; Set use_current_user = true to skip username/password prompts
use_current_user      = false
max_password_attempts = 3

; Pre-fill username / password (leave blank to prompt)
default_username      =
default_password      =

; Pre-fill destination directory (leave blank to prompt)
default_dest_dir      = C:\
```

### Section details

| Section | Key | Description |
|---------|-----|-------------|
| `[network]` | `max_scan_threads` | Number of concurrent ARP requests |
| | `scan_timeout_ms` | Wait per IP (not used in current ARP scan) |
| | `subnet_mask` | CIDR prefix (e.g., 24 for /24) |
| | `max_ip_range` | Last octet maximum (usually 254) |
| `[deploy]` | `admin_share` | Share name (e.g., `C$`, `ADMIN$`) |
| | `copy_timeout_ms` | Timeout for `xcopy` or `CopyFile` |
| | `overwrite_existing` | Overwrite files already present |
| | `verbose_output` | Show all discovered hosts during scan |
| `[log]` | `enable` | Master switch for logging |
| | `file` / `path` | Log file name and directory |
| | `log_success` / `log_failures` | Filter log entries |
| `[results]` | `sort` | Sort discovered hosts by IP |
| | `show_mac_addresses` | Display MAC addresses in lists |
| | `export` | Write `scan_results.txt` after deployment |
| `[arp]` | `use_cache_fallback` | Use `arp -a` if active scan finds nothing |
| | `cache_filter` | String to filter cache entries (`dynamic` or `static`) |
| `[credentials]` | `default_username` / `default_password` | Leave empty to prompt at runtime |
| | `default_dest_dir` | Remote destination folder |

---

## Build Requirements

- **Windows** (Vista / 7 / 8 / 10 / 11)  
- **C++17** compiler  
  - MSVC 2019 or later  
  - MinGW‑w64 (GCC 8+)  
- **Libraries** (automatically linked via `#pragma comment` when using MSVC):  
  - `ws2_32` – sockets  
  - `iphlpapi` – `SendARP`, `GetAdaptersAddresses`  
  - `netapi32`, `mpr` – network shares (`WNetAddConnection2`)  
  - `advapi32`, `shell32`, `ole32` – miscellaneous  

For MinGW, manually link with `-lws2_32 -liphlpapi -lnetapi32 -lmpr -lole32 -lshell32`.

---

## How It Works

1. **Network scan** – determines the local IP and subnet, then sends ARP requests to all possible addresses in parallel.  
2. **Host enumeration** – collects responding IPs and their MAC addresses.  
3. **Target selection** – interactive menu (all, list, or range).  
4. **Authentication** – connects to the admin share (`\\IP\C$`) using supplied credentials.  
5. **Deployment** – copies a file or entire folder to the remote destination.  
6. **Remote execution** (optional) – creates a temporary Windows service on the target, starts it (which runs the deployed executable), then deletes the service.  
7. **Logging & export** – records successes/failures and optionally exports the list of discovered hosts.

> **Note:** Administrative shares (`C$`, `ADMIN$`) require **administrator privileges** on the target machine. The account used must have appropriate rights.

---

## Limitations & Security

- **Windows only** – uses WinAPI and administrative shares.  
- **Network visibility** – ARP scan only works within the local broadcast domain (no routing).  
- **Credentials** – passwords are handled in plain text (stored in memory, optionally in config file). Use with caution.  
- **No encryption** – deployment traffic is not encrypted; suitable for trusted LAN environments.  
- **Antivirus** – creating a remote service may trigger security software.  

---

## Command Line

| Argument | Description |
|----------|-------------|
| `--config <path>` | Use an alternative configuration file (default: `deploy.ini`) |

---

## License

MIT – see the `LICENSE` file for details.
