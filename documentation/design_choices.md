# **Design Choices Document**

---

## **Data Structures**

### **User Indexing: AVL Tree** 

**Why AVL Tree?**

* **O(log n) Operations**: Login, user creation, and deletion all require username lookups. AVL trees provide guaranteed O(log n) search time.  
* **Sorted Iteration**: Can do inOrder traversal for alphabetical order Usernames. 

**Trade-offs**:

+ Guaranteed O(log n) for all operations  
+ Automatic sorting  
+ No Wasted Space  
- Hash table is faster

---

### **Directory Tree: Custom Tree Structure**

**Why This Structure?**

* **Natural Hierarchy**: Files and directories form a natural tree structure  
* **Parent Pointers**: Each TreeNode has a parent pointer, enabling:  
  * Fast upward traversal  
  * Path reconstruction   
  * Efficient rename operations

**Trade-offs**:

+ Intuitive mapping to file system hierarchy  
+ Fast directory operations (children vector)  
+ Memory-efficient  
- Path lookup is higher

---

### **Free Space Management: Block List** 

**Why List?**

* **Space Efficiency**: Only stores (start, count) pairs for free regions  
  * Empty filesystem: 1 segment (1, total\_blocks-1)  
* **Merge-on-Free**: Adjacent segments merged automatically, reducing fragmentation

**Trade-offs**:

+ Good for sequential allocation patterns  
+ Automatic defragmentation through merging  
- O(n) allocation if checking all segments

---

### **Path-to-Disk**

**How It Works**:

**Example**: Reading /docs/report.txt

1. Parse path: \["docs", "report.txt"\]  
2. Traverse tree: root → "docs" node → "report.txt" node  
3. Extract entryIndex (e.g., 42\)  
4. Read FileEntry at offset: fileEntryOffset \+ (42 × sizeof(FileEntry))  
5. Extract startBlockIndex (e.g., 100\)  
6. Follow like in a linked list until next becomes 0: block 100 → next block pointer → ...

---

## **.omni File Structure**

### **Omni File Layout**

\[OMNIHeader\]\[UserInfo×maxUsers\]\[FileEntry×maxFiles\]\[DataBlocks\]\[FreeSpaceManager\]

### **Data Block Structure**

Each block: **4 bytes next-pointer \+ (block\_size \- 4\) bytes data**

**Good Because:**

* **Linked List on Disk**: Blocks form linked list, allowing scattered allocation  
* **Simple Sequential Read**: Follow chain by reading 4 bytes per block

**Disadvantage**: Wastes 4 bytes per block, and minimum 4kb use even if we want few bytes

## **Memory Management Strategies**

### **What Lives in Memory?**

**Always Loaded**:

1. **OMNIHeader** (512 bytes)  
   * Read once during fs\_init  
2. **Complete Directory Tree**   
   * All TreeNodes loaded during initialization  
   * Rebuilt by scanning FileEntry table  
3. **User Table**  
   * All active users loaded  
   * Max size: max\_users × 128 bytes  
4. **Free Space Metadata**  
   * Segment list in memory  
   * Serialized to disk only on shutdown  
   * Size: \~8 bytes per free segment

**Never Fully Loaded**:

* **File Content**: Always read from disk on-demand  
* **FileEntry Table**: Individual entries read as needed