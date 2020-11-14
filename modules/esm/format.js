export const vprintf = imports._format.vprintf;

export function format(...args) {
    return vprintf(this, args);
}