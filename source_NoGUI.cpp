// libraries
#include <iostream>
#include <vector>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <string>
#include <fstream>
#include <algorithm>
#include <limits>
#include <cstring>
using namespace std;

// disk specifications
static int DISK_SIZE = 10 * 1024 * 1024;             // 10 MB total disk size
static int DIRECTORY_SPACE_SIZE = 1 * 1024 * 1024;   // 1 MB for file entries
static int METADATA_SPACE_SIZE = 1 * 1024 * 1024;    // 1 MB for free blocks metadata
static int DATA_SPACE_SIZE = 8 * 1024 * 1024;        // 8 MB for file data
static int BLOCK_SIZE = 1024;                        // 1 KB basic block unit
static int MAX_DIRECTORY_ENTRIES = DIRECTORY_SPACE_SIZE / 500; // 500 B per file entry
static const char* TEMP_CREATE = "temp_create.txt";     // placeholder txt for editing new files
static const char* TEMP_MODIFY = "temp_modify.txt";     // placeholder txt for modifying old files

// define class file entry //
class FileEntry {
private:
    // attributes
    string name;
    vector<size_t> blocks; // block indices
    size_t size = 0;
public:
    // ctor
    FileEntry() = default;

    // parameterized constructor
    FileEntry(const string& name, const vector<size_t>& blocks, size_t size)
        : name(name), blocks(blocks), size(size) {
    }

    // getters
    string getName() const { return name; }
    vector<size_t> getBlocks() const { return blocks; }
    const vector<size_t>& getBlocksRef() const { return blocks; }
    size_t getSize() const { return size; }
    // setter for size
    void setSize(size_t newSize) { size = newSize; }
};

// define class virtual file system //
class VirtualFileSystem {
private:
    // attributes
    vector<char> disk;
    queue<size_t> freeBlocks;          // initial free blocks
    stack<size_t> freedBlocks;         // blocks freed from disk after being filled
    list<FileEntry> directory;         // file entries
    map<string, list<FileEntry>::iterator> directoryIndex; // map filename to entry

public:
    // ctor
    VirtualFileSystem() : disk(DISK_SIZE) {
        // initialize freeBlocks with all data-block indices
        size_t blockCount = DATA_SPACE_SIZE / BLOCK_SIZE;
        for (size_t i = 0; i < blockCount; ++i) {
            freeBlocks.push(i);
        }
    }
    // class methods
    ///////////////////////////////////////////////////
    void run() {  // method to run filesystem
        bool running = true;
        while (running) {
            cout << "\n=== Virtual File System ===\n"
                << "1. Create new file\n"
                << "2. List and view files\n"
                << "3. Copy from Windows\n"
                << "4. Copy to Windows\n"
                << "5. Modify file\n"
                << "6. Delete file\n"
                << "7. Exit\n"
                << "Choose option: ";

            int choice;

            if (!(cin >> choice)) break;
            cin.ignore(numeric_limits<streamsize>::max(), '\n'); // clear cin buffer
            switch (choice) {
            case 1: createFile(); break;
            case 2: listAndView(); break;
            case 3: copyFromWindows(); break;
            case 4: copyToWindows(); break;
            case 5: modifyFile(); break;
            case 6: deleteFile(); break;
            case 7: running = false; break;
            default: std::cout << "Invalid option.\n";
            }
        }
    }
    ///////////////////////////////////////////////////
    double usagePercent() const {  // return disk space status
        size_t totalSpace = DATA_SPACE_SIZE;
        size_t freeSpace = freeBlocks.size() + freedBlocks.size();
        size_t usedSpace = totalSpace - freeSpace * BLOCK_SIZE;
        return double(usedSpace) / totalSpace;
    }
    ///////////////////////////////////////////////////
    vector<size_t> allocateBlocks(size_t bytes) {  // return blocks Array to store data
        size_t blocksNeeded = (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
        vector<size_t> alloc;       // temp vector to store block array needed

        // first try stack (freed blockss)
        while (!freedBlocks.empty() && alloc.size() < blocksNeeded) {
            alloc.push_back(freedBlocks.top());
            freedBlocks.pop();
        }
        // second try queue (free blocks)
        while (!freeBlocks.empty() && alloc.size() < blocksNeeded) {
            alloc.push_back(freeBlocks.front());
            freeBlocks.pop();
        } // if not enough blocks, rollback
        if (alloc.size() < blocksNeeded) {
            // rollback
            for (auto i : alloc) freedBlocks.push(i);
            alloc.clear();
        }
        return alloc;
    }
    ///////////////////////////////////////////////////
    void freeAllocatedBlocks(const vector<size_t>& blocks) {  // free allocated blocks from disk
        for (auto i : blocks) {
            freedBlocks.push(i);
        }
    }
    ///////////////////////////////////////////////////
    void writeToDisk(const vector<size_t>& blocks, const string& data) {
        size_t pos = 0;  // initialize position pointer to track data written 
        for (auto b : blocks) {
            size_t toWrite = min(static_cast<size_t>(BLOCK_SIZE), data.size() - pos);  // calculate bytes to write
            size_t offset = DIRECTORY_SPACE_SIZE + METADATA_SPACE_SIZE + b * BLOCK_SIZE;  // calculate disk offset to write to
            memcpy(&disk[offset], data.data() + pos, toWrite);  // copy data to calculated disk offset
            pos += toWrite;
        }
    }
    ///////////////////////////////////////////////////
    string readFromDisk(const FileEntry& f) const {  // read data from disk
        string out;  // string to store disk data
        out.reserve(f.getSize());  // reserve space for data to be read
        // read data from disk
        size_t remaining = f.getSize();  // remaining bytes to read
        for (auto b : f.getBlocks()) {   // iterate through blocks
            size_t toRead = min(static_cast<size_t>(BLOCK_SIZE), remaining); // calculate bytes to read
            size_t offset = DIRECTORY_SPACE_SIZE + METADATA_SPACE_SIZE + b * BLOCK_SIZE;  // calculate disk offset to read from
            out.append(&disk[offset], toRead);  // append data to string
            remaining -= toRead;
        }
        return out;
    }
    ///////////////////////////////////////////////////
    void createFile() {  // create new file in disk
        // if there's no space, return
        if (directory.size() >= MAX_DIRECTORY_ENTRIES || usagePercent() > 0.8) {
            cout << "Disk full or >80% used. Delete files first." << endl;
            return;
        }

        // prompt user for filename
        string name;
        cout << "Enter filename (.txt): ";
        getline(cin, name);

        // check if filename is valid
        if (directoryIndex.count(name)) {
            cout << "File exists." << endl;
            return;
        }

        // create and open temporary txt file for user editing
        ofstream tmp(TEMP_CREATE);  // create and open a temp file
        tmp.close();                // close the temp file(to ensure it exists)
        cout << "Edit content in Notepad, save and close." << endl;

        // open the temp file in notepad for user to edit
        system((string("notepad ") + TEMP_CREATE).c_str());
        ifstream in(TEMP_CREATE, ios::binary);  // open the temp file for reading(byte level)
        string data((istreambuf_iterator<char>(in)), {}); // read the file content into a string
        in.close(); // close the temp file

        // allocate blocks for the file
        auto blocks = allocateBlocks(data.size());  // calculate blocks needed to store data
        if (blocks.empty()) {
            cout << "Not enough space." << endl;
            return;
        }

        writeToDisk(blocks, data);  //  write content to disk

        // add file entry to directory list
        directory.emplace_back(FileEntry{ name, blocks, data.size() });
        auto i = prev(directory.end());
        directoryIndex[name] = i;
        cout << "Created.";
    }
    ///////////////////////////////////////////////////
    void listAndView() {
        // check if directory is empty
        if (directory.empty()) {
            cout << "No files." << endl;
            return;
        }

        // list files in directory
        int i = 1;
        for (auto& f : directory) {
            cout << i++ << ". " << f.getName() << " (" << f.getSize() << " bytes)" << endl;
        }

        // prompt user for file index or 'e' to exit
        cout << "Index or 'e': ";
        string s;
        getline(cin, s);
        if (s == "e") return;

        int index = stoul(s) - 1;  // convert input string to int 
        if (index >= directory.size()) {
            cout << "Invalid." << endl;
            return;
        }

        auto j = next(directory.begin(), index);     // locate file
        cout << "--- " << j->getName() << " ---" << endl;
        cout << readFromDisk(*j) << endl;            // read disk
    }
    ///////////////////////////////////////////////////
    void copyFromWindows() {
        // check if disk is full
        if (directory.size() >= MAX_DIRECTORY_ENTRIES || usagePercent() > 0.8) {
            cout << "Disk full or >80% used. Delete files first." << endl;
            return;
        }

        // prompt user for filepath
        cout << "Enter source path: ";
        string path;
        getline(std::cin, path);

        ifstream in(path, ios::binary);     // open file to read(byte-level)

        if (!in) { // check if path is correct
            cout << "Cannot open." << endl;
            return;
        }

        string data((std::istreambuf_iterator<char>(in)), {});
        in.close();

        // extract filename from path
        auto pos = path.find_last_of("\\/");  // find last slash in path
        string name;
        if (pos == string::npos) {  // if no slash found use the whole path as name 
            name = path;
        }
        else {  // else use the part after the last slash as name
            name = path.substr(pos + 1);
        }

        // check if filename is valid
        if (directoryIndex.count(name)) {
            cout << "Exists." << endl;
            return;
        }

        // allocate blocks for the file
        auto blocks = allocateBlocks(data.size());
        if (blocks.empty()) {
            cout << "Not enough space." << endl;
            return;
        }

        // write data to disk
        writeToDisk(blocks, data);

        // add file entry to directory list
        directory.emplace_back(FileEntry{ name, blocks, data.size() });
        directoryIndex[name] = prev(directory.end());
        cout << "Copied in." << endl;
    }

    void copyToWindows() {
        // check if directory is empty
        if (directory.empty()) {
            cout << "No files." << endl;
            return;
        }

        // prompt user for file index or 'e' to exit
        listNames();
        cout << "Index or 'e': " << endl;
        string s;
        getline(cin, s);
        if (s == "e") return;

        int index = stoul(s) - 1;  // convert input string to int
        if (index >= directory.size()) {
            cout << "Invalid." << endl;
            return;
        }

        auto j = next(directory.begin(), index);    // locate file

        // ask user for destination
        cout << "Enter destination path (including filename): ";
        string destinationPath;
        getline(std::cin, destinationPath);

        ofstream out(destinationPath, std::ios::binary); // open file to write(byte-level)
        if (!out) {  // check if path is correct 
            cout << "Cannot open destination." << endl;
            return;
        }

        auto data = readFromDisk(*j); // read data from disk

        // write data to destination
        out.write(data.data(), data.size());
        out.close();  // close the file
        cout << "Copied to " << destinationPath << endl;
    }
    ///////////////////////////////////////////////////
    void modifyFile() {
        // check if directory is empty
        if (directory.empty()) {
            cout << "No files." << endl;
            return;
        }

        // prompt user for file index or 'e' to exit
        listNames();
        cout << "Index or 'e': ";
        string s;
        getline(std::cin, s);
        if (s == "e") return;

        int index = std::stoul(s) - 1;  // convert input string to int
        if (index >= directory.size()) {  // check if index is valid 
            cout << "Invalid." << endl;
            return;
        }

        // locate file and read data from disk
        auto j = next(directory.begin(), index);  // locate file
        ofstream tmp(TEMP_MODIFY, std::ios::binary); // create and open a temp file(byte-level)
        auto oldData = readFromDisk(*j); // read data from disk

        // write data to temp file
        tmp.write(oldData.data(), oldData.size());
        tmp.close();  // close the temp file
        cout << "Edit and save." << endl;

        // open the temp file in notepad for user to edit
        system((string("notepad ") + TEMP_MODIFY).c_str());
        ifstream in2(TEMP_MODIFY, ios::binary);  // open the temp file for reading(byte-level)
        string data((istreambuf_iterator<char>(in2)), {}); // read the file content into a string
        in2.close(); // close the temp file

        freeAllocatedBlocks(j->getBlocks());     // free old blocks from disk

        auto newBlocks = allocateBlocks(data.size()); // allocate new blocks for the file
        if (newBlocks.empty()) { // check if there is enough space
            cout << "No space; restoring." << endl;  // if not enough space, restore old data
            auto restore = allocateBlocks(oldData.size());  // allocate blocks for old data
            writeToDisk(restore, oldData);   // write old data to disk
            j->getBlocks() = move(restore);  // update blocks in file entry
        }
        else {  // if enough space, write new data to disk
            writeToDisk(newBlocks, data);      // write new data to disk
            j->getBlocks() = move(newBlocks);  // update blocks in file entry
            j->setSize(data.size());           // update size in file entry
            cout << "Modified." << endl;
        }
    }
    ///////////////////////////////////////////////////
    void deleteFile() {
        // check if directory is empty
        if (directory.empty()) {
            cout << "No files." << endl;
            return;
        }

        // prompt user for file index or 'e' to exit
        listNames();
        cout << "Index or 'e': " << endl;
        string s;
        getline(std::cin, s);
        if (s == "e") return;

        int index = std::stoul(s) - 1;  // convert input string to int
        if (index >= directory.size()) {  // check if index is valid 
            cout << "Invalid." << endl;
            return;
        }

        auto j = next(directory.begin(), index);  // locate file
        freeAllocatedBlocks(j->getBlocks());      // free blocks from disk
        directoryIndex.erase(j->getName());       // remove file entry from directory index
        directory.erase(j);                       // remove file entry from directory list       
        cout << "Deleted." << endl;
    }

    void listNames() const {
        int i = 1;
        for (auto& f : directory)
            cout << i++ << ". " << f.getName() << endl;
    }
};

int main() {
    VirtualFileSystem vfs;
    vfs.run();
    return 0;
}
