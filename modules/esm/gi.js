const gi = import.meta.require('gi');

const Gi = {
    require(name, version = null) {
        if (version !== null)
            gi.versions[name] = version;

        return gi[name];
    },
};

Object.freeze(Gi);

export default Gi;
