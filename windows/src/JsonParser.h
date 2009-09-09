// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JSON_PARSER_H__
#define JSON_PARSER_H__

#include "yajl_parse.h"

template <typename T>
T* ReverseListGeneric(T *head)
{
	T *curr = head;
	head = NULL;
	while (curr) {
		T *next = curr->next;
		curr->next = head;
		head = curr;
		curr = next;
	}
	return head;
}

template <typename T>
size_t ListLengthGeneric(T *head)
{
	size_t len = 0;
	T *curr = head;
	while (curr) {
		len += 1;
		curr = curr->next;
	}
	return len;
}

/* Json data represented as an ad-hoc document in memory */
typedef enum {
	JsonTypeBool = 1,
	JsonTypeInteger,
	JsonTypeDouble,
	JsonTypeString,
	JsonTypeMap,
	JsonTypeArray
} JsonElType;

typedef struct {
	JsonElType		type;
} JsonEl;

typedef struct {
	JsonElType		type;
	int				boolVal;
} JsonElBool;

typedef struct {
	JsonElType		type;
	long			intVal;
} JsonElInteger;

typedef struct {
	JsonElType		type;
	double			doubleVal;
} JsonElDouble;

typedef struct {
	JsonElType		type;
	char *			stringVal;
} JsonElString;

// Note: make 'next' be first in the struct for perf
typedef struct JsonElMapData {
	struct JsonElMapData *	next;
	char *		key;
	JsonEl *	val;
} JsonElMapData;

typedef struct {
	JsonElType		type;
	JsonElMapData	*firstVal;
} JsonElMap;

// Note: make 'next' be first in the struct for perf
typedef struct JsonElArrayData {
	struct JsonElArrayData *next;
	JsonEl *	val;
} JsonElArrayData;

typedef struct {
	JsonElType		type;
	JsonElArrayData *firstVal;
} JsonElArray;

void JsonElFree(JsonEl *el);
JsonElMap *JsonElAsMap(JsonEl *el);
JsonElArray *JsonElAsArray(JsonEl *el);
JsonElString *JsonElAsString(JsonEl *el);
char *JsonElAsStringVal(JsonEl *el);
JsonElInteger *JsonElAsInteger(JsonEl *el);
bool JsonElAsIntegerVal(JsonEl *el, long *val);
JsonElBool *JsonElAsBool(JsonEl *el);

JsonEl *ParseJsonToDoc(const char *s);
JsonEl *GetMapElByName(JsonEl *json, const char *name);

#endif
