// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

const gi = import.meta.importSync('gi');

const Gi = {
    require(name, version = null) {
        if (version !== null)
            gi.versions[name] = version;

        if (name === 'versions')
            throw new Error('Cannot import namespace "versions", use the version parameter of Gi.require to specify versions.');


        return gi[name];
    },
};
Object.freeze(Gi);

export default Gi;
