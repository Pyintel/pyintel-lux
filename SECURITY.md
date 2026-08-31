# Security Policy

## 🛡️ Supported Versions

We actively issue security updates for the following versions of Pyintel Lux:

| Version | Supported |
| ------- | ------------------ |
| 0.1.x   | :white_check_mark: |
| < 0.1.0 | :x:                |

---

## 🔒 Reporting a Vulnerability

The Pyintel team takes the security of our binary telemetry standard, MCU libraries, and host ingest proxy tools seriously.

If you believe you have discovered a security vulnerability in Pyintel Lux (such as frame parsing buffer overflows, CRC validation bypasses, or denial-of-service vulnerabilities in `luxd`), please report it privately:

### How to Report Privately
- **Email:** Send your report directly to **dev@pyintel.cc**.
- **Information to include:**
  - Description of the vulnerability and potential impact.
  - Affected component (`lux-emb`, `luxd`, wire frame parser, or transport layer).
  - Proof of Concept (PoC) code or steps to reproduce.

### Response Timeline
- **Acknowledgement:** We will acknowledge receipt of your security report within **48 hours**.
- **Assessment & Fix:** We aim to evaluate the issue and release a patch or security advisory within **14 days**.
- **Public Disclosure:** We request that you do not publicly disclose the vulnerability until we have released a fix and advisory.

Thank you for helping keep Pyintel Lux and the open-source hardware community secure!
