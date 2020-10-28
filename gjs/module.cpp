/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil; -*- */
// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2017 Philip Chimento <philip.chimento@gmail.com>
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#include <config.h>

#include <stddef.h>     // for size_t
#include <sys/types.h>  // for ssize_t

#include <string>  // for u16string

#include <gio/gio.h>
#include <glib.h>

#include <js/Class.h>
#include <js/CompilationAndEvaluation.h>
#include <js/CompileOptions.h>
#include <js/GCVector.h>  // for RootedVector
#include <js/Modules.h>
#include <js/PropertyDescriptor.h>
#include <js/RootingAPI.h>
#include <js/SourceText.h>
#include <js/TypeDecls.h>
#include <js/Value.h>  // for Value
#include <jsapi.h>  // for JS_DefinePropertyById, ...

#include "gjs/context-private.h"
#include "gjs/global.h"
#include "gjs/jsapi-util.h"
#include "gjs/mem-private.h"
#include "gjs/module.h"
#include "util/log.h"

class GjsScriptModule {
    char *m_name;

    GjsScriptModule(const char* name) {
        m_name = g_strdup(name);
        GJS_INC_COUNTER(module);
    }

    ~GjsScriptModule() {
        g_free(m_name);
        GJS_DEC_COUNTER(module);
    }

    /* Private data accessors */

    [[nodiscard]] static inline GjsScriptModule* priv(JSObject* module) {
        return static_cast<GjsScriptModule*>(JS_GetPrivate(module));
    }

    /* Creates a JS module object. Use instead of the class's constructor */
    [[nodiscard]] static JSObject* create(JSContext* cx, const char* name) {
        JSObject* module = JS_NewObject(cx, &GjsScriptModule::klass);
        JS_SetPrivate(module, new GjsScriptModule(name));
        return module;
    }

    /* Defines the empty module as a property on the importer */
    GJS_JSAPI_RETURN_CONVENTION
    bool
    define_import(JSContext       *cx,
                  JS::HandleObject module,
                  JS::HandleObject importer,
                  JS::HandleId     name) const
    {
        if (!JS_DefinePropertyById(cx, importer, name, module,
                                   GJS_MODULE_PROP_FLAGS & ~JSPROP_PERMANENT)) {
            gjs_debug(GJS_DEBUG_IMPORTER, "Failed to define '%s' in importer",
                      m_name);
            return false;
        }

        return true;
    }

    /* Carries out the actual execution of the module code */
    GJS_JSAPI_RETURN_CONVENTION
    bool evaluate_import(JSContext* cx, JS::HandleObject module,
                         const char* script, ssize_t script_len,
                         const char* filename) {
        std::u16string utf16_string =
            gjs_utf8_script_to_utf16(script, script_len);
        // COMPAT: This could use JS::SourceText<mozilla::Utf8Unit> directly,
        // but that messes up code coverage. See bug
        // https://bugzilla.mozilla.org/show_bug.cgi?id=1404784
        JS::SourceText<char16_t> buf;
        if (!buf.init(cx, utf16_string.c_str(), utf16_string.size(),
                      JS::SourceOwnership::Borrowed))
            return false;

        JS::RootedObjectVector scope_chain(cx);
        if (!scope_chain.append(module)) {
            JS_ReportOutOfMemory(cx);
            return false;
        }

        JS::CompileOptions options(cx);
        options.setFileAndLine(filename, 1);

        JS::RootedValue ignored_retval(cx);
        if (!JS::Evaluate(cx, scope_chain, options, buf, &ignored_retval))
            return false;

        GjsContextPrivate* gjs = GjsContextPrivate::from_cx(cx);
        gjs->schedule_gc_if_needed();

        gjs_debug(GJS_DEBUG_IMPORTER, "Importing module %s succeeded", m_name);

        return true;
    }

    /* Loads JS code from a file and imports it */
    GJS_JSAPI_RETURN_CONVENTION
    bool
    import_file(JSContext       *cx,
                JS::HandleObject module,
                GFile           *file)
    {
        GError *error = nullptr;
        char *unowned_script;
        size_t script_len = 0;

        if (!(g_file_load_contents(file, nullptr, &unowned_script, &script_len,
                                   nullptr, &error)))
            return gjs_throw_gerror_message(cx, error);

        GjsAutoChar script = unowned_script;  /* steals ownership */
        g_assert(script);

        GjsAutoChar full_path = g_file_get_parse_name(file);
        return evaluate_import(cx, module, script, script_len, full_path);
    }

    /* JSClass operations */

    GJS_JSAPI_RETURN_CONVENTION
    bool
    resolve_impl(JSContext       *cx,
                 JS::HandleObject module,
                 JS::HandleId     id,
                 bool            *resolved)
    {
        JS::RootedObject lexical(cx, JS_ExtensibleLexicalEnvironment(module));
        if (!lexical) {
            *resolved = false;
            return true;  /* nothing imported yet */
        }

        if (!JS_HasPropertyById(cx, lexical, id, resolved))
            return false;
        if (!*resolved)
            return true;

        /* The property is present in the lexical environment. This should not
         * be supported according to ES6. For compatibility with earlier GJS,
         * we treat it as if it were a real property, but warn about it. */

        g_warning("Some code accessed the property '%s' on the module '%s'. "
                  "That property was defined with 'let' or 'const' inside the "
                  "module. This was previously supported, but is not correct "
                  "according to the ES6 standard. Any symbols to be exported "
                  "from a module must be defined with 'var'. The property "
                  "access will work as previously for the time being, but "
                  "please fix your code anyway.",
                  gjs_debug_id(id).c_str(), m_name);

        JS::Rooted<JS::PropertyDescriptor> desc(cx);
        return JS_GetPropertyDescriptorById(cx, lexical, id, &desc) &&
            JS_DefinePropertyById(cx, module, id, desc);
    }

    GJS_JSAPI_RETURN_CONVENTION
    static bool
    resolve(JSContext       *cx,
            JS::HandleObject module,
            JS::HandleId     id,
            bool            *resolved)
    {
        return priv(module)->resolve_impl(cx, module, id, resolved);
    }

    static void finalize(JSFreeOp*, JSObject* module) { delete priv(module); }

    static constexpr JSClassOps class_ops = {
        nullptr,  // addProperty
        nullptr,  // deleteProperty
        nullptr,  // enumerate
        nullptr,  // newEnumerate
        &GjsScriptModule::resolve,
        nullptr,  // mayResolve
        &GjsScriptModule::finalize,
    };

    static constexpr JSClass klass = {
        "GjsScriptModule",
        JSCLASS_HAS_PRIVATE | JSCLASS_BACKGROUND_FINALIZE,
        &GjsScriptModule::class_ops,
    };

 public:
    /* Carries out the import operation */
    GJS_JSAPI_RETURN_CONVENTION
    static JSObject *
    import(JSContext       *cx,
           JS::HandleObject importer,
           JS::HandleId     id,
           const char      *name,
           GFile           *file)
    {
        JS::RootedObject module(cx, GjsScriptModule::create(cx, name));
        if (!module ||
            !priv(module)->define_import(cx, module, importer, id) ||
            !priv(module)->import_file(cx, module, file))
            return nullptr;

        return module;
    }
};

/**
 * gjs_module_import:
 * @cx: the JS context
 * @importer: the JS importer object, parent of the module to be imported
 * @id: module name in the form of a jsid
 * @name: module name, used for logging and identification
 * @file: location of the file to import
 *
 * Carries out an import of a GJS module.
 * Defines a property @name on @importer pointing to the module object, which
 * is necessary in the case of cyclic imports.
 * This property is not permanent; the caller is responsible for making it
 * permanent if the import succeeds.
 *
 * Returns: the JS module object, or nullptr on failure.
 */
JSObject *
gjs_module_import(JSContext       *cx,
                  JS::HandleObject importer,
                  JS::HandleId     id,
                  const char      *name,
                  GFile           *file)
{
    return GjsScriptModule::import(cx, importer, id, name, file);
}

decltype(GjsScriptModule::klass) constexpr GjsScriptModule::klass;
decltype(GjsScriptModule::class_ops) constexpr GjsScriptModule::class_ops;

/**
 * gjs_get_module_registry:
 *
 * Retrieves a global's native registry from the NATIVE_REGISTRY slot.
 * Registries are actually JS Maps.
 *
 * @param cx the current #JSContext
 * @param global a global #JSObject
 *
 * @returns the registry map as a #JSObject
 */
JSObject* gjs_get_native_registry(JSContext* cx, JSObject* global) {
    JS::Value native_registry =
        gjs_get_global_slot(global, GjsGlobalSlot::NATIVE_REGISTRY);

    g_assert(native_registry.isObject());

    JS::RootedObject root_registry(cx, &native_registry.toObject());

    return root_registry;
}

/**
 * gjs_get_module_registry:
 *
 * Retrieves a global's module registry from the MODULE_REGISTRY slot.
 * Registries are actually JS Maps.
 *
 * @param cx the current #JSContext
 * @param global a global #JSObject
 *
 * @returns the registry map as a #JSObject
 */
JSObject* gjs_get_module_registry(JSContext* cx, JSObject* global) {
    JS::Value esm_registry =
        gjs_get_global_slot(global, GjsGlobalSlot::MODULE_REGISTRY);

    g_assert(esm_registry.isObject());

    JS::RootedObject root_registry(cx, &esm_registry.toObject());

    return root_registry;
}

/**
 * gjs_module_load:
 *
 * Loads and registers a module given a specifier and
 * URI.
 *
 * @param importer the private value of the #Module object initiating the import
 *                 or undefined.
 * @param meta_object the import.meta object
 *
 * @returns whether an error occurred while resolving the specifier.
 */
JSObject* gjs_module_load(JSContext* cx, const char* identifier,
                          const char* file_uri) {
    JS::RootedValue id_value(cx), uri_value(cx);

    if (!gjs_string_from_utf8(cx, identifier, &id_value) ||
        !gjs_string_from_utf8(cx, file_uri, &uri_value)) {
        return nullptr;
    }

    JS::RootedString id(cx, id_value.toString()), uri(cx, uri_value.toString());

    g_assert((gjs_global_is_type(cx, GjsGlobalType::DEFAULT) ||
              gjs_global_is_type(cx, GjsGlobalType::INTERNAL)) &&
             "gjs_module_load can only be called from module-enabled "
             "globals.");

    JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

    JS::RootedValue hook(
        cx, gjs_get_global_slot(global, GjsGlobalSlot::MODULE_HOOK));

    JS::RootedValueArray<2> args(cx);
    args[0].setString(id);
    args[1].setString(uri);

    JS::RootedValue result(cx);

    if (!JS_CallFunctionValue(cx, nullptr, hook, args, &result)) {
        gjs_log_exception(cx);
        return nullptr;
    }

    g_assert(result.isObject() && "Module hook failed to return an object!");

    JS::RootedObject module(cx, &result.toObject());

    return module;
}

/**
 * gjs_populate_module_meta:
 *
 * Hook SpiderMonkey calls to populate the import.meta object.
 *
 * @param private_ref the private value for the #Module object
 * @param meta_object the import.meta object
 *
 * @returns whether an error occurred while populating the module meta.
 */
bool gjs_populate_module_meta(JSContext* cx, JS::Handle<JS::Value> private_ref,
                              JS::Handle<JSObject*> meta_object_handle) {
    JS::RootedObject meta(cx, meta_object_handle);

    if (private_ref.isObject()) {
        JS::RootedObject module(cx, &private_ref.toObject());

        JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

        JS::RootedValue hook(
            cx, gjs_get_global_slot(global, GjsGlobalSlot::META_HOOK));

        JS::RootedValueArray<3> args(cx);
        args[0].setObject(*module);
        args[1].setObject(*meta);

        JS::RootedValue result(cx);

        JS::RootedValue ignore_result(cx);
        if (!JS_CallFunctionValue(cx, nullptr, hook, args, &ignore_result)) {
            gjs_log_exception(cx);
            return false;
        }
    }

    return true;
}

/**
 * gjs_module_resolve:
 *
 * Hook SpiderMonkey calls to resolve import specifiers.
 *
 * @param importer the private value of the #Module object initiating the import
 *                 or undefined.
 * @param meta_object the import.meta object
 *
 * @returns whether an error occurred while resolving the specifier.
 */
JSObject* gjs_module_resolve(JSContext* cx, JS::HandleValue importer,
                             JS::HandleString specifier) {
    g_assert((gjs_global_is_type(cx, GjsGlobalType::DEFAULT) ||
              gjs_global_is_type(cx, GjsGlobalType::INTERNAL)) &&
             "gjs_module_resolve can only be called from module-enabled "
             "globals.");

    JS::RootedObject global(cx, JS::CurrentGlobalOrNull(cx));

    JS::RootedValue hookValue(
        cx, gjs_get_global_slot(global, GjsGlobalSlot::IMPORT_HOOK));

    JS::RootedValueArray<3> args(cx);
    args[0].set(importer);
    args[1].setString(specifier);

    JS::RootedValue result(cx);

    if (!JS_CallFunctionValue(cx, nullptr, hookValue, args, &result)) {
        gjs_log_exception(cx);
        return nullptr;
    }

    JS::RootedObject module(cx, &result.toObject());

    return module;
}
