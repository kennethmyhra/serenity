/*
 * Copyright (c) 2023, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Optional.h>
#include <AK/Variant.h>
#include <LibJS/Forward.h>
#include <LibJS/Heap/Cell.h>
#include <LibJS/Heap/GCPtr.h>
#include <LibJS/Runtime/ArrayBuffer.h>
#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/Realm.h>
#include <LibJS/Runtime/TypedArray.h>

namespace Web::WebIDL {

using BufferableObject = Variant<
    JS::NonnullGCPtr<JS::TypedArrayBase>,
    JS::NonnullGCPtr<JS::DataView>,
    JS::NonnullGCPtr<JS::ArrayBuffer>>;

class BufferableObjectBase : public JS::Object {
    JS_CELL(BufferableObjectBase, JS::Object);

public:
    virtual ~BufferableObjectBase() override = default;

    u32 byte_length() const;

    JS::GCPtr<JS::Object> raw_object();
    JS::GCPtr<JS::ArrayBuffer> array_buffer();

    BufferableObject const& bufferable_object() const { return *m_bufferable_object; }
    BufferableObject& bufferable_object() { return *m_bufferable_object; }

protected:
    BufferableObjectBase(JS::Realm&, JS::Handle<JS::Object>);

    void visit_edges(Visitor&) override;

    bool is_typed_array_base() const;
    bool is_data_view() const;
    bool is_array_buffer() const;

    Optional<BufferableObject> m_bufferable_object;
};

class ArrayBufferView : public BufferableObjectBase {
    JS_CELL(ArrayBufferView, BufferableObjectBase);

public:
    ArrayBufferView(JS::Realm&, JS::Handle<JS::Object>);
    virtual ~ArrayBufferView() override = default;

    using BufferableObjectBase::is_data_view;
    using BufferableObjectBase::is_typed_array_base;

    u32 byte_offset() const;
};

class BufferSource : public BufferableObjectBase {
    JS_CELL(BufferSource, BufferableObjectBase);

public:
    BufferSource(JS::Realm&, JS::Handle<JS::Object>);
    virtual ~BufferSource() override = default;

    using BufferableObjectBase::is_array_buffer;
    using BufferableObjectBase::is_data_view;
    using BufferableObjectBase::is_typed_array_base;
};

}
