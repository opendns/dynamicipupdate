// Copyright (c) 2009 OpenDNS, LLC. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdafx.h"
#include "MemSegment.h"

// caller needs to free() the result
void *MemSegment::getData(DWORD *sizeOut)
{
    DWORD totalSize = dataSize;
    MemSegment *curr = next;
    while (curr) {
        totalSize += curr->dataSize;
        curr = curr->next;
    }
    if (0 == dataSize)
        return NULL;
    char *buf = (char*)malloc(totalSize + 1); // +1 for 0 termination
    if (!buf)
        return NULL;
    buf[totalSize] = 0;
    // the chunks are linked in reverse order, so we must reassemble them properly
    char *end = buf + totalSize;
    curr = next;
    while (curr) {
        end -= curr->dataSize;
        memcpy(end, curr->data, curr->dataSize);
        curr = curr->next;
    }
    end -= dataSize;
    memcpy(end, data, dataSize);
    assert(end == buf);
	if (sizeOut)
		*sizeOut = totalSize;
    return (void*)buf;
}

