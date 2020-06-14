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

#ifndef GJS_INTERNAL_H_
#define GJS_INTERNAL_H_

#include <config.h>
#include <gio/gio.h>
#include <js/GCHashTable.h>
#include <js/GCVector.h>
#include <js/TypeDecls.h>
#include <jsapi.h>        // for JS_GetContextPrivate
#include <jsfriendapi.h>  // for ScriptEnvironmentPreparer

#include <string>

#include "gjs/macros.h"

bool gjs_load_internal_script(JSContext* cx, const char* identifier);

// setModuleResolveHook
bool SetModuleResolveHook(JSContext* cx, unsigned argc, JS::Value* vp);

// compileAndEvalModule(id: string)
bool CompileAndEvalModule(JSContext* cx, unsigned argc, JS::Value* vp);

// registerModule(id: string, path: string, text: string, length: number, ?:
// boolean)
bool RegisterModule(JSContext* cx, unsigned argc, JS::Value* vp);

// registerInternalModule(id: string, path: string, text: string, length:
// number, is_legacy: boolean)
bool RegisterInternalModule(JSContext* cx, unsigned argc, JS::Value* vp);

// lookupInternalModule(id: string)
bool LookupInternalModule(JSContext* cx, unsigned argc, JS::Value* vp);

// lookupModule(id: string)
bool LookupModule(JSContext* cx, unsigned argc, JS::Value* vp);

// debug(msg: string)
bool Debug(JSContext* cx, unsigned argc, JS::Value* vp);

// getModuleURI(module): string
bool GetModuleURI(JSContext* cx, unsigned argc, JS::Value* vp);

#endif  // GJS_INTERNAL_H_
