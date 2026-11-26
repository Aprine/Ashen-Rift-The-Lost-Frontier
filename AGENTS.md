# Project instructions

## Scope

- This directory is the Unreal Engine project and the GitHub repository root.
- The canonical repository is `https://github.com/Aprine/Ashen-Rift-The-Lost-Frontier`; its default branch is `main`.
- Keep the project on Unreal Engine 4.27.2 with C++ gameplay systems and Blueprint subclasses unless the user requests a migration.
- Preserve source maps and binary assets. Rename or move Unreal assets through the editor rather than the filesystem.

## Version updates

Every completed project update must include its documentation and be synchronized to the canonical GitHub repository:

1. Update `README.md` and `README.zh-CN.md` with the current version, implemented features, launch instructions and remaining limitations.
   - The two README files must be complete, paragraph-by-paragraph translations with the same section order, paragraphs, lists, tables, code examples, screenshots and substantive links. Translate prose, captions and code comments; preserve executable commands, paths, identifiers and technical values. Keep language-switch links and translated heading anchors functional. Do not add content to only one language version.
2. Add a dated version entry to `CHANGELOG.md`, newest first. Record actual additions, changes, fixes and relevant validation; do not invent completed work or test results.
3. Keep the in-game version label and applicable development notes consistent with the release being prepared.
4. Include these documentation changes in the same commit as the version update. Check relative documentation and screenshot links before publishing.
5. After validation, push the update's source, assets and documentation to `origin/main`. Verify that GitHub points to the intended commit and that the changelog and updated content are present. Limit the push to the authorized project changes.

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
