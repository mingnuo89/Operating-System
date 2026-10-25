# Changelog

## 2026-10-25

- Consolidated the project into root `code/` and `report/`; removed the obsolete `lab/` snapshot.
- Moved the Docker environment and four verification/debugging tools into the integrated code directory.
- Moved all thirteen member B screenshots and their provenance index to `report/images/member-b/`, updating every report reference.
- Preserved all four complete member B prompts in `report/prompt_b.md` and in the group report.
- Retained the existing integrated-code annotations and restored member B student-ID comments without changing executable logic.
- Kept the original test hash baseline; the integrity checker recognizes only the exact existing QEMU loader-line adjustment in the grader.
- Kept the historical snapshot descriptions below as records of the earlier layout.

## 2026-10-24

- Started the member A + member B Lab2 merge into `code/`.
- Preserved `lab/code/` as the original member B snapshot.
- Documented the merged workspace and validation entry points.
- Merged member B's Best-Fit implementation into member A's physical-memory initialization base.
- Added member B's simplified SLUB allocator, black-box checks, and GDB 3/4 MMU probe.
- Kept member C's Buddy implementation outside the A+B merged `code/` target.
- Added allocator selection through `PMM_FIRST_FIT` and retained Best-Fit as the default.
- Corrected Best-Fit tail insertion so a newly inserted node is not traversed and inserted again.
- Updated QEMU launch arguments to use `-kernel`, allowing OpenSBI to receive the 0x80200000 next-stage address on QEMU 8.2.2.
- Updated the bundled grading script to use the same compatible QEMU kernel-loading path.
- Verified the default Best-Fit build with the injected official checks (`20/20`) and all SLUB checks.
- Verified the optional First-Fit build with the page-manager check, MMU probe, and all SLUB checks.
- Added member C's Buddy System manager and registered the `PMM_BUDDY` build-time selection.
- Added Buddy checks for power-of-two rounding, alignment, splitting, coalescing, unique allocation, accounting recovery, and allocation failure.
- Added member C's two required four-part Buddy prompts, validation evidence, and completed report sections for Buddy and GDB requirements 5/6.
- Recorded the real QEMU/OpenSBI loader compatibility diagnosis instead of reconstructing fictional AI iterations.
- Documented member C's assistance on GDB requirement 1: reproducible firmware hand-off, temporary identity mapping, and the fixed MMU probe preconditions.
- Reorganized `report/report_lab2.md` to match the Lab1 group-report format and merged member A, B, and C sections into one document.
- Inserted all seven member A screenshots, all thirteen member B screenshots, and all eight member C screenshots into the merged report.
- Copied member C's timestamp-named screenshots to `report/images/member-c/` with descriptive English filenames while preserving the originals.

