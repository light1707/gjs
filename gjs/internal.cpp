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
 * Loads a module source from an internal resource,
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

    if (!gjs_load_internal_source(cx, full_path, &script, &script_len)) {
        return false;
    }

    std::u16string utf16_string = gjs_utf8_script_to_utf16(script, script_len);

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

    JS::RootedValue ignored_retval(cx);

    JS::RootedObject module(cx, JS::CompileModule(cx, options, buf));
    JS::RootedObject registry(cx, gjs_get_module_registry(cx, internal_global));

    JS::RootedId key(cx);
    JS::RootedValue keyVal(cx);

    if (!gjs_string_from_utf8(cx, full_path, &keyVal)) {
        gjs_throw(cx, "no string");
        return false;
    }
    if (!JS_ValueToId(cx, keyVal, &key)) {
        gjs_throw(cx, "bad val to id");
        return false;
    }

    if (!gjs_global_registry_set(cx, registry, key, module)) {
        gjs_throw(cx, "failed to set registry");
        return false;
    }

    if (!JS::ModuleInstantiate(cx, module)) {
        gjs_log_exception(cx);
        gjs_throw(cx, "not instantiation");
        return false;
    }

    if (!JS::ModuleEvaluate(cx, module)) {
        gjs_log_exception(cx);
        gjs_throw(cx, "failed to evaluate");
        return false;
    }

    return true;
}

/**
 * SetModuleLoadHook:
 *
 * Sets the MODULE_HOOK slot of the passed global to the second argument which
 * must be an object. Setting a non-function object is possible but will throw
 * a not-callable error when gjs_module_load is used.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while setting the module hook.
 */
bool SetModuleLoadHook(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleLoadHook", 2)) {
        return false;
    }

    JS::RootedValue gv(cx, args[0]);
    JS::RootedValue mv(cx, args[1]);

    g_assert(gv.isObject());

    // The hook is stored in the internal global.
    JS::RootedObject global(cx, &gv.toObject());
    gjs_set_global_slot(global, GjsGlobalSlot::MODULE_HOOK, mv);

    args.rval().setUndefined();
    return true;
}

/**
 * SetModuleResolveHook:
 *
 * Sets the IMPORT_HOOK slot of the passed global to the second argument which
 * must be an object. Setting a non-function object is possible but will throw
 * a not-callable error when gjs_module_load is used.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while setting the import hook.
 */
bool SetModuleResolveHook(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleResolveHook", 2)) {
        return false;
    }

    JS::RootedValue gv(cx, args[0]);
    JS::RootedValue mv(cx, args[1]);

    g_assert(gv.isObject());

    // The hook is stored in the internal global.
    JS::RootedObject global(cx, &gv.toObject());
    gjs_set_global_slot(global, GjsGlobalSlot::IMPORT_HOOK, mv);

    args.rval().setUndefined();
    return true;
}

/**
 * SetModuleResolveHook:
 *
 * Sets the META_HOOK slot of the passed global to the second argument which
 * must be an object. Setting a non-function object is possible but will throw
 * a not-callable error when gjs_module_load is used.
 *
 * The META_HOOK is passed two parameters, a plain object for population with
 * meta properties and the module's private object.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while setting the meta hook.
 */
bool SetModuleMetaHook(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleMetaHook", 2)) {
        return false;
    }

    JS::RootedValue gv(cx, args[0]);
    JS::RootedValue mv(cx, args[1]);

    g_assert(gv.isObject());

    // The hook is stored in the internal global.
    JS::RootedObject global(cx, &gv.toObject());
    gjs_set_global_slot(global, GjsGlobalSlot::META_HOOK, mv);

    args.rval().setUndefined();
    return true;
}

/**
 * compile_module:
 *
 * Compiles the a module source text into an internal #Module object
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
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, s2);
    size_t text_len = JS_GetStringLength(s2);

    JS::CompileOptions options(cx);
    options.setFileAndLine(uri.get(), 1).setSourceIsLazy(false);

    std::u16string utf16_string(gjs_utf8_script_to_utf16(text.get(), text_len));

    JS::SourceText<char16_t> buf;
    if (!buf.init(cx, utf16_string.c_str(), utf16_string.size(),
                  JS::SourceOwnership::Borrowed))
        return false;

    JS::RootedObject new_module(cx, JS::CompileModule(cx, options, buf));
    if (!new_module) {
        gjs_log_exception(cx);
        return false;
    }
    args.rval().setObjectOrNull(new_module);

    return true;
}

/**
 * CompileInternalModule:
 *
 * Compiles a module source text within the internal global's realm.
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
bool CompileInternalModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "compileInternalModule", 2)) {
        return false;
    }

    JS::RootedObject global(cx, gjs_get_internal_global(cx));
    JSAutoRealm ar(cx, global);

    return compile_module(cx, args);
}

/**
 * CompileModule:
 *
 * Compiles a module source text within the import global's realm.
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
bool CompileModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "compileModule", 2)) {
        return false;
    }

    JS::RootedObject global(cx, gjs_get_import_global(cx));
    JSAutoRealm ar(cx, global);

    return compile_module(cx, args);
}

/**
 * SetModulePrivate:
 *
 * Sets the private object of an internal #Module object.
 * The private object must be a #JSObject.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while setting the private.
 */
bool SetModulePrivate(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "setModulePrivate", 1)) {
        return false;
    }

    JS::RootedObject moduleObj(cx, &args[0].toObject());
    JS::RootedObject privateObj(cx, &args[1].toObject());

    JS::SetModulePrivate(moduleObj, JS::ObjectValue(*privateObj));
    return true;
}

/**
 * ImportSync:
 *
 * Synchronously imports native "modules" from the import global's
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
bool ImportSync(JSContext* m_cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);

    if (argc != 1) {
        gjs_throw(m_cx, "Must pass a single argument to importSync()");
        return false;
    }

    JS::RootedObject global(m_cx, gjs_get_import_global(m_cx));

    JSAutoRealm ar(m_cx, global);

    JS::AutoSaveExceptionState exc_state(m_cx);
    JS::RootedString jstr(m_cx, JS::ToString(m_cx, argv[0]));
    exc_state.restore();

    if (!jstr) {
        g_message("JS LOG: <cannot convert value to string>");
        return true;
    }

    JS::UniqueChars id(JS_EncodeStringToUTF8(m_cx, jstr));

    if (!id) {
        gjs_throw(m_cx, "Invalid native id.");
        return false;
    }

    JS::RootedObject native_registry(m_cx,
                                     gjs_get_native_registry(m_cx, global));
    JS::RootedObject map_val(m_cx);

    JS::RootedId map_key(m_cx);
    if (!JS_StringToId(m_cx, jstr, &map_key)) {
        return false;
    }

    if (!gjs_global_registry_get(m_cx, native_registry, map_key, &map_val)) {
        return false;
    }
    if (map_val) {
        argv.rval().setObject(*map_val);
        return true;
    }

    JS::RootedObject native_obj(m_cx);

    if (!gjs_load_native_module(m_cx, id.get(), &native_obj)) {
        gjs_throw(m_cx, "Failed to load native module: %s", id.get());
        return false;
    }

    if (!gjs_global_registry_set(m_cx, native_registry, map_key, native_obj)) {
        return false;
    }

    argv.rval().setObject(*native_obj);
    return true;
}

/**
 * GetRegistry:
 *
 * Retrieves the module registry for the passed global object.
 *
 * @param cx the current JSContext
 * @param argc
 * @param vp
 *
 * @returns whether an error occurred while retrieving the registry.
 */
bool GetRegistry(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "getRegistry", 1)) {
        return false;
    }

    JS::RootedObject global(cx, &args[0].toObject());  // global

    {
        JSAutoRealm ar(cx, global);

        JS::RootedObject registry(cx, gjs_get_module_registry(cx, global));

        args.rval().setObject(*registry);

        return true;
    }
}
