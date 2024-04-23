#pragma once
#include "util.h"

#include <vector>


struct Buffer
{
    char* data = nullptr;
    u32 size = 0;
    bool operator==(const Buffer& buf) const { return data == buf.data && size == buf.size; }
};

struct AddressArray
{
    void** Addresses;
    u64 Count;
};

struct TrackedBuffer
{
    Buffer buffer; // the actual buffer this is tracking
    AddressArray changedPages; // the pages that have been written to
};

u32 GetPageSize();

void TrackedFree(char* ptr);
char* TrackedAlloc(size_t size);
void PrintTrackedBuf(const TrackedBuffer& buf);
void ResetWrittenPages();
void GetAndResetWrittenPages(std::vector<AddressArray>& out);
