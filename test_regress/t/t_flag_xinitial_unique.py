#!/usr/bin/env python3
# DESCRIPTION: Verilator: Verilog Test driver/expect definition
#
# Copyright 2024 by Wilson Snyder. This program is free software; you
# can redistribute it and/or modify it under the terms of either the GNU
# Lesser General Public License Version 3 or the Perl Artistic License
# Version 2.0.
# SPDX-License-Identifier: LGPL-3.0-only OR Artistic-2.0

import vltest_bootstrap

test.scenarios("vlt_all")

test.compile(verilator_flags2=["--x-initial unique"])

test.execute()

files = glob.glob(test.obj_dir + "/" + test.vm_prefix + "___024root__DepSet_*__Slow.cpp")
test.file_grep_any(files, r"VL_SCOPED_RAND_RESET")

test.passes()
