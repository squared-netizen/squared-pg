# kit.testcart

Fixture kit for the sqcart test suite. Not a real integration: it declares the
smallest shape that satisfies the Squared Cartridge Specification 1.0 for
`kind: "kit"`, so that the reader, writer, validator and CLI can be exercised
against something conforming rather than something merely accepted.

Payload follows the kit layout convention of format spec 6.3:

    bridge/     adapter source connecting Squared to the external system
    platform/   platform-specific integration
    build/      build-system fragments the generated project consumes

`SQ-INF/` carries only reserved names, so `sqcart verify` emits no
"unrecognised file" diagnostics.
