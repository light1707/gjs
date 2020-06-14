/*
 * Copyright (c) 2020 Evan Welsh <contact@evanwelsh.com>
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

#include "gjs/internal.h"

#include <config.h>
#include <gio/gio.h>
#include <girepository.h>
#include <glib-object.h>
#include <glib.h>
#include <js/Class.h>
#include <js/CompilationAndEvaluation.h>
#include <js/CompileOptions.h>
#include <js/Conversions.h>
#include <js/GCVector.h>  // for RootedVector
#include <js/Promise.h>
#include <js/PropertyDescriptor.h>
#include <js/RootingAPI.h>
#include <js/SourceText.h>
#include <js/TypeDecls.h>
#include <jsapi.h>      // for JS_DefinePropertyById, ...
#include <stddef.h>     // for size_t
#include <sys/types.h>  // for ssize_t

#include <codecvt>  // for codecvt_utf8_utf16
#include <locale>   // for wstring_convert
#include <string>   // for u16string
#include <vector>

#include "gjs/context-private.h"
#include "gjs/context.h"
#include "gjs/error-types.h"
#include "gjs/global.h"
#include "gjs/importer.h"
#include "gjs/jsapi-util.h"
#include "gjs/mem-private.h"
#include "gjs/module.h"
#include "gjs/native.h"
#include "util/log.h"

// You have to be very careful in this file to only do operations within the
// correct global!
using AutoGFile = GjsAutoUnref<GFile>;

bool gjs_load_internal_script(JSContext* cx, const char* identifier) {
    GjsAutoChar full_path(g_strdup_printf(
        "resource://org/gnome/gjs/gjs/internal/%s.js", identifier));
    AutoGFile gfile(g_file_new_for_uri(full_path));

    char* script_text_raw;
    gsize script_text_len;
    GError* error = nullptr;

    if (!g_file_load_contents(gfile, NULL, &script_text_raw, &script_text_len,
                              nullptr, &error)) {
        gjs_throw(cx, "Failed to read internal resource: %s \n%s",
                  full_path.get(), error->message);
        return false;
    }

    GjsAutoChar script_text(script_text_raw);

    JS::CompileOptions options(cx);
    options.setIntroductionType("Internal Script Loader");
    options.setFileAndLine(full_path, 1);
    options.setSelfHostingMode(false);

    std::u16string utf16_string =
        gjs_utf8_script_to_utf16(script_text, script_text_len);
    // COMPAT: This could use JS::SourceText<mozilla::Utf8Unit> directly,
    // but that messes up code coverage. See bug
    // https://bugzilla.mozilla.org/show_bug.cgi?id=1404784
    JS::SourceText<char16_t> buf;
    if (!buf.init(cx, utf16_string.c_str(), utf16_string.size(),
                  JS::SourceOwnership::Borrowed))
        return false;

    JS::RootedObject internal_global(cx, gjs_get_internal_global(cx));

    JSAutoRealm ar(cx, internal_global);

    JS::RootedValue ignored_retval(cx);
    JS::RootedObject module(cx, JS_NewPlainObject(cx));
    JS::RootedObjectVector scope_chain(cx);

    if (!scope_chain.append(module)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    auto success = JS::Evaluate(cx, scope_chain, options, buf, &ignored_retval);

    if (!success) {
        gjs_log_exception(cx);
        return false;
    }
    auto add = gjs_get_internal_script_registry(cx)->lookupForAdd(identifier);

    if (add.found()) {
        gjs_throw(cx, "Internal script %s already loaded.", identifier);
        return false;
    }

    GjsAutoChar iden(g_strdup(identifier));

    if (!gjs_get_internal_script_registry(cx)->add(add, iden.get(), module)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }
    return true;
}

bool GetModuleURI(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "getModuleURI", 1)) {
        return false;
    }

    JS::RootedValue importer(cx, args[0]);

    if (importer.isUndefined()) {
        gjs_throw(cx,
                  "Cannot import from relative path when module path "
                  "is unknown.");
        return false;
    }
    // The module from which the resolve request is coming
    GjsESModule* priv_module = static_cast<GjsESModule*>(importer.toPrivate());
    // Get the module's path.
    auto module_location = priv_module->uri();
    // Get the module's directory.
    const gchar* module_file_location = module_location.c_str();
    return gjs_string_from_utf8(cx, module_file_location, args.rval());
}

bool SetModuleResolveHook(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);
    if (!args.requireAtLeast(cx, "setModuleResolveHook", 1)) {
        return false;
    }

    JS::RootedValue mv(cx, args[0]);

    // The hook is stored in the internal global.
    JS::RootedObject global(cx, gjs_get_internal_global(cx));
    gjs_set_global_slot(global, GjsInternalGlobalSlot::IMPORT_HOOK, mv);

    args.rval().setUndefined();
    return true;
}

bool CompileAndEvalModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "compileAndEvalModule", 1)) {
        return false;
    }

    JS::RootedString s1(cx, args[0].toString());

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, s1);

    {
        JSAutoRealm ar(cx, gjs_get_import_global(cx));
        auto registry = gjs_get_esm_registry(cx);
        auto result = registry->lookup(id.get());
        if (result) {
            JS::RootedObject res(cx, result->value());
            auto init = JS::ModuleInstantiate(cx, res);
            if (!init) {
                gjs_log_exception(cx);
            }

            auto eval = JS::ModuleEvaluate(cx, res);

            if (!eval) {
                gjs_log_exception(cx);
            }

            args.rval().setBoolean(init && eval);
        } else {
            args.rval().setBoolean(false);
        }
    }

    return true;
}

bool gjs_require_module(JSContext* m_cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs argv = JS::CallArgsFromVp(argc, vp);

    if (argc != 1) {
        gjs_throw(m_cx, "Must pass a single argument to require()");
        return false;
    }

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

    auto native_registry = gjs_get_native_module_registry(m_cx);

    auto nativeModuleAdd = native_registry->lookupForAdd(id.get());

    if (nativeModuleAdd.found()) {
        JS::RootedObject obj(m_cx, nativeModuleAdd->value().get());

        argv.rval().setObject(*obj);
        return true;
    }

    JS::RootedObject native_obj(m_cx);

    if (!gjs_load_native_module(m_cx, id.get(), &native_obj)) {
        gjs_throw(m_cx, "Failed to load native module: %s", id.get());
        return false;
    }

    if (!native_registry->add(nativeModuleAdd, id.get(), native_obj)) {
        JS_ReportOutOfMemory(m_cx);
        return false;
    }

    argv.rval().setObject(*native_obj);
    return true;
}

static bool register_module(JSContext* cx, const char* identifier,
                            const char* path, const char* text, size_t length,
                            bool* success) {
    auto esm_registry = gjs_get_esm_registry(cx);

    auto it = esm_registry->lookupForAdd(path);

    if (it.found()) {
        gjs_throw(cx, "Module '%s' already registered", path);
        return false;
    }

    auto module = new GjsESModule(identifier, path);

    JS::RootedObject module_record(cx, module->compile(cx, text, length));

    if (module_record && !esm_registry->add(it, identifier, module_record)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    *success = module_record;

    return true;
}

// registerModule(id: string, path: string, text: string, length: number,
// unused: boolean)
bool RegisterModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "registerModule", 5)) {
        return false;
    }

    JS::RootedString str0(cx, args[0].toString()),  // id
        str1(cx, args[1].toString()),               // path
        str2(cx, args[2].toString());               // text

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, str0);
    JS::UniqueChars path = JS_EncodeStringToUTF8(cx, str1);
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, str2);
    auto length = args[3].toInt32();

    {
        JSAutoRealm ar(cx, gjs_get_import_global(cx));

        bool success = false;
        bool result = register_module(cx, id.get(), path.get(), text.get(),
                                      length, &success);

        args.rval().setBoolean(success);

        return result;
    }
}

static bool register_internal_module(JSContext* cx, const char* identifier,
                                     const char* filename, const char* module,
                                     size_t module_len, bool* success) {
    auto internal_registry = gjs_get_internal_module_registry(cx);
    auto it = internal_registry->lookupForAdd(identifier);

    if (it.found()) {
        gjs_throw(cx, "Internal module '%s' is already registered", identifier);
        return false;
    }

    auto internal_module = new GjsESModule(identifier, filename, true);

    JS::RootedObject module_record(
        cx, internal_module->compile(cx, module, module_len));

    if (module_record &&
        !internal_registry->add(it, identifier, module_record)) {
        JS_ReportOutOfMemory(cx);
        return false;
    }

    *success = module_record;

    return true;
}

// registerInternalModule(id: string, path: string, text: string, length:
// number)
bool RegisterInternalModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "registerInternalModule", 4)) {
        return false;
    }

    JS::RootedString str0(cx, args[0].toString()),  // id
        str1(cx, args[1].toString()),               // path
        str2(cx, args[2].toString());               // text

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, str0);
    JS::UniqueChars path = JS_EncodeStringToUTF8(cx, str1);
    JS::UniqueChars text = JS_EncodeStringToUTF8(cx, str2);
    auto length = args[3].toInt32();

    {
        JSAutoRealm ar(cx, gjs_get_import_global(cx));

        bool success = false;
        bool result = register_internal_module(cx, id.get(), path.get(),
                                               text.get(), length, &success);
        args.rval().setBoolean(success);

        return result;
    }
}

// lookupInternalModule(id: string)
bool LookupInternalModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "lookupInternalModule", 1)) {
        return false;
    }

    JS::RootedString s1(cx, args[0].toString());

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, s1);

    {
        JSAutoRealm ar(cx, gjs_get_import_global(cx));
        auto registry = gjs_get_internal_module_registry(cx);
        auto it = registry->lookup(id.get());

        if (!it.found()) {
            args.rval().setNull();
            return true;
        }
        JS::RootedObject lookup(cx, it->value());
        if (!lookup) {
            args.rval().setNull();
        } else {
            args.rval().setObject(*lookup.get());
        }
    }
    return true;
}

// lookupModule(id: string)
bool LookupModule(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "lookupModule", 1)) {
        return false;
    }

    JS::RootedString s1(cx, args[0].toString());

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, s1);

    {
        JSAutoRealm ar(cx, gjs_get_import_global(cx));
        auto registry = gjs_get_esm_registry(cx);
        auto it = registry->lookup(id.get());

        if (!it.found()) {
            args.rval().setNull();
            return true;
        }
        JS::RootedObject lookup(cx, it->value());

        if (!lookup) {
            args.rval().setNull();
        } else {
            args.rval().setObject(*lookup.get());
        }
    }
    return true;
}

// debug(msg: string)
bool Debug(JSContext* cx, unsigned argc, JS::Value* vp) {
    JS::CallArgs args = CallArgsFromVp(argc, vp);

    if (!args.requireAtLeast(cx, "debug", 1)) {
        return false;
    }

    JS::RootedString s1(cx, args[0].toString());

    JS::UniqueChars id = JS_EncodeStringToUTF8(cx, s1);

    gjs_debug(GJS_DEBUG_IMPORTER, id.get());

    return true;
}
