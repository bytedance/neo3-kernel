#!/bin/sh -eu
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022, Pico Immersive Pte. Ltd ("Pico"). All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */


tmp_file=$(mktemp)
trap "rm -f $tmp_file.o $tmp_file $tmp_file.bin" EXIT

cat << "END" | "$CC" -c -x c - -o $tmp_file.o >/dev/null 2>&1
void *p = &p;
END
"$LD" $tmp_file.o -shared -Bsymbolic --pack-dyn-relocs=relr -o $tmp_file

# Despite printing an error message, GNU nm still exits with exit code 0 if it
# sees a relr section. So we need to check that nothing is printed to stderr.
test -z "$("$NM" $tmp_file 2>&1 >/dev/null)"

"$OBJCOPY" -O binary $tmp_file $tmp_file.bin
