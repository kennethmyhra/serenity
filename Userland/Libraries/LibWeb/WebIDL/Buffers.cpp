/*
 * Copyright (c) 2023, Matthew Olsson <mattco@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibJS/Runtime/DataView.h>
#include <LibJS/Runtime/TypedArray.h>
#include <LibWeb/WebIDL/Buffers.h>

namespace Web::WebIDL {

u32 BufferableObjectBase::byte_length() const
{
    return m_bufferable_object->visit([](auto& obj) { return static_cast<u32>(obj->byte_length()); });
}

JS::GCPtr<JS::Object> BufferableObjectBase::raw_object()
{
    return m_bufferable_object->visit([](auto const& obj) -> JS::GCPtr<JS::Object> { return obj.ptr(); });
}

JS::GCPtr<JS::ArrayBuffer> BufferableObjectBase::array_buffer()
{
    return m_bufferable_object->visit(
        [](JS::NonnullGCPtr<JS::ArrayBuffer> array_buffer) -> JS::GCPtr<JS::ArrayBuffer> { return array_buffer; },
        [](auto const& view) -> JS::GCPtr<JS::ArrayBuffer> { return view->viewed_array_buffer(); });
}

BufferableObjectBase::BufferableObjectBase(JS::Realm& realm, JS::Handle<JS::Object> object)
    : JS::Object(realm, nullptr)
{
    if (is<JS::TypedArrayBase>(*object)) {
        m_bufferable_object = JS::NonnullGCPtr { static_cast<JS::TypedArrayBase&>(*object) };
    } else if (is<JS::DataView>(*object)) {
        m_bufferable_object = JS::NonnullGCPtr { static_cast<JS::DataView&>(*object) };
    } else if (is<JS::ArrayBuffer>(*object)) {
        m_bufferable_object = JS::NonnullGCPtr { static_cast<JS::ArrayBuffer&>(*object) };
    } else {
        VERIFY_NOT_REACHED();
    }
}

bool BufferableObjectBase::is_typed_array_base() const
{
    return m_bufferable_object->has<JS::NonnullGCPtr<JS::TypedArrayBase>>();
}

bool BufferableObjectBase::is_data_view() const
{
    return m_bufferable_object->has<JS::NonnullGCPtr<JS::DataView>>();
}

bool BufferableObjectBase::is_array_buffer() const
{
    return m_bufferable_object->has<JS::NonnullGCPtr<JS::ArrayBuffer>>();
}

void BufferableObjectBase::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    m_bufferable_object->visit([&](auto& obj) { visitor.visit(obj); });
}

ArrayBufferView::ArrayBufferView(JS::Realm& realm, JS::Handle<JS::Object> object)
    : BufferableObjectBase(realm, object)
{
    VERIFY(!is_array_buffer());
}

u32 ArrayBufferView::byte_offset() const
{
    return m_bufferable_object->visit(
        [](JS::NonnullGCPtr<JS::ArrayBuffer>) -> u32 { VERIFY_NOT_REACHED(); },
        [](auto& view) -> u32 { return static_cast<u32>(view->byte_offset()); });
}

BufferSource::BufferSource(JS::Realm& realm, JS::Handle<JS::Object> object)
    : BufferableObjectBase(realm, object)
{
}

}
