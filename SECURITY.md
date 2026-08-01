# Security Policy

## Supported versions

Until the first stable release, security fixes are applied to the current
0.6 development line only.

## Reporting a vulnerability

Please use GitHub's private vulnerability-reporting feature for the public
`squared-pg` repository. Do not open a public issue containing exploit details,
private paths, credentials, signing material, or unpublished package contents.

The generator validates local archives and operates offline by default, but
generated application scripts are a capability boundary rather than a
hardened hostile-code sandbox. Reports should state whether they concern the
generator process, `.sq` archive handling, generated build files, or the
application scripting runtime.
