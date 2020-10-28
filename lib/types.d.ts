import * as glib from "glib";
import * as gio from "gio";

import { ESModule } from "./bootstrap/module"; 

interface Module {
    // This prevents any object's type from structurally matching Module
    __internal: never;
}

export type { Module };

declare global {
    export interface ImportMeta {
        /**
         * 
         */
        url?: string;

        /**
         * 
         */
        importSync?: <T = any>(id: string) => T;
    }

    /**
     * 
     * @param id 
     */
    export function importSync<T = any>(id: string): T

    /**
     * 
     * @param msg 
     */
    export function debug(msg: string): void;

    /**
     * 
     * @param uri 
     * @param text 
     */
    export function compileModule(uri: string, text: string): Module;

    /**
     * 
     * @param uri 
     * @param text 
     */
    export function compileInternalModule(uri: string, text: string): Module;

    /**
     * 
     * @param module 
     * @param private 
     */
    export function setModulePrivate(module: Module, private: ESModule);

    /**
     * 
     * @param global 
     * @param hook 
     */
    export function setModuleLoadHook(global: typeof globalThis, hook: (id: string, uri: string) => Module);

    /**
     * 
     * @param global 
     * @param hook 
     */
    export function setModuleResolveHook(global: typeof globalThis, hook: (module: ESModule, specifier: string) => Module): void;

    /**
     * 
     * @param global 
     * @param hook 
     */
    export function setModuleMetaHook(global: typeof globalThis, hook: (module: ESModule, meta: ImportMeta) => void): void;

    /**
     * 
     * @param args 
     */
    export function registerModule(...args: any[]): any;

    /**
     * 
     * @param global 
     */
    export function getRegistry(global: typeof globalThis): Map<string, Module>;

    /**
     * 
     */
    export const moduleGlobalThis: typeof globalThis;

    /**
     * 
     */
    export const GLib: typeof glib

    /**
     * 
     */
    export const Gio: typeof gio;

    /**
     * 
     */
    export const ByteUtils: any;
}
