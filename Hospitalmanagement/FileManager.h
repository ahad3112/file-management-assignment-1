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

#endif
