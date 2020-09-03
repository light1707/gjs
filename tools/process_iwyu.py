# coding: utf8

# IWYU is missing the feature to designate a certain header as a "forward-decls
# header". In the case of SpiderMonkey, there are certain commonly used forward
# declarations that are all gathered in js/TypeDecls.h.
# We postprocess the IWYU output to fix this, and also fix the output format
# which is quite verbose, making it tedious to scroll through for 60 files.

import re
import sys


class Colors:
    NORMAL = '\33[0m'
    RED = '\33[31m'
    GREEN = '\33[32m'


ADD, REMOVE, FULL = range(3)
state = None
file = None
add = {}
remove = {}
all_includes = {}
there_were_errors = False

# When encountering one of these lines, move to a different state
MATCHERS = {
    r'\.\./(.*) should add these lines:': ADD,
    r'\.\./(.*) should remove these lines:': REMOVE,
    r'The full include-list for \.\./(.*):': FULL,
    r'\(\.\./(.*) has correct #includes/fwd-decls\)': None
}

FWD_HEADER = '#include <js/TypeDecls.h>'
FWD_DECLS_IN_HEADER = (
    'class JSAtom;',
    'struct JSContext;',
    'struct JSClass;',
    'class JSFunction;',
    'class JSFreeOp;',
    'class JSObject;',
    'struct JSRuntime;',
    'class JSScript;',
    'class JSString;',
    'namespace js { class TempAllocPolicy; }'
    'namespace JS { struct PropertyKey; }',
    'namespace JS { class Symbol; }',
    'namespace JS { class BigInt; }',
    'namespace JS { class Value; }',
    'namespace JS { class Compartment; }',
    'namespace JS { class Realm; }',
    'namespace JS { struct Runtime; }',
    'namespace JS { class Zone; }',
)
add_fwd_header = False

FALSE_POSITIVES = (
    # The bodies of these structs already come before their usage,
    # we don't need to have forward declarations of them as well
    ('gjs/atoms.h', 'class GjsAtoms;', ''),
    ('gjs/atoms.h', 'struct GjsSymbolAtom;', ''),

    # IWYU weird false positive when using std::vector::emplace_back() or
    # std::vector::push_back()
    ('gi/function.cpp', '#include <algorithm>', 'for max'),
    ('gi/private.cpp', '#include <algorithm>', 'for max'),
    ('gjs/importer.cpp', '#include <algorithm>', 'for max'),
    ('modules/cairo-context.cpp', '#include <algorithm>', 'for max'),

    # Weird false positive on some versions of IWYU
    ('gi/arg.cpp', 'struct _GHashTable;', ''),
    ('gi/arg.cpp', 'struct _GVariant;', ''),
)


def output():
    global file, state, add_fwd_header, there_were_errors

    if add_fwd_header:
        if FWD_HEADER not in all_includes:
            if FWD_HEADER in remove:
                remove.pop(FWD_HEADER, None)
            else:
                add[FWD_HEADER] = ''

    if add or remove:
        print(f'\n== {file} ==')
        for line, why in add.items():
            if why:
                why = '  // ' + why
            print(f'{Colors.GREEN}+{line}{Colors.NORMAL}{why}')
        for line, why in remove.items():
            if why:
                why = '  // ' + why
            print(f'{Colors.RED}-{line}{Colors.NORMAL}{why}')
        there_were_errors = True

    state = None
    file = None
    add.clear()
    remove.clear()
    all_includes.clear()
    add_fwd_header = False


for line in sys.stdin:
    line = line.strip()
    if not line:
        continue

    # filter out errors having to do with compiler arguments unknown to IWYU
    if line.startswith('error:'):
        continue

    if line == '---':
        output()
        continue

    state_changed = False
    file_changed = False
    for matcher, newstate in MATCHERS.items():
        match = re.match(matcher, line)
        if match:
            state = newstate
            if match.group(1) != file:
                if file is not None:
                    file_changed = True
                file = match.group(1)
            state_changed = True
            break
    if file_changed:
        output()
        continue
    if state_changed:
        continue

    line, _, why = line.partition(' // ')
    line = line.strip()
    if state == ADD:
        if line in FWD_DECLS_IN_HEADER:
            add_fwd_header = True
            continue
        if (file, line, why) in FALSE_POSITIVES:
            continue
        add[line] = why
    elif state == REMOVE:
        if line.startswith('- '):
            line = line[2:]
        remove[line] = why
    elif state == FULL:
        all_includes[line] = why

if there_were_errors:
    sys.exit(1)
