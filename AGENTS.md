# Repository workflow

`C:\StuntRaceFXWE` is the canonical working repository for all future Stunt Race
FX Enhanced Windows development. Finished versions must be created here directly
rather than assembled elsewhere and moved in afterward.

## New-version layout

Every finished iteration must use this layout:

```text
source/<NN-version-name>/
  build/
  source/
releases/windows/<NN-version-name>-windows.zip
```

- Increment `NN` chronologically and use a concise descriptive suffix.
- `build/` contains the runnable ROM-free Windows distribution, launchers,
  runtime dependencies, README, and licenses.
- The nested `source/` contains the expanded source corresponding exactly to
  that build.
- Never place `source-code.zip` inside `build/`.
- The Windows release ZIP contains only the contents of `build/`.
- Update repository documentation for every finished iteration.
- Keep earlier numbered iterations unchanged as historical snapshots.

## Safety and repository hygiene

- Never add ROMs, save RAM, save states, replays, captures, or ROM-derived
  exports.
- Conduct generated capture and test work outside this repository.
- Verify the expanded version and release ZIP before staging changes.
- Confirm the packaged executable matches the tested executable.
- Stage completed updates for review, but do not commit, rewrite history,
  force-push, or publish unless the user explicitly requests it.
- Keep the configured `origin` remote and `main` branch intact.

