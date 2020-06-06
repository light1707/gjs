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

/* global debug */

// NOTE: Gio, GLib, and GObject are *raw* and have no overrides.

function isValidUri(uri) {
    const uri_scheme = GLib.uri_parse_scheme(uri);

    // Handle relative imports from URI-based modules.
    if (!uri_scheme)
        return false;

    // Only allow imports from file:// and resource:// URIs
    return uri_scheme === 'file' || uri_scheme === 'resource';
}

function isRelativePath(id) {
    // Check if the path is relative.
    return id.startsWith('./') || id.startsWith('../');
}

function resolveAbsolutePath(id) {
    const output = Gio.File.new_for_path(id);

    return {output, full_path: output.get_path()};
}

function isAbsolutePath(id) {
    return id.startsWith('/');
}

function resolveURI(id) {
    const output = Gio.File.new_for_uri(id);

    return {output, full_path: output.get_uri()};
}

function resolveRelativePath(id, module_uri) {
    // If a module has a path, we'll have stored it in the host field
    let full_path;
    let output;

    if (!module_uri)
        throw new Error('Cannot import from relative path when module path is unknown.');


    debug(`module_uri: ${module_uri}`);

    const uri_scheme = GLib.uri_parse_scheme(module_uri);

    // Handle relative imports from URI-based modules.
    if (uri_scheme) {
        // Only allow relative imports from file:// and resource:// URIs
        if (isValidUri(uri_scheme)) {
            let module_file = Gio.File.new_for_uri(module_uri);
            let module_parent_file = module_file.get_parent();

            output = module_parent_file.resolve_relative_path(id);
            full_path = output.get_uri();
        } else {
            throw new Error(
                'Relative imports can only occur from file:// and resource:// URIs');
        }
    } else {
        // Get the module directory path.
        const module_file = Gio.File.new_for_path(module_uri);
        const module_parent_file = module_file.get_parent();

        // Resolve file relative to the module directory.
        output = module_parent_file.resolve_relative_path(id);
        full_path = output.get_path();
    }

    return {output, full_path};
}

function resolveId(id, module_uri) {
    // If a module has a path, we'll have stored it in the host field
    let full_path = null;
    let output = null;

    if (isAbsolutePath(id))
        ({output, full_path} = resolveAbsolutePath(id));
    else if (isRelativePath(id))
        ({output, full_path} = resolveRelativePath(id, module_uri));
    else if (isValidUri(id))
        ({output, full_path} = resolveURI(id));


    if (!output || !full_path)
        return null;


    return {output, full_path};
}

function fromBytes(bytes) {
    return ByteUtils.toString(bytes, 'utf-8');
}

function loadFileSync(output, full_path) {
    try {
        const [, bytes] = output.load_contents(null);
        return fromBytes(bytes);
    } catch (error) {
        throw new Error(`Unable to load file from: ${full_path}`);
    }
}

const ESM_MODULE_URI = 'resource:///org/gnome/gjs/modules/esm/';
const CORE_MODULE_URI = 'resource:///org/gnome/gjs/modules/core/';

function buildInternalPaths(id) {
    const directory_uris = [ESM_MODULE_URI, CORE_MODULE_URI];

    return directory_uris
        .map(uri => {
            const full_path = GLib.build_pathv('/', [uri, `${id}.js`]);

            debug(`Built path ${full_path} with ${id} for ${uri}.`);

            return {uri, full_path, file: Gio.File.new_for_uri(full_path)};
        });
}

function resolveModule(id, module_uri) {
    // Check if the module has already been loaded
    //
    // Order:
    // - Local imports
    // - Internal imports

    debug(`Resolving: ${id}`);

    let lookup_module = lookupModule(id);

    if (lookup_module)
        return lookup_module;


    lookup_module = lookupInternalModule(id);

    if (lookup_module)
        return lookup_module;


    // 1) Resolve path and URI-based imports.

    const resolved = resolveId(id, module_uri);

    if (resolved) {
        const {output, full_path} = resolved;

        debug(`Full path found: ${full_path}`);

        lookup_module = lookupModule(full_path);

        // Check if module is already loaded (relative handling)
        if (lookup_module)
            return lookup_module;


        const text = loadFileSync(output);

        if (!registerModule(full_path, full_path, text, text.length, false))
            throw new Error(`Failed to register module: ${full_path}`);


        return lookupModule(full_path);
    }

    // 2) Resolve internal imports.

    const result = buildInternalPaths(id).find(({file}) => file && file.query_exists(null));

    if (!result)
        throw new Error(`Attempted to load unregistered global module: ${id}`);


    const {full_path, file} = result;

    const text = loadFileSync(file, full_path);

    if (!registerInternalModule(id, full_path, text, text.length))
        return null;


    return lookupInternalModule(id);
}

setModuleResolveHook((referencingInfo, specifier) => {
    debug('Starting module import...');
    const uri = getModuleUri(referencingInfo);
    debug(`Found base URI: ${uri}`);

    return resolveModule(specifier, uri);
});
