#ifndef FREE_SPACE_MANAGER_H
#define FREE_SPACE_MANAGER_H

#include <vector>
#include <cstdint>
#include <algorithm>
#include <iostream>

using namespace std;

struct FreeSegment {
    uint32_t startBlock;
    uint32_t blockCount;

    FreeSegment() : startBlock(0), blockCount(0) {}
    FreeSegment(uint32_t start, uint32_t count)
        : startBlock(start), blockCount(count) {}

    uint32_t endBlock() const {
        return startBlock + blockCount - 1;
    }

    bool isAdjacentTo(const FreeSegment& other) const {
        return (endBlock() + 1 == other.startBlock) ||
               (other.endBlock() + 1 == startBlock);
    }
};

class FreeSpaceManager {
private:
    vector<FreeSegment> freeSegments;
    uint32_t totalBlocks;
    uint32_t freeBlocks;

    static bool compareSegments(const FreeSegment& a, const FreeSegment& b) {
    return a.startBlock < b.startBlock;
    }

    void sortSegments() {
        sort(freeSegments.begin(), freeSegments.end(), compareSegments);
    }


    void mergeAdjacentSegments() {
        if (freeSegments.size() <= 1) return;

        sortSegments();

        vector<FreeSegment> merged;
        merged.push_back(freeSegments[0]);

        for (size_t i = 1; i < freeSegments.size(); i++) {
            FreeSegment& last = merged.back();
            FreeSegment& current = freeSegments[i];

            if (last.endBlock() + 1 == current.startBlock) {
                last.blockCount += current.blockCount;
            } else {
                merged.push_back(current);
            }
        }

        freeSegments = merged;
    }

    int findSegmentForAllocation(uint32_t blocksNeeded) {
        for (size_t i = 0; i < freeSegments.size(); i++) {
            if (freeSegments[i].blockCount >= blocksNeeded) {
                return i;
            }
        }
        return -1;
    }

public:
    FreeSpaceManager(uint32_t numBlocks)
        : totalBlocks(numBlocks), freeBlocks(numBlocks - 1) {

        if (numBlocks > 1) {
            freeSegments.push_back(FreeSegment(1, numBlocks - 1));
        }
    }

    vector<uint32_t> allocateBlocks(uint32_t count) {
        vector<uint32_t> allocatedBlocks;

        if (count == 0 || count > freeBlocks) {
            return allocatedBlocks;
        }

        int segmentIndex = findSegmentForAllocation(count);

        if (segmentIndex == -1) {
            mergeAdjacentSegments();
            segmentIndex = findSegmentForAllocation(count);

            if (segmentIndex == -1) {
                return allocatedBlocks;
            }
        }

        FreeSegment& segment = freeSegments[segmentIndex];

        for (uint32_t i = 0; i < count; i++) {
        uint32_t blockNum = segment.startBlock + i;
        if (blockNum == 0) {
            if (!allocatedBlocks.empty()) {
                freeBlockSegments(allocatedBlocks);
            }
            return vector<uint32_t>(); 
        }
        allocatedBlocks.push_back(blockNum);
    }

        if (segment.blockCount == count) {
            freeSegments.erase(freeSegments.begin() + segmentIndex);
        } else {
            segment.startBlock += count;
            segment.blockCount -= count;
        }

        freeBlocks -= count;

        return allocatedBlocks;
    }

    void freeBlock(uint32_t blockIndex) {
        // Don't free block 0 (it's reserved)
        if (blockIndex == 0) {
            cerr << "WARNING: Attempt to free reserved block 0" << endl;
            return;
        }
        
        vector<uint32_t> blocks = {blockIndex};
        freeBlockSegments(blocks);
    }

    void freeBlockSegments(const vector<uint32_t>& blocks) {
        if (blocks.empty()) return;

        vector<uint32_t> sortedBlocks = blocks;
        sort(sortedBlocks.begin(), sortedBlocks.end());

        // Remove block 0 if accidentally included
        if (!sortedBlocks.empty() && sortedBlocks[0] == 0) {
            cerr << "WARNING: Removing reserved block 0 from free list" << endl;
            sortedBlocks.erase(sortedBlocks.begin());
        }
        
        if (sortedBlocks.empty()) return;

        vector<FreeSegment> newSegments;
        uint32_t segStart = sortedBlocks[0];
        uint32_t segCount = 1;

        for (size_t i = 1; i < sortedBlocks.size(); i++) {
            if (sortedBlocks[i] == sortedBlocks[i - 1] + 1) {
                segCount++;
            } else {
                newSegments.push_back(FreeSegment(segStart, segCount));
                segStart = sortedBlocks[i];
                segCount = 1;
            }
        }
        newSegments.push_back(FreeSegment(segStart, segCount));

        for (size_t i = 0; i < newSegments.size(); i++) {
            freeSegments.push_back(newSegments[i]);
        }

        freeBlocks += sortedBlocks.size();

        mergeAdjacentSegments();
    }

    bool isFree(uint32_t blockIndex) const {
        if (blockIndex == 0) return false;
        
        for (size_t i = 0; i < freeSegments.size(); i++) {
            const FreeSegment& seg = freeSegments[i];
            if (blockIndex >= seg.startBlock &&
                blockIndex <= seg.endBlock()) {
                return true;
            }
        }
        return false;
    }

    bool isUsed(uint32_t blockIndex) const {
        if (blockIndex == 0) return true;
        return !isFree(blockIndex);
    }

    uint32_t getTotalBlocks() const {
        return totalBlocks;
    }

    uint32_t getFreeBlocks() const {
        return freeBlocks;
    }

    uint32_t getUsedBlocks() const {
        return totalBlocks - freeBlocks;
    }

    size_t getSegmentCount() const {
        return freeSegments.size();
    }

    double getFragmentation() const {
        if (freeBlocks == 0 || freeBlocks == totalBlocks - 1) return 0.0;

        size_t segments = getSegmentCount();
        if (segments <= 1) return 0.0;

        return ((double)(segments - 1) / freeBlocks) * 100.0;
    }

    uint32_t getLargestContiguousBlock() const {
        uint32_t largest = 0;
        for (size_t i = 0; i < freeSegments.size(); i++) {
            const FreeSegment& seg = freeSegments[i];
            if (seg.blockCount > largest) {
                largest = seg.blockCount;
            }
        }
        return largest;
    }

    vector<uint8_t> serialize() const {
        vector<uint8_t> data;

        data.push_back((totalBlocks >> 24) & 0xFF);
        data.push_back((totalBlocks >> 16) & 0xFF);
        data.push_back((totalBlocks >> 8) & 0xFF);
        data.push_back(totalBlocks & 0xFF);

        data.push_back((freeBlocks >> 24) & 0xFF);
        data.push_back((freeBlocks >> 16) & 0xFF);
        data.push_back((freeBlocks >> 8) & 0xFF);
        data.push_back(freeBlocks & 0xFF);

        uint32_t segCount = freeSegments.size();
        data.push_back((segCount >> 24) & 0xFF);
        data.push_back((segCount >> 16) & 0xFF);
        data.push_back((segCount >> 8) & 0xFF);
        data.push_back(segCount & 0xFF);

        for (size_t i = 0; i < freeSegments.size(); i++) {
            const FreeSegment& seg = freeSegments[i];

            data.push_back((seg.startBlock >> 24) & 0xFF);
            data.push_back((seg.startBlock >> 16) & 0xFF);
            data.push_back((seg.startBlock >> 8) & 0xFF);
            data.push_back(seg.startBlock & 0xFF);

            data.push_back((seg.blockCount >> 24) & 0xFF);
            data.push_back((seg.blockCount >> 16) & 0xFF);
            data.push_back((seg.blockCount >> 8) & 0xFF);
            data.push_back(seg.blockCount & 0xFF);
        }

        return data;
    }

    static FreeSpaceManager* deserialize(const vector<uint8_t>& data) {
        if (data.size() < 12) return nullptr;

        uint32_t totalBlocks = ((uint32_t)data[0] << 24) |
                               ((uint32_t)data[1] << 16) |
                               ((uint32_t)data[2] << 8) |
                               (uint32_t)data[3];

        uint32_t freeBlocks = ((uint32_t)data[4] << 24) |
                              ((uint32_t)data[5] << 16) |
                              ((uint32_t)data[6] << 8) |
                              (uint32_t)data[7];

        uint32_t segCount = ((uint32_t)data[8] << 24) |
                            ((uint32_t)data[9] << 16) |
                            ((uint32_t)data[10] << 8) |
                            (uint32_t)data[11];

        FreeSpaceManager* manager = new FreeSpaceManager(totalBlocks);
        manager->freeBlocks = freeBlocks;
        manager->freeSegments.clear();

        size_t offset = 12;
        for (size_t i = 0; i < segCount; i++) {
            if (offset + 8 > data.size()) break;

            uint32_t startBlock = ((uint32_t)data[offset] << 24) |
                                  ((uint32_t)data[offset + 1] << 16) |
                                  ((uint32_t)data[offset + 2] << 8) |
                                  (uint32_t)data[offset + 3];

            uint32_t blockCount = ((uint32_t)data[offset + 4] << 24) |
                                  ((uint32_t)data[offset + 5] << 16) |
                                  ((uint32_t)data[offset + 6] << 8) |
                                  (uint32_t)data[offset + 7];

            manager->freeSegments.push_back(FreeSegment(startBlock, blockCount));
            offset += 8;
        }

        return manager;
    }

    void clear() {
        freeSegments.clear();
        // Start from block 1 (block 0 is reserved)
        if (totalBlocks > 1) {
            freeSegments.push_back(FreeSegment(1, totalBlocks - 1));
            freeBlocks = totalBlocks - 1;
        } else {
            freeBlocks = 0;
        }
    }

    void printSegments() const {
        cout << "\n=== Free Space Segments ===\n";
        cout << "Block 0: RESERVED (not shown)\n";
        for (size_t i = 0; i < freeSegments.size(); i++) {
            cout << "Start: " << freeSegments[i].startBlock
                 << ", Count: " << freeSegments[i].blockCount << endl;
        }
    }
};

#endif