/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
/*
 * Copyright (c) 2008  litl, LLC
 * Copyright (c) 2009 Red Hat, Inc.
 * Copyright (c) 2017  Philip Chimento <philip.chimento@gmail.com>
 * Copyright (c) 2020  Evan Welsh <contact@evanwelsh.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <config.h>

#include <stddef.h>  // for size_t

#include <glib.h>

#include <js/CallArgs.h>
#include <js/CharacterEncoding.h>
#include <js/Class.h>
#include <js/CompilationAndEvaluation.h>
#include <js/CompileOptions.h>
#include <js/Conversions.h>
#include <js/PropertyDescriptor.h>  // for JSPROP_PERMANENT, JSPROP_RE...
#include <js/PropertySpec.h>
#include <js/Realm.h>  // for GetObjectRealmOrNull, SetRealmPrivate
#include <js/RealmOptions.h>
#include <js/RootingAPI.h>
#include <js/SourceText.h>
#include <js/TypeDecls.h>
#include <js/Utility.h>  // for UniqueChars
#include <jsapi.h>       // for AutoSaveExceptionState, ...
#include <jsfriendapi.h>

#include <codecvt>
#include <locale>
#include <string>  // for u16string

#include "gi/ns.h"
#include "gi/repo.h"

#include "gjs/atoms.h"
#include "gjs/context-private.h"
#include "gjs/engine.h"
#include "gjs/global.h"
#include "gjs/jsapi-util-args.h"
#include "gjs/jsapi-util.h"
#include "gjs/module.h"
#include "gjs/native.h"

namespace mozilla {
union Utf8Unit;
}

inline JSObject* global_create(JSContext* cx, const JSClass* clasp,
                               JS::RealmCreationOptions options) {
    options.setBigIntEnabled(true);

    JS::RealmBehaviors behaviors;
    JS::RealmOptions compartment_options(options, behaviors);

    JS::RootedObject global(
        cx, JS_NewGlobalObject(cx, clasp, nullptr, JS::FireOnNewGlobalHook,
                               compartment_options));

    if (!global)
        return nullptr;

    JSAutoRealm ac(cx, global);

    if (!JS_InitReflectParse(cx, global) ||
        !JS_DefineDebuggerObject(cx, global))
        return nullptr;

    return global;
}

inline JSObject* global_create_new(JSContext* cx, const JSClass* clasp) {
    JS::RealmCreationOptions creation;
    creation.setNewCompartmentAndZone();

    return global_create(cx, clasp, creation);
}

inline JSObject* global_create_with_existing(JSContext* cx, JSObject* existing,
                                             const JSClass* clasp) {
    JS::RealmCreationOptions creation;
    JS::RootedObject comp(cx, existing);
    creation.setExistingCompartment(comp);

    return global_create(cx, clasp, creation);
}

GJS_JSAPI_RETURN_CONVENTION
static bool
run_bootstrap(JSContext       *cx,
              const char      *bootstrap_script,
              JS::HandleObject global)
{
    GjsAutoChar uri = g_strdup_printf(
        "resource:///org/gnome/gjs/modules/script/_bootstrap/%s.js",
        bootstrap_script);

    JSAutoRealm ar(cx, global);

    JS::CompileOptions options(cx);
    options.setFileAndLine(uri, 1).setSourceIsLazy(true);

    char* script;
    size_t script_len;
    if (!gjs_load_internal_source(cx, uri, &script, &script_len))
        return false;

    JS::SourceText<mozilla::Utf8Unit> source;
    if (!source.init(cx, script, script_len,
                     JS::SourceOwnership::TakeOwnership))
        return false;

    JS::RootedScript compiled_script(cx, JS::Compile(cx, options, source));
    if (!compiled_script)
        return false;

    JS::RootedValue ignored(cx);
    return JS::CloneAndExecuteScript(cx, compiled_script, &ignored);
}

GJS_JSAPI_RETURN_CONVENTION
static bool
gjs_log(JSContext *cx,
        unsigned   argc,
        JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);

    if (argc != 1) {
        gjs_throw(cx, "Must pass a single argument to log()");
        return false;
    }

    /* JS::ToString might throw, in which case we will only log that the value
     * could not be converted to string */
    JS::AutoSaveExceptionState exc_state(cx);
    JS::RootedString jstr(cx, JS::ToString(cx, argv[0]));
    exc_state.restore();

    if (!jstr) {
        g_message("JS LOG: <cannot convert value to string>");
        return true;
    }

    JS::UniqueChars s(JS_EncodeStringToUTF8(cx, jstr));
    if (!s)
        return false;

    g_message("JS LOG: %s", s.get());

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
gjs_log_error(JSContext *cx,
              unsigned   argc,
              JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);

    if ((argc != 1 && argc != 2) || !argv[0].isObject()) {
        gjs_throw(cx, "Must pass an exception and optionally a message to logError()");
        return false;
    }

    JS::RootedString jstr(cx);

    if (argc == 2) {
        /* JS::ToString might throw, in which case we will only log that the
         * value could not be converted to string */
        JS::AutoSaveExceptionState exc_state(cx);
        jstr = JS::ToString(cx, argv[1]);
        exc_state.restore();
    }

    gjs_log_exception_full(cx, argv[0], jstr, G_LOG_LEVEL_WARNING);

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
gjs_print_parse_args(JSContext              *cx,
                     const JS::CallArgs&    argv,
                     GjsAutoChar            *buffer)
{
    GString *str;
    guint n;

    str = g_string_new("");
    for (n = 0; n < argv.length(); ++n) {
        /* JS::ToString might throw, in which case we will only log that the
         * value could not be converted to string */
        JS::AutoSaveExceptionState exc_state(cx);
        JS::RootedString jstr(cx, JS::ToString(cx, argv[n]));
        exc_state.restore();

        if (jstr) {
            JS::UniqueChars s(JS_EncodeStringToUTF8(cx, jstr));
            if (!s) {
                g_string_free(str, true);
                return false;
            }

            g_string_append(str, s.get());
            if (n < (argv.length()-1))
                g_string_append_c(str, ' ');
        } else {
            *buffer = g_string_free(str, true);
            if (!*buffer)
                *buffer = g_strdup("<invalid string>");
            return true;
        }

    }
    *buffer = g_string_free(str, false);

    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
gjs_print(JSContext *context,
          unsigned   argc,
          JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp (argc, vp);

    GjsAutoChar buffer;
    if (!gjs_print_parse_args(context, argv, &buffer))
        return false;

    g_print("%s\n", buffer.get());

    argv.rval().setUndefined();
    return true;
}

GJS_JSAPI_RETURN_CONVENTION
static bool
gjs_printerr(JSContext *context,
             unsigned   argc,
             JS::Value *vp)
{
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);

    GjsAutoChar buffer;
    if (!gjs_print_parse_args(context, argv, &buffer))
        return false;

    g_printerr("%s\n", buffer.get());

    argv.rval().setUndefined();
    return true;
}

const JSClassOps defaultclassops = JS::DefaultGlobalClassOps;

class GjsGlobal {
    static constexpr JSClass klass = {
        // Keep this as "GjsGlobal" until Jasmine is upgraded to support
        // globalThis
        "GjsGlobal",
        JSCLASS_HAS_PRIVATE | JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(
                                  static_cast<uint32_t>(GjsGlobalSlot::LAST)),
        &defaultclassops,
    };

    static constexpr JSFunctionSpec static_funcs[] = {
        JS_FN("log", gjs_log, 1, GJS_MODULE_PROP_FLAGS),
        JS_FN("logError", gjs_log_error, 2, GJS_MODULE_PROP_FLAGS),
        JS_FN("print", gjs_print, 0, GJS_MODULE_PROP_FLAGS),
        JS_FN("printerr", gjs_printerr, 0, GJS_MODULE_PROP_FLAGS),
        JS_FS_END};

 public:
    GJS_USE
    static JSObject* create(JSContext* cx) {
        auto global = global_create_new(cx, &klass);

        return global;
    }

    GJS_USE
    static JSObject* create_with_compartment(JSContext* cx,
                                             JSObject* cmp_global) {
        auto global = global_create_with_existing(cx, cmp_global, &klass);

        return global;
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool define_properties(JSContext* cx, JS::HandleObject global,
                                  const char* realm_name,
                                  const char* bootstrap_script) {
        const GjsAtoms& atoms = GjsContextPrivate::atoms(cx);
        if (!JS_DefinePropertyById(cx, global, atoms.window(), global,
                                   JSPROP_READONLY | JSPROP_PERMANENT) ||
            !JS_DefineFunctions(cx, global, GjsGlobal::static_funcs))
            return false;

        JS::Realm* realm = JS::GetObjectRealmOrNull(global);
        g_assert(realm && "Global object must be associated with a realm");
        // const_cast is allowed here if we never free the realm data
        JS::SetRealmPrivate(realm, const_cast<char*>(realm_name));

        JS::Value v_importer =
            gjs_get_global_slot(global, GjsGlobalSlot::IMPORTS);
        g_assert(((void)"importer should be defined before passing null "
                        "importer to GjsGlobal::define_properties",
                  v_importer.isObject()));
        JS::RootedObject root_importer(cx, &v_importer.toObject());

        // Wrapping is a no-op if the importer is already in the same realm.
        if (!JS_WrapObject(cx, &root_importer) ||
            !JS_DefinePropertyById(cx, global, atoms.imports(), root_importer,
                                   GJS_MODULE_PROP_FLAGS))
            return false;

        if (bootstrap_script) {
            if (!run_bootstrap(cx, bootstrap_script, global))
                return false;
        }

        return true;
    }
};

class GjsDebuggerGlobal {
    static constexpr JSClass klass = {
        "GjsDebuggerGlobal",
        JSCLASS_GLOBAL_FLAGS_WITH_SLOTS(
            static_cast<uint32_t>(GjsGlobalSlot::LAST)),
        &defaultclassops,
    };

    static constexpr JSFunctionSpec static_funcs[] = {
        JS_FN("logError", gjs_log_error, 2, GJS_MODULE_PROP_FLAGS),
        JS_FN("print", gjs_print, 0, GJS_MODULE_PROP_FLAGS), JS_FS_END};

 public:
    GJS_USE
    static JSObject* create(JSContext* cx) {
        return global_create_new(cx, &klass);
    }

    GJS_USE
    static JSObject* create_with_compartment(JSContext* cx,
                                             JSObject* cmp_global) {
        return global_create_with_existing(cx, cmp_global, &klass);
    }

    static bool define_properties(JSContext* cx, JS::HandleObject global,
                                  const char* realm_name,
                                  const char* bootstrap_script) {
        const GjsAtoms& atoms = GjsContextPrivate::atoms(cx);
        if (!JS_DefinePropertyById(cx, global, atoms.window(), global,
                                   JSPROP_READONLY | JSPROP_PERMANENT) ||
            !JS_DefineFunctions(cx, global, GjsDebuggerGlobal::static_funcs))
            return false;

        JS::Realm* realm = JS::GetObjectRealmOrNull(global);
        g_assert(realm && "Global object must be associated with a realm");
        // const_cast is allowed here if we never free the realm data
        JS::SetRealmPrivate(realm, const_cast<char*>(realm_name));

        if (bootstrap_script) {
            if (!run_bootstrap(cx, bootstrap_script, global))
                return false;
        }

        return true;
    }
};

/**
 * gjs_create_global_object:
 * @cx: a #JSContext
 *
 * Creates a global object, and initializes it with the default API.
 *
 * Returns: the created global object on success, nullptr otherwise, in which
 * case an exception is pending on @cx
 */
JSObject* gjs_create_global_object(JSContext* cx, GjsGlobalType global_type,
                                   JSObject* current_global) {
    if (current_global) {
        switch (global_type) {
            case GjsGlobalType::DEFAULT:
                return GjsGlobal::create_with_compartment(cx, current_global);
            case GjsGlobalType::DEBUGGER:
                return GjsDebuggerGlobal::create_with_compartment(
                    cx, current_global);
            default:
                return nullptr;
        }
    }

    switch (global_type) {
        case GjsGlobalType::DEFAULT:
            return GjsGlobal::create(cx);
        case GjsGlobalType::DEBUGGER:
            return GjsDebuggerGlobal::create(cx);
        default:
            return nullptr;
    }
}

GjsGlobalType gjs_global_get_type(JSContext* cx) {
    auto global = JS::CurrentGlobalOrNull(cx);

    g_assert(global && "gjs_global_get_type called when no global is present");

    auto global_type = gjs_get_global_slot(global, GjsGlobalSlot::GLOBAL_TYPE);

    g_assert(global_type.isInt32() &&
             "Invalid type for GLOBAL_TYPE slot. Expected int32.");

    return static_cast<GjsGlobalType>(global_type.toInt32());
}

GjsGlobalType gjs_global_get_type(JSObject* global) {
    auto global_type = gjs_get_global_slot(global, GjsGlobalSlot::GLOBAL_TYPE);

    g_assert(global_type.isInt32() &&
             "Invalid type for GLOBAL_TYPE slot. Expected int32.");

    return static_cast<GjsGlobalType>(global_type.toInt32());
}

/**
 * gjs_define_global_properties:
 * @cx: a #JSContext
 * @global: a JS global object that has not yet been passed to this function
 * @realm_name: (nullable): name of the realm, for debug output
 * @bootstrap_script: (nullable): name of a bootstrap script (found at
 * resource://org/gnome/gjs/modules/script/_bootstrap/@bootstrap_script) or
 * %NULL for none
 *
 * Defines properties on the global object such as 'window' and 'imports', and
 * runs a bootstrap JS script on the global object to define any properties
 * that can be defined from JS.
 * This function completes the initialization of a new global object, but it
 * is separate from gjs_create_global_object() because all globals share the
 * same root importer.
 * The code creating the main global for the JS context needs to create the
 * root importer in between calling gjs_create_global_object() and
 * gjs_define_global_properties().
 *
 * The caller of this function should be in the realm for @global.
 * If the root importer object belongs to a different realm, this function will
 * create a wrapper for it.
 *
 * Returns: true on success, false otherwise, in which case an exception is
 * pending on @cx
 */
bool gjs_define_global_properties(JSContext* cx, JS::HandleObject global,
                                  GjsGlobalType global_type,
                                  const char* realm_name,
                                  const char* bootstrap_script) {
    gjs_set_global_slot(global.get(), GjsGlobalSlot::GLOBAL_TYPE,
                        JS::Int32Value(static_cast<uint32_t>(global_type)));

    switch (global_type) {
        case GjsGlobalType::DEFAULT:
            return GjsGlobal::define_properties(cx, global, realm_name,
                                                bootstrap_script);
        case GjsGlobalType::DEBUGGER:
            return GjsDebuggerGlobal::define_properties(cx, global, realm_name,
                                                        bootstrap_script);
    }

    return false;
}

template <typename GlobalSlot>
void gjs_set_global_slot(JSObject* global, GlobalSlot slot, JS::Value value) {
    JS_SetReservedSlot(
        global, JSCLASS_GLOBAL_SLOT_COUNT + static_cast<uint32_t>(slot), value);
}
template void gjs_set_global_slot(JSObject* global, GjsGlobalSlot slot,
                                  JS::Value value);

template <typename GlobalSlot>
JS::Value gjs_get_global_slot(JSObject* global, GlobalSlot slot) {
    return JS_GetReservedSlot(
        global, JSCLASS_GLOBAL_SLOT_COUNT + static_cast<uint32_t>(slot));
}
template JS::Value gjs_get_global_slot(JSObject* global, GjsGlobalSlot slot);

decltype(GjsGlobal::klass) constexpr GjsGlobal::klass;
decltype(GjsGlobal::static_funcs) constexpr GjsGlobal::static_funcs;

decltype(GjsDebuggerGlobal::klass) constexpr GjsDebuggerGlobal::klass;
decltype(
    GjsDebuggerGlobal::static_funcs) constexpr GjsDebuggerGlobal::static_funcs;
