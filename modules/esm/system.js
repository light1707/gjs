// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

const system = import.meta.importSync('system');

export let {
    addressOf,
    refcount,
    breakpoint,
    gc,
    exit,
    version,
    programInvocationName,
    clearDateCaches,
} = system;

let _ = {
    addressOf,
    refcount,
    breakpoint,
    gc,
    exit,
    version,
    programInvocationName,
    clearDateCaches,
};

export default _;
