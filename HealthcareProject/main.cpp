#include <iostream>
#include <fstream>
#include "FileManager.h"
#include "AppointmentManager.h"
#include "DoctorIndexManager.h"
#include "QueryEngine.h"
using namespace std;

const char* DOCTOR_FILE = "doctors.dat";
const char* APPT_FILE   = "appointments.dat";

static string readLine(const string& prompt) {
    string value;
    cout << prompt;
    getline(cin, value);
    return value;
}

static bool badKey(const string& id) {
    // '*' as first char would look like a deleted record
    return !id.empty() && id[0] == '*';
}

int main() {

    initFile(DOCTOR_FILE);
    initFile(APPT_FILE);

    fstream doctorFile(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    fstream apptFile(APPT_FILE, ios::in | ios::out | ios::binary);
    if (!doctorFile.is_open()) { cout << "Could not open " << DOCTOR_FILE << endl; return 1; }
    if (!apptFile.is_open())   { cout << "Could not open " << APPT_FILE << endl;   return 1; }

    // rebuild every index from the data files
    load_doctor_index(doctorFile);
    load_doctor_secondary_indexes(doctorFile);
    load_appointment_index(apptFile);

    int choice = 0;

    while (true) {
        cout << "\n===== Healthcare Management System =====" << endl;
        cout << "6.  Add New Doctor" << endl;
        cout << "7.  Add New Appointment" << endl;
        cout << "8.  Update Doctor Name / Address (Doctor ID)" << endl;
        cout << "9.  Update Doctor ID [Key Update Test]" << endl;
        cout << "10. Update Appointment Date (Appointment ID)" << endl;
        cout << "11. Delete Appointment (Appointment ID)" << endl;
        cout << "12. Delete Doctor (Doctor ID)" << endl;
        cout << "13. Print Doctor Info (Doctor ID)" << endl;
        cout << "14. Print Appointment Info (Appointment ID)" << endl;
        cout << "15. Write Query" << endl;
        cout << "16. Exit" << endl;
        cout << "Enter choice: ";

        if (!(cin >> choice)) {
            if (cin.eof()) break;
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Wrong choice, try again." << endl;
            continue;
        }
        cin.ignore(1000, '\n');

        if (choice == 6) {
            Doctor d;
            string id = readLine("Doctor ID     : ");
            if (badKey(id)) { cout << "Doctor ID cannot start with '*'." << endl; continue; }
            setField(d.id, 15, id);
            setField(d.name, 30, readLine("Doctor Name   : "));
            setField(d.specialty, 20, readLine("Specialty     : "));
            setField(d.address, 30, readLine("Address       : "));

            int offset = add_doctor_with_secondary(doctorFile, d);
            if (offset == -1)
                cout << "Doctor ID is empty or already exists, nothing was written." << endl;
            else
                cout << "Doctor added at offset " << offset << "." << endl;
        }
        else if (choice == 7) {
            Appointment a;
            string id = readLine("Appointment ID : ");
            if (badKey(id)) { cout << "Appointment ID cannot start with '*'." << endl; continue; }
            setField(a.id, 15, id);
            setField(a.date, 30, readLine("Date           : "));
            setField(a.doctorId, 15, readLine("Doctor ID      : "));

            int r = add_appointment_indexed(apptFile, a);
            if (r == -1)      cout << "Appointment/Doctor ID is empty or appointment already exists, nothing was written." << endl;
            else if (r == -2) cout << "Doctor ID does not exist, nothing was written." << endl;
            else if (r == -3) cout << "Appointment index is full, nothing was written." << endl;
            else              cout << "Appointment added at offset " << r << "." << endl;
        }
        else if (choice == 8) {
            string id = readLine("Doctor ID: ");
            if (find_doctor_offset(id) == -1) {
                cout << "Warning: Doctor ID not found." << endl;
                continue;
            }

            string field = readLine("Field to update (n = name, a = address): ");
            if (!field.empty() && (field[0] == 'n' || field[0] == 'N')) {
                string value = readLine("New name: ");
                update_doctor_name_with_secondary(doctorFile, id, value);   // keeps name index in sync
                cout << "Name updated." << endl;
            }
            else if (!field.empty() && (field[0] == 'a' || field[0] == 'A')) {
                string value = readLine("New address: ");
                update_doctor_address(doctorFile, id, value);
                cout << "Address updated." << endl;
            }
            else {
                cout << "Unknown field." << endl;
            }
        }
        else if (choice == 9) {
            string oldId = readLine("Current Doctor ID: ");
            string newId = readLine("New Doctor ID    : ");

            if (find_doctor_offset(oldId) == -1) {
                cout << "Warning: Doctor ID not found." << endl;
                continue;
            }
            if (update_doctor_id(doctorFile, oldId, newId)) {
                cout << "Doctor ID updated (primary index re-sorted)." << endl;
                if (!get_appointment_ids_by_doctor(oldId).empty())
                    cout << "Note: existing appointments still reference the old Doctor ID." << endl;
            }
            else
                cout << "Update failed: new ID is empty, too long, starts with '*', or already exists." << endl;
        }
        else if (choice == 10) {
            string id = readLine("Appointment ID: ");
            if (find_appt_offset(id) == -1) {
                cout << "Warning: Appointment ID not found." << endl;
                continue;
            }
            string date = readLine("New date: ");
            update_appointment_date(apptFile, id, date);
            cout << "Date updated." << endl;
        }
        else if (choice == 11) {
            string id = readLine("Appointment ID: ");
            if (remove_appointment(apptFile, id))
                cout << "Appointment deleted (record marked and added to avail list)." << endl;
            else
                cout << "Warning: Appointment ID does not exist." << endl;
        }
        else if (choice == 12) {
            string id = readLine("Doctor ID: ");
            if (find_doctor_offset(id) == -1) {
                cout << "Warning: Doctor ID does not exist." << endl;
                continue;
            }
            size_t n = get_appointment_ids_by_doctor(id).size();
            if (n > 0) {
                cout << "This doctor still has " << n << " appointment(s)." << endl;
                string yn = readLine("Delete anyway? (y/n): ");
                if (yn.empty() || (yn[0] != 'y' && yn[0] != 'Y')) {
                    cout << "Cancelled." << endl;
                    continue;
                }
            }
            if (remove_doctor_with_secondary(doctorFile, id))
                cout << "Doctor deleted (record marked and added to avail list)." << endl;
            else
                cout << "Warning: Doctor ID does not exist." << endl;
        }
        else if (choice == 13) {
            string id = readLine("Doctor ID: ");
            if (!print_doctor_info(doctorFile, id))
                cout << "Warning: Doctor ID not found." << endl;
        }
        else if (choice == 14) {
            string id = readLine("Appointment ID: ");
            if (!print_appointment_info(apptFile, id))
                cout << "Warning: Appointment ID not found." << endl;
        }
        else if (choice == 15) {
            cout << "Examples:\n"
                 << "  Select all from Doctors where Doctor ID='D1';\n"
                 << "  Select all from Appointments where Doctor ID='D1';\n"
                 << "  Select Doctor Name from Doctors where Doctor Name='Ali';\n"
                 << "  Select all from Doctors where Doctor Name='Ali' and Doctor Specialty='Cardio';\n";
            string query = readLine("Query> ");
            run_query(doctorFile, apptFile, query);
        }
        else if (choice == 16) {
            break;
        }
        else {
            cout << "Wrong choice, try again." << endl;
        }
    }

    doctorFile.close();
    apptFile.close();
    return 0;
}
