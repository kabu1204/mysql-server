/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

#include "adapter_global.h"
#include "unified_debug.h"
#include "ColumnProxy.h"

using namespace v8;

Handle<Value> ColumnProxy::get(char *buffer) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> val;
  
  if(! isLoaded) {    
    val = handler->read(buffer);
    jsValue = Persistent<Value>::New(val);
    isLoaded = true;
  }
  return scope.Close(jsValue);
}


void ColumnProxy::set(Handle<Value> newValue) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> val = newValue;
  
  /* Drop our claim on the old value */
  if(! jsValue.IsEmpty()) jsValue.Dispose();
  
  isNull = jsValue->IsNull();
  isDirty = true;  
  jsValue = Persistent<Value>::New(val);
}


Handle<Value> ColumnProxy::write(char *buffer) {
  HandleScope scope;
  DEBUG_MARKER(UDEB_DEBUG);
  Handle<Value> rval;

  if(isDirty || (jsValue->IsObject() && jsValue->ToObject()->IsDirty())) {
    rval = handler->write(jsValue, buffer);
  }
  isDirty = false;
  
  return scope.Close(rval);
}

