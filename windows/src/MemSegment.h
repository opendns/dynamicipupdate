// Copyright (c) 2009 OpenDNS Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEM_SEGMENT_H__
#define MEM_SEGMENT_H__

class MemSegment {
private:
    class MemSegment *next;

public:
    void *  data;
    DWORD   dataSize;

    MemSegment(const void *buf, DWORD size) {
        next = NULL;
        data = NULL;
        add(buf, size);
    };

    MemSegment() {
        next = NULL;
        data = NULL;
        dataSize = 0;
    }

    bool addToMyself(const void *buf, DWORD size) {
        data = malloc(size);
        if (!data)
            return false;
        dataSize = size;
        memcpy(data, buf, size);
        return true;
    }

    bool addToNew(const void *buf, DWORD size) {
        MemSegment *ms = new MemSegment(buf, size);
        if (!ms)
            return false;
        if (!ms->data) {
            delete ms;
            return false;
        }
        ms->next = next;
        next = ms;
        return true;
    }

    bool add(const void *buf, DWORD size) {
        assert(size > 0);
        if (!data) {
            return addToMyself(buf, size);
        } else {
            return addToNew(buf, size);
        }
    }

    void freeAll() {
        free(data);
        data = NULL;
        dataSize = 0;
        // clever trick: each segment will delete the next segment
        if (next) {
            delete next;
            next = NULL;
        }
    }

    ~MemSegment() {
        freeAll();
    }

    void *getData(DWORD *sizeOut);
};

#endif

