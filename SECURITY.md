# Security Policy — Sugarota Desktop

## Architecture & Security Model

Sugarota Desktop is designed with a strict **"Local-First, Privacy-by-Default"** security model.

The application serves as a dedicated, lightweight personal telemetry monitor for continuous glucose data. It operates completely independently of third-party cloud infrastructure beyond direct HTTPS communication with your designated Nightscout CGM provider.

### Core Architectural Principles:
- **Zero Cloud Intermediaries:** Sugarota Desktop does not run, connect to, or route telemetry through any developer-operated relay servers, third-party analytics services, or cloud proxies. All telemetry flows exclusively between your desktop and your configured Nightscout endpoint.
- **Local Client-Side Storage:** All user configuration (including Nightscout URL, API secret token, and visual preferences) is stored locally on your machine in `%APPDATA%\sugarota-desktop\config.json`. Credentials are never transmitted to external destinations or third parties.
- **Direct HTTPS Communication:** Connections to your Nightscout instance are established via native Windows HTTP Services (`winhttp.lib`) using modern TLS (TLS 1.2/1.3) encryption.
- **Zero Background Web Runtimes:** The application is built entirely as a native Win32/Direct2D binary without Node.js, Electron, or embedded Chromium instances, drastically reducing the local attack surface and external dependency supply-chain risks.

---

## Threat Model & Mitigations

| Threat | Mitigation |
|:---|:---|
| **Eavesdropping on telemetry in transit** | Strict TLS 1.2/1.3 encryption over HTTPS for all WinHTTP network requests to the Nightscout API. |
| **Credential exfiltration via cloud sync** | No cloud sync or third-party servers; settings and tokens reside strictly on the local filesystem in `%APPDATA%\sugarota-desktop\config.json`. |
| **Tampering with local config** | The uninstaller offers a clean wipe option for `%APPDATA%\sugarota-desktop` to ensure no lingering credentials remain upon removal. |
| **Memory / supply-chain vulnerabilities** | Zero third-party runtime package dependencies (npm, electron, chromium); compiled with MSVC `/MT` using trusted native Windows APIs (`user32`, `d2d1`, `dwrite`, `winhttp`). |
| **Multi-instance hijacking** | Single-instance execution enforced via a named Win32 kernel Mutex (`SugarotaDesktopNativeMutex`). |

---

## Reporting a Vulnerability

If you discover a security vulnerability or security-related issue in Sugarota Desktop, please do not open a public issue on GitHub. Instead, report it privately to the maintainers:

- **Email:** Report via GitHub Security Advisories or maintainer contact on GitHub.
- **Details to include:**
  - Description of the issue and potential impact
  - Steps to reproduce or proof-of-concept
  - Operating system and version of Sugarota Desktop

We take security vulnerabilities seriously and will investigate and address verified issues promptly.
