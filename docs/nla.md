# Network Level Authentication

xrdp can require CredSSP Network Level Authentication (NLA) before starting
the RDP session. CredSSP runs over TLS and uses SPNEGO to authenticate the
client before the graphical login is created.

NLA is disabled by default at runtime. Set `security_layer=nla` to require it,
or set `enable_nla=true` with `security_layer=negotiate` to offer NLA and TLS.

## Requirements

NLA adds a build dependency on a GSSAPI implementation with `krb5-config`,
headers and libraries. Install the development package for the build host:

```sh
# Debian and Ubuntu
sudo apt install libkrb5-dev

# Fedora, RHEL and compatible distributions
sudo dnf install krb5-devel
```

The usual xrdp OpenSSL development dependency is also required. Kerberos
administration and troubleshooting tools are commonly packaged as `krb5-user`
on Debian/Ubuntu and `krb5-workstation` on Fedora/RHEL.

The default build mode is `auto`: NLA is built when GSSAPI is found and omitted
otherwise. Use `--enable-nla` to require the feature and fail configuration if
the dependency is missing:

```sh
./bootstrap                 # required for a Git checkout
./configure --enable-nla
make
sudo make install
```

Use `--disable-nla` to omit the feature. In a configured build tree, the
following command confirms that support was enabled:

```sh
grep '^#define XRDP_NLA 1' config_ac.h
```

## Configure the server

### TLS certificate

NLA requires the certificate and private key configured in `xrdp.ini` to be
readable by the xrdp daemon. Clients should trust the certificate and its name
should match the DNS name used to connect.

### GSSAPI acceptor

For Kerberos, provide a keytab containing an acceptor principal for every DNS
name clients use to reach the server. For example:

```text
TERMSRV/rdp.example.com@EXAMPLE.COM
```

The KDC-specific procedure for creating the principal and exporting its key is
outside xrdp. DNS resolution, the Kerberos realm configuration and system time
must also be correct on the server and client.

GSSAPI uses its default keytab unless `KRB5_KTNAME` is set. A dedicated keytab
can be selected through the xrdp service environment. For packaged systemd
units, `/etc/default/xrdp` or `/etc/sysconfig/xrdp` can contain:

```sh
KRB5_KTNAME=FILE:/etc/xrdp/krb5.keytab
```

The keytab must be readable after xrdp changes to the configured `runtime_user`
and `runtime_group`. Verify its contents without printing key material:

```sh
KRB5_KTNAME=FILE:/etc/xrdp/krb5.keytab klist -k
```

xrdp uses the mechanisms exposed by the system GSSAPI. Kerberos is available
with the Kerberos GSSAPI implementation. NTLM fallback is available only when
an external mechanism such as `gss-ntlmssp` and its credential provider are
installed and configured; xrdp does not include an NTLM implementation.

### xrdp.ini

To require NLA, configure the global security layer and TLS files:

```ini
[Globals]
security_layer=nla
certificate=/etc/xrdp/cert.pem
key_file=/etc/xrdp/key.pem
```

To prefer NLA while retaining TLS compatibility for clients which do not offer
it, use:

```ini
[Globals]
security_layer=negotiate
enable_nla=true
certificate=/etc/xrdp/cert.pem
key_file=/etc/xrdp/key.pem
```

`enable_nla` defaults to `false` and only affects `security_layer=negotiate`.
If the client selects NLA, an authentication failure ends that connection; it
does not retry with TLS. Restart xrdp after changing the service environment or
`xrdp.ini`. With `security_layer=nla`, a client which does not offer NLA is
rejected before an RDP session is created.

NLA supplies the delegated username, domain and password to the normal xrdp
login flow. The session manager still applies its configured PAM and account
policy, so a successful CredSSP exchange does not bypass Linux session
authentication.

## CredSSP compatibility

This implementation:

- sends CredSSP version 6 and requires clients to send version 5 or later;
- accepts a client version value greater than 6 if it remains unchanged during
  the exchange;
- requires the version 5 SHA-256 public-key binding with a 32-byte client
  nonce;
- requires confidentiality and integrity from the negotiated GSSAPI context;
- accepts only `TSPasswordCreds` delegated credentials; and
- uses RDP `PROTOCOL_HYBRID` rather than `PROTOCOL_HYBRID_EX`.

Versions 2 through 4 are rejected intentionally. They use the older public-key
binding which the current CredSSP specification recommends replacing with
version 5 or later.

CredSSP delegates the user's password to the server inside the protected
exchange. NLA authenticates the server and protects that delegation in
transit; it does not keep the password out of the server process. Use a trusted
TLS certificate, protect the keytab and private key, and restrict access to the
xrdp host accordingly.

## Test with FreeRDP

Use the same DNS name present in the TLS certificate and `TERMSRV` principal.
The following performs the authentication without keeping the desktop open:

```sh
xfreerdp /v:rdp.example.com /u:alice@EXAMPLE.COM /sec:nla +auth-only \
  /from-stdin:force
```

`/from-stdin:force` prompts for credentials before connecting. Do not put
`/p:<password>` on the command line, where it can be exposed through shell
history or the process list. Depending on the distribution, the executable may
be named `xfreerdp3`.

To ensure the test uses Kerberos rather than falling back to NTLM, first check
that FreeRDP was built with Kerberos support, acquire a ticket, and filter the
authentication packages:

```sh
xfreerdp /buildconfig | grep 'WITH_KRB5=ON'
kinit alice@EXAMPLE.COM
xfreerdp /v:rdp.example.com /u:alice@EXAMPLE.COM /sec:nla +auth-only \
  /auth-pkg-list:none,kerberos
```

The `+auth-only` form verifies CredSSP and then disconnects. Remove that option
for an end-to-end desktop test. A ticket-only client may complete CredSSP
without delegating a usable password; the later PAM login will then fail, as
credential-less session logon is not supported.

The NTLM path was tested end-to-end with Debian 12, `gss-ntlmssp` 1.2.0 and
FreeRDP 2.11.7, both with required NLA and with `security_layer=negotiate` plus
`enable_nla=true`. A valid password completed CredSSP and delegated password
decoding; an invalid password returned `STATUS_LOGON_FAILURE` without falling
back to TLS. With `enable_nla=false`, the same negotiation selected TLS and did
not invoke GSSAPI. The `+auth-only` test does not create a graphical desktop or
exercise the later PAM session login.

Use a CA-trusted certificate for normal tests. For a disposable lab server
with a self-signed certificate, `/cert:ignore` can be added temporarily; it
must not be used as a production configuration.

## Test with Windows MSTSC

Test from a Windows client which can resolve the server name and reach the
domain controller:

```text
mstsc.exe /v:rdp.example.com /prompt
```

Enter the account as `EXAMPLE\alice` or `alice@example.com`. Connect by DNS
name, not IP address, so Windows requests the expected `TERMSRV` service
principal and validates the certificate name. A successful test proceeds to
the selected desktop without displaying the xrdp login screen a second time.

Follow the server log while testing:

```sh
journalctl -u xrdp -f
```

The successful path logs `Selected HYBRID security` followed by
`NLA authentication completed for <user>`. Authentication or keytab failures
are logged before the RDP session starts.

## Automated tests

The CredSSP parser, credential handling and Kerberos wrap-token conversion are
covered by the libxrdp test binary:

```sh
make -C tests/libxrdp check
```

These tests do not replace an interoperability test with FreeRDP or MSTSC and
a real GSSAPI acceptor.

## Not supported yet

- CredSSP versions 2, 3 and 4;
- CredSSP smart-card credentials (`TSSmartCardCreds`);
- Restricted Admin mode and credential-less logon;
- Remote Credential Guard (`TSRemoteGuardCreds`);
- `PROTOCOL_HYBRID_EX` and the Early User Authorization Result PDU;
- a built-in NTLM provider or xrdp-specific NTLM credential store; and
- xrdp.ini settings for the acceptor principal or keytab. Use the standard
  GSSAPI configuration and `KRB5_KTNAME` environment variable instead.

NTLM interoperability depends on the installed GSSAPI mechanism and still
needs broader client and distribution coverage beyond the tested combination
above. Kerberos remains the primary interoperability path for this initial
implementation.

## Protocol references

- [MS-CSSP: Credential Security Support Provider Protocol](https://learn.microsoft.com/openspecs/windows_protocols/ms-cssp/)
- [MS-RDPBCGR: Remote Desktop Protocol Basic Connectivity and Graphics Remoting](https://learn.microsoft.com/openspecs/windows_protocols/ms-rdpbcgr/)
