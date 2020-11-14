// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later

(function (exports) {
    'use strict';

    const {print, printerr, log, logError} = imports._print;
    const {TextDecoder, TextEncoder} = imports._text;

    Object.defineProperties(exports, {
        TextDecoder: {
            configurable: false,
            enumerable: true,
            writable: false,
            value: TextDecoder,
        },
        TextEncoder: {
            configurable: false,
            enumerable: true,
            writable: false,
            value: TextEncoder,
        },
        print: {
            configurable: false,
            enumerable: true,
            writable: true,
            value: print,
        },
        printerr: {
            configurable: false,
            enumerable: true,
            writable: true,
            value: printerr,
        },
        log: {
            configurable: false,
            enumerable: true,
            writable: true,
            value: log,
        },
        logError: {
            configurable: false,
            enumerable: true,
            writable: true,
            value: logError,
        },
    });
})(globalThis);
