# Publishing this checkpoint to GitHub

## Use the public source ZIP

Extract `StarSuccessor_v20_GitHub.zip`. Use Git or GitHub Desktop to publish the **contents** of its `StarSuccessor-v20` directory as the repository root. Uploading the ZIP itself does not create an extracted source tree, and browser uploads can impose file-count limits. It includes the README, source, build helpers, license files and issue template. The suggested release/tag name is `v0.1.0-v20`, marked as a pre-release.

**Do not upload the private milestone backup**, any previous Windows test ZIP, the game EXE, ISO/RVZ, extracted DATA tree, translated game functions, generated data initializers, objects, caches or saves. The current executable incorporates game code/data; selecting an ISO at runtime does not make that executable data-free.

The public package intentionally omits the game-derived DSP boot header; the build helper regenerates it from the owner's local data. Address maps, input digests and runtime source are included.

## Suggested initial push

```bash
git init
git add .
git commit -m "Preserve v20 first-stage milestone"
git branch -M main
git remote add origin https://github.com/YOUR_USERNAME/YOUR_REPOSITORY.git
git push -u origin main
git tag -a v0.1.0-v20 -m "Experimental v20 first-stage milestone"
git push origin v0.1.0-v20
```

Replace the URL with your own repository. These commands have not been run on your account and nothing has been published automatically.

## Review before pushing

- Run `python3 scripts/check_public_tree.py` and review `git status`.
- Keep audio-incomplete, first-stage-only and unsupported-Intel notices visible.
- Do not present the reference PC as a minimum specification or claim stable 60 FPS.
- Keep all upstream and third-party license notices.
- Check logs/screenshots manually if you add any; do not expose personal paths or private data.
- Start subsequent fixes on a separate development branch so this milestone remains identifiable.

The checker scans the filesystem, including ignored outputs. Run it on a clean source export, not a populated private build workspace; do not delete saves or original data merely to make it pass.

The checker is an omission guard, not a complete copyright/security audit. If a game file has been committed accidentally, adding `.gitignore` does not remove it from repository history.
