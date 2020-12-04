// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#include "gjs/internal.h"

#include <config.h>
#include <gio/gio.h>
#include <girepository.h>
#include <glib-object.h>
#include <glib.h>
#include <js/Array.h>
#include <js/Class.h>
#include <js/CompilationAndEvaluation.h>
#include <js/CompileOptions.h>
#include <js/Conversions.h>
#include <js/GCVector.h>  // for RootedVector
#include <js/Modules.h>
#include <js/Promise.h>
#include <js/PropertyDescriptor.h>
#include <js/RootingAPI.h>
#include <js/SourceText.h>
#include <js/TypeDecls.h>
#include <js/Wrapper.h>
#include <jsapi.h>  // for JS_DefinePropertyById, ...
#include <jsfriendapi.h>
#include <stddef.h>     // for size_t
#include <sys/types.h>  // for ssize_t

#include <codecvt>  // for codecvt_utf8_utf16
#include <locale>   // for wstring_convert
#include <string>   // for u16string
#include <vector>

#include "gjs/context-private.h"
#include "gjs/context.h"
#include "gjs/engine.h"
#include "gjs/error-types.h"
#include "gjs/global.h"
#include "gjs/importer.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/mem-private.h"
#include "gjs/module.h"
#include "gjs/native.h"
#include "util/log.h"

#include "gi/repo.h"

// NOTE: You have to be very careful in this file to only do operations within
// the correct global!

/**
 * gjs_load_internal_module:
 *
 * @brief Loads a module source from an internal resource,
 * resource:///org/gnome/gjs/lib/{#identifier}.js, registers it in the internal
 * global's module registry, and proceeds to compile, initialize, and evaluate
 * the module.
 *
 * @param cx the current JSContext
 * @param identifier the identifier of the internal module
 *
 * @returns whether an error occurred while loading or evaluating the module.
 */
bool gjs_load_internal_module(JSContext* cx, const char* identifier) {
    GjsAutoChar full_path(
        g_strdup_printf("resource:///org/gnome/gjs/lib/%s.js", identifier));

    char* script;
    size_t script_len;

    if (!gjs_load_internal_source(cx, full_path, &script, &script_len))
        return false;

    std::u16string utf16_string = gjs_utf8_script_to_utf16(script, script_len);
    g_free(script);

    // COMPAT: This could use JS::SourceText<mozilla::Utf8Unit> directly,
    // but that messes up code coverage. See bug
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1404784
    JS::SourceText<char16_t> buf;
    if (!buf.init(cx, utf16_string.c_str(), utf16_string.size(),
                  JS::SourceOwnership::Borrowed))
        return false;

    JS::CompileOptions options(cx);
    options.setIntroductionType("Internal Module Bootstrap");
    options.setFileAndLine(full_path, 1);
    options.setSelfHostingMode(false);

    JS::RootedObject internal_global(cx, gjs_get_internal_global(cx));
    JSAutoRealm ar(cx, internal_global);

    JS::RootedObject module(cx, JS::CompileModule(cx, options, buf));
    JS::RootedObject registry(cx, gjs_get_module_registry(internal_global));

    JS::RootedId key(cx, gjs_intern_string_to_id(cx, full_path));

    if (!gjs_global_registry_set(cx, registry, key, module) ||
        !JS::ModuleInstantiate(cx, module) || !JS::ModuleEvaluate(cx, module)) {
        gjs_log_exception(cx);
        return false;
    }

    return true;
}

/**
 * Asserts the correct arguments for a hook setting function.
 *
 * Asserts: (arg0: object, arg1: Function) => void
 */
static bool set_module_hook(JSContext* cx, JS::CallArgs args,
                            GjsGlobalSlot slot) {
    JS::RootedValue v_global(cx, args[0]);
    JS::RootedValue v_hook(cx, args[1]);

    g_assert(v_global.isObject());
    g_assert(v_hook.isObject());

    JS::RootedObject hook(cx, &v_hook.toObject());
    g_assert(JS::IsCallable(hook));
    gjs_set_global_slot(&v_global.toObject(), slot, v_hook);

    args.rval().setUndefined();
    return true;
}

/**
 * gjs_internal_global_set_module_hook:
 *
 * @brief Sets the MODULE_HOOK slot of the passed global object.
 * Asserts that the second argument must be callable (e.g. Function)
 * The passed callable is called by gjs_module_load.
 *
 * @example (in JavaScript)
 * setModuleLoadHook(globalThis, (id, uri) => {
 *   id // the module's identifier
 *   uri // the URI to load from
 * });
 *
 * @returns whether an error occurred while setting the module hook.
 */
bool gjs_internal_global_set_module_hook(JSContext* cx, unsigned argc,
                                         JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleLoadHook", 2))
        return false;

    return set_module_hook(cx, args, GjsGlobalSlot::MODULE_HOOK);
}

/**
 * gjs_internal_global_set_module_resolve_hook:
 *
 * @brief Sets the IMPORT_HOOK slot of the passed global object.
 * Asserts that the second argument must be callable (e.g. Function)
 * The passed callable is called by gjs_module_resolve.
 *
 * @example (in JavaScript)
 * setModuleResolveHook(globalThis, (module, specifier) => {
 *   module // the importing module object
 *   specifier // the import specifier
 * });
 *
 * @returns whether an error occurred while setting the import hook.
 */
bool gjs_internal_global_set_module_resolve_hook(JSContext* cx, unsigned argc,
                                                 JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleResolveHook", 2))
        return false;

    return set_module_hook(cx, args, GjsGlobalSlot::IMPORT_HOOK);
}

/**
 * gjs_internal_global_set_module_meta_hook:
 *
 * @brief Sets the META_HOOK slot of the passed passed global object.
 * Asserts that the second argument must be callable (e.g. Function).
 * The passed callable is called by gjs_populate_module_meta.
 *
 * The META_HOOK is passed two parameters, a plain object for population with
 * meta properties and the module's private object.
 *
 * @example (in JavaScript)
 * setModuleMetaHook(globalThis, (module, meta) => {
 *   module // the module object
 *   meta // the meta object
 * });
 *
 * @returns whether an error occurred while setting the meta hook.
 */
bool gjs_internal_global_set_module_meta_hook(JSContext* cx, unsigned argc,
                                              JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleMetaHook", 2))
        return false;

    return set_module_hook(cx, args, GjsGlobalSlot::META_HOOK);
}

/**
 * compile_module:
 *
 * @brief Compiles the a module source text into an internal #Module object
 * given the module's URI as the first argument.
 *
 * @param cx the current JSContext
 * @param args the call args from the native function call
 *
 * @returns whether an error occurred while compiling the module.
 */
static bool compile_module(JSContext* cx, JS::CallArgs args) {
    g_assert(args[0].isString());
    g_assert(args[1].isString());

    JS::RootedString s1(cx, args[0].toString());
    JS::RootedString s2(cx, args[1].toString());

    JS::UniqueChars uri = JS_EncodeStringToUTF8(cx, s1);
    if (!uri)
        return false;

    JS::CompileOptions options(cx);
    options.setFileAndLine(uri.get(), 1).setSourceIsLazy(false);

    size_t text_len;
    char16_t* text;
    if (!gjs_string_get_char16_data(cx, s2, &text, &text_len))
        return false;

    JS::SourceText<char16_t> buf;
    if (!buf.init(cx, text, text_len, JS::SourceOwnership::TakeOwnership))
        return false;

    JS::RootedObject new_module(cx, JS::CompileModule(cx, options, buf));
    if (!new_module)
        return false;

    args.rval().setObject(*new_module);
    return true;
}

/**
 * gjs_internal_compile_internal_module:
 *
 * @brief Compiles a module source text within the internal global's realm.
 *
 * NOTE: Modules compiled with this function can only be executed
 * within the internal global's realm.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while compiling the module.
 */
bool gjs_internal_compile_internal_module(JSContext* cx, unsigned argc,
                                          JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "compileInternalModule", 2)) {
        return false;
    }

    JS::RootedObject global(cx, gjs_get_internal_global(cx));
    JSAutoRealm ar(cx, global);

    return compile_module(cx, args);
}

/**
 * gjs_internal_compile_module:
 *
 * @brief Compiles a module source text within the import global's realm.
 *
 * NOTE: Modules compiled with this function can only be executed
 * within the import global's realm.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while compiling the module.
 */
bool gjs_internal_compile_module(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "compileModule", 2))
        return false;

    JS::RootedObject global(cx, gjs_get_import_global(cx));
    JSAutoRealm ar(cx, global);

    return compile_module(cx, args);
}

/**
 * gjs_internal_set_module_private:
 *
 * @brief Sets the private object of an internal #Module object.
 * The private object must be a #JSObject.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while setting the private.
 */
bool gjs_internal_set_module_private(JSContext* cx, unsigned argc,
                                     JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModulePrivate", 2))
        return false;

    g_assert(args[0].isObject());
    g_assert(args[1].isObject());

    JS::RootedObject moduleObj(cx, &args[0].toObject());
    JS::RootedObject privateObj(cx, &args[1].toObject());

    JS::SetModulePrivate(moduleObj, JS::ObjectValue(*privateObj));
    return true;
}

/**
 * gjs_internal_global_import_sync:
 *
 * @brief Synchronously imports native "modules" from the import global's
 * native registry. This function does not do blocking I/O so it is
 * safe to call it synchronously for accessing native "modules" within
 * modules. This function is always called within the import global's
 * realm.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while importing the native module.
 */
bool gjs_internal_global_import_sync(JSContext* cx, unsigned argc,
                                     JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    JS::UniqueChars id;
    if (!gjs_parse_call_args(cx, "importSync", args, "s", "identifier", &id))
        return false;

    JS::RootedObject global(cx, gjs_get_import_global(cx));
    JSAutoRealm ar(cx, global);

    JS::AutoSaveExceptionState exc_state(cx);

    JS::RootedObject native_registry(cx, gjs_get_native_registry(global));
    JS::RootedObject v_module(cx);

    JS::RootedId key(cx, gjs_intern_string_to_id(cx, id.get()));
    if (!gjs_global_registry_get(cx, native_registry, key, &v_module))
        return false;

    if (v_module) {
        args.rval().setObject(*v_module);
        return true;
    }

    JS::RootedObject native_obj(cx);
    if (!gjs_load_native_module(cx, id.get(), &native_obj)) {
        gjs_throw(cx, "Failed to load native module: %s", id.get());
        return false;
    }

    if (!gjs_global_registry_set(cx, native_registry, key, native_obj))
        return false;

    args.rval().setObject(*native_obj);
    return true;
}

/**
 * gjs_internal_global_get_registry:
 *
 * @brief Retrieves the module registry for the passed global object.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while retrieving the registry.
 */
bool gjs_internal_global_get_registry(JSContext* cx, unsigned argc,
                                      JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "getRegistry", 1))
        return false;

    JS::RootedObject global(cx, &args[0].toObject());
    JSAutoRealm ar(cx, global);

    JS::RootedObject registry(cx, gjs_get_module_registry(global));
    args.rval().setObject(*registry);
    return true;
}
