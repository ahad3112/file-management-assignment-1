#ifndef APPOINTMENT_MANAGER_H
#define APPOINTMENT_MANAGER_H

#include "FileManager.h"
#include <string>
#include <vector>

const int MAX_APPTS = 200;
const char APPT_SEC_FILE[] = "appointment_doctor_idx.dat";

struct ApptIndexEntry {
    char id[15];
    int  offset;
};

extern ApptIndexEntry appointment_index[MAX_APPTS];
extern int appointment_count;

struct ApptSecIndexEntry {
    char doctorId[15];
    int  headOffset;
};

const int SEC_NODE_SIZE = 15 + sizeof(int);

void load_appointment_index(fstream& f);
int  find_appt_position(const string& id);
int  find_appt_offset(const string& id);

int  add_appointment_indexed(fstream& f, const Appointment& a);
bool remove_appointment(fstream& f, const string& id);
bool update_appointment_date(fstream& f, const string& id, const string& newDate);
bool print_appointment_info(fstream& f, const string& id);

int  find_appt_sec_position(const string& doctorId);
bool add_to_appt_sec_index(const string& doctorId, const string& apptId);
bool remove_from_appt_sec_index(const string& doctorId, const string& apptId);
void print_appointments_by_doctor(fstream& apptFile, const string& doctorId);
vector<string> get_appointment_ids_by_doctor(const string& doctorId);

#endif