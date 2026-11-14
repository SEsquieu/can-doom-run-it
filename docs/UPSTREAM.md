# Upstream provenance and maintenance

The baseline is official ZDoom/gzdoom tag **g4.14.2**, commit
`99aa489d09015a95bb78df2b30ede29f328cc874` (2025-05-01).
It was the latest stable release tag available during bootstrap on 2026-11-14;
`master` was ahead of it. Development snapshots are not implicit upgrades.

Before import, origin/main contained one commit, `3900c73359fb119206e96ffb7a806f79eb8e510a`,
with a one-line README. The import uses `git merge --allow-unrelated-histories`
and has both that commit and the official release as parents. No history was
replaced, squashed, or force-pushed. The README add/add conflict was resolved in
favor of the project identity; the upstream README is retained verbatim in
`docs/upstream/README.md`. Upstream LICENSE, notices, assets, and per-file
copyrights remain intact. See `docs/licenses/` and `docs/ASSETS.md`.

## Dependent Phase 2 development

PR #1 (`bootstrap/gzdoom-foundation`) contains the import. While it is open and
main remains README-only, Phase 2 uses `feature/doom-io-runtime`, based directly
on that foundation, with PR #2 targeting the bootstrap branch. Do not start engine
work from the disconnected main tree. Merge the foundation preserving its merge
parents first, then retarget the dependent PR to main. No rebase, squash or force
push is needed. After integration, use the normal upstream merge procedure below.

## Configure a new clone

```sh
git remote add upstream https://github.com/ZDoom/gzdoom.git
git fetch upstream --tags
```

`origin` is SEsquieu/can-doom-run-it. Git remotes are local configuration and do
not travel with clones. Keep the working branch tracking origin, not upstream.

## Update procedure

1. Start a clean branch from the project main branch. Fetch upstream tags.
2. Review release notes, license changes, build dependencies, save/demo/network
   compatibility, and the diff from the recorded baseline to the chosen tag.
3. `git merge --no-ff <reviewed-release-tag>`; resolve upstream conflicts while
   preserving native I/O boundaries. Never squash or rebase shared upstream ancestry.
4. Update this baseline, dependency pins, and upstream README if applicable.
5. Build Linux and Windows, run native bus tests and the real engine event smoke
   test. Check ordinary gameplay, first launch, and shutdown with unavailable adapters.
6. Review a PR before merging. Record any intentional upstream divergence.

Use individual upstream bug-fix cherry-picks only with `-x` attribution and an
explanation. Keep downstream code in `src/cdr/` and small, clearly named hooks
in engine code. Do not push imported upstream tags en masse to origin.
