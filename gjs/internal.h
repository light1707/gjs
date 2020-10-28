// SPDX-License-Identifier: MIT OR LGPL-2.0-or-later
// SPDX-FileCopyrightText: 2020 Evan Welsh <contact@evanwelsh.com>

#ifndef GJS_INTERNAL_H_
#define GJS_INTERNAL_H_

#include <config.h>

#include <js/TypeDecls.h>
#include <jsapi.h>

bool gjs_load_internal_module(JSContext* cx, const char* identifier);

bool CompileModule(JSContext* cx, unsigned argc, JS::Value* vp);

bool CompileInternalModule(JSContext* cx, unsigned argc, JS::Value* vp);

bool GetRegistry(JSContext* cx, unsigned argc, JS::Value* vp);

bool ImportSync(JSContext* cx, unsigned argc, JS::Value* vp);

bool SetModuleLoadHook(JSContext* cx, unsigned argc, JS::Value* vp);

bool SetModuleMetaHook(JSContext* cx, unsigned argc, JS::Value* vp);

bool SetModulePrivate(JSContext* cx, unsigned argc, JS::Value* vp);

bool SetModuleResolveHook(JSContext* cx, unsigned argc, JS::Value* vp);

#endif  // GJS_INTERNAL_H_
