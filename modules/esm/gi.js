const gi = import.meta.importSync('gi');

import System from 'system';

const Gi = {
    require(name, version = null) {
        if (version !== null)
            gi.versions[name] = version;

        if (name === 'versions')
            throw new Error('Cannot import namespace "versions", use the version parameter of Gi.require to specify versions.');


        return gi[name];
    },
    requireSymbol(lib, ver, symbol) {
        if (!checkSymbol(lib, ver, symbol)) {
            if (symbol)
                printerr(`Unsatisfied dependency: No ${symbol} in ${lib}`);
            else
                printerr(`Unsatisfied dependency: ${lib}`);
            System.exit(1);
        }
    },

    /**
     * Check whether an external GI typelib can be imported
     * and provides @symbol.
     *
     * Symbols may refer to
     *  - global functions         ('main_quit')
     *  - classes                  ('Window')
     *  - class / instance methods ('IconTheme.get_default' / 'IconTheme.has_icon')
     *  - GObject properties       ('Window.default_height')
     *
     * @param {string} lib an external dependency to import
     * @param {string} [ver] version of the dependency
     * @param {string} [symbol] symbol to check for
     * @returns {boolean} true if `lib` can be imported and provides `symbol`, false
     * otherwise
     */
    checkSymbol(lib, ver, symbol) {
        let Lib = null;

        if (ver)
            gi.versions[lib] = ver;

        try {
            Lib = gi[lib];
        } catch (e) {
            return false;
        }

        if (!symbol)
            return true; // Done

        let [klass, sym] = symbol.split('.');
        if (klass === symbol) // global symbol
            return typeof Lib[symbol] !== 'undefined';

        let obj = Lib[klass];
        if (typeof obj === 'undefined')
            return false;

        if (typeof obj[sym] !== 'undefined' ||
            obj.prototype && typeof obj.prototype[sym] !== 'undefined')
            return true; // class- or object method

        // GObject property
        let pspec = null;
        if (GObject.type_is_a(obj.$gtype, GObject.TYPE_INTERFACE)) {
            let iface = GObject.type_default_interface_ref(obj.$gtype);
            pspec = GObject.Object.interface_find_property(iface, sym);
        } else if (GObject.type_is_a(obj.$gtype, GObject.TYPE_OBJECT)) {
            pspec = GObject.Object.find_property.call(obj.$gtype, sym);
        }

        return pspec !== null;
    }
};
Object.freeze(Gi);

Gi.require('GjsPrivate');
Gi.require('GLib');
Gi.require('GObject');
Gi.require('Gio');

export default Gi;
