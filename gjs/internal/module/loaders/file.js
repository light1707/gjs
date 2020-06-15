/* global registerScheme */

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

registerScheme('file', 'resource')
    .relativeResolver((moduleURI, relativePath) => {
        let module_file = Gio.File.new_for_uri(moduleURI.raw);
        let module_parent_file = module_file.get_parent();

        let output = module_parent_file.resolve_relative_path(relativePath);

        return output.get_uri();
    }).loader(uri => {
        const file = Gio.File.new_for_uri(uri.raw);

        return loadFileSync(file, file.get_uri());
    });
