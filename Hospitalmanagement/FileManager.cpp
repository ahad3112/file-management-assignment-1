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
