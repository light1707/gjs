/*
 * Copyright (c) 2020 Evan Welsh <contact@evanwelsh.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

/* global debug, Soup, ImportError */

if (typeof ImportError !== 'function') {
    throw new Error('ImportError is not defined in module loader.');
}

// NOTE: Gio, GLib, and GObject have no overrides.

function isRelativePath(id) {
    // Check if the path is relative.
    return id.startsWith('./') || id.startsWith('../');
}

const allowedRelatives = ["file", "resource"];

const relativeResolvers = new Map();
const loaders = new Map();

function registerScheme(...schemes) {
    function forEach(fn, ...args) {
        schemes.forEach(s => fn(s, ...args));
    }

    const schemeBuilder = {
        relativeResolver(handler) {
            forEach((scheme) => {
                allowedRelatives.push(scheme);
                relativeResolvers.set(scheme, handler);
            });

            return schemeBuilder;
        },
        loader(handler) {
            forEach(scheme => {
                loaders.set(scheme, handler);
            });

            return schemeBuilder;
        }
    };

    return Object.freeze(schemeBuilder);
}

globalThis.registerScheme = registerScheme;

function parseURI(uri) {
    const parsed = Soup.URI.new(uri);

    if (!parsed) {
        return null;
    }

    return {
        raw: uri,
        query: parsed.query ? Soup.form_decode(parsed.query) : {},
        rawQuery: parsed.query,
        scheme: parsed.scheme,
        host: parsed.host,
        port: parsed.port,
        path: parsed.path,
        fragment: parsed.fragment
    };

}

/** 
 * @type {Set<string>}
 * 
 * The set of "module" URIs (the module search path)
 */
const moduleURIs = new Set();

function registerModuleURI(uri) {
    moduleURIs.add(uri);
}

// Always let ESM-specific modules take priority over core modules.
registerModuleURI('resource:///org/gnome/gjs/modules/esm/');
registerModuleURI('resource:///org/gnome/gjs/modules/core/');

/**
 * @param {string} specifier
 */
function buildInternalURIs(specifier) {
    const builtURIs = [];

    for (const uri of moduleURIs) {
        const builtURI = `${uri}/${specifier}.js`;

        debug(`Built internal URI ${builtURI} with ${specifier} for ${uri}.`);

        builtURIs.push(builtURI);
    }

    return builtURIs
}

function resolveRelativePath(moduleURI, relativePath) {
    // If a module has a path, we'll have stored it in the host field
    if (!moduleURI) {
        throw new ImportError('Cannot import from relative path when module path is unknown.');
    }

    debug(`moduleURI: ${moduleURI}`);

    const parsed = parseURI(moduleURI);

    // Handle relative imports from URI-based modules.
    if (parsed) {
        const resolver = relativeResolvers.get(parsed.scheme);

        if (resolver) {
            return resolver(parsed, relativePath);
        } else {
            throw new ImportError(
                `Relative imports can only occur from the following URI schemes: ${
                Array.from(relativeResolvers.keys()).map(s => `${s}://`).join(', ')
                }`);
        }
    } else {
        throw new ImportError(`Module has invalid URI: ${moduleURI}`);
    }
}

function loadURI(uri) {
    debug(`URI: ${uri.raw}`);

    if (uri.scheme) {
        const loader = loaders.get(uri.scheme);

        if (loader) {
            return loader(uri);
        } else {
            throw new ImportError(`No resolver found for URI: ${uri.raw || uri}`);
        }
    } else {
        throw new ImportError(`Unable to load module, module has invalid URI: ${uri.raw || uri}`);
    }
}

function resolveSpecifier(specifier, moduleURI = null) {
    // If a module has a path, we'll have stored it in the host field
    let output = null;
    let uri = null;
    let parsedURI = null;

    if (isRelativePath(specifier)) {
        let resolved = resolveRelativePath(moduleURI, specifier);

        parsedURI = parseURI(resolved);
        uri = resolved;
    } else {
        const parsed = parseURI(specifier);

        if (parsed) {
            uri = parsed.raw;
            parsedURI = parsed;
        }
    }

    if (parsedURI) {
        output = loadURI(parsedURI);
    }

    if (!output)
        return null;

    return { output, uri };
}

function resolveModule(specifier, moduleURI) {
    // Check if the module has already been loaded
    //
    // Order:
    // - Local imports
    // - Internal imports

    debug(`Resolving: ${specifier}`);

    let lookup_module = lookupModule(specifier);

    if (lookup_module)
        return lookup_module;

    lookup_module = lookupInternalModule(specifier);

    if (lookup_module)
        return lookup_module;

    // 1) Resolve path and URI-based imports.

    const resolved = resolveSpecifier(specifier, moduleURI);

    if (resolved) {
        const { output, uri } = resolved;

        debug(`Full path found: ${uri}`);

        lookup_module = lookupModule(uri);

        // Check if module is already loaded (relative handling)
        if (lookup_module)
            return lookup_module;

        const text = output;

        if (!registerModule(uri, uri, text, text.length, false))
            throw new ImportError(`Failed to register module: ${uri}`);


        return lookupModule(uri);
    }

    // 2) Resolve internal imports.

    const uri = buildInternalURIs(specifier).find((uri) => {
        let file = Gio.File.new_for_uri(uri);

        return file && file.query_exists(null);
    });

    if (!uri)
        throw new ImportError(`Attempted to load unregistered global module: ${specifier}`);

    const text = loaders.get('resource')(parseURI(uri));

    if (!registerInternalModule(specifier, uri, text, text.length))
        return null;

    return lookupInternalModule(specifier);
}

setModuleResolveHook((referencingInfo, specifier) => {
    debug('Starting module import...');
    const uri = getModuleURI(referencingInfo);

    if (uri) {
        debug(`Found base URI: ${uri}`);
    }

    return resolveModule(specifier, uri);
});
