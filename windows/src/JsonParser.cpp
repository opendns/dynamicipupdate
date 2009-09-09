// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"

#include "JsonParser.h"

#include "MiscUtil.h"
#include "StrUtil.h"

typedef struct MapArrayNestingChain {
	JsonEl *	el; /* either map or array */
	struct MapArrayNestingChain *prev;
} MapArrayNestingChain;

typedef struct {
	yajl_handle 			yajl_handle;
	MapArrayNestingChain *	nestingChain;
	JsonEl *				firstEl;
} JsonParserCtx;

static JsonEl *NewBool(int boolVal)
{
	JsonElBool *el = SA(JsonElBool);
	el->type = JsonTypeBool;
	el->boolVal = boolVal;
	return (JsonEl*)el;
}

static JsonEl *NewInteger(long integerVal)
{
	JsonElInteger *el = SA(JsonElInteger);
	el->type = JsonTypeInteger;
	el->intVal = integerVal;
	return (JsonEl*)el;
}

static JsonEl *NewDouble(double doubleVal)
{
	JsonElDouble *el = SA(JsonElDouble);
	el->type = JsonTypeDouble;
	el->doubleVal = doubleVal;
	return (JsonEl*)el;
}

static JsonEl *NewString(const unsigned char *s, size_t len)
{
	JsonElString *el = SA(JsonElString);
	el->type = JsonTypeString;
	el->stringVal = strdupn((const char*)s, len);
	return (JsonEl*)el;
}

static JsonEl *NewArray()
{
	JsonElArray *el = SA(JsonElArray);
	el->type = JsonTypeArray;
	el->firstVal = NULL;
	return (JsonEl*)el;
}

static JsonEl *NewMap()
{
	JsonElMap *el = SA(JsonElMap);
	el->type = JsonTypeMap;
	el->firstVal = NULL;
	return (JsonEl*)el;
}

static JsonElArrayData *NewArrayData(JsonEl *el)
{
	JsonElArrayData *arrayData = SA(JsonElArrayData);
	arrayData->val = el;
	arrayData->next = NULL;
	return arrayData;
}

static JsonElMapData *NewMapData(const unsigned char *key, size_t keyLen)
{
	JsonElMapData *mapData = SA(JsonElMapData);
	mapData->next = NULL;
	mapData->val = NULL;
	mapData->key = strdupn((const char*)key, keyLen);
	return mapData;
}

static void JsonElArrayDataFree(JsonElArrayData *el)
{
	while (el) {
		JsonElArrayData *next = el->next;
		assert(el->val);
		if (el->val)
			JsonElFree(el->val);
		el = next;
	}
}

static void JsonElMapDataFree(JsonElMapData *el)
{
	while (el) {
		JsonElMapData *next = el->next;
		// el->val can be NULL (represents value of json 'null' type)
		if (el->val)
			JsonElFree(el->val);
		free(el->key);
		el = next;
	}
}

// recursively free json elements
void JsonElFree(JsonEl *el)
{
	if (!el)
		return;
	JsonElType type = el->type;
	switch (type) {
		case JsonTypeBool:
		case JsonTypeInteger:
		case JsonTypeDouble:
			// nothing to do
			break;
		case JsonTypeString:
		{
			JsonElString *s = (JsonElString*)el;
			free(s->stringVal);
			break;
		}
		case JsonTypeArray:
		{
			JsonElArray *arr = (JsonElArray*)el;
			JsonElArrayDataFree(arr->firstVal);
			break;
		}
		case JsonTypeMap:
		{
			JsonElMap *map = (JsonElMap*)el;
			JsonElMapDataFree(map->firstVal);
			break;
		}
	}
	free(el);
}

JsonElMap *JsonElAsMap(JsonEl *el)
{
	if (el && (JsonTypeMap == el->type))
		return (JsonElMap*)el;
	return NULL;
}

JsonElArray *JsonElAsArray(JsonEl *el)
{
	if (el && (JsonTypeArray == el->type))
		return (JsonElArray*)el;
	return NULL;
}

JsonElString *JsonElAsString(JsonEl *el)
{
	if (el && (JsonTypeString == el->type))
		return (JsonElString*)el;
	return NULL;
}

char *JsonElAsStringVal(JsonEl *el)
{
	if (!el)
		return NULL;
	JsonElString *elString = JsonElAsString(el);
	if (!elString)
		return NULL;
	return elString->stringVal;
}

JsonElInteger *JsonElAsInteger(JsonEl *el)
{
	if (el && (JsonTypeInteger == el->type))
		return (JsonElInteger*)el;
	return NULL;
}

bool JsonElAsIntegerVal(JsonEl *el, long *val)
{
	if (!el)
		return false;
	JsonElInteger *elInt = JsonElAsInteger(el);
	if (!elInt)
		return false;
	*val = elInt->intVal;
	return true;
}

JsonElBool *JsonElAsBool(JsonEl *el)
{
	if (el && (JsonTypeBool == el->type))
		return (JsonElBool*)el;
	return NULL;
}

static void ReverseMapData(JsonElMap *map)
{
	map->firstVal = ReverseListGeneric(map->firstVal);;
}

static void ReverseArrayData(JsonElArray *arr)
{
	arr->firstVal = ReverseListGeneric(arr->firstVal);
}

static void ReverseMapOrArrayData(JsonEl *el)
{
	JsonElMap *map = JsonElAsMap(el);
	if (map) {
		ReverseMapData(map);
		return;
	}
	JsonElArray *arr = JsonElAsArray(el);
	if (arr) {
		ReverseArrayData(arr);
		return;
	}
	assert(0);
}

static JsonEl *GetMapElByNameFromMap(JsonElMap *map, const char *name)
{
	JsonElMapData *mapData = map->firstVal;
	while (mapData) {
		char *key = mapData->key;
		JsonEl *val = mapData->val;
		if (streq(name, key)) {
			return val;
		}
		mapData = mapData->next;
	}

	// search the values as well
	// TODO: this has unpleasant effect that nested map might be found before
	// non-nested, but we don't have this problem in our simple responses
	// To fix this we would need to keep a stack of maps and arrays to visit after
	// we're done with current level
	mapData = map->firstVal;
	while (mapData) {
		JsonEl *val = mapData->val;
		JsonEl *tmp = GetMapElByName(val, name);
		if (tmp)
			return tmp;
		mapData = mapData->next;
	}

	return NULL;
}

static JsonEl *GetMapElByNameFromArray(JsonElArray *arr, const char *name)
{
	JsonElArrayData *arrData = arr->firstVal;
	while (arrData) {
		JsonEl *el = arrData->val;
		JsonEl *tmp = GetMapElByName(el, name);
		if (tmp)
			return tmp;
		arrData = arrData->next;
	}
	return NULL;
}

// recursively find map element with a given <name>
// returns NULL if not found
JsonEl *GetMapElByName(JsonEl *json, const char *name)
{
	JsonElMap *map = JsonElAsMap(json);
	if (map)
		return GetMapElByNameFromMap(map, name);
	JsonElArray *arr = JsonElAsArray(json);
	if (arr)
		return GetMapElByNameFromArray(arr, name);

	// we won't find map entry in any other element
	return NULL;
}

#define CONTINUE_PARSE 1
#define CANCEL_PARSE 0

static void jp_init(JsonParserCtx *ctx)
{
	ctx->yajl_handle = 0;
	ctx->nestingChain = NULL;
	ctx->firstEl = NULL;
}

static JsonEl *jp_steal_result(JsonParserCtx *ctx)
{
	JsonEl *result = ctx->firstEl;
	ctx->firstEl = NULL;
	return result;
}

static int jp_add_element(JsonParserCtx *ctx, JsonEl *el)
{
	assert(ctx->nestingChain);
	if (!ctx->nestingChain)
		return CANCEL_PARSE;

	JsonEl *nestingEl = ctx->nestingChain->el;
	if (JsonTypeArray == nestingEl->type) {
		JsonElArray *elArr = (JsonElArray*)nestingEl;
		JsonElArrayData *newData = NewArrayData(el);
		/* Put in front. Elements will be in reverse order when we're done. */
		newData->next = elArr->firstVal;
		elArr->firstVal = newData;
	} else if (JsonTypeMap == nestingEl->type) {
		JsonElMap *elMap = (JsonElMap*)nestingEl;
		JsonElMapData *mapData = elMap->firstVal;
		/* element with key must have been created before in OnMapKey() callback */
		assert(mapData);
		if (!mapData)
			return CANCEL_PARSE;
		assert(mapData->key);
		if (!mapData->key)
			return CANCEL_PARSE;
		mapData->val = el;
	} else {
		assert(0);
		return CANCEL_PARSE;
	}

	return CONTINUE_PARSE;
}

static int jp_nesting_chain_head_push(JsonParserCtx *ctx, JsonEl *el)
{
	MapArrayNestingChain *newHead = SA(MapArrayNestingChain);
	newHead->el = el;
	newHead->prev = ctx->nestingChain;
	if (!ctx->firstEl) {
		assert(!ctx->nestingChain);
		// remember the first element we push
		ctx->firstEl = el;
	}
	ctx->nestingChain = newHead;
	return CONTINUE_PARSE;
}

static MapArrayNestingChain *jp_nesting_chain_head_get(JsonParserCtx *ctx, JsonElType expectedType)
{
	assert(ctx->nestingChain);
	if (!ctx->nestingChain)
		return NULL;
	MapArrayNestingChain *head = ctx->nestingChain;
	assert(head->el->type == expectedType);
	if (head->el->type != expectedType)
		return NULL;
	return head;
}

static void jp_map_array_nesting_chain_free_head(JsonParserCtx *ctx, MapArrayNestingChain *prevHead)
{
	MapArrayNestingChain *newHead = prevHead->prev;
	ctx->nestingChain = newHead;
	free(prevHead);
}

static int jp_nesting_chain_head_pop(JsonParserCtx *ctx, JsonElType expectedType)
{
	MapArrayNestingChain *prevHead = jp_nesting_chain_head_get(ctx, expectedType);
	if (!prevHead)
		return CANCEL_PARSE;
	// during map/array construction we ended up with data elements
	// in reverse order, so we need to reverse again to get the
	// right order
	ReverseMapOrArrayData(prevHead->el);
	jp_map_array_nesting_chain_free_head(ctx, prevHead);
	return CONTINUE_PARSE;
}

static void jp_nesting_chain_head_pop(JsonParserCtx *ctx)
{
	if (!ctx->nestingChain)
		return;
	jp_map_array_nesting_chain_free_head(ctx, ctx->nestingChain);
}

static void jp_destroy(JsonParserCtx *ctx)
{
	while (ctx->nestingChain) {
		jp_nesting_chain_head_pop(ctx);
	}

	JsonElFree(ctx->firstEl);
	if (ctx->yajl_handle) {
		yajl_free(ctx->yajl_handle);
	}
}

static int yp_yajl_null(void * /* ctx */)
{
	// we ignore nulls
	return CONTINUE_PARSE;
}

static int yp_yajl_boolean(void *o, int boolVal)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	assert(ctx->nestingChain);
	JsonEl *el = NewBool(boolVal);
	return jp_add_element(ctx, el);
}

static int yp_yajl_integer(void *o, long integerVal)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	JsonEl *el = NewInteger(integerVal);
	return jp_add_element(ctx, el);
}

static int yp_yajl_double(void *o, double doubleVal)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	JsonEl *el = NewDouble(doubleVal);
	return jp_add_element(ctx, el);
}

/** A callback which passes the string representation of the number
 *  back to the client.  Will be used for all numbers when present */
//int (* yajl_number)(void * ctx, const char * numberVal, unsigned int numberLen);

static int yp_yajl_string(void *o, const unsigned char * stringVal, unsigned int stringLen)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	JsonEl *el = NewString(stringVal, stringLen);
	return jp_add_element(ctx, el);
}

static int yp_yajl_start_map(void *o)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	JsonEl *map = NewMap();
	if (ctx->nestingChain)
		jp_add_element(ctx, map);
	return jp_nesting_chain_head_push(ctx, map);
}

static int yp_yajl_map_key(void *o, const unsigned char * key, unsigned int keyLen)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	MapArrayNestingChain *head = jp_nesting_chain_head_get(ctx, JsonTypeMap);
	if (!head)
		return CANCEL_PARSE;
	JsonElMap *map = (JsonElMap*)head->el;
	JsonElMapData *mapData = NewMapData(key, keyLen);
	mapData->next = map->firstVal;
	map->firstVal = mapData;
	return CONTINUE_PARSE;
}

static int yp_yajl_end_map(void *o)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	// TODO: error out if we have JsonElMapData without val
	return jp_nesting_chain_head_pop(ctx, JsonTypeMap);
}

static int yp_yajl_start_array(void *o)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	JsonEl *arr = NewArray();
	if (ctx->nestingChain)
		jp_add_element(ctx, arr);
	return jp_nesting_chain_head_push(ctx, arr);
}

static int yp_yajl_end_array(void *o)
{
	JsonParserCtx *ctx = static_cast<JsonParserCtx*>(o);
	return jp_nesting_chain_head_pop(ctx, JsonTypeArray);
}

static const yajl_callbacks yp_yajl_callbacks = {
	yp_yajl_null,
	yp_yajl_boolean,
	yp_yajl_integer,
	yp_yajl_double,
	NULL, // yp_yajl_number,
	yp_yajl_string,
	yp_yajl_start_map,
	yp_yajl_map_key,
	yp_yajl_end_map,
	yp_yajl_start_array,
	yp_yajl_end_array
};

static yajl_status jp_parse(JsonParserCtx *ctx, const unsigned char *s, unsigned len)
{
	if (!ctx->yajl_handle) {
		ctx->yajl_handle = yajl_alloc(&yp_yajl_callbacks, NULL, static_cast<void*>(ctx));
	}
	yajl_status status = yajl_parse(ctx->yajl_handle, s, len);
	return status;
}

JsonEl *ParseJsonToDoc(const char *s)
{
	JsonEl *result = NULL;
	JsonParserCtx parserCtx;

	if (!s)
		return NULL;

	jp_init(&parserCtx);

	yajl_status status = jp_parse(&parserCtx, (const unsigned char*)s, strlen(s));
	if (yajl_status_ok != status)
		goto Exit;
	result = jp_steal_result(&parserCtx);
Exit:
	jp_destroy(&parserCtx);
	return result;
}

