// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

// NOTE: Gio, GLib, and GObject have no overrides.

/** @typedef {{ uri: string; scheme: string; host: string; path: string; query: Query }} Uri */

/** @typedef {{ load(uri: Uri): [string, boolean]; }} SchemeHandler */
/** @typedef {{ [key: string]: string | undefined; }} Query */

/**
 * Thrown when there is an error importing a module.
 */
export class ImportError extends Error {
    /**
     * @param {string | undefined} message the import error message
     */
    constructor(message) {
        super(message);

        this.name = 'ImportError';
    }
}

/**
 * ESModule is the "private" object of every module.
 */
export class ESModule {
    /**
     *
     * @param {string} id the module's identifier
     * @param {string} uri the module's URI
     * @param {boolean} [internal] whether this module is "internal"
     */
    constructor(id, uri, internal = false) {
        this.id = id;
        this.uri = uri;
        this.internal = internal;
    }
}

/**
 * Returns whether a string represents a relative path (e.g. ./, ../)
 *
 * @param {string} path a path to check if relative
 * @returns {boolean}
 */
function isRelativePath(path) {
    // Check if the path is relative.
    return path.startsWith('./') || path.startsWith('../');
}

/**
 * Encodes a Uint8Array into a UTF-8 string
 *
 * @param {Uint8Array} bytes the bytes to convert
 * @returns {string}
 */
function fromBytes(bytes) {
    return ByteUtils.toString(bytes, 'utf-8');
}

/**
 * @param {import("gio").File} file the Gio.File to load from.
 * @returns {string}
 */
function loadFileSync(file) {
    try {
        const [, bytes] = file.load_contents(null);

        return fromBytes(bytes);
    } catch (error) {
        throw new ImportError(`Unable to load file from: ${file.get_uri()}`);
    }
}

/**
 * Synchronously loads a file's text from a URI.
 *
 * @param {string} uri the URI to load
 * @returns {string}
 */
export function loadResourceOrFile(uri) {
    let output = Gio.File.new_for_uri(uri);

    return loadFileSync(output);
}

/**
 * Resolves a relative path against a URI.
 *
 * @param {string} uri the base URI
 * @param {string} relativePath the relative path to resolve against the base URI
 * @returns {Uri}
 */
function resolveRelativeResourceOrFile(uri, relativePath) {
    let module_file = Gio.File.new_for_uri(uri);
    let module_parent_file = module_file.get_parent();

    if (module_parent_file) {
        let output = module_parent_file.resolve_relative_path(relativePath);

        return parseURI(output.get_uri());
    }

    throw new ImportError('File does not have a valid parent!');
}


/**
 * Parses a string into a Uri object
 *
 * @param {string} uri the URI to parse as a string
 * @returns {Uri}
 */
export function parseURI(uri) {
    try {
        const parsed = GLib.Uri.parse(uri, GLib.UriFlags.NONE);

        const raw_query = parsed.get_query();
        const query = raw_query ? GLib.Uri.parse_params(raw_query, -1, '&', GLib.UriParamsFlags.NONE) : {};

        return {
            uri,
            scheme: parsed.get_scheme(),
            host: parsed.get_host(),
            path: parsed.get_path(),
            query,

        };
    } catch (error) {
        throw new ImportError(`Attempted to import invalid URI: ${uri}`);
    }
}

/**
 * Handles resolving and loading URIs.
 *
 * @class
 */
export class ModuleLoader {
    /**
     * @param {typeof globalThis} global the global object to handle module resolution
     */
    constructor(global) {
        this.global = global;
    }

    /**
     * Loads a file or resource URI synchronously
     *
     * @param {Uri} uri the file or resource URI to load
     * @returns {[string] | [string, boolean] | null}
     */
    loadURI(uri) {
        if (uri.scheme === 'file' || uri.scheme === 'resource')
            return [loadResourceOrFile(uri.uri)];


        return null;
    }

    /**
     * Resolves an import specifier given an optional parent importer.
     *
     * @param {string} specifier the import specifier
     * @param {string | null} [parentURI] the URI of the module importing the specifier
     * @returns {Uri | null}
     */
    resolveSpecifier(specifier, parentURI = null) {
        try {
            const uri = parseURI(specifier);

            if (uri)
                return uri;
        } catch (err) {

        }

        if (isRelativePath(specifier)) {
            if (!parentURI)
                throw new ImportError('Cannot import from relative path when module path is unknown.');


            return this.resolveRelativePath(specifier, parentURI);
        }


        return null;
    }

    /**
     * Resolves a path relative to a URI, throwing an ImportError if
     * the parentURI isn't valid.
     *
     * @param {string} relativePath the relative path to resolve against the base URI
     * @param {string} parentURI the parent URI
     * @returns {Uri}
     */
    resolveRelativePath(relativePath, parentURI) {
        // Ensure the parent URI is valid.
        parseURI(parentURI);

        // Handle relative imports from URI-based modules.
        return resolveRelativeResourceOrFile(parentURI, relativePath);
    }

    /**
     * Compiles a module source text with the module's URI
     *
     * @param {ESModule} module a module private object
     * @param {string} text the module source text to compile
     * @returns {import("../types").Module}
     */
    compileModule(module, text) {
        const compiled = compileInternalModule(module.uri, text);

        setModulePrivate(compiled, module);

        return compiled;
    }

    /**
     * @param {string} specifier the specifier (e.g. relative path, root package) to resolve
     * @param {string | null} parentURI the URI of the module triggering this resolve
     *
     * @returns {import("../types").Module | null}
     */
    resolveModule(specifier, parentURI) {
        const registry = getRegistry(this.global);


        // Check if the module has already been loaded

        let module = registry.get(specifier);

        if (module)
            return module;


        // 1) Resolve path and URI-based imports.

        const uri = this.resolveSpecifier(specifier, parentURI);

        if (uri) {
            module = registry.get(uri.uri);
            //
            // Check if module is already loaded (relative handling)
            if (module)
                return module;


            const result = this.loadURI(uri);

            if (!result)
                return null;


            const [text, internal = false] = result;

            const esmodule = new ESModule(uri.uri, uri.uri, internal);

            const compiled = this.compileModule(esmodule, text);

            if (!compiled)
                throw new ImportError(`Failed to register module: ${uri}`);


            registry.set(uri.uri, compiled);
            return compiled;
        }

        return null;
    }
}

export const internalModuleLoader = new ModuleLoader(globalThis);

setModuleResolveHook(globalThis, (module, specifier) => {
    const resolved = internalModuleLoader.resolveModule(specifier, module?.uri ?? null);

    if (!resolved)
        throw new ImportError(`Module not found: ${specifier}`);


    return resolved;
});

setModuleMetaHook(globalThis, (module, meta) => {
    meta.url = module.uri;
});

setModuleLoadHook(globalThis, (id, uri) => {
    const m = new ESModule(id, uri);

    const result = internalModuleLoader.loadURI(parseURI(uri));

    if (!result)
        throw new ImportError(`URI not found: ${uri}`);


    const [text] = result;
    const compiled = internalModuleLoader.compileModule(m, text);

    const registry = getRegistry(globalThis);

    registry.set(uri, compiled);

    return compiled;
});
