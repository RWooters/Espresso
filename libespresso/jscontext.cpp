// This file is part of the VroomJs library.
//
// Author:
//     Federico Di Gregorio <fog@initd.org>
//
// Copyright © 2013 Federico Di Gregorio <fog@initd.org>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

// MIT, 2015-2019, EngineKit, brezza92

#include <iostream>
#include <vector>
#include "espresso.h"

using namespace v8;

long js_mem_debug_context_count;

JsContext* JsContext::New(int id, JsEngine* engine) {
  JsContext* context = new JsContext();
  if (context != NULL) {
    context->id_ = id;
    context->engine_ = engine;
    context->isolate_ = engine->GetIsolate();

    Locker locker(context->isolate_);
    Isolate::Scope isolate_scope(context->isolate_);
    HandleScope scope(context->isolate_);
    context->context_ = new Persistent<Context>(
        context->isolate_, Context::New(context->isolate_));
  }
  return context;
}
JsContext* JsContext::NewFromExistingContext(
    int id, JsEngine* engine, Persistent<Context>* nativeJsContext) {
  JsContext* context = new JsContext();
  if (context != NULL) {
    context->id_ = id;
    context->engine_ = engine;
    context->isolate_ = engine->GetIsolate();

    Locker locker(context->isolate_);
    Isolate::Scope isolate_scope(context->isolate_);
    HandleScope scope(context->isolate_);
    context->context_ = nativeJsContext;
  }
  return context;
}
void JsContext::Dispose() {
  if (engine_->GetIsolate() != NULL) {
    Locker locker(isolate_);
    Isolate::Scope isolate_scope(isolate_);
    context_->Reset();
    delete context_;
  }
}

void JsContext::Execute(const uint16_t* str,
                        const uint16_t* resourceName,
                        jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  v8::Local<v8::String> source;

  String::NewFromTwoByte(isolate_, str).ToLocal(&source);
  v8::Local<v8::Script> script;

  if (resourceName != NULL) {
    v8::Local<v8::String> name;
    String::NewFromTwoByte(isolate_, resourceName).ToLocal(&name);

    ScriptOrigin scriptOrg(name);

    script = Script::Compile(isolate_->GetCurrentContext(), source, &scriptOrg)
                 .ToLocalChecked();

  } else {
    script = Script::Compile(isolate_->GetCurrentContext(), source, nullptr)
                 .ToLocalChecked();
  }
  ///
  if (!script.IsEmpty()) {
    Local<Value> result;
    script->Run(isolate_->GetCurrentContext()).ToLocal(&result);

    if (result.IsEmpty()) {
      engine_->ErrorFromV8(trycatch, output);
    } else {
      engine_->AnyFromV8(result, Local<Object>(), output);
    }
  } else {
    engine_->ErrorFromV8(trycatch, output);
  }

  ctx->Exit();
}
// TODO: JS_VALUE
void JsContext::Execute(JsScript* jsscript, jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();
  TryCatch trycatch(isolate_);
  Persistent<Script>* script = jsscript->GetScript();
  Local<Script> scriptHandle = Local<Script>::New(isolate_, *script);  // 0.12.x
  if (!((*script).IsEmpty())) {
    Local<Value> result;
    scriptHandle->Run(isolate_->GetCurrentContext()).ToLocal(&result);
    if (result.IsEmpty())
      engine_->ErrorFromV8(trycatch, output);
    else
      engine_->AnyFromV8(result, Local<Object>(), output);
  }

  ctx->Exit();
}

void JsContext::SetVariable(const uint16_t* name,
                            jsvalue* value,
                            jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  Local<Value> v = engine_->AnyToV8(value, id_);
  Local<v8::String> var_name;
  String::NewFromTwoByte(isolate_, name).ToLocal(&var_name);
  ctx->Global()->Set(ctx, var_name, v);

  // if (ctx->Global()->Set(String::NewFromTwoByte(isolate_, name), v) ==
  //    false) {  // 0.12.x
  //  // TODO: Return an error if set failed.
  //}

  ctx->Exit();

  engine_->AnyFromV8(Null(isolate_), Local<Object>(), output);
}
void JsContext::GetGlobal(jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<Value> value = ctx->Global();
  if (!value.IsEmpty()) {
    engine_->AnyFromV8(value, Local<Object>(), output);
  } else {
    engine_->ErrorFromV8(trycatch, output);
  }

  ctx->Exit();
}

void JsContext::GetVariable(const uint16_t* name, jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<String> key;
  String::NewFromTwoByte(isolate_, name).ToLocal(&key);

  Local<Value> value;
  if (ctx->Global()->Get(ctx, key).ToLocal(&value) && !value.IsEmpty()) {
    engine_->AnyFromV8(value, Local<Object>(), output);
  } else {
    engine_->ErrorFromV8(trycatch, output);
  }

  ctx->Exit();
}

void JsContext::GetPropertyNames(Persistent<Object>* obj, jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<Object> objLocal = Local<Object>::New(isolate_, *obj);
  Local<Value> value = objLocal->GetPropertyNames(ctx).ToLocalChecked();
  if (!value.IsEmpty()) {
    engine_->AnyFromV8(value, Local<Object>(), output);
  } else {
    engine_->ErrorFromV8(trycatch, output);
  }
  ctx->Exit();
}

void JsContext::GetPropertyValue(Persistent<Object>* obj,
                                 const uint16_t* name,
                                 jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<Object> objLocal = Local<Object>::New(isolate_, *obj);
  Local<v8::String> key;
  String::NewFromTwoByte(isolate_, name).ToLocal(&key);
  Local<Value> value;

  if (objLocal->Get(ctx, key).ToLocal(&value) && !value.IsEmpty()) {
    Local<v8::Object> obj_handle = Local<v8::Object>::New(isolate_, *obj);
    engine_->AnyFromV8(value, obj_handle, output);
  } else {
    engine_->ErrorFromV8(trycatch, output);
  }
  //                     // Local<Value> value = objLocal->Get(ctx,
  //                     // String::NewFromTwoByte(isolate_, name));

  //                     if (!value.IsEmpty()) {
  //  // TODO-> review here
  //  Local<v8::Object> obj_handle = Local<v8::Object>::New(isolate_, *obj);
  //  engine_->AnyFromV8(value, obj_handle, output);
  //}
  // else {
  //  engine_->ErrorFromV8(trycatch, output);
  //}
  ctx->Exit();
}

void JsContext::SetPropertyValue(Persistent<Object>* obj,
                                 const uint16_t* name,
                                 jsvalue* value,
                                 jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  Local<Value> v = engine_->AnyToV8(value, id_);

  Local<Object> objLocal = Local<Object>::New(isolate_, *obj);

  Local<String> prop_name;
  String::NewFromTwoByte(isolate_, name).ToLocal(&prop_name);
  objLocal->Set(ctx, prop_name, v);

  //if (objLocal->Set(String::NewFromTwoByte(isolate_, name), v) == false) {
  //  // TODO: Return an error if set failed.
  //}

  ctx->Exit();

  engine_->AnyFromV8(Null(isolate_), Local<Object>(), output);
}
void JsContext::InvokeFunction(Persistent<Function>* func,
                               Persistent<Object>* thisArg,
                               jsvalue* args,
                               jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<Function> prop = Local<Function>::New(isolate_, *func);
  if (prop.IsEmpty() || !prop->IsFunction()) {
    v8::Local<v8::String> str_value;
    String::NewFromUtf8(isolate_, "isn't a function").ToLocal(&str_value);
    engine_->StringFromV8(str_value, output);

    output->type = JSVALUE_TYPE_STRING_ERROR;
    return;  //?
  }
  Local<Object> reciever = Local<Object>::New(isolate_, *thisArg);
  if (reciever.IsEmpty()) {
    reciever = ctx->Global();
  }

  else {
    std::vector<Local<Value> > argv(args->i32);  // i32 as length
    engine_->ArrayToV8Args(args, id_, &argv[0]);
    // TODO: Check ArrayToV8Args return value (but right now can't fail, right?)
    Local<Function> func = Local<Function>::Cast(prop);
    Local<Value> value =
        func->Call(ctx, reciever, args->i32, &argv[0]).ToLocalChecked();
    if (!value.IsEmpty()) {
      engine_->AnyFromV8(value, Local<Object>(), output);
    } else {
      engine_->ErrorFromV8(trycatch, output);
    }
  }
  ctx->Exit();
}

void JsContext::InvokeProperty(Persistent<Object>* obj,
                               const uint16_t* name,
                               jsvalue* args,
                               jsvalue* output) {
  Locker locker(isolate_);
  Isolate::Scope isolate_scope(isolate_);
  HandleScope scope(isolate_);

  Local<Context> ctx = Local<Context>::New(isolate_, *context_);
  ctx->Enter();

  TryCatch trycatch(isolate_);

  Local<Object> objLocal = Local<Object>::New(isolate_, *obj);

  Local<v8::String> key;
  String::NewFromTwoByte(isolate_, name).ToLocal(&key);

  Local<Value> prop;
  if (!objLocal->Get(ctx, key).ToLocal(&prop) || !prop.IsEmpty() ||
      !prop->IsFunction()) {
    std::string err_msg1 = "property not found or isn't a function";
    v8::Local<v8::String> err_msg;
    String::NewFromUtf8(isolate_, err_msg1.c_str()).ToLocal(&err_msg);

    engine_->StringFromV8(err_msg, output);
    output->type = JSVALUE_TYPE_STRING_ERROR;
  } else {
    std::vector<Local<Value> > argv(args->i32);  // i32 as length
    engine_->ArrayToV8Args(args, id_, &argv[0]);
    // TODO: Check ArrayToV8Args return value (but right now can't fail, right?)
    Local<Function> func = Local<Function>::Cast(prop);

    Local<Value> value =
        func->Call(ctx, objLocal, args->i32, &argv[0]).ToLocalChecked();
    if (!value.IsEmpty()) {
      engine_->AnyFromV8(value, Local<Object>(), output);
    } else {
      engine_->ErrorFromV8(trycatch, output);
    }
  }

  ctx->Exit();
}

void JsContext::ConvAnyFromV8(Local<Value> value,
                              Local<Object> thisArg,
                              jsvalue* output) {
  this->engine_->AnyFromV8(value, thisArg, output);
}

Local<Value> JsContext::AnyToV8(jsvalue* v) {
  EscapableHandleScope h01(isolate_);
  return h01.Escape(
      Local<Value>::New(isolate_, this->engine_->AnyToV8(v, this->id_)));
}