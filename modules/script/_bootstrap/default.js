// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later

(function (exports) {
    'use strict';

    const {print, printerr, log, logError} = imports._print;
    const {TextEncoder, TextDecoder} = imports._text;

    Object.defineProperties(exports, {
        TextEncoder: {
            configurable: false,
            enumerable: true,
            writable: false,
            value: TextEncoder,
        },
        TextDecoder: {
            configurable: false,
            enumerable: true,
            writable: false,
            value: TextDecoder,
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
