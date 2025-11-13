# **How to Run** 

### **Step 1: Navigate to Project Directory**

Project Home Directory: (whatever it is)  
cd file-verse  
cd bscs24043\_phase1

### **Step 2: Clean Previous Builds (Optional)**

make clean

**Output:**

rm \-f testing

### **Step 3: Compile the Project**

make

**Output:**

g++ \-std=c++17 \-I./source/include \-o testing source/core/bscs24043.cpp source/core/fs\_format.cpp source/core/fs\_init.cpp source/core/file\_operations.cpp source/core/directory\_operations.cpp source/core/user\_management.cpp source/core/info\_operations.cpp

### **Step 4: Run the Program**

./testing

