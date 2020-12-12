// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

/* global debug, finishDynamicModuleImport, initAndEval, setModuleDynamicImportHook, parseURI */

import {ESModule, ImportError, loadResourceOrFile, ModuleLoader} from '../bootstrap/module.js';

import {generateModule} from './gi.js';

export class ESModuleLoader extends ModuleLoader {
    /**
     * @param {typeof globalThis} global the global object to register modules with.
     */
    constructor(global) {
        super(global);

        /**
         * @type {Set<string>}
         *
         * The set of "module" URIs (the module search path)
         */
        this.moduleURIs = new Set();

        /**
         * @type {Map<string, import("../bootstrap/module.js").SchemeHandler>}
         *
         * A map of handlers for URI schemes (e.g. gi://)
         */
        this.schemeHandlers = new Map();
    }

    /**
     * @param {ESModule} module a module private object
     * @param {string} text the module source text
     */
    compileModule(module, text) {
        const compiled = compileModule(module.uri, text);

        setModulePrivate(compiled, module);

        return compiled;
    }

    /**
     * @param {string} specifier the package specifier
     * @returns {string[]} the possible internal URIs
     */
    buildInternalURIs(specifier) {
        const {moduleURIs} = this;
        const builtURIs = [];

        for (const uri of moduleURIs) {
            const builtURI = `${uri}/${specifier}.js`;
            builtURIs.push(builtURI);
        }

        return builtURIs;
    }

    /**
     * @param {string} scheme the URI scheme to register
     * @param {import("../bootstrap/module.js").SchemeHandler} handler a handler
     */
    registerScheme(scheme, handler) {
        this.schemeHandlers.set(scheme, handler);
    }

    /**
     * @param {import("../bootstrap/module.js").Uri} uri a Uri object to load
     */
    loadURI(uri) {
        const {schemeHandlers} = this;

        if (uri.scheme) {
            const loader = schemeHandlers.get(uri.scheme);

            if (loader)
                return loader.load(uri);
        }

        const result = super.loadURI(uri);

        if (result)
            return result;

        throw new ImportError(`Unable to load module from invalid URI: ${uri.uri}`);
    }

    /**
     * Registers an internal resource URI as a bare-specifier root.
     *
     * For example, registering "resource:///org/gnome/gjs/modules/esm/" allows
     * import "system" if "resource:///org/gnome/gjs/modules/esm/system.js"
     * exists.
     *
     * @param {string} uri the URI to register.
     */
    registerModuleURI(uri) {
        const {moduleURIs} = this;

        moduleURIs.add(uri);
    }

    /**
     * Resolves a module import with optional handling for relative imports.
     *
     * @param {string} specifier the module specifier to resolve for an import
     * @param {string | null} moduleURI the importing module's URI or null if importing from the entry point
     * @returns {import("../types").Module}
     */
    resolveModule(specifier, moduleURI) {
        const module = super.resolveModule(specifier, moduleURI);
        if (module)
            return module;

        // 2) Resolve internal imports.

        const uri = this.buildInternalURIs(specifier).find(u => {
            let file = Gio.File.new_for_uri(u);
            return file && file.query_exists(null);
        });

        if (!uri)
            throw new ImportError(`Attempted to load unregistered global module: ${specifier}`);

        const parsed = parseURI(uri);
        if (parsed.scheme !== 'file' && parsed.scheme !== 'resource')
            throw new ImportError('Only file:// and resource:// URIs are currently supported.');

        const text = loadResourceOrFile(parsed.uri);
        const priv = new ESModule(specifier, uri, true);
        const compiled = this.compileModule(priv, text);
        if (!compiled)
            throw new ImportError(`Failed to register module: ${uri}`);

        const registry = getRegistry(this.global);
        if (!registry.has(specifier))
            registry.set(specifier, compiled);

        return compiled;
    }
}

export const moduleLoader = new ESModuleLoader(moduleGlobalThis);

// Always let ESM-specific modules take priority over core modules.
moduleLoader.registerModuleURI('resource:///org/gnome/gjs/modules/esm/');
moduleLoader.registerModuleURI('resource:///org/gnome/gjs/modules/core/');

const giVersionMap = new Map();

giVersionMap.set('GLib', '2.0');
giVersionMap.set('Gio', '2.0');
giVersionMap.set('GObject', '2.0');

/**
 * @param {string} lib the GI namespace to get the version for.
 */
function getGIVersionMap(lib) {
    return giVersionMap.get(lib);
}

moduleLoader.registerScheme('gi', {
    /**
     * @param {import("../bootstrap/module.js").Uri} uri the URI to load
     */
    load(uri) {
        const version = uri.query.version ?? getGIVersionMap(uri.host);

        if (version)
            giVersionMap.set(uri.host, version);

        return [generateModule(uri.host, version), true];
    },
});

/**
 * @param {ESModule} module
 * @param {ImportMeta} meta
 */
setModuleMetaHook(moduleGlobalThis, (module, meta) => {
    meta.url = module.uri;

    if (module.internal)
        meta.importSync = globalThis.importSync;
});

/**
 * @param {string} id
 * @param {string} uri
 */
setModuleLoadHook(moduleGlobalThis, (id, uri) => {
    const priv = new ESModule(id, uri);

    const [text] = moduleLoader.loadURI(parseURI(uri));
    const compiled = moduleLoader.compileModule(priv, text);

    const registry = getRegistry(moduleGlobalThis);

    registry.set(id, compiled);

    return compiled;
});

setModuleResolveHook(moduleGlobalThis, (module, specifier) => {
    return moduleLoader.resolveModule(specifier, module.uri);
});

