#include "FileManager.h"

void setField(char* dest, int size, const string& value) {
    memset(dest, ' ', size);
    int len = value.length();
    if (len > size) len = size;
    memcpy(dest, value.c_str(), len);
}

void initFile(const char* filename) {

    ifstream check(filename, ios::binary);
    if (check.is_open()) {
        check.close();
        return;
    }


    ofstream out(filename, ios::binary);
    int head = -1;
    out.write(reinterpret_cast<char*>(&head), sizeof(int));
    out.close();
}

int readHeader(fstream& f) {
    int head = -1;
    f.seekg(0, ios::beg);
    f.read(reinterpret_cast<char*>(&head), sizeof(int));
    return head;
}

void writeHeader(fstream& f, int value) {
    f.seekp(0, ios::beg);
    f.write(reinterpret_cast<char*>(&value), sizeof(int));
    f.flush();
}


int appendDoctor(fstream& f, const Doctor& d) {
    f.clear();
    f.seekp(0, ios::end);
    int offset = (int)f.tellp();
    f.write(reinterpret_cast<const char*>(&d), DOCTOR_SIZE);
    f.flush();
    return offset;
}

bool readDoctorAt(fstream& f, int offset, Doctor& d) {
    f.clear();
    f.seekg(offset, ios::beg);
    f.read(reinterpret_cast<char*>(&d), DOCTOR_SIZE);
    return f.gcount() == DOCTOR_SIZE;
}

void writeDoctorAt(fstream& f, int offset, const Doctor& d) {
    f.clear();
    f.seekp(offset, ios::beg);
    f.write(reinterpret_cast<const char*>(&d), DOCTOR_SIZE);
    f.flush();
}


int appendAppointment(fstream& f, const Appointment& a) {
    f.clear();
    f.seekp(0, ios::end);
    int offset = (int)f.tellp();
    f.write(reinterpret_cast<const char*>(&a), APPT_SIZE);
    f.flush();
    return offset;
}

bool readAppointmentAt(fstream& f, int offset, Appointment& a) {
    f.clear();
    f.seekg(offset, ios::beg);
    f.read(reinterpret_cast<char*>(&a), APPT_SIZE);
    return f.gcount() == APPT_SIZE;
}

void writeAppointmentAt(fstream& f, int offset, const Appointment& a) {
    f.clear();
    f.seekp(offset, ios::beg);
    f.write(reinterpret_cast<const char*>(&a), APPT_SIZE);
    f.flush();
}


bool isDeleted(const char* record) {
    return record[0] == '*';
}


void deleteRecordAt(fstream& f, int offset) {
    f.clear();
    int oldHead = readHeader(f);

    f.clear();
    f.seekp(offset, ios::beg);
    char marker = '*';
    f.write(&marker, 1);
    f.write(reinterpret_cast<char*>(&oldHead), sizeof(int));
    f.flush();

    writeHeader(f, offset);
}

int getNextAvail(fstream& f, int offset) {
    f.clear();
    f.seekg(offset + 1, ios::beg);
    int next = -1;
    f.read(reinterpret_cast<char*>(&next), sizeof(int));
    return next;
}

int addDoctor(fstream& f, const Doctor& d) {
    f.clear();
    int head = readHeader(f);

    if (head == -1) {
        return appendDoctor(f, d);
    }

    int next = getNextAvail(f, head);
    writeDoctorAt(f, head, d);
    writeHeader(f, next);
    return head;
}

int addAppointment(fstream& f, const Appointment& a) {
    f.clear();
    int head = readHeader(f);

    if (head == -1) {
        return appendAppointment(f, a);
    }

    int next = getNextAvail(f, head);
    writeAppointmentAt(f, head, a);
    writeHeader(f, next);
    return head;
}


// Primary index on Doctor ID: sorted array of (key, record offset), binary search
struct IndexEntry {
    char id[15];
    int  offset;
};

static IndexEntry doctor_index[MAX_DOCTORS];
static int doctor_count = 0;

// reads a fixed length field and gives it back as a normal string (no padding)
static string field_of(const char* src, int size) {
    string value = "";
    // as log as I am in the field and it's not EOF
    for (int i = 0; i < size && src[i] != '\0'; i++)
        value += src[i]; //you are one  of us now

    //remove the spaces and stuff
    while (!value.empty() && value[value.size() - 1] == ' ')
        value.erase(value.size() - 1);
    return value;
}

// pads every field to its exact size so the record stays 95 bytes (15 + 30 + 20 + 30)
static void normalize_doctor(Doctor& d) {
    setField(d.id, 15, field_of(d.id, 15));
    setField(d.name, 30, field_of(d.name, 30));
    setField(d.specialty, 20, field_of(d.specialty, 20));
    setField(d.address, 30, field_of(d.address, 30));
}

// binary search inside the sorted index, returns position or -1
int find_doctor_position(const string& id) {
    string key = id;
    //remove the spaces at the end
    while (!key.empty() && key[key.size() - 1] == ' ')
        key.erase(key.size() - 1);

    //binary search
    int low = 0, high = doctor_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        string mid_key = field_of(doctor_index[mid].id, 15);
        if (mid_key == key) return mid;
        if (mid_key < key)  low = mid + 1; else high = mid - 1;
    }
    return -1;
}

int find_doctor_offset(const string& id) {
    int pos = find_doctor_position(id);
    if (pos == -1) return -1;
    return doctor_index[pos].offset;
}

// keeps the index sorted ascending, shifting the entries after the insert place
static bool insert_index(const string& id, int offset) {
    if (doctor_count >= MAX_DOCTORS) return false;

    int pos = 0;
    while (pos < doctor_count && field_of(doctor_index[pos].id, 15) < id) // while we still didn't reach the sweet spot, keep going
        pos++;

    for (int i = doctor_count; i > pos; i--) //after we have found the right place, shift everything forward
        doctor_index[i] = doctor_index[i - 1];

    setField(doctor_index[pos].id, 15, id); //actually insert the stuff
    doctor_index[pos].offset = offset;
    doctor_count++;
    return true;
}

// shifts the entries left so the array stays sorted after a delete
static void erase_index(int pos) {
    for (int i = pos; i < doctor_count - 1; i++) //shift (backward) everything after the deleted index
        doctor_index[i] = doctor_index[i + 1];
    doctor_count--;
}

// rebuilds the index by scanning the doctors file (skips deleted records)
void load_doctor_index(fstream& f) {
    doctor_count = 0;

    f.clear();
    f.seekg(0, ios::end);
    int fileSize = (int)f.tellg();

    for (int offset = HEADER_SIZE; offset + DOCTOR_SIZE <= fileSize; offset += DOCTOR_SIZE) {
        Doctor d;
        if (!readDoctorAt(f, offset, d)) break;
        if (isDeleted((char*)&d)) continue;

        string id = field_of(d.id, 15);
        if (id.empty() || find_doctor_position(id) != -1) continue;

        insert_index(id, offset);
    }
    f.clear();
}

int add_doctor_indexed(fstream& f, const Doctor& input) {
    Doctor d = input;
    normalize_doctor(d);

    string id = field_of(d.id, 15);
    if (id.empty()) return -1;

    // we said no duplicate primary keys
    if (find_doctor_position(id) != -1) return -1;

    int offset = addDoctor(f, d);   // reuses the avail list when possible
    insert_index(id, offset);
    return offset;
}

bool remove_doctor(fstream& f, const string& id) {
    int pos = find_doctor_position(id);
    if (pos == -1) return false;

    deleteRecordAt(f, doctor_index[pos].offset);  // marker + avail list
    erase_index(pos);
    return true;
}

bool update_doctor_name(fstream& f, const string& id, const string& name) {
    int offset = find_doctor_offset(id);
    if (offset == -1) return false;

    Doctor d;
    if (!readDoctorAt(f, offset, d)) return false;

    setField(d.name, 30, name);     // truncated if longer than 30
    writeDoctorAt(f, offset, d);
    return true;
}

bool update_doctor_address(fstream& f, const string& id, const string& address) {
    int offset = find_doctor_offset(id);
    if (offset == -1) return false;

    Doctor d;
    if (!readDoctorAt(f, offset, d)) return false;

    setField(d.address, 30, address);
    writeDoctorAt(f, offset, d);
    return true;
}

bool print_doctor_info(fstream& f, const string& id) {
    int offset = find_doctor_offset(id);
    if (offset == -1) return false;

    Doctor d;
    if (!readDoctorAt(f, offset, d)) return false;

    cout << "doctor ID: " << field_of(d.id, 15) << '\n';
    cout << "doctor name: " << field_of(d.name, 30) << '\n';
    cout << "Specialty : " << field_of(d.specialty, 20) << endl;
    cout << "Address: " << field_of(d.address, 30) << endl;
    cout << "Record offset: " << offset << endl;
    return true;
}
