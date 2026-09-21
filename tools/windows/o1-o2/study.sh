#!/bin/sh
# The O1/O2 study, under Git bash on the Windows box after vcvars64.
#
# Every cl option is spelt with a dash: MSYS rewrites a leading slash into a
# path. `link` is named by its full path: Git's own /usr/bin/link shadows it.
# cl6x needs its include directory named; it has no default search list.
#
# Ran on 2026-09-21 as three parts while the three traps above were found;
# this is the three joined, and the results in the docs are theirs.
set -u
W=/c/o12study
R=$W/results
CPP="$(cygpath -u 'C:\Users\GRA\Documents\Compiler++')"
TI='/c/ti/ccsv7/tools/compiler/ti-cgt-c6000_8.2.2'
export PATH="$TI/bin:$PATH"
LINK="$(cygpath -u "$VCToolsInstallDir")bin/Hostx64/x64/link.exe"
rm -rf "$R"; mkdir -p "$R"
cd "$W"

echo "=== cl banner"        | tee "$R/log.txt"
cl 2>&1 | head -2           | tee -a "$R/log.txt"
echo "=== cl -help, the optimization lines" | tee -a "$R/log.txt"
cl -help 2>&1 | tr -d '\r' > "$R/cl-help.txt"
grep -nE '^/O|^/G[Fy]|favor|OPTIMIZATION' "$R/cl-help.txt" | tee -a "$R/log.txt"

echo "=== cl6x help, the optimization lines" | tee -a "$R/log.txt"
cl6x --help 2>&1 | tr -d '\r' > "$R/cl6x-help.txt"
grep -nEi 'opt_level|opt_for|-ms|-mf' "$R/cl6x-help.txt" | tee -a "$R/log.txt"

# ---- the sixteen Compiler++ units under each spelling -------------------
# name|flags; each compiled with -FA so the listings can be compared, and
# linked so the .text of the program can be read off the exe.
CONFIGS='Od|-Od
O1|-O1
O1x|-Og -Os -Oy -Ob2 -GF -Gy
O2|-O2
O2x|-Og -Oi -Ot -Oy -Ob2 -GF -Gy
Ox|-Ox
O1Oi|-O1 -Oi
O1OiOt|-O1 -Oi -Ot
O2Os|-O2 -Os
O2Ob1|-O2 -Ob1'
echo "=== the sixteen units: .text of the linked exe, and the listings" | tee -a "$R/log.txt"
printf '%s\n' "$CONFIGS" | while IFS='|' read -r name flags; do
    d="$W/b-$name"; rm -rf "$d"; mkdir -p "$d"
    wd="$(cygpath -w "$d")"
    ( cd "$CPP" && cl -nologo -EHsc -W3 -c $flags -FA -Fo"$wd\\" -Fa"$wd\\" Compiler++/*.cpp > "$d/cl.out" 2>&1 ); st=$?
    ( cd "$d" && "$LINK" -nologo -out:compilerpp.exe *.obj > link.out 2>&1 ); ls=$?
    hx=$(dumpbin -headers "$d/compilerpp.exe" 2>/dev/null | tr -d '\r' | grep -A1 '\.text name' | grep 'virtual size' | awk '{print $1}')
    text=$((16#${hx:-0}))
    exe=$(wc -c < "$d/compilerpp.exe" 2>/dev/null || echo 0)
    cat "$d"/*.asm | tr -d '\r' | grep -v '^; Listing\|^; File\|^TITLE' > "$d/all.asm"
    h=$(md5sum "$d/all.asm" | cut -c1-8)
    printf '%-8s cl=%d link=%d .text=%d exe=%s asm-lines=%s asm-md5=%s  [%s]\n' "$name" $st $ls "$text" "$exe" "$(wc -l < "$d/all.asm")" "$h" "$flags" | tee -a "$R/log.txt"
done

echo "=== which spellings are the same program (listing-identical)" | tee -a "$R/log.txt"
for pair in "O1 O1x" "O2 O2x" "O2 Ox" "O2 O1OiOt" "O1Oi O2Os" "O1 O1Oi" "O2 O2Ob1"; do
    set -- $pair
    if cmp -s "$W/b-$1/all.asm" "$W/b-$2/all.asm"; then r=identical; else r="differ ($(diff "$W/b-$1/all.asm" "$W/b-$2/all.asm" | grep -c '^[<>]') lines)"; fi
    printf '  %-6s vs %-7s %s\n' "$1" "$2" "$r" | tee -a "$R/log.txt"
done
diff "$W/b-O1/all.asm" "$W/b-O2/all.asm" > "$R/O1-vs-O2.diff"
diff "$W/b-O1/all.asm" "$W/b-O1Oi/all.asm" > "$R/O1-vs-O1Oi.diff"

# ---- run time: the VM-bound benchmark, best of ten -----------------------
# bench.cpp runs past compilerpp's 50-million-step limit on purpose: every
# build then does the same 50 million steps, whatever the program's answer.
echo "=== run time of compilerpp.exe on bench.cpp, best of 10 (ms)" | tee -a "$R/log.txt"
for name in Od O1 O2 O1Oi O2Os Ox O2Ob1; do
    best=999999; i=0
    while [ $i -lt 10 ]; do
        s=$(date +%s%N); "$W/b-$name/compilerpp.exe" -run -q "$W/bench.cpp" > "$W/b-$name/bench.out" 2>&1; e=$(date +%s%N)
        ms=$(( (e - s) / 1000000 )); [ $ms -lt $best ] && best=$ms
        i=$((i+1))
    done
    printf '  %-6s %6d ms   %s\n' "$name" "$best" "$(tr -d '\r' < "$W/b-$name/bench.out" | head -1)" | tee -a "$R/log.txt"
done

# ---- the pattern probe under cl ------------------------------------------
echo "=== probe.c under cl: .text bytes per config" | tee -a "$R/log.txt"
RW="$(cygpath -w "$R")"
for cfg in "Od|-Od" "O1|-O1" "O2|-O2" "O1Oi|-O1 -Oi" "O2Os|-O2 -Os" "Os|-Os" "Ot|-Ot" "O2Oi-|-O2 -Oi-"; do
    name=${cfg%%|*}; flags=${cfg#*|}
    cl -nologo -c $flags -FA -Fo"$RW\\probe-cl-$name.obj" -Fa"$RW\\probe-cl-$name.asm" probe.c > "$R/probe-cl-$name.out" 2>&1
    tr -d '\r' < "$R/probe-cl-$name.asm" > "$R/t" && mv "$R/t" "$R/probe-cl-$name.asm"
    sz=0; for hx in $(dumpbin -headers "$R/probe-cl-$name.obj" | tr -d '\r' | grep -A4 '\.text' | grep 'size of raw data' | awk '{print $1}'); do sz=$((sz + 16#$hx)); done
    printf '  %-6s .text=%s  npad=%s  [%s]\n' "$name" "$sz" "$(grep -c 'npad' "$R/probe-cl-$name.asm")" "$flags" | tee -a "$R/log.txt"
done
diff "$R/probe-cl-O1.asm" "$R/probe-cl-O2.asm" > "$R/probe-cl-O1-vs-O2.diff"

# ---- the pattern probe under cl6x ----------------------------------------
echo "=== probe.c under cl6x: the .text line of ofd6x per config" | tee -a "$R/log.txt"
mkdir -p "$W/ti"; cp probe.c "$W/ti/"
cd "$W/ti"
for cfg in "none|" "O0|-O0" "O1|-O1" "O2|-O2" "O3|-O3" "O2ms0|-O2 -ms0" "O2ms1|-O2 -ms1" "O2ms2|-O2 -ms2" "O2ms3|-O2 -ms3" "O3ms3|-O3 -ms3" "O2mf5|-O2 -mf5" "O2mf0|-O2 -mf0" "O3mf5|-O3 -mf5" "O0ms3|-O0 -ms3" "O0mf5|-O0 -mf5"; do
    name=${cfg%%|*}; flags=${cfg#*|}
    rm -f probe.obj probe.asm
    cl6x -mv6740 --abi=eabi -I "$(cygpath -w "$TI/include")" $flags -k -c probe.c > "cl6x-$name.out" 2>&1; st=$?
    ofd6x probe.obj 2>/dev/null | tr -d '\r' > "$R/ofd-ti-$name.txt"
    cp probe.asm "$R/probe-ti-$name.asm" 2>/dev/null
    printf '  %-6s st=%d %s [%s]\n' "$name" $st "$(grep -E '^ *[0-9]+ +\.text ' "$R/ofd-ti-$name.txt" | tr -s ' ')" "$flags" | tee -a "$R/log.txt"
done
cp "$W/bench.cpp" "$W/probe.c" "$R/"
cd "$W" && tar cf results.tar results
echo "=== done" | tee -a "$R/log.txt"
