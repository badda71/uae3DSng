Lhasa - free LHA/LZH archive library, vendored for the WHDLoad installer (see ../whdload3ds.cpp).

  Upstream : https://github.com/fragglet/lhasa  (commit 75ed835, "Update NEWS, bump version number", June 2025)
  Licence  : ISC (COPYING.md in this directory), (c) 2011-2025 Simon Howard. Compatible with the GPL of uae4all2.

Content of this directory = upstream lib/ (+ lib/public/), unmodified, except:
  * lha_arch_unix.c / lha_arch_win32.c are NOT copied; lha_arch_3ds.c (ours) replaces them. The 3DS has no chown/chmod/
    utime/symlink, and nothing here needs them (files are written by whdload3ds.cpp, not by lha_reader_extract()).
  * lh_new_decoder.c, bit_stream_reader.c, tree_decode.c and pma_common.c are only #included by other files. They are
    listed in EXCLUDE_FILES in the top-level Makefile so that they are not compiled a second time on their own.

Known, harmless: UBSan (tools/whdload_test/run.sh builds with it) reports one signed left shift in the MS-DOS time stamp
decoder (lha_file_header.c). The result is masked afterwards and ARM gcc simply shifts; the file is left unmodified.
