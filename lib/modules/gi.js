// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

/**
 * Creates a module source text to expose a GI namespace via a default export.
 *
 * @param {string} namespace the GI namespace to import
 * @param {string} [version] the version string of the namespace
 *
 * @returns {string} the generated module source text
 */
export function generateModule(namespace, version) {
    const source = `
    import $$gi from 'gi';
    
    const $$ns = $$gi.require${version ? `('${namespace}', '${version}')` : `('${namespace}')`};

    export default $$ns;
    `;

    return source;
}
