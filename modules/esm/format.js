// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

export const {vprintf} = imports._format;

/**
 * @param {any[]} args FIXME
 */
export function format(...args) {
    // eslint-disable-next-line no-invalid-this
    return vprintf(this, args);
}
