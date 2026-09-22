#!/bin/sh
# The comparison proper, under Git bash after vcvars: cxx1-msvc.exe at each
# level builds Compiler++ through the project's assembler and link.exe.
set -u
ROOT="$(cygpath -u "$1")"
CXX1="$ROOT/cxx1-msvc.exe"
CPP="$(cygpath -u 'C:\Users\GRA\Documents\Compiler++')"
W=/c/o12study/cx; R=/c/o12study/results-cx
LINK="$(cygpath -u "$VCToolsInstallDir")bin/Hostx64/x64/link.exe"
rm -rf "$W" "$R"; mkdir -p "$W" "$R"
text_of() { hx=$(dumpbin -headers "$1" 2>/dev/null | tr -d '\r' | grep -A1 '\.text name' | grep 'virtual size' | awk '{print $1}'); echo $((16#${hx:-0})); }

echo "=== cxx1: $("$CXX1" -version 2>&1 | tr -d '\r' | head -2 | tail -1)" | tee "$R/log.txt"
for L in 0 1 2; do
    d="$W/O$L"; mkdir -p "$d"
    s=$(date +%s%N)
    fails=0
    for u in "$CPP"/Compiler++/*.cpp; do
        n=$(basename "$u" .cpp)
        "$CXX1" -nologo -O$L -masm=masm -c "$(cygpath -w "$u")" -o "$(cygpath -w "$d/$n.obj")" > "$d/$n.build" 2>&1 || { fails=$((fails+1)); echo "  O$L: $n did not compile: $(tail -2 "$d/$n.build")"; }
    done
    e=$(date +%s%N)
    # The runtime cxx1's own link step names, so the program is linked as cxx1 links it.
    ( cd "$d" && "$LINK" -nologo -subsystem:console -stack:8388608 -out:compilerpp.exe *.obj libcmt.lib libucrt.lib libvcruntime.lib kernel32.lib legacy_stdio_definitions.lib > link.out 2>&1 ) || echo "  O$L: link failed: $(tail -3 "$d/link.out")"
    printf 'cxx1 -O%d  compile %5d ms  units-failed=%d  .text=%d\n' $L $(( (e-s)/1000000 )) $fails "$(text_of "$d/compilerpp.exe")" | tee -a "$R/log.txt"
done
for L in Od O1 O2; do
    printf 'cl   /%s  .text=%d\n' $L "$(text_of /c/o12study/b-$L/compilerpp.exe)" | tee -a "$R/log.txt"
done

echo "=== run time, bench.cpp to the 50-million-step limit, best of 10 (ms)" | tee -a "$R/log.txt"
B="$(cygpath -w "$ROOT/tools/windows/o1-o2/bench.cpp")"
for x in "cxx1 -O0|$W/O0" "cxx1 -O1|$W/O1" "cxx1 -O2|$W/O2" "cl /Od|/c/o12study/b-Od" "cl /O1|/c/o12study/b-O1" "cl /O2|/c/o12study/b-O2"; do
    name=${x%%|*}; d=${x#*|}
    [ -x "$d/compilerpp.exe" ] || { echo "  $name: no program" | tee -a "$R/log.txt"; continue; }
    best=999999; i=0
    while [ $i -lt 10 ]; do
        s=$(date +%s%N); "$d/compilerpp.exe" -run -q "$B" > "$d/bench.out" 2>&1; e=$(date +%s%N)
        ms=$(( (e-s)/1000000 )); [ $ms -lt $best ] && best=$ms; i=$((i+1))
    done
    printf '  %-9s %6d ms\n' "$name" $best | tee -a "$R/log.txt"
done

# ---- the loop kernels, natively: what -O2's unrolling is worth --------------
echo "=== loops.cpp, each kernel's ms as the program reports it, best of 3" | tee -a "$R/log.txt"
LP="$(cygpath -w "$ROOT/tools/windows/o1-o2/loops.cpp")"
mkdir -p "$W/loops"; cd "$W/loops"
# Compiled and assembled by cxx1, linked here: under Git bash, cxx1's own
# `link.exe` would find coreutils' link on PATH first.
for L in 1 2; do
    "$CXX1" -nologo -O$L -masm=masm -c "$LP" -o "$(cygpath -w "$W/loops/cx-O$L.obj")" > cx-O$L.build 2>&1 || echo "  cxx1 -O$L did not build loops: $(tail -2 cx-O$L.build)"
    "$LINK" -nologo -subsystem:console -stack:8388608 -out:cx-O$L.exe cx-O$L.obj libcmt.lib libucrt.lib libvcruntime.lib kernel32.lib legacy_stdio_definitions.lib > cx-O$L.link 2>&1 || echo "  cxx1 -O$L loops did not link: $(tail -2 cx-O$L.link)"
done
cl -nologo -O1 -Fe:cl-O1.exe "$LP" > cl-O1.build 2>&1 || echo "  cl /O1 did not build loops"
cl -nologo -O2 -Fe:cl-O2.exe "$LP" > cl-O2.build 2>&1 || echo "  cl /O2 did not build loops"
for x in cx-O1 cx-O2 cl-O1 cl-O2; do
    [ -x "$x.exe" ] || continue
    for i in 1 2 3; do ./$x.exe | tr -d '\r' > "$x.run$i"; done
    for k in sum fill while dot; do
        best=$(grep "^$k " $x.run1 $x.run2 $x.run3 | awk '{print $2}' | sort -n | head -1)
        printf '  %-6s %-6s %6s ms  %s\n' "$x" "$k" "$best" "$(grep "^$k " $x.run1 | sed 's/.*checksum/checksum/')" | tee -a "$R/log.txt"
    done
done

echo "=== every runnable Compiler++ case: cxx1's program against cl /O2's, output and exit code" | tee -a "$R/log.txt"
cd "$CPP/tests"
for L in 0 1 2; do
    same=0; diff_=0
    for c in cases/*run_*.cpp; do
        n=$(basename "$c" .cpp); in=/dev/null; [ -f "input/$n.txt" ] && in="input/$n.txt"
        /c/o12study/b-O2/compilerpp.exe -run -q "$c" < "$in" > "$R/ref.out" 2>&1; r1=$?
        "$W/O$L/compilerpp.exe" -run -q "$c" < "$in" > "$R/got.out" 2>&1; r2=$?
        if [ $r1 -eq $r2 ] && cmp -s "$R/ref.out" "$R/got.out"; then same=$((same+1)); else diff_=$((diff_+1)); echo "  cxx1 -O$L differs on $n (exit $r2 vs $r1)" | tee -a "$R/log.txt"; fi
    done
    printf 'cxx1 -O%d: %d identical, %d differ\n' $L $same $diff_ | tee -a "$R/log.txt"
done
echo "=== done" | tee -a "$R/log.txt"
