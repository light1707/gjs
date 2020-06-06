#!/bin/sh

if test "$GJS_USE_UNINSTALLED_FILES" = "1"; then
    gjs="$TOP_BUILDDIR/gjs-console"
else
    gjs=gjs-console
fi

echo 1..1

JS_SCRIPT="$1"
EXPECTED_OUTPUT="$1.output"
THE_DIFF=$("$gjs" -m "$JS_SCRIPT" \
    | diff -u "$EXPECTED_OUTPUT" -)
if test $? -ne 0; then
    echo "not ok 1 - $1  # command failed"
    exit 1
fi

if test -n "$THE_DIFF"; then
    echo "not ok 1 - $1"
    echo "$THE_DIFF" | while read line; do echo "#$line"; done
else
    echo "ok 1 - $1"
fi
