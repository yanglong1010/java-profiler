#ifndef _BUFFERS_H
#define _BUFFERS_H

#include <cassert>
#include <unistd.h>

#include <arpa/inet.h>

#include "os.h"

const int BUFFER_SIZE = 1024;
const int BUFFER_LIMIT = BUFFER_SIZE - 128;
const int RECORDING_BUFFER_SIZE = 65536;
const int RECORDING_BUFFER_LIMIT = RECORDING_BUFFER_SIZE - 4096;
const int MAX_STRING_LENGTH = 8191;

typedef ssize_t (*FlushCallback)(char* data, int len);

class Buffer {
  private:
    int _offset;
    static const int _limit = BUFFER_SIZE - sizeof(int);
    char _data[_limit];

  public:
    Buffer() : _offset(0) {}

    virtual int limit() const {
        return _limit;
    }

    bool flushIfNeeded(FlushCallback callback, int limit = BUFFER_LIMIT) {
        if (_offset > limit) {
            if (callback(_data, _offset) == _offset) {
                reset();
                return true;
            }
        }
        return false;
    }

    const char* data() const {
        return _data;
    }

    int offset() const {
        return _offset;
    }

    int skip(int delta) {
        assert(_offset + delta < limit());
        int offset = _offset;
        _offset = offset + delta;
        return offset;
    }

    void reset() {
        _offset = 0;
    }

    void put(const char* v, u32 len) {
        assert(_offset + len < limit());
        memcpy(_data + _offset, v, len);
        _offset += (int)len;
    }

    void put8(char v) {
        assert(_offset < limit());
        _data[_offset++] = v;
    }

    void put16(short v) {
        assert(_offset + 2 < limit());
        *(short*)(_data + _offset) = htons(v);
        _offset += 2;
    }

    void put32(int v) {
        assert(_offset + 4 < limit());
        *(int*)(_data + _offset) = htonl(v);
        _offset += 4;
    }

    void put64(u64 v) {
        assert(_offset + 8 < limit());
        *(u64*)(_data + _offset) = OS::hton64(v);
        _offset += 8;
    }

    void putFloat(float v) {
        union {
            float f;
            int i;
        } u;

        u.f = v;
        put32(u.i);
    }

    void putVar32(u32 v) {
        assert(_offset + 5 < limit());
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void putVar64(u64 v) {
        assert(_offset + 9 < limit());
        int iter = 0;
        while (v > 0x1fffff) {
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            _data[_offset++] = (char)v | 0x80; v >>= 7;
            if (++iter == 3) return;
        }
        while (v > 0x7f) {
            _data[_offset++] = (char)v | 0x80;
            v >>= 7;
        }
        _data[_offset++] = (char)v;
    }

    void putUtf8(const char* v) {
        if (v == NULL) {
            put8(0);
        } else {
            size_t len = strlen(v);
            putUtf8(v, len < MAX_STRING_LENGTH ? len : MAX_STRING_LENGTH);
        }
    }

    void putUtf8(const char* v, u32 len) {
        put8(3);
        putVar32(len);
        put(v, len);
    }

    void put8(int offset, char v) {
        _data[offset] = v;
    }

    void putVar32(int offset, u32 v) {
        _data[offset] = v | 0x80;
        _data[offset + 1] = (v >> 7) | 0x80;
        _data[offset + 2] = (v >> 14) | 0x80;
        _data[offset + 3] = (v >> 21) | 0x80;
        _data[offset + 4] = (v >> 28);
    }
};

class RecordingBuffer : public Buffer {
  private:
    static const int _limit = RECORDING_BUFFER_SIZE - sizeof(Buffer);
    char _buf[_limit];

  public:
    RecordingBuffer() : Buffer() {
    }

    virtual int limit() const {
        return _limit;
    }

    bool flushIfNeeded(FlushCallback callback, int limit = RECORDING_BUFFER_LIMIT) {
        return Buffer::flushIfNeeded(callback, limit);
    }
};

#endif // _BUFFERS_H