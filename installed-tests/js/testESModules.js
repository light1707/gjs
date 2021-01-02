// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

import gi from 'gi';
import system from 'system';

import $ from 'resource:///org/gjs/jsunit/modules/exports.js';
import {NamedExport} from 'resource:///org/gjs/jsunit/modules/exports.js';

describe('ES module imports', function () {
    it('default import', function () {
        expect($).toEqual(5);
    });

    it('named import', function () {
        expect(NamedExport).toEqual('Hello, World');
    });

    it('system import', function () {
        expect(system.exit.toString()).toEqual('function exit() {\n    [native code]\n}');
    });

    it('GObject introspection import', function () {
        expect(gi.require('GObject').toString()).toEqual('[object GIRepositoryNamespace]');
    });

    it('import.meta.url', function () {
        expect(import.meta.url).toMatch(/\/installed-tests\/js\/testESModules\.js$/);
    });
});
