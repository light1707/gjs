const cairo = import.meta.importSync('cairoNative');

export default Object.assign(
    {},
    imports._cairo,
    cairo
);