#ifndef QUERY_ENGINE_H
#define QUERY_ENGINE_H

#include "FileManager.h"

// Parses and runs a query such as:
//   Select all from Doctors where Doctor ID='D1';
//   Select Doctor Name from Doctors where Doctor Name='Ali' and Doctor Specialty='Cardio';
//   Select all from Appointments where Doctor ID='D1';
void run_query(fstream& doctorFile, fstream& apptFile, const string& query);

#endif
