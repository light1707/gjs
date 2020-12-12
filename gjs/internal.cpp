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

#include "gjs/byteArray.h"
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

using GjsAutoFile = GjsAutoUnref<GFile>;

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
        return false;
    }

    return true;
}

/**
 * Asserts the correct arguments for a hook setting function.
 *
 * Asserts: (arg0: object, arg1: Function) => void
 */
static void set_module_hook(JS::CallArgs args, GjsGlobalSlot slot) {
    JS::Value v_global = args[0];
    JS::Value v_hook = args[1];

    g_assert(v_global.isObject());
    g_assert(v_hook.isObject());

    g_assert(JS::IsCallable(&v_hook.toObject()));
    gjs_set_global_slot(&v_global.toObject(), slot, v_hook);

    args.rval().setUndefined();
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
 * @returns guaranteed to return true or assert.
 */
bool gjs_internal_global_set_module_hook([[maybe_unused]] JSContext* cx,
                                         unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "setModuleLoadHook takes 2 arguments");

    set_module_hook(args, GjsGlobalSlot::MODULE_HOOK);
    return true;
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
 * @returns guaranteed to return true or assert.
 */
bool gjs_internal_global_set_module_resolve_hook([[maybe_unused]] JSContext* cx,
                                                 unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "setModuleResolveHook takes 2 arguments");

    set_module_hook(args, GjsGlobalSlot::IMPORT_HOOK);
    return true;
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
 * @returns guaranteed to return true or assert.
 */
bool gjs_internal_global_set_module_meta_hook([[maybe_unused]] JSContext* cx,
                                              unsigned argc, JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "setModuleMetaHook takes 2 arguments");

    set_module_hook(args, GjsGlobalSlot::META_HOOK);
    return true;
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
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "compileInternalModule takes 2 arguments");

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
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "compileModule takes 2 arguments");

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
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 2 && "setModulePrivate takes 2 arguments");
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
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
    g_assert(args.length() == 1 && "getRegistry takes 1 arguments");
    g_assert(args[0].isObject());

    JS::RootedObject global(cx, &args[0].toObject());
    JSAutoRealm ar(cx, global);

    JS::RootedObject registry(cx, gjs_get_module_registry(global));
    args.rval().setObject(*registry);
    return true;
}

bool gjs_internal_parse_uri(JSContext* cx, unsigned argc, JS::Value* vp) {
    using AutoHashTable =
        GjsAutoPointer<GHashTable, GHashTable, g_hash_table_destroy>;
    using AutoURI = GjsAutoPointer<GUri, GUri, g_uri_unref>;

    JS::CallArgs args = CallArgsFromVp(argc, vp);

    g_assert(args.length() == 1 && "parseUri() takes one string argument");
    g_assert(args[0].isString() && "parseUri() takes one string argument");

    JS::RootedString string_arg(cx, args[0].toString());
    JS::UniqueChars uri = JS_EncodeStringToUTF8(cx, string_arg);
    if (!uri)
        return false;

    GError* error = nullptr;
    AutoURI parsed = g_uri_parse(uri.get(), G_URI_FLAGS_NONE, &error);
    if (!parsed)
        return gjs_throw_gerror_message(cx, error);

    JS::RootedObject query_obj(cx, JS_NewPlainObject(cx));
    if (!query_obj)
        return false;

    const char* raw_query = g_uri_get_query(parsed);
    if (raw_query) {
        AutoHashTable query =
            g_uri_parse_params(raw_query, -1, "&", G_URI_PARAMS_NONE, &error);
        if (!query)
            return gjs_throw_gerror_message(cx, error);

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, query);

        void* key_ptr;
        void* value_ptr;
        while (g_hash_table_iter_next(&iter, &key_ptr, &value_ptr)) {
            auto* key = static_cast<const char*>(key_ptr);
            auto* value = static_cast<const char*>(value_ptr);

            JS::ConstUTF8CharsZ value_chars{value, strlen(value)};
            JS::RootedString value_str(cx,
                                       JS_NewStringCopyUTF8Z(cx, value_chars));
            if (!value_str || !JS_DefineProperty(cx, query_obj, key, value_str,
                                                 JSPROP_ENUMERATE))
                return false;
        }
    }

    JS::RootedObject return_obj(cx, JS_NewPlainObject(cx));
    if (!return_obj)
        return false;

    // JS_NewStringCopyZ() used here and below because the URI components are
    // %-encoded, meaning ASCII-only
    JS::RootedString scheme(cx,
                            JS_NewStringCopyZ(cx, g_uri_get_scheme(parsed)));
    if (!scheme)
        return false;

    JS::RootedString host(cx, JS_NewStringCopyZ(cx, g_uri_get_host(parsed)));
    if (!host)
        return false;

    JS::RootedString path(cx, JS_NewStringCopyZ(cx, g_uri_get_path(parsed)));
    if (!path)
        return false;

    if (!JS_DefineProperty(cx, return_obj, "uri", string_arg,
                           JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, return_obj, "scheme", scheme,
                           JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, return_obj, "host", host, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, return_obj, "path", path, JSPROP_ENUMERATE) ||
        !JS_DefineProperty(cx, return_obj, "query", query_obj,
                           JSPROP_ENUMERATE))
        return false;

    args.rval().setObject(*return_obj);
    return true;
}

bool gjs_internal_resolve_relative_resource_or_file(JSContext* cx,
                                                    unsigned argc,
                                                    JS::Value* vp) {
    JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

    g_assert(args.length() == 2 && "resolveRelativeResourceOrFile(str, str)");
    g_assert(args[0].isString() && "resolveRelativeResourceOrFile(str, str)");
    g_assert(args[1].isString() && "resolveRelativeResourceOrFile(str, str)");

    JS::RootedString string_arg(cx, args[0].toString());
    JS::UniqueChars uri = JS_EncodeStringToUTF8(cx, string_arg);
    if (!uri)
        return false;
    string_arg = args[1].toString();
    JS::UniqueChars relative_path = JS_EncodeStringToUTF8(cx, string_arg);
    if (!relative_path)
        return false;

    GjsAutoUnref<GFile> module_file = g_file_new_for_uri(uri.get());
    GjsAutoUnref<GFile> module_parent_file = g_file_get_parent(module_file);

    if (module_parent_file) {
        GjsAutoUnref<GFile> output = g_file_resolve_relative_path(
            module_parent_file, relative_path.get());
        GjsAutoChar output_uri = g_file_get_uri(output);

        JS::ConstUTF8CharsZ uri_chars(output_uri, strlen(output_uri));
        JS::RootedString retval(cx, JS_NewStringCopyUTF8Z(cx, uri_chars));
        if (!retval)
            return false;

        args.rval().setString(retval);
        return true;
    }

    args.rval().setNull();
    return true;
}
