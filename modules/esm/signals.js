
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

        let id = this._nextConnectionId;
        this._nextConnectionId += 1n;

        // this makes it O(n) in total connections to emit, but I think
        // it's right to optimize for low memory and reentrancy-safety
        // rather than speed

        this._connectListener(name, {
            id,
            name,
            callback,
            'disconnected': false,
        });

        return id;
    }

    disconnect(id) {
        this._events.forEach(_events => {
            for (let i = 0; i < _events.length; ++i) {
                let connection = _events[i];
                if (connection.id === id) {
                    if (connection.disconnected)
                        throw new Error(`Signal handler id ${id} already disconnected`);

                    // set a flag to deal with removal during emission
                    connection.disconnected = true;
                    _events.splice(i, 1);

                    return;
                }
            }
        });
    }

    signalHandlerIsConnected(id) {
        this._events.forEach(_events => {
            for (let i = 0; i < _events.length; ++i) {
                const connection = this._events[i];
                if (connection.id === id)
                    return !connection.disconnected;
            }
        });

        return false;
    }

    disconnectAll() {
        this._events.forEach(_events => {
            while (_events.length > 0)
                this.disconnect.call(this, _events[0].id);
        });
    }

    emit(name, ...args) {
        // may not be any signal handlers at all, if not then return
        if (!('_events' in this))
            return;

        // To deal with re-entrancy (removal/addition while
        // emitting), we copy out a list of what was connected
        // at emission start; and just before invoking each
        // handler we check its disconnected flag.
        let handlers = [...(this._events.get(name) || [])];

        // create arg array which is emitter + everything passed in except
        // signal name. Would be more convenient not to pass emitter to
        // the callback, but trying to be 100% consistent with GObject
        // which does pass it in. Also if we pass in the emitter here,
        // people don't create closures with the emitter in them,
        // which would be a cycle.
        let argArray = [this, ...args];

        for (let i = 0; i < handlers.length; ++i) {
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
