const system = import.meta.importSync('system');

export default system;

export let addressOf = system.addressOf;

export let refcount = system.refcount;

export let breakpoint = system.breakpoint;

export let gc = system.gc;

export let exit = system.exit;

export let version = system.version;

export let programInvocationName = system.programInvocationName;

export let clearDateCaches = system.clearDateCaches;