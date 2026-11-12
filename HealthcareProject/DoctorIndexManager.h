#ifndef DOCTOR_INDEX_MANAGER_H
#define DOCTOR_INDEX_MANAGER_H

#include "FileManager.h"
#include <vector>
#include <string>

class SecondaryIndex {
private:
    struct Entry {
        string key;
        int    head;
    };

    string        baseName;
    int           keyLen;
    vector<Entry> keys;
    fstream       listFile;

    int  readAvail();
    void writeAvail(int value);
    void readNode(int node, string& doctorId, int& next);
    void writeNode(int node, const string& doctorId, int next);
    void writeNext(int node, int next);
    int  allocNode(const string& doctorId, int next);
    void freeNode(int node);
    int  binarySearch(const string& key, bool& found) const;
    void openListFile(bool truncate);

public:
    SecondaryIndex(const string& name, int keyLength);
    ~SecondaryIndex();

    void reset();
    void save();
    void load();

    bool insert(const string& key, const string& doctorId);
    bool remove(const string& key, const string& doctorId);
    vector<string> searchKey(const string& key);
    void print();
};

void load_doctor_secondary_indexes(fstream& f);

int  add_doctor_with_secondary(fstream& f, const Doctor& d);
bool remove_doctor_with_secondary(fstream& f, const string& id);
bool update_doctor_name_with_secondary(fstream& f, const string& id, const string& name);

bool update_doctor_id(fstream& f, const string& oldId, const string& newId);

vector<string> find_doctors_by_name(const string& name);
vector<string> find_doctors_by_specialty(const string& specialty);
void print_doctor_secondary_indexes();

#endif