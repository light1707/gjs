// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

export const {vprintf} = imports._format;

export function format(...args) {
    return vprintf(this, args);
}
