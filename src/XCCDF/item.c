/*
 * Copyright 2009 Red Hat Inc., Durham, North Carolina.
 * Copyright (C) 2010 Tresys Technology, LLC
 * All Rights Reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors:
 *      Lukas Kuklinek <lkuklinek@redhat.com>
 * 	Josh Adams <jadams@tresys.com>
 */

#include "item.h"
#include "helpers.h"
#include "xccdf_impl.h"
#include "../common/util.h"
#include <string.h>
#include <time.h>
#include <math.h>

const struct oscap_string_map XCCDF_OPERATOR_MAP[] = {
	{XCCDF_OPERATOR_EQUALS, "equals"},
	{XCCDF_OPERATOR_NOT_EQUAL, "not equal"},
	{XCCDF_OPERATOR_GREATER, "greater than"},
	{XCCDF_OPERATOR_GREATER_EQUAL, "greater than or equal"},
	{XCCDF_OPERATOR_LESS, "less than"},
	{XCCDF_OPERATOR_LESS_EQUAL, "less than or equal"},
	{XCCDF_OPERATOR_PATTERN_MATCH, "pattern match"},
	{0, NULL}
};

const struct oscap_string_map XCCDF_LEVEL_MAP[] = {
	{XCCDF_UNKNOWN, "unknown"},
	{XCCDF_INFO, "info"},
	{XCCDF_LOW, "low"},
	{XCCDF_MEDIUM, "medium"},
	{XCCDF_HIGH, "high"},
	{0, NULL}
};

static const struct oscap_string_map XCCDF_STATUS_MAP[] = {
	{XCCDF_STATUS_ACCEPTED, "accepted"},
	{XCCDF_STATUS_DEPRECATED, "deprecated"},
	{XCCDF_STATUS_DRAFT, "draft"},
	{XCCDF_STATUS_INCOMPLETE, "incomplete"},
	{XCCDF_STATUS_INTERIM, "interim"},
	{XCCDF_STATUS_NOT_SPECIFIED, NULL}
};


struct xccdf_item *xccdf_item_new(xccdf_type_t type, struct xccdf_item *parent)
{
	struct xccdf_item *item;
	size_t size = sizeof(*item) - sizeof(item->sub);

	switch (type) {
	case XCCDF_BENCHMARK:
		size += sizeof(struct xccdf_benchmark_item);
		break;
	case XCCDF_RULE:
		size += sizeof(struct xccdf_rule_item);
		break;
	case XCCDF_GROUP:
		size += sizeof(struct xccdf_group_item);
		break;
	case XCCDF_VALUE:
		size += sizeof(struct xccdf_value_item);
		break;
	case XCCDF_RESULT:
		size += sizeof(struct xccdf_result_item);
		break;
	default:
		size += sizeof(item->sub);
		break;
	}

	item = oscap_calloc(1, size);
	item->type = type;
	item->item.title = oscap_list_new();
	item->item.description = oscap_list_new();
	item->item.question = oscap_list_new();
	item->item.rationale = oscap_list_new();
	item->item.statuses = oscap_list_new();
	item->item.platforms = oscap_list_new();
	item->item.warnings = oscap_list_new();
	item->item.references = oscap_list_new();
	item->item.weight = 1.0;
	item->item.flags.selected = true;
	item->item.parent = parent;
	return item;
}

void xccdf_item_release(struct xccdf_item *item)
{
	if (item) {
		oscap_list_free(item->item.statuses, (oscap_destruct_func) xccdf_status_free);
		oscap_list_free(item->item.platforms, oscap_free);
		oscap_list_free(item->item.title, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.description, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.rationale, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.question, (oscap_destruct_func) oscap_text_free);
		oscap_list_free(item->item.warnings, (oscap_destruct_func) xccdf_warning_free);
		oscap_list_free(item->item.references, (oscap_destruct_func) xccdf_reference_free);
		oscap_free(item->item.id);
		oscap_free(item->item.cluster_id);
		oscap_free(item->item.version_update);
		oscap_free(item->item.version);
		oscap_free(item->item.extends);
		oscap_free(item);
	}
}

void xccdf_item_free(struct xccdf_item *item)
{
	if (item == NULL)
		return;
	switch (item->type) {
	case XCCDF_BENCHMARK:
		xccdf_benchmark_free(XBENCHMARK(item));
		break;
	case XCCDF_GROUP:
		xccdf_group_free(item);
		break;
	case XCCDF_RULE:
		xccdf_rule_free(item);
		break;
	case XCCDF_VALUE:
		xccdf_value_free(item);
		break;
	case XCCDF_RESULT:
		xccdf_result_free(XRESULT(item));
		break;
	default:
		assert((fprintf(stderr, "Deletion of item of type no. %u is not yet supported.", item->type), false));
	}
}

void xccdf_item_dump(struct xccdf_item *item, int depth)
{
	if (item == NULL)
		return;
	switch (item->type) {
	case XCCDF_BENCHMARK:
		xccdf_benchmark_dump(XBENCHMARK(item));
		break;
	case XCCDF_GROUP:
		xccdf_group_dump(item, depth);
		break;
	case XCCDF_RULE:
		xccdf_rule_dump(item, depth);
		break;
	default:
		xccdf_print_depth(depth);
		fprintf(stderr, "I cannot yet dump an item of type no. %u.", item->type);
	}
}

void xccdf_item_print(struct xccdf_item *item, int depth)
{
	if (item) {
		if (item->item.parent) {
			xccdf_print_depth(depth);
			printf("parent  : %s\n", item->item.parent->item.id);
		}
		if (item->item.extends) {
			xccdf_print_depth(depth);
			printf("extends : %s\n", item->item.extends);
		}
		if (item->type == XCCDF_BENCHMARK) {
			xccdf_print_depth(depth);
			printf("resolved: %d\n", item->item.flags.resolved);
		}
		if (item->type & XCCDF_CONTENT) {
			xccdf_print_depth(depth);
			printf("selected: %d\n", item->item.flags.selected);
		}
		if (item->item.version) {
			xccdf_print_depth(depth);
			printf("version : %s\n", item->item.version);
		}
		xccdf_print_depth(depth);
		printf("title   : ");
        xccdf_print_textlist(oscap_iterator_new(item->item.title), depth + 1, 70, "...");
		xccdf_print_depth(depth);
		printf("desc    : ");
        xccdf_print_textlist(oscap_iterator_new(item->item.description), depth + 1, 70, "...");
		xccdf_print_depth(depth);
		printf("platforms ");
		oscap_list_dump(item->item.platforms, xccdf_cstring_dump, depth + 1);
		xccdf_print_depth(depth);
		printf("status (cur = %d)", xccdf_item_get_current_status(item));
		oscap_list_dump(item->item.statuses, xccdf_status_dump, depth + 1);
	}
}

xmlNode *xccdf_item_to_dom(struct xccdf_item *item, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	xmlNode *item_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "Item", NULL);

	/* Handle generic item attributes */
	const char *id = xccdf_item_get_id(item);
	xmlNewProp(item_node, BAD_CAST "id", BAD_CAST id);

	const char *cluster_id = xccdf_item_get_cluster_id(item);
	if (cluster_id)
		xmlNewProp(item_node, BAD_CAST "cluster-id", BAD_CAST cluster_id);

	if (xccdf_item_get_hidden(item))
		xmlNewProp(item_node, BAD_CAST "hidden", BAD_CAST "True");

	if (xccdf_item_get_prohibit_changes(item))
		xmlNewProp(item_node, BAD_CAST "prohibitChanges", BAD_CAST "True");

	if (xccdf_item_get_abstract(item))
		xmlNewProp(item_node, BAD_CAST "abstract", BAD_CAST "True");

	/* Handle generic item child nodes */
        struct oscap_text_iterator *titles = xccdf_item_get_title(item);
        while (oscap_text_iterator_has_more(titles)) {
                struct oscap_text *title = oscap_text_iterator_next(titles);
                xmlNode * child = xmlNewChild(item_node, ns_xccdf, BAD_CAST "title", BAD_CAST oscap_text_get_text(title));
                if (oscap_text_get_lang(title) != NULL) xmlNewProp(child, BAD_CAST "xml:lang", BAD_CAST oscap_text_get_lang(title));
        }
        oscap_text_iterator_free(titles);

        struct oscap_text_iterator *descriptions = xccdf_item_get_description(item);
        while (oscap_text_iterator_has_more(descriptions)) {
                struct oscap_text *description = oscap_text_iterator_next(descriptions);
                xmlNode * child = xmlNewChild(item_node, ns_xccdf, BAD_CAST "description", BAD_CAST oscap_text_get_text(description));
                if (oscap_text_get_lang(description) != NULL) xmlNewProp(child, BAD_CAST "xml:lang", BAD_CAST oscap_text_get_lang(description));
        }
        oscap_text_iterator_free(descriptions);

	const char *version= xccdf_item_get_version(item);
	if (version)
		xmlNewProp(item_node, BAD_CAST "version", BAD_CAST version);

	struct xccdf_status_iterator *statuses = xccdf_item_get_statuses(item);
	while (xccdf_status_iterator_has_more(statuses)) {
		struct xccdf_status *status = xccdf_status_iterator_next(statuses);
		xccdf_status_to_dom(status, doc, item_node);
	}
	xccdf_status_iterator_free(statuses);

        struct oscap_text_iterator *questions = xccdf_item_get_question(item);
        while (oscap_text_iterator_has_more(questions)) {
                struct oscap_text *question = oscap_text_iterator_next(questions);
                xmlNode * child = xmlNewChild(item_node, ns_xccdf, BAD_CAST "question", BAD_CAST oscap_text_get_text(question));
                if (oscap_text_get_lang(question) != NULL) xmlNewProp(child, BAD_CAST "xml:lang", BAD_CAST oscap_text_get_lang(question));
        }
        oscap_text_iterator_free(questions);

	struct xccdf_reference_iterator *references = xccdf_item_get_references(item);
	while (xccdf_reference_iterator_has_more(references)) {
		struct xccdf_reference *ref = xccdf_reference_iterator_next(references);
		xccdf_reference_to_dom(ref, doc, item_node);
	}

	/* Handle type specific attributes and children */
	switch (xccdf_item_get_type(item)) {
		case XCCDF_RULE:
			xmlNodeSetName(item_node,BAD_CAST "Rule");
			xccdf_rule_to_dom(XRULE(item), item_node, doc, parent);
			break;
		case XCCDF_BENCHMARK:
			xmlNodeSetName(item_node,BAD_CAST "Benchmark");
			break;
		case XCCDF_PROFILE:
			xmlNodeSetName(item_node,BAD_CAST "Profile");
			xccdf_profile_to_dom(XPROFILE(item), item_node, doc, parent);
			break;
		case XCCDF_RESULT:
			xmlNodeSetName(item_node,BAD_CAST "Result");
			break;
		case XCCDF_GROUP:
			xmlNodeSetName(item_node,BAD_CAST "Group");
			xccdf_group_to_dom(XGROUP(item), item_node, doc, parent);
			break;
		case XCCDF_VALUE:
			xmlNodeSetName(item_node,BAD_CAST "Value");
			xccdf_value_to_dom(XVALUE(item), item_node, doc, parent);
			break;
		case XCCDF_CONTENT:
			xmlNodeSetName(item_node,BAD_CAST "Content");
			break;
		case XCCDF_OBJECT:
			xmlNodeSetName(item_node,BAD_CAST "Object");
			break;
		default:
			return item_node;
	}

	return item_node;
}

xmlNode *xccdf_reference_to_dom(struct xccdf_reference *ref, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	xmlNode *reference_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "reference", BAD_CAST xccdf_reference_get_content(ref));

	const char *lang = xccdf_reference_get_lang(ref);
	if (lang != NULL) xmlNewProp(reference_node, BAD_CAST "xml:lang", BAD_CAST lang);

	const char *href = xccdf_reference_get_href(ref);
	xmlNewProp(reference_node, BAD_CAST "href", BAD_CAST href);

        /* TODO: Dublin Core Elements /XML spec p. 69/ */

	return reference_node;
}

xmlNode *xccdf_profile_note_to_dom(struct xccdf_profile_note *note, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	xmlNode *note_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "profile-note", NULL);

	// This is in the XCCDF Spec, but not implemented in OpenSCAP
	//const char *lang = xccdf_profile_note_get_lang(note);
	//xmlNewProp(note_node, BAD_CAST "xml:lang", BAD_CAST lang);

	struct oscap_text *text = xccdf_profile_note_get_text(note);
	xmlNewChild(note_node, ns_xccdf, BAD_CAST "sub", BAD_CAST oscap_text_get_text(text));

	const char *reftag = xccdf_profile_note_get_reftag(note);
	xmlNewChild(note_node, ns_xccdf, BAD_CAST "tag", BAD_CAST reftag);

	return note_node;
}

xmlNode *xccdf_status_to_dom(struct xccdf_status *status, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);

	xccdf_level_t level = xccdf_status_get_status(status);
	xmlNode *status_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "status",
					   BAD_CAST XCCDF_STATUS_MAP[level - 1].string);

	time_t date = xccdf_status_get_date(status);
	xmlNewProp(status_node, BAD_CAST "date", BAD_CAST ctime(&date));

	return status_node;
}

xmlNode *xccdf_fixtext_to_dom(struct xccdf_fixtext *fixtext, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	xmlNode *fixtext_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "fixtext", NULL);

	// This is in the XCCDF Spec, but not implemented in OpenSCAP
	//const char *lang = xccdf_fixtext_get_lang(note);
	//xmlNewProp(note_node, BAD_CAST "xml:lang", BAD_CAST lang);
	//if (xccdf_fixtext_get_override(note))
	//	xmlNewProp(fixtext_node, BAD_CAST "override", BAD_CAST "True");
	
	if (xccdf_fixtext_get_reboot(fixtext))
		xmlNewProp(fixtext_node, BAD_CAST "reboot", BAD_CAST "True");

	const char *fixref = xccdf_fixtext_get_fixref(fixtext);
	xmlNewProp(fixtext_node, BAD_CAST "fixref", BAD_CAST fixref);

	xccdf_level_t complexity = xccdf_fixtext_get_complexity(fixtext);
        if (complexity != 0)
	        xmlNewProp(fixtext_node, BAD_CAST "complexity", BAD_CAST XCCDF_LEVEL_MAP[complexity].string);

	xccdf_level_t disruption = xccdf_fixtext_get_disruption(fixtext);
        if (disruption != 0)
	        xmlNewProp(fixtext_node, BAD_CAST "disruption", BAD_CAST XCCDF_LEVEL_MAP[disruption].string);

	xccdf_strategy_t strategy = xccdf_fixtext_get_strategy(fixtext);
        if (strategy != 0)
	        xmlNewProp(fixtext_node, BAD_CAST "strategy", BAD_CAST XCCDF_STRATEGY_MAP[strategy].string);

	const char *content = fixtext->content;
	xmlNewChild(fixtext_node, ns_xccdf, BAD_CAST "sub", BAD_CAST content);

	return fixtext_node;
}

xmlNode *xccdf_fix_to_dom(struct xccdf_fix *fix, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	const char *content = xccdf_fix_get_content(fix);
	xmlNode *fix_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "fix", BAD_CAST content);

	const char *id = xccdf_fix_get_id(fix);
	if (id != NULL) xmlNewProp(fix_node, BAD_CAST "id", BAD_CAST id);

	const char *sys = xccdf_fix_get_system(fix);
	if (sys != NULL) xmlNewProp(fix_node, BAD_CAST "system", BAD_CAST sys);

	if (xccdf_fix_get_reboot(fix))
		xmlNewProp(fix_node, BAD_CAST "reboot", BAD_CAST "True");

	xccdf_level_t complexity = xccdf_fix_get_complexity(fix);
        if (complexity != 0) {
	    xmlNewProp(fix_node, BAD_CAST "complexity", BAD_CAST XCCDF_LEVEL_MAP[complexity-1].string);
        }

	xccdf_level_t disruption = xccdf_fix_get_disruption(fix);
        if (disruption != 0)
	        xmlNewProp(fix_node, BAD_CAST "disruption", BAD_CAST XCCDF_LEVEL_MAP[disruption-1].string);

	xccdf_strategy_t strategy = xccdf_fix_get_strategy(fix);
        if (strategy != 0)
	        xmlNewProp(fix_node, BAD_CAST "strategy", BAD_CAST XCCDF_STRATEGY_MAP[strategy-1].string);

        // Sub element is used to store XCCDF value substitutions, not a content
	//xmlNewChild(fix_node, ns_xccdf, BAD_CAST "sub", BAD_CAST content);

	// This is in the XCCDF Spec, but not implemented in OpenSCAP
	//const char *instance = xccdf_fix_get_instance(fix);
	//xmlNewChild(fix_node, ns_xccdf, BAD_CAST "instance", BAD_CAST instance);
	
	return fix_node;
}

xmlNode *xccdf_ident_to_dom(struct xccdf_ident *ident, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	const char *id = xccdf_ident_get_id(ident);
	xmlNode *ident_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "ident", BAD_CAST id);

	const char *sys = xccdf_ident_get_system(ident);
	xmlNewProp(ident_node, BAD_CAST "system", BAD_CAST sys);

	return ident_node;
}

xmlNode *xccdf_check_to_dom(struct xccdf_check *check, xmlDoc *doc, xmlNode *parent)
{
	xmlNs *ns_xccdf = xmlSearchNsByHref(doc, parent, XCCDF_BASE_NAMESPACE);
	xmlNode *check_node = NULL;
	if (xccdf_check_get_complex(check))
		check_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "complex-check", NULL);
	else
		check_node = xmlNewChild(parent, ns_xccdf, BAD_CAST "check", NULL);

	const char *id = xccdf_check_get_id(check);
	if (id)
		xmlNewProp(check_node, BAD_CAST "id", BAD_CAST id);

	const char *sys = xccdf_check_get_system(check);
	xmlNewProp(check_node, BAD_CAST "system", BAD_CAST sys);

	const char *selector = xccdf_check_get_selector(check);
	if (selector)
		xmlNewProp(check_node, BAD_CAST "selector", BAD_CAST selector);

	/* Handle complex checks */
	struct xccdf_check_iterator *checks = xccdf_check_get_children(check);
	while (xccdf_check_iterator_has_more(checks)) {
		struct xccdf_check *new_check = xccdf_check_iterator_next(checks);
		xccdf_check_to_dom(new_check, doc, check_node);
	}
	xccdf_check_iterator_free(checks);

	struct xccdf_check_import_iterator *imports = xccdf_check_get_imports(check);
	while (xccdf_check_import_iterator_has_more(imports)) {
		struct xccdf_check_import *import = xccdf_check_import_iterator_next(imports);
		const char *name = xccdf_check_import_get_name(import);
		const char *content = xccdf_check_import_get_content(import);
		xmlNode *import_node = xmlNewChild(check_node, ns_xccdf, BAD_CAST "check-import", BAD_CAST content);
		xmlNewProp(import_node, BAD_CAST "import-name", BAD_CAST name);
	}
	xccdf_check_import_iterator_free(imports);

	struct xccdf_check_export_iterator *exports = xccdf_check_get_exports(check);
	while (xccdf_check_export_iterator_has_more(exports)) {
		struct xccdf_check_export *export = xccdf_check_export_iterator_next(exports);
		// This function seems like it should be in OpenSCAP, but isn't according to the docs
		//const char *name = xccdf_check_export_get_name(export);
		const char *name = export->name;
		const char *value= xccdf_check_export_get_value(export);
		xmlNode *export_node = xmlNewChild(check_node, ns_xccdf, BAD_CAST "check-export", NULL);
		xmlNewProp(export_node, BAD_CAST "export-name", BAD_CAST name);
		xmlNewProp(export_node, BAD_CAST "value-id", BAD_CAST value);
	}
	xccdf_check_export_iterator_free(exports);

	const char *content = xccdf_check_get_content(check);
	if (content)
		xmlNewChild(check_node, ns_xccdf, BAD_CAST "check-content", BAD_CAST content);

	struct xccdf_check_content_ref_iterator *refs = xccdf_check_get_content_refs(check);
	while (xccdf_check_content_ref_iterator_has_more(refs)) {
		struct xccdf_check_content_ref *ref = xccdf_check_content_ref_iterator_next(refs);
		xmlNode *ref_node = xmlNewChild(check_node, ns_xccdf, BAD_CAST "check-content-ref", NULL);

		const char *name = xccdf_check_content_ref_get_name(ref);
		xmlNewProp(ref_node, BAD_CAST "name", BAD_CAST name);

		const char *href = xccdf_check_content_ref_get_href(ref);
		xmlNewProp(ref_node, BAD_CAST "href", BAD_CAST href);
	}
	xccdf_check_content_ref_iterator_free(refs);

	return check_node;
}

#define XCCDF_ITEM_PROCESS_FLAG(reader,flag,attr) \
	if (xccdf_attribute_has((reader), (attr))) \
		item->item.flags.flag = xccdf_attribute_get_bool((reader), (attr));

bool xccdf_item_process_attributes(struct xccdf_item *item, xmlTextReaderPtr reader)
{
	item->item.id = xccdf_attribute_copy(reader, XCCDFA_ID);

	XCCDF_ITEM_PROCESS_FLAG(reader, resolved, XCCDFA_RESOLVED);
	XCCDF_ITEM_PROCESS_FLAG(reader, hidden, XCCDFA_HIDDEN);
	XCCDF_ITEM_PROCESS_FLAG(reader, selected, XCCDFA_SELECTED);
	XCCDF_ITEM_PROCESS_FLAG(reader, prohibit_changes, XCCDFA_PROHIBITCHANGES);
	XCCDF_ITEM_PROCESS_FLAG(reader, multiple, XCCDFA_MULTIPLE);
	XCCDF_ITEM_PROCESS_FLAG(reader, abstract, XCCDFA_ABSTRACT);
	XCCDF_ITEM_PROCESS_FLAG(reader, interactive, XCCDFA_INTERACTIVE);

	if (xccdf_attribute_has(reader, XCCDFA_WEIGHT))
		item->item.weight = xccdf_attribute_get_float(reader, XCCDFA_WEIGHT);
	if (xccdf_attribute_has(reader, XCCDFA_EXTENDS))
		item->item.extends = xccdf_attribute_copy(reader, XCCDFA_EXTENDS);
	item->item.cluster_id = xccdf_attribute_copy(reader, XCCDFA_CLUSTER_ID);

	if (item->item.id) {
		struct xccdf_item *bench = XITEM(xccdf_item_get_benchmark_internal(item));
		if (bench != NULL)
			oscap_htable_add(bench->sub.benchmark.dict, item->item.id, item);
	}
	return item->item.id != NULL;
}

bool xccdf_item_process_element(struct xccdf_item * item, xmlTextReaderPtr reader)
{
	xccdf_element_t el = xccdf_element_get(reader);

	switch (el) {
	case XCCDFE_TITLE:
        oscap_list_add(item->item.title, oscap_text_new_parse(XCCDF_TEXT_PLAIN, reader));
		return true;
	case XCCDFE_DESCRIPTION:
        oscap_list_add(item->item.description, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
		return true;
	case XCCDFE_WARNING:
        oscap_list_add(item->item.warnings, xccdf_warning_new_parse(reader));
		return true;
	case XCCDFE_REFERENCE:
        oscap_list_add(item->item.references, xccdf_reference_new_parse(reader));
		return true;
	case XCCDFE_STATUS:{
        const char *date = xccdf_attribute_get(reader, XCCDFA_DATE);
        char *str = oscap_element_string_copy(reader);
        struct xccdf_status *status = xccdf_status_new_fill(str, date);
        oscap_free(str);
        if (status) {
            oscap_list_add(item->item.statuses, status);
            return true;
        }
        break;
    }
	case XCCDFE_VERSION:
		item->item.version_time = oscap_get_datetime(xccdf_attribute_get(reader, XCCDFA_TIME));
		item->item.version_update = xccdf_attribute_copy(reader, XCCDFA_UPDATE);
		item->item.version = oscap_element_string_copy(reader);
		break;
	case XCCDFE_RATIONALE:
        oscap_list_add(item->item.rationale, oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader));
		return true;
	case XCCDFE_QUESTION:
        oscap_list_add(item->item.question, oscap_text_new_parse(XCCDF_TEXT_PLAIN, reader));
		return true;
	default:
		break;
	}
	return false;
}

inline struct xccdf_item* xccdf_item_get_benchmark_internal(struct xccdf_item* item)
{
	if (item == NULL) return NULL;
	while (xccdf_item_get_parent(item) != NULL)
		item = xccdf_item_get_parent(item);
	return (xccdf_item_get_type(item) == XCCDF_BENCHMARK ? item : NULL);
}

#define XCCDF_BENCHGETTER(TYPE) \
	struct xccdf_benchmark* xccdf_##TYPE##_get_benchmark(const struct xccdf_##TYPE* item) \
	{ return XBENCHMARK(xccdf_item_get_benchmark_internal(XITEM(item))); }
XCCDF_BENCHGETTER(item)  XCCDF_BENCHGETTER(profile) XCCDF_BENCHGETTER(rule)
XCCDF_BENCHGETTER(group) XCCDF_BENCHGETTER(value)   XCCDF_BENCHGETTER(result)
#undef XCCDF_BENCHGETTER

static void *xccdf_item_convert(struct xccdf_item *item, xccdf_type_t type)
{
	return ((item != NULL && (item->type & type)) ? item : NULL);
}

#define XCCDF_ITEM_CONVERT(T1,T2) struct xccdf_##T1* xccdf_item_to_##T1(struct xccdf_item* item) { return xccdf_item_convert(item, XCCDF_##T2); }
XCCDF_ITEM_CONVERT(benchmark, BENCHMARK)
XCCDF_ITEM_CONVERT(profile, PROFILE)
XCCDF_ITEM_CONVERT(rule, RULE)
XCCDF_ITEM_CONVERT(group, GROUP)
XCCDF_ITEM_CONVERT(value, VALUE)
XCCDF_ITEM_CONVERT(result, RESULT)
#undef XCCDF_ITEM_CONVERT

#define XCCDF_ITEM_UPCAST(T) struct xccdf_item *xccdf_##T##_to_item(struct xccdf_##T *item) { return XITEM(item); }
XCCDF_ITEM_UPCAST(benchmark) XCCDF_ITEM_UPCAST(profile) XCCDF_ITEM_UPCAST(rule)
XCCDF_ITEM_UPCAST(group) XCCDF_ITEM_UPCAST(value) XCCDF_ITEM_UPCAST(result)
#undef XCCDF_ITEM_UPCAST

XCCDF_ABSTRACT_GETTER(xccdf_type_t, item, type, type)
XCCDF_ITEM_GETTER(const char *, id)

XCCDF_ITEM_TIGETTER(question);
XCCDF_ITEM_TIGETTER(rationale);
XCCDF_ITEM_TIGETTER(title);
XCCDF_ITEM_TIGETTER(description);
XCCDF_ITEM_ADDER(struct oscap_text *, question, question)
XCCDF_ITEM_ADDER(struct oscap_text *, title, title)
XCCDF_ITEM_ADDER(struct oscap_text *, description, description)
XCCDF_ITEM_ADDER(struct oscap_text *, rationale, rationale)

XCCDF_ITEM_GETTER(const char *, version)
XCCDF_ITEM_GETTER(const char *, cluster_id)
XCCDF_ITEM_GETTER(const char *, version_update) XCCDF_ITEM_GETTER(time_t, version_time) XCCDF_ITEM_GETTER(float, weight)
XCCDF_ITEM_GETTER(struct xccdf_item *, parent)
XCCDF_ITEM_GETTER(const char *, extends)
XCCDF_FLAG_GETTER(resolved)
XCCDF_FLAG_GETTER(hidden)
XCCDF_FLAG_GETTER(selected)
XCCDF_FLAG_GETTER(multiple)
XCCDF_FLAG_GETTER(prohibit_changes)
XCCDF_FLAG_GETTER(abstract)
XCCDF_FLAG_GETTER(interactive)
XCCDF_ITEM_SIGETTER(platforms)
XCCDF_ITEM_ADDER_STRING(platform, platforms)
XCCDF_ITEM_IGETTER(reference, references)
XCCDF_ITEM_IGETTER(warning, warnings)
XCCDF_ITEM_IGETTER(status, statuses)
XCCDF_ITEM_ADDER(struct xccdf_reference *, reference, references)
XCCDF_ITEM_ADDER(struct xccdf_warning *, warning, warnings)
XCCDF_ITEM_ADDER(struct xccdf_status *, status, statuses)
XCCDF_ITERATOR_GEN_S(item) XCCDF_ITERATOR_GEN_S(status) XCCDF_ITERATOR_GEN_S(reference)
OSCAP_ITERATOR_GEN_T(struct xccdf_warning *, xccdf_warning)

XCCDF_ITEM_SETTER_SIMPLE(xccdf_numeric, weight)
XCCDF_ITEM_SETTER_SIMPLE(time_t, version_time)
XCCDF_ITEM_SETTER_STRING(version)
XCCDF_ITEM_SETTER_STRING(version_update)
XCCDF_ITEM_SETTER_STRING(extends)
XCCDF_ITEM_SETTER_STRING(cluster_id)

#define XCCDF_SETTER_ID(T) bool xccdf_##T##_set_id(struct xccdf_##T *item, const char *newval) \
                { return xccdf_benchmark_rename_item(XITEM(item), newval); }
XCCDF_SETTER_ID(item) XCCDF_SETTER_ID(benchmark) XCCDF_SETTER_ID(profile)
XCCDF_SETTER_ID(rule) XCCDF_SETTER_ID(group) XCCDF_SETTER_ID(value) XCCDF_SETTER_ID(result)
#undef XCCDF_SETTER_ID

struct xccdf_item_iterator *xccdf_item_get_content(const struct xccdf_item *item)
{
	if (item == NULL) return NULL;
	switch (xccdf_item_get_type(item)) {
		case XCCDF_GROUP:     return xccdf_group_get_content(XGROUP(item));
		case XCCDF_BENCHMARK: return xccdf_benchmark_get_content(XBENCHMARK(item));
		default: return NULL;
	}
}

struct xccdf_status *xccdf_status_new_fill(const char *status, const char *date)
{
	struct xccdf_status *ret;
	if (!status)
		return NULL;
	ret = oscap_calloc(1, sizeof(struct xccdf_status));
	if ((ret->status = oscap_string_to_enum(XCCDF_STATUS_MAP, status)) == XCCDF_STATUS_NOT_SPECIFIED) {
		oscap_free(ret);
		return NULL;
	}
	ret->date = oscap_get_date(date);
	return ret;
}

struct xccdf_status *xccdf_status_new(void)
{
    return oscap_calloc(1, sizeof(struct xccdf_status));
}

void xccdf_status_dump(struct xccdf_status *status, int depth)
{
	xccdf_print_depth(depth);
	time_t date = xccdf_status_get_date(status);
	printf("%-10s (%24.24s)\n", oscap_enum_to_string(XCCDF_STATUS_MAP, xccdf_status_get_status(status)),
	       (date ? ctime(&date) : "   date not specified   "));
}

void xccdf_status_free(struct xccdf_status *status)
{
	oscap_free(status);
}

OSCAP_ACCESSOR_SIMPLE(time_t, xccdf_status, date)
OSCAP_ACCESSOR_SIMPLE(xccdf_status_type_t, xccdf_status, status)

xccdf_status_type_t xccdf_item_get_current_status(const struct xccdf_item *item)
{
	time_t maxtime = 0;
	xccdf_status_type_t maxtype = XCCDF_STATUS_NOT_SPECIFIED;
	const struct oscap_list_item *li = item->item.statuses->first;
	struct xccdf_status *status;
	while (li) {
		status = li->data;
		if (status->date == 0 || status->date >= maxtime) {
			maxtime = status->date;
			maxtype = status->status;
		}
		li = li->next;
	}
	return maxtype;
}

struct xccdf_model *xccdf_model_new(void)
{
    struct xccdf_model *model = oscap_calloc(1, sizeof(struct xccdf_model));
    model->params = oscap_htable_new();
    return model;
}

struct xccdf_model *xccdf_model_new_xml(xmlTextReaderPtr reader)
{
	xccdf_element_t el = xccdf_element_get(reader);
	int depth = oscap_element_depth(reader) + 1;
	struct xccdf_model *model;

	if (el != XCCDFE_MODEL)
		return NULL;

	model = xccdf_model_new();
	model->system = xccdf_attribute_copy(reader, XCCDFA_SYSTEM);

	while (oscap_to_start_element(reader, depth)) {
		if (xccdf_element_get(reader) == XCCDFE_PARAM) {
			const char *name = xccdf_attribute_get(reader, XCCDFA_NAME);
			char *value = oscap_element_string_copy(reader);
			if (!name || !value || !oscap_htable_add(model->params, name, value))
				oscap_free(value);
		}
	}

	return model;
}

OSCAP_ACCESSOR_STRING(xccdf_model, system)

static const struct oscap_string_map XCCDF_WARNING_MAP[] = {
	{ XCCDF_WARNING_GENERAL, "general" },
	{ XCCDF_WARNING_FUNCTIONALITY, "functionality" },
	{ XCCDF_WARNING_PERFORMANCE, "performance" },
	{ XCCDF_WARNING_HARDWARE, "hardware" },
	{ XCCDF_WARNING_LEGAL, "legal" },
	{ XCCDF_WARNING_REGULATORY, "regulatory" },
	{ XCCDF_WARNING_MANAGEMENT, "management" },
	{ XCCDF_WARNING_AUDIT, "audit" },
	{ XCCDF_WARNING_DEPENDENCY, "dependency" },
	{ XCCDF_WARNING_GENERAL, NULL }
};

void xccdf_model_free(struct xccdf_model *model)
{
	if (model) {
		oscap_free(model->system);
		oscap_htable_free(model->params, oscap_free);
		oscap_free(model);
	}
}

void xccdf_cstring_dump(const char *data, int depth)
{
	xccdf_print_depth(depth);
	printf("%s\n", data);
}

struct xccdf_warning *xccdf_warning_new(void)
{
    struct xccdf_warning *w = oscap_calloc(1, sizeof(struct xccdf_warning));
    w->category = XCCDF_WARNING_GENERAL;
    return w;
}

struct xccdf_warning *xccdf_warning_new_parse(xmlTextReaderPtr reader)
{
    struct xccdf_warning *w = xccdf_warning_new();
    w->text = oscap_text_new_parse(XCCDF_TEXT_HTMLSUB, reader);
    w->category = oscap_string_to_enum(XCCDF_WARNING_MAP, xccdf_attribute_get(reader, XCCDFA_CATEGORY));
    return w;
}

void xccdf_warning_free(struct xccdf_warning * w)
{
    if (w != NULL) {
        oscap_text_free(w->text);
        oscap_free(w);
    }
}

OSCAP_ACCESSOR_SIMPLE(xccdf_warning_category_t, xccdf_warning, category)
OSCAP_ACCESSOR_TEXT(xccdf_warning, text)

struct xccdf_reference *xccdf_reference_new(void)
{
    struct xccdf_reference *ref = oscap_calloc(1, sizeof(struct xccdf_reference));
    return ref;
}

struct xccdf_reference *xccdf_reference_new_parse(xmlTextReaderPtr reader)
{
    struct xccdf_reference *ref = xccdf_reference_new();
    if (xccdf_attribute_has(reader, XCCDFA_OVERRIDE))
            ref->override = oscap_string_to_enum(OSCAP_BOOL_MAP, xccdf_attribute_get(reader, XCCDFA_OVERRIDE));
    ref->href = xccdf_attribute_copy(reader, XCCDFA_HREF);
    ref->content = oscap_element_string_copy(reader);
    // TODO Dublin Core
    return ref;
}

void xccdf_reference_free(struct xccdf_reference *ref)
{
    if (ref != NULL) {
        oscap_free(ref->lang);
        oscap_free(ref->href);
        oscap_free(ref->content);
        oscap_free(ref);
    }
}

OSCAP_ACCESSOR_STRING(xccdf_reference, lang)
OSCAP_ACCESSOR_STRING(xccdf_reference, href)
OSCAP_ACCESSOR_STRING(xccdf_reference, content)
OSCAP_ACCESSOR_SIMPLE(bool,        xccdf_reference, override)

const struct oscap_text_traits XCCDF_TEXT_PLAIN    = { .can_override = true };
const struct oscap_text_traits XCCDF_TEXT_HTML     = { .html = true, .can_override = true };
const struct oscap_text_traits XCCDF_TEXT_PLAINSUB = { .can_override = true, .can_substitute = true };
const struct oscap_text_traits XCCDF_TEXT_HTMLSUB  = { .html = true, .can_override = true, .can_substitute = true };
const struct oscap_text_traits XCCDF_TEXT_NOTICE   = { .html = true };
const struct oscap_text_traits XCCDF_TEXT_PROFNOTE = { .html = true, .can_substitute = true };

