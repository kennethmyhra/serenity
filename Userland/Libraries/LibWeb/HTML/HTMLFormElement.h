/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/Vector.h>
#include <LibWeb/Forward.h>
#include <LibWeb/HTML/HTMLElement.h>
#include <LibWeb/HTML/HTMLInputElement.h>
#include <LibWeb/XHR/FormData.h>

namespace Web::HTML {

class HTMLFormElement final : public HTMLElement {
    WEB_PLATFORM_OBJECT(HTMLFormElement, HTMLElement);

public:
    virtual ~HTMLFormElement() override;

    String action() const;
    String method() const { return attribute(HTML::AttributeNames::method); }

    void submit_form(JS::GCPtr<HTMLElement> submitter, bool from_submit_binding = false);

    // NOTE: This is for the JS bindings. Use submit_form instead.
    void submit();

    void add_associated_element(Badge<FormAssociatedElement>, HTMLElement&);
    void remove_associated_element(Badge<FormAssociatedElement>, HTMLElement&);
    Vector<JS::NonnullGCPtr<DOM::Element>> get_submittable_elements();
    Optional<HashMap<String, Vector<XHR::FormDataEntryValue>>> construct_entry_list();

    JS::NonnullGCPtr<DOM::HTMLCollection> elements() const;
    unsigned length() const;

    bool constructing_entry_list() const { return m_constructing_entry_list; }
    void set_constructing_entry_list(bool value) { m_constructing_entry_list = value; }

private:
    HTMLFormElement(DOM::Document&, DOM::QualifiedName);

    virtual void visit_edges(Cell::Visitor&) override;

    void retrieve_submittable_elements(Vector<JS::NonnullGCPtr<DOM::Element>>& elements, JS::NonnullGCPtr<DOM::Element> element);

    bool m_firing_submission_events { false };

    Vector<JS::GCPtr<HTMLElement>> m_associated_elements;

    JS::GCPtr<DOM::HTMLCollection> mutable m_elements;

    bool m_constructing_entry_list { false };
};

}
