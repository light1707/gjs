const _signals = imports._signals;

function _addSignalMethod(proto, functionName, func) {
    if (proto[functionName] && proto[functionName] !== func)
        log(`WARNING: addSignalMethods is replacing existing ${proto} ${functionName} method`);

    proto[functionName] = func;
}

export class EventEmitter {
    constructor() {
        this._events = new Map();
        this._nextConnectionId = 1n;
    }

    _connectListener(eventName, listener) {
        const listeners = this._events.get(eventName);
        if (listeners) {
            listeners.push(listener);
        } else {
            this._events.set(eventName, [listener]);
        }
    }

    connect(name, callback) {
        // be paranoid about callback arg since we'd start to throw from emit()
        // if it was messed up
        if (typeof callback !== 'function')
            throw new Error('When connecting signal must give a callback that is a function');

        // we instantiate the "signal machinery" only on-demand if anything
        // gets connected.
        if (!('_events' in this)) {
            this._events = [];
            this._nextConnectionId = 1n;
        }

        let id = this._nextConnectionId;
        this._nextConnectionId += 1n;

        // this makes it O(n) in total connections to emit, but I think
        // it's right to optimize for low memory and reentrancy-safety
        // rather than speed
        this._events.push({
            id,
            name,
            callback,
            'disconnected': false,
        });

        this._connectListener(name, callback);

        return id;
    }

    disconnect(id) {
        if ('_events' in this) {
            let i;
            let length = this._events.length;
            for (i = 0; i < length; ++i) {
                let connection = this._events[i];
                if (connection.id === id) {
                    if (connection.disconnected)
                        throw new Error(`Signal handler id ${id} already disconnected`);

                    // set a flag to deal with removal during emission
                    connection.disconnected = true;
                    this._events.splice(i, 1);

                    return;
                }
            }
        }
        throw new Error(`No signal connection ${id} found`);
    }

    signalHandlerIsConnected(id) {
        if (!('_events' in this))
            return false;

        const { length } = this._events;
        for (let i = 0; i < length; ++i) {
            const connection = this._events[i];
            if (connection.id === id)
                return !connection.disconnected;
        }

        return false;
    }

    disconnectAll() {
        if ('_events' in this) {
            while (this._events.length > 0)
                _disconnect.call(this, this._events[0].id);
        }
    }

    emit(name, ...args) {
        // may not be any signal handlers at all, if not then return
        if (!('_events' in this))
            return;

        // To deal with re-entrancy (removal/addition while
        // emitting), we copy out a list of what was connected
        // at emission start; and just before invoking each
        // handler we check its disconnected flag.
        let handlers = [];
        let i;
        let length = this._events.length;
        for (i = 0; i < length; ++i) {
            let connection = this._events[i];
            if (connection.name === name)
                handlers.push(connection);
        }

        // create arg array which is emitter + everything passed in except
        // signal name. Would be more convenient not to pass emitter to
        // the callback, but trying to be 100% consistent with GObject
        // which does pass it in. Also if we pass in the emitter here,
        // people don't create closures with the emitter in them,
        // which would be a cycle.
        let argArray = [this, ...args];

        length = handlers.length;
        for (i = 0; i < length; ++i) {
            let connection = handlers[i];
            if (!connection.disconnected) {
                try {
                    // since we pass "null" for this, the global object will be used.
                    let ret = connection.callback.apply(null, argArray);

                    // if the callback returns true, we don't call the next
                    // signal handlers
                    if (ret === true)
                        break;
                } catch (e) {
                    // just log any exceptions so that callbacks can't disrupt
                    // signal emission
                    logError(e, `Exception in callback for signal: ${name}`);
                }
            }
        }
    }
}

export function addSignalMethods(proto) {
    _addSignalMethod(proto, 'connect', _connect);
    _addSignalMethod(proto, 'disconnect', _disconnect);
    _addSignalMethod(proto, 'emit', _emit);
    _addSignalMethod(proto, 'signalHandlerIsConnected', _signalHandlerIsConnected);
    // this one is not in GObject, but useful
    _addSignalMethod(proto, 'disconnectAll', _disconnectAll);
}
