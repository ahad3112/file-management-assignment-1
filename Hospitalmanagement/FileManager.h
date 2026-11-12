#ifndef FILEMANAGER_H_INCLUDED
#define FILEMANAGER_H_INCLUDED

#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
using namespace std;


const int DOCTOR_SIZE  = 95;
const int APPT_SIZE    = 60;
const int HEADER_SIZE  = sizeof(int);
const int MAX_DOCTORS  = 100;

struct Doctor {
    char id[15];
    char name[30];
    char specialty[20];
    char address[30];
};

struct Appointment {
    char id[15];
    char date[30];
    char doctorId[15];
};
void setField(char* dest, int size, const string& value);
void initFile(const char* filename);
int  readHeader(fstream& f);
void writeHeader(fstream& f, int value);

int  appendDoctor(fstream& f, const Doctor& d);
bool readDoctorAt(fstream& f, int offset, Doctor& d);
void writeDoctorAt(fstream& f, int offset, const Doctor& d);

int  appendAppointment(fstream& f, const Appointment& a);
bool readAppointmentAt(fstream& f, int offset, Appointment& a);
void writeAppointmentAt(fstream& f, int offset, const Appointment& a);

bool isDeleted(const char* record);
void deleteRecordAt(fstream& f, int offset);
int  getNextAvail(fstream& f, int offset);

int addDoctor(fstream& f, const Doctor& d);
int addAppointment(fstream& f, const Appointment& a);

// Primary index on Doctor ID (sorted array, binary search)
void load_doctor_index(fstream& f);
int  find_doctor_position(const string& id);
int  find_doctor_offset(const string& id);
int  add_doctor_indexed(fstream& f, const Doctor& d);
bool remove_doctor(fstream& f, const string& id);
bool update_doctor_name(fstream& f, const string& id, const string& name);
bool update_doctor_address(fstream& f, const string& id, const string& address);
bool print_doctor_info(fstream& f, const string& id);

#endif
