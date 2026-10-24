# Project Structure

## Lab2 merged workspace

- `code/`: merged runnable Lab2 kernel. Member A's physical-memory initialization and First-Fit, member B's Best-Fit/SLUB/GDB 3/4 probe, and member C's Buddy manager are integrated here.
- `lab/code/`: member B's standalone submission snapshot; retained as merge source and evidence.
- `lab/report/`: member B's report and screenshots.
- `report/report_lab2.md`: group Lab2 report with member C's Buddy and GDB 5/6 sections.
- `report/prompt_c.md`: member C's required implementation and test prompts.
- `report/evidence/member_c/`: reproducible output excerpts for Buddy and GDB 5/6.
- `report/images/member-c/`: English-named copies of member C's eight original screenshots, referenced by the merged report.
- `lab/report/images/`: member B's thirteen screenshots; all are referenced from the merged group report without duplicating the binary files.

## Build and run

Run commands from `code/` in WSL. The default physical-page manager is Best-Fit. Define `PMM_FIRST_FIT` through `EXTRA_CFLAGS` to select First-Fit.

- `make`: cross-compile the RISC-V kernel and raw image.
- `make qemu`: boot the image through OpenSBI; `-kernel` communicates the 0x80200000 next-stage entry to current QEMU versions.
- `make clean && make EXTRA_CFLAGS=-DPMM_FIRST_FIT`: build the merged kernel with member A's First-Fit manager.
- `make clean && make EXTRA_CFLAGS=-DPMM_BUDDY`: build the merged kernel with member C's Buddy manager.

The merged startup sequence is `kern_entry -> kern_init -> pmm_init -> mmu_probe -> slub_init -> slub_check`.

