# Decode-Orc plugin registry

This directory hosts the **curated plugin index** — the list of third-party
stage plugins that Decode-Orc offers under *Plugin Manager → Browse Plugins…*
and `orc-cli plugins search / info / install`.

The host fetches [`index.yaml`](index.yaml) from the repository's default
branch over plain HTTPS. Because it reads the branch head, **a merged pull
request publishes immediately** — no Decode-Orc release and no registry tag are
required.

> The index currently lives in the Decode-Orc repository for convenience. It is
> expected to move to a dedicated `orc-plugin-registry` repository later; the
> schema and contribution model below are written so that move is transparent
> to hosts (the fetch URL is configurable via `ORC_PLUGIN_INDEX_URL`).

## Schema (`registry_schema: 1`)

```yaml
registry_schema: 1          # index schema major version (required)
plugins:
  - id: com.example.deinterlace          # unique plugin id (required)
    display_name: Example Deinterlacer    # human-readable name
    description: Motion-adaptive deinterlacing for PAL/NTSC
    maintainer: Example Author
    license_spdx: GPL-3.0-or-later        # SPDX identifier (required)
    source_repo_url: https://github.com/example/orc-plugin_deinterlace
    tags: [transform, video]
    artifacts:                            # at least one (required)
      - platform: linux                   # linux | macos | windows (required)
        host_abi: 8                       # target host ABI (required)
        url: https://github.com/example/orc-plugin_deinterlace/releases/download/v1.0.0/orc-plugin_deinterlace_linux_abi8.so
        sha256: <64-hex digest>           # mandatory
        plugin_version: 1.0.0
        min_host_app_version: 1.0.0       # optional
```

### Forward compatibility

- Hosts **ignore unknown fields**, so additions within schema major `1` are
  non-breaking. Older hosts tolerate a newer index and simply skip fields and
  entries they do not understand.
- An artifact is selected by **(platform, host ABI)**. Publish one artifact per
  platform and host ABI you support; an older host that finds no matching build
  reports "no build for this host" instead of downloading an incompatible one.
- Artifact filenames must follow the convention
  `orc-plugin_<stage>_<platform>[_abi<N>].<so|dylib|dll>`.

## Contributing

1. Fork and add (or update) your plugin's entry in `index.yaml`.
2. Ensure every artifact carries a reachable `url` and a correct `sha256`.
3. Open a pull request. The validation workflow
   (`.github/workflows/validate-plugin-index.yml`) checks: schema conformance,
   artifact naming (including the ABI token), that every `url` resolves, that
   each downloaded artifact's digest equals its `sha256`, and that a valid SPDX
   `license_spdx` is present.
4. **A maintainer's merge is the curation and trust decision.** There is no
   separate release step; the list goes live when the PR merges.

Only GPLv3-compatible licenses are accepted (GPLv3, GPLv2, LGPL, BSD, MIT,
Apache-2.0, ISC and similar).

## Validation locally

```bash
# Offline checks (schema, naming, license, digest presence):
python3 tools/validate_index.py index.yaml

# Online checks as well (URL reachability + digest match):
python3 tools/validate_index.py --online index.yaml
```
