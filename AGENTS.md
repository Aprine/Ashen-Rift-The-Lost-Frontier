# Project instructions

## Scope

- This directory is the Unreal Engine project and the GitHub repository root.
- The canonical repository is `https://github.com/Aprine/Ashen-Rift-The-Lost-Frontier`; its default branch is `main`.
- Keep the project on Unreal Engine 4.27.2 with C++ gameplay systems and Blueprint subclasses unless the user requests a migration.
- Preserve source maps and binary assets. Rename or move Unreal assets through the editor rather than the filesystem.

## Version updates

The user requires documentation to be maintained with every version update:

1. Update `README.md` and `README.zh-CN.md` with the current version, implemented features, launch instructions and remaining limitations.
2. Add a dated version entry to `CHANGELOG.md`, newest first. Record actual additions, changes, fixes and relevant validation; do not invent completed work or test results.
3. Keep the in-game version label and applicable development notes consistent with the release being prepared.
4. Include these documentation changes in the same commit as the version update. Check relative documentation and screenshot links before publishing.
5. When GitHub synchronization is part of the user's task, push the corresponding source and documentation together and verify the remote commit. Do not infer permission to publish unrelated changes.

## Repository contents

- Include `Source`, `Config`, `Content`, `Scripts`, `Tools`, `Data`, `Docs`, the `.uproject` descriptor and repository documentation.
- Exclude generated build output, caches, player saves, local IDE files, installers and source backup archives.
- The proposal, teacher feedback, course PDFs, interim report and temporary authoring files in the parent directory are outside this repository's scope.
- Store README screenshots under `Docs/Images`; `Saved` is ignored.
- Never commit credentials or local environment secrets.

## Validation

- Use the existing build and test scripts when relevant to gameplay changes. Documentation-only changes need link and content checks, not a full game rebuild.
- Distinguish recorded validation from checks run in the current task, and distinguish editor play from packaged Windows acceptance.
- Do not claim random maze topology, final art, animation-driven hit timing or validated packaging while those features remain unfinished.
