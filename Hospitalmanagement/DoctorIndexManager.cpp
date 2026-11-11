#include "DoctorIndexManager.h"

static const int SEC_ID_LEN = 15;
static const int SEC_NAME_LEN = 30;
static const int SEC_SPEC_LEN = 20;
static const int SEC_NODE_SIZE = SEC_ID_LEN + sizeof(int);

static string sec_text(const char* src, int size) {
    string value(src, size);
    size_t zero = value.find('\0');
    if (zero != string::npos) value.resize(zero);
    size_t last = value.find_last_not_of(' ');
    return (last == string::npos) ? "" : value.substr(0, last + 1);
}

static void sec_pad(char* dest, const string& value, int size) {
    memset(dest, ' ', size);
    int len = (int)value.size();
    if (len > size) len = size;
    memcpy(dest, value.c_str(), len);
}

static string sec_trim(string s) {
    while (!s.empty() && s[s.size() - 1] == ' ') s.erase(s.size() - 1);
    return s;
}

int SecondaryIndex::readAvail() {
    listFile.clear();
    listFile.seekg(0, ios::beg);
    int value = -1;
    listFile.read(reinterpret_cast<char*>(&value), sizeof(int));
    return value;
}

void SecondaryIndex::writeAvail(int value) {
    listFile.clear();
    listFile.seekp(0, ios::beg);
    listFile.write(reinterpret_cast<char*>(&value), sizeof(int));
    listFile.flush();
}

void SecondaryIndex::readNode(int node, string& doctorId, int& next) {
    listFile.clear();
    listFile.seekg(HEADER_SIZE + (long)node * SEC_NODE_SIZE, ios::beg);
    char buf[SEC_ID_LEN];
    listFile.read(buf, SEC_ID_LEN);
    listFile.read(reinterpret_cast<char*>(&next), sizeof(int));
    doctorId = sec_text(buf, SEC_ID_LEN);
}

void SecondaryIndex::writeNode(int node, const string& doctorId, int next) {
    listFile.clear();
    listFile.seekp(HEADER_SIZE + (long)node * SEC_NODE_SIZE, ios::beg);
    char buf[SEC_ID_LEN];
    sec_pad(buf, doctorId, SEC_ID_LEN);
    listFile.write(buf, SEC_ID_LEN);
    listFile.write(reinterpret_cast<char*>(&next), sizeof(int));
    listFile.flush();
}

void SecondaryIndex::writeNext(int node, int next) {
    listFile.clear();
    listFile.seekp(HEADER_SIZE + (long)node * SEC_NODE_SIZE + SEC_ID_LEN, ios::beg);
    listFile.write(reinterpret_cast<char*>(&next), sizeof(int));
    listFile.flush();
}

int SecondaryIndex::allocNode(const string& doctorId, int next) {
    int avail = readAvail();
    int node;

    if (avail != -1) {
        string dummy;
        int nextFree;
        readNode(avail, dummy, nextFree);
        node = avail;
        writeAvail(nextFree);
    }
    else {
        listFile.clear();
        listFile.seekg(0, ios::end);
        long size = (long)listFile.tellg();
        node = (int)((size - HEADER_SIZE) / SEC_NODE_SIZE);
    }

    writeNode(node, doctorId, next);
    return node;
}

void SecondaryIndex::freeNode(int node) {
    writeNode(node, "*", readAvail());
    writeAvail(node);
}

void SecondaryIndex::openListFile(bool truncate) {
    if (listFile.is_open()) listFile.close();

    string name = baseName + ".lst";
    if (truncate) { ofstream t(name.c_str(), ios::binary | ios::trunc); }

    listFile.open(name.c_str(), ios::in | ios::out | ios::binary);
    if (!listFile.is_open()) {
        ofstream create(name.c_str(), ios::binary);
        create.close();
        listFile.open(name.c_str(), ios::in | ios::out | ios::binary);
    }

    listFile.clear();
    listFile.seekg(0, ios::end);
    if ((long)listFile.tellg() < HEADER_SIZE) writeAvail(-1);
}

int SecondaryIndex::binarySearch(const string& key, bool& found) const {
    int low = 0, high = (int)keys.size() - 1;
    found = false;

    while (low <= high) {
        int mid = (low + high) / 2;
        int cmp = keys[mid].key.compare(key);
        if (cmp == 0) { found = true; return mid; }
        if (cmp < 0)  low = mid + 1; else high = mid - 1;
    }
    return low;
}

SecondaryIndex::SecondaryIndex(const string& name, int keyLength)
    : baseName(name), keyLen(keyLength) {
    openListFile(false);
    load();
}

SecondaryIndex::~SecondaryIndex() {
    save();
    if (listFile.is_open()) listFile.close();
}

void SecondaryIndex::reset() {
    keys.clear();
    openListFile(true);
    save();
}

void SecondaryIndex::save() {
    ofstream out((baseName + ".idx").c_str(), ios::binary | ios::trunc);
    int count = (int)keys.size();
    out.write(reinterpret_cast<char*>(&count), sizeof(int));

    for (size_t i = 0; i < keys.size(); i++) {
        char buf[SEC_NAME_LEN];
        sec_pad(buf, keys[i].key, keyLen);
        out.write(buf, keyLen);
        out.write(reinterpret_cast<char*>(&keys[i].head), sizeof(int));
    }
}

void SecondaryIndex::load() {
    keys.clear();
    ifstream in((baseName + ".idx").c_str(), ios::binary);
    if (!in.is_open()) return;

    int count = 0;
    in.read(reinterpret_cast<char*>(&count), sizeof(int));

    for (int i = 0; i < count; i++) {
        char buf[SEC_NAME_LEN];
        Entry e;
        in.read(buf, keyLen);
        in.read(reinterpret_cast<char*>(&e.head), sizeof(int));
        if (!in) break;
        e.key = sec_text(buf, keyLen);
        keys.push_back(e);
    }
}

bool SecondaryIndex::insert(const string& key, const string& doctorId) {
    bool found;
    int pos = binarySearch(key, found);

    if (!found) {
        Entry e;
        e.key = key;
        e.head = allocNode(doctorId, -1);
        keys.insert(keys.begin() + pos, e);
        return true;
    }

    int prev = -1, cur = keys[pos].head;
    while (cur != -1) {
        string id;
        int next;
        readNode(cur, id, next);
        if (id == doctorId) return false;
        if (id > doctorId) break;
        prev = cur;
        cur = next;
    }

    int node = allocNode(doctorId, cur);
    if (prev == -1) keys[pos].head = node;
    else            writeNext(prev, node);
    return true;
}

bool SecondaryIndex::remove(const string& key, const string& doctorId) {
    bool found;
    int pos = binarySearch(key, found);
    if (!found) return false;

    int prev = -1, cur = keys[pos].head;
    while (cur != -1) {
        string id;
        int next;
        readNode(cur, id, next);

        if (id == doctorId) {
            if (prev == -1) keys[pos].head = next;
            else            writeNext(prev, next);
            freeNode(cur);
            if (keys[pos].head == -1) keys.erase(keys.begin() + pos);
            return true;
        }
        prev = cur;
        cur = next;
    }
    return false;
}

vector<string> SecondaryIndex::searchKey(const string& key) {
    vector<string> ids;
    bool found;
    int pos = binarySearch(key, found);
    if (!found) return ids;

    int cur = keys[pos].head;
    while (cur != -1) {
        string id;
        int next;
        readNode(cur, id, next);
        ids.push_back(id);
        cur = next;
    }
    return ids;
}

void SecondaryIndex::print() {
    for (size_t i = 0; i < keys.size(); i++) {
        cout << keys[i].key << " -> ";
        vector<string> ids = searchKey(keys[i].key);
        for (size_t j = 0; j < ids.size(); j++) cout << ids[j] << " ";
        cout << endl;
    }
}

static SecondaryIndex doctor_name_index("doctor_name", SEC_NAME_LEN);
static SecondaryIndex doctor_specialty_index("doctor_specialty", SEC_SPEC_LEN);

static void link_doctor(const string& id, const string& name, const string& specialty) {
    doctor_name_index.insert(name, id);
    doctor_specialty_index.insert(specialty, id);
}

static void unlink_doctor(const string& id, const string& name, const string& specialty) {
    doctor_name_index.remove(name, id);
    doctor_specialty_index.remove(specialty, id);
}

void load_doctor_secondary_indexes(fstream& f) {
    doctor_name_index.reset();
    doctor_specialty_index.reset();

    f.clear();
    f.seekg(0, ios::end);
    int fileSize = (int)f.tellg();

    for (int offset = HEADER_SIZE; offset + DOCTOR_SIZE <= fileSize; offset += DOCTOR_SIZE) {
        Doctor d;
        if (!readDoctorAt(f, offset, d)) break;
        if (isDeleted((char*)&d)) continue;

        string id = sec_text(d.id, SEC_ID_LEN);
        if (id.empty()) continue;
        link_doctor(id, sec_text(d.name, SEC_NAME_LEN), sec_text(d.specialty, SEC_SPEC_LEN));
    }
    f.clear();
}

int add_doctor_with_secondary(fstream& f, const Doctor& d) {
    int offset = add_doctor_indexed(f, d);
    if (offset == -1) return -1;

    link_doctor(sec_text(d.id, SEC_ID_LEN),
        sec_text(d.name, SEC_NAME_LEN),
        sec_text(d.specialty, SEC_SPEC_LEN));
    return offset;
}

bool remove_doctor_with_secondary(fstream& f, const string& id) {
    int offset = find_doctor_offset(id);
    if (offset == -1) return false;

    Doctor d;
    if (!readDoctorAt(f, offset, d)) return false;

    if (!remove_doctor(f, id)) return false;

    unlink_doctor(sec_text(d.id, SEC_ID_LEN),
        sec_text(d.name, SEC_NAME_LEN),
        sec_text(d.specialty, SEC_SPEC_LEN));
    return true;
}

bool update_doctor_name_with_secondary(fstream& f, const string& id, const string& name) {
    int offset = find_doctor_offset(id);
    if (offset == -1) return false;

    Doctor before;
    if (!readDoctorAt(f, offset, before)) return false;

    if (!update_doctor_name(f, id, name)) return false;

    Doctor after;
    if (!readDoctorAt(f, offset, after)) return false;

    doctor_name_index.remove(sec_text(before.name, SEC_NAME_LEN), sec_text(before.id, SEC_ID_LEN));
    doctor_name_index.insert(sec_text(after.name, SEC_NAME_LEN), sec_text(after.id, SEC_ID_LEN));
    return true;
}

vector<string> find_doctors_by_name(const string& name) {
    return doctor_name_index.searchKey(name);
}

vector<string> find_doctors_by_specialty(const string& specialty) {
    return doctor_specialty_index.searchKey(specialty);
}

void print_doctor_secondary_indexes() {
    cout << "--- Doctor Name index ---" << endl;
    doctor_name_index.print();
    cout << "--- Doctor Specialty index ---" << endl;
    doctor_specialty_index.print();
}

bool update_doctor_id(fstream& f, const string& oldId, const string& newId) {
    string oldKey = sec_trim(oldId);
    string newKey = sec_trim(newId);

    if (newKey.empty() || newKey.size() > SEC_ID_LEN || newKey[0] == '*') return false;

    int offset = find_doctor_offset(oldKey);
    if (offset == -1) return false;
    if (find_doctor_position(newKey) != -1) return false;

    Doctor d;
    if (!readDoctorAt(f, offset, d)) return false;
    string name = sec_text(d.name, SEC_NAME_LEN);
    string spec = sec_text(d.specialty, SEC_SPEC_LEN);

    if (!remove_doctor(f, oldKey)) return false;

    setField(d.id, SEC_ID_LEN, newKey);
    if (add_doctor_indexed(f, d) == -1) {
        setField(d.id, SEC_ID_LEN, oldKey);
        add_doctor_indexed(f, d);
        return false;
    }

    unlink_doctor(oldKey, name, spec);
    link_doctor(newKey, name, spec);
    return true;
}