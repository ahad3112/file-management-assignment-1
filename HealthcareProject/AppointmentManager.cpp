#include "AppointmentManager.h"

ApptIndexEntry appointment_index[MAX_APPTS];
int appointment_count = 0;

static ApptSecIndexEntry sec_index[MAX_DOCTORS];
static int sec_count = 0;

static string field_of_appt(const char* src, int size) {
    string value = "";
    for (int i = 0; i < size && src[i] != '\0'; i++)
        value += src[i];
    while (!value.empty() && value[value.size() - 1] == ' ')
        value.erase(value.size() - 1);
    return value;
}

static void insert_appt_index(const string& id, int offset) {
    int pos = 0;
    while (pos < appointment_count && field_of_appt(appointment_index[pos].id, 15) < id)
        pos++;
    for (int i = appointment_count; i > pos; i--)
        appointment_index[i] = appointment_index[i - 1];
    setField(appointment_index[pos].id, 15, id);
    appointment_index[pos].offset = offset;
    appointment_count++;
}

static void erase_appt_index(int pos) {
    for (int i = pos; i < appointment_count - 1; i++)
        appointment_index[i] = appointment_index[i + 1];
    appointment_count--;
}

void load_appointment_index(fstream& f) {
    appointment_count = 0;

    sec_count = 0;
    { ofstream trunc(APPT_SEC_FILE, ios::binary | ios::trunc); }

    f.clear();
    f.seekg(0, ios::end);
    int fileSize = (int)f.tellg();

    for (int offset = HEADER_SIZE; offset + APPT_SIZE <= fileSize; offset += APPT_SIZE) {
        Appointment a;
        if (!readAppointmentAt(f, offset, a)) break;
        if (isDeleted((char*)&a)) continue;

        string id = field_of_appt(a.id, 15);
        if (id.empty() || find_appt_position(id) != -1) continue;

        insert_appt_index(id, offset);
        add_to_appt_sec_index(field_of_appt(a.doctorId, 15), id);
    }
    f.clear();
}

int find_appt_position(const string& id) {
    string key = id;
    while (!key.empty() && key[key.size() - 1] == ' ')
        key.erase(key.size() - 1);

    int low = 0, high = appointment_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        string mid_key = field_of_appt(appointment_index[mid].id, 15);
        if (mid_key == key) return mid;
        if (mid_key < key) low = mid + 1;
        else               high = mid - 1;
    }
    return -1;
}

int find_appt_offset(const string& id) {
    int pos = find_appt_position(id);
    if (pos == -1) return -1;
    return appointment_index[pos].offset;
}

int add_appointment_indexed(fstream& f, const Appointment& input) {
    Appointment a = input;
    string id  = field_of_appt(a.id, 15);
    string did = field_of_appt(a.doctorId, 15);
    string dt  = field_of_appt(a.date, 30);

    if (id.empty() || did.empty()) return -1;
    if (find_appt_position(id) != -1) return -1;
    if (find_doctor_offset(did) == -1) return -2;
    if (appointment_count >= MAX_APPTS) return -3;

    setField(a.id, 15, id);
    setField(a.date, 30, dt);
    setField(a.doctorId, 15, did);

    int offset = addAppointment(f, a);

    insert_appt_index(id, offset);
    add_to_appt_sec_index(did, id);
    return offset;
}

bool remove_appointment(fstream& f, const string& id) {
    int pos = find_appt_position(id);
    if (pos == -1) return false;

    int offset = appointment_index[pos].offset;

    Appointment a;
    if (!readAppointmentAt(f, offset, a)) return false;
    string did = field_of_appt(a.doctorId, 15);

    deleteRecordAt(f, offset);
    remove_from_appt_sec_index(did, field_of_appt(a.id, 15));
    erase_appt_index(pos);
    return true;
}

bool update_appointment_date(fstream& f, const string& id, const string& newDate) {
    int offset = find_appt_offset(id);
    if (offset == -1) return false;

    Appointment a;
    if (!readAppointmentAt(f, offset, a)) return false;

    setField(a.date, 30, newDate);
    writeAppointmentAt(f, offset, a);
    return true;
}

bool print_appointment_info(fstream& f, const string& id) {
    int offset = find_appt_offset(id);
    if (offset == -1) return false;

    Appointment a;
    if (!readAppointmentAt(f, offset, a)) return false;

    cout << "Appointment ID: " << field_of_appt(a.id, 15) << '\n';
    cout << "Date: " << field_of_appt(a.date, 30) << '\n';
    cout << "Doctor ID: " << field_of_appt(a.doctorId, 15) << '\n';
    cout << "Record offset: " << offset << endl;
    return true;
}

static bool open_sec_file(fstream& sf) {
    sf.open(APPT_SEC_FILE, ios::binary | ios::in | ios::out);
    if (!sf.is_open()) {
        ofstream create(APPT_SEC_FILE, ios::binary);
        create.close();
        sf.clear();
        sf.open(APPT_SEC_FILE, ios::binary | ios::in | ios::out);
    }
    return sf.is_open();
}

static void read_sec_node(fstream& sf, int off, string& apptId, int& next) {
    char buf[15];
    sf.clear();
    sf.seekg(off, ios::beg);
    sf.read(buf, 15);
    sf.read(reinterpret_cast<char*>(&next), sizeof(int));
    apptId = field_of_appt(buf, 15);
}

static void write_sec_node(fstream& sf, int off, const string& apptId, int next) {
    char buf[15];
    setField(buf, 15, apptId);
    sf.clear();
    sf.seekp(off, ios::beg);
    sf.write(buf, 15);
    sf.write(reinterpret_cast<const char*>(&next), sizeof(int));
    sf.flush();
}

static void write_sec_next(fstream& sf, int off, int next) {
    sf.clear();
    sf.seekp(off + 15, ios::beg);
    sf.write(reinterpret_cast<const char*>(&next), sizeof(int));
    sf.flush();
}

int find_appt_sec_position(const string& doctorId) {
    string key = doctorId;
    while (!key.empty() && key[key.size() - 1] == ' ')
        key.erase(key.size() - 1);

    int low = 0, high = sec_count - 1;
    while (low <= high) {
        int mid = (low + high) / 2;
        string mid_key = field_of_appt(sec_index[mid].doctorId, 15);
        if (mid_key == key) return mid;
        if (mid_key < key) low = mid + 1;
        else               high = mid - 1;
    }
    return -1;
}

bool add_to_appt_sec_index(const string& doctorId, const string& apptId) {
    fstream sf;
    if (!open_sec_file(sf)) return false;

    sf.seekp(0, ios::end);
    int newOff = (int)sf.tellp();

    int pos = find_appt_sec_position(doctorId);

    if (pos == -1) {
        if (sec_count >= MAX_DOCTORS) return false;

        int p = 0;
        while (p < sec_count && field_of_appt(sec_index[p].doctorId, 15) < doctorId)
            p++;
        for (int i = sec_count; i > p; i--)
            sec_index[i] = sec_index[i - 1];

        setField(sec_index[p].doctorId, 15, doctorId);
        sec_index[p].headOffset = newOff;
        sec_count++;

        write_sec_node(sf, newOff, apptId, -1);
    } else {
        int prev = -1;
        int cur  = sec_index[pos].headOffset;
        while (cur != -1) {
            string id; int next;
            read_sec_node(sf, cur, id, next);
            if (id > apptId) break;
            prev = cur;
            cur  = next;
        }
        write_sec_node(sf, newOff, apptId, cur);
        if (prev == -1) sec_index[pos].headOffset = newOff;
        else            write_sec_next(sf, prev, newOff);
    }
    sf.close();
    return true;
}

bool remove_from_appt_sec_index(const string& doctorId, const string& apptId) {
    int pos = find_appt_sec_position(doctorId);
    if (pos == -1) return false;

    fstream sf;
    if (!open_sec_file(sf)) return false;

    int prev = -1;
    int cur  = sec_index[pos].headOffset;
    while (cur != -1) {
        string id; int next;
        read_sec_node(sf, cur, id, next);
        if (id == apptId) {
            if (prev == -1) sec_index[pos].headOffset = next;
            else            write_sec_next(sf, prev, next);
            if (sec_index[pos].headOffset == -1) {
                for (int i = pos; i < sec_count - 1; i++)
                    sec_index[i] = sec_index[i + 1];
                sec_count--;
            }
            sf.close();
            return true;
        }
        prev = cur;
        cur  = next;
    }
    sf.close();
    return false;
}

void print_appointments_by_doctor(fstream& apptFile, const string& doctorId) {
    cout << "\n--- Appointments for Doctor " << doctorId << " ---\n";

    int pos = find_appt_sec_position(doctorId);
    if (pos == -1) {
        cout << "No active appointments found for this doctor.\n";
        return;
    }

    fstream sf;
    if (!open_sec_file(sf)) return;

    int cur = sec_index[pos].headOffset;
    while (cur != -1) {
        string id; int next;
        read_sec_node(sf, cur, id, next);
        print_appointment_info(apptFile, id);
        cout << "-----\n";
        cur = next;
    }
    sf.close();
}

// Added by Member 5 (integration): returns the appointment IDs of a doctor via the secondary index
vector<string> get_appointment_ids_by_doctor(const string& doctorId) {
    vector<string> ids;
    int pos = find_appt_sec_position(doctorId);
    if (pos == -1) return ids;

    fstream sf;
    if (!open_sec_file(sf)) return ids;

    int cur = sec_index[pos].headOffset;
    while (cur != -1) {
        string id; int next;
        read_sec_node(sf, cur, id, next);
        ids.push_back(id);
        cur = next;
    }
    sf.close();
    return ids;
}
