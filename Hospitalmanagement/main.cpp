#include <iostream>
#include <fstream>
#include "FileManager.h"
using namespace std;

const char* DOCTOR_FILE = "doctors.dat";

int main() {

    initFile(DOCTOR_FILE);

    fstream file(DOCTOR_FILE, ios::in | ios::out | ios::binary);
    if (!file.is_open()) {
        cout << "Could not open " << DOCTOR_FILE << endl;
        return 1;
    }

    load_doctor_index(file);

    string line;
    int choice = 0;

    while (true) {
        cout << "\n===== Healthcare Management System =====" << endl;
        cout << "6.  Add New Doctor" << endl;
        cout << "8.  Update Doctor Name / Address (Doctor ID)" << endl;
        cout << "12. Delete Doctor (Doctor ID)" << endl;
        cout << "13. Print Doctor Info (Doctor ID)" << endl;
        cout << "16. Exit" << endl;
        cout << "Enter choice: ";

        if (!(cin >> choice)) {
            cin.clear();
            cin.ignore(1000, '\n');
            cout << "Wrong choice, try again." << endl;
            continue;
        }
        cin.ignore(1000, '\n');

        if (choice == 6) {

            Doctor d;
            string value;

            cout << "Doctor ID     : "; getline(cin, value); setField(d.id, 15, value);
            cout << "Doctor Name   : "; getline(cin, value); setField(d.name, 30, value);
            cout << "Specialty     : "; getline(cin, value); setField(d.specialty, 20, value);
            cout << "Address       : "; getline(cin, value); setField(d.address, 30, value);

            int offset = add_doctor_indexed(file, d);
            if (offset == -1)
                cout << "Doctor ID is empty or already exists, nothing was written." << endl;
            else
                cout << "Doctor added at offset " << offset << "." << endl;
        }
        else if (choice == 8) {

            string id, value;
            cout << "Doctor ID: "; getline(cin, id);

            if (find_doctor_offset(id) == -1) {
                cout << "Warning: Doctor ID not found." << endl;
                continue;
            }

            cout << "Field to update (n = name, a = address): ";
            getline(cin, line);

            if (!line.empty() && (line[0] == 'n' || line[0] == 'N')) {
                cout << "New name: "; getline(cin, value);
                update_doctor_name(file, id, value);
                cout << "Name updated." << endl;
            }
            else if (!line.empty() && (line[0] == 'a' || line[0] == 'A')) {
                cout << "New address: "; getline(cin, value);
                update_doctor_address(file, id, value);
                cout << "Address updated." << endl;
            }
            else {
                cout << "Unknown field." << endl;
            }
        }
        else if (choice == 12) {

            string id;
            cout << "Doctor ID: "; getline(cin, id);

            if (remove_doctor(file, id))
                cout << "Doctor deleted (record marked and added to avail list)." << endl;
            else
                cout << "Warning: Doctor ID does not exist." << endl;
        }
        else if (choice == 13) {

            string id;
            cout << "Doctor ID: "; getline(cin, id);

            if (!print_doctor_info(file, id))
                cout << "Warning: Doctor ID not found." << endl;
        }
        else if (choice == 16) {
            break;
        }
        else {
            cout << "Wrong choice, try again." << endl;
        }
    }

    file.close();
    return 0;
}
