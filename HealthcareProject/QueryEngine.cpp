#include "QueryEngine.h"
#include "AppointmentManager.h"
#include "DoctorIndexManager.h"
#include <vector>
#include <map>
#include <cctype>

struct Condition {
    string field;   // normalized (lowercase, single spaces)
    string value;   // text between the quotes
};

static string q_trim(const string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static string q_lower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = (char)tolower((unsigned char)s[i]);
    return s;
}

// lowercase + collapse repeated whitespace ("Doctor   ID" -> "doctor id")
static string q_norm(const string& s) {
    string out;
    bool space = false;
    string t = q_trim(s);
    for (size_t i = 0; i < t.size(); i++) {
        if (isspace((unsigned char)t[i])) space = true;
        else {
            if (space && !out.empty()) out += ' ';
            space = false;
            out += (char)tolower((unsigned char)t[i]);
        }
    }
    return out;
}

static string q_text(const char* src, int size) {
    string v;
    for (int i = 0; i < size && src[i] != '\0'; i++) v += src[i];
    return q_trim(v);
}

// splits on the keyword "and" (only outside quotes)
static vector<string> split_and(const string& s) {
    vector<string> parts;
    string cur;
    bool inQuote = false;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] == '\'') { inQuote = !inQuote; cur += s[i]; i++; continue; }
        if (!inQuote && i + 4 < s.size() && isspace((unsigned char)s[i]) &&
            q_lower(s.substr(i + 1, 3)) == "and" && isspace((unsigned char)s[i + 4])) {
            parts.push_back(cur);
            cur = "";
            i += 5;
            continue;
        }
        cur += s[i];
        i++;
    }
    parts.push_back(cur);
    return parts;
}

static bool parse_condition(const string& part, Condition& c) {
    size_t eq = part.find('=');
    if (eq == string::npos) return false;
    c.field = q_norm(part.substr(0, eq));
    string right = q_trim(part.substr(eq + 1));
    if (right.size() < 2) return false;
    char q = right[0];
    if ((q != '\'' && q != '"') || right[right.size() - 1] != q) return false;
    c.value = q_trim(right.substr(1, right.size() - 2));
    return !c.field.empty();
}

// ---- field name mapping -------------------------------------------------
static string doctor_field(const string& f) {
    if (f == "doctor id" || f == "id")               return "id";
    if (f == "doctor name" || f == "name")           return "name";
    if (f == "doctor specialty" || f == "specialty") return "specialty";
    if (f == "address")                              return "address";
    return "";
}

static string appt_field(const string& f) {
    if (f == "appointment id" || f == "id")              return "id";
    if (f == "appointment date" || f == "date")          return "date";
    if (f == "doctor id")                                return "doctor";
    return "";
}

static bool read_conditions(const string& wherePart, const string& table,
                            map<string, string>& out, bool& conflict) {
    vector<string> parts = split_and(wherePart);
    conflict = false;
    for (size_t i = 0; i < parts.size(); i++) {
        Condition c;
        if (!parse_condition(parts[i], c)) {
            cout << "Syntax error in condition: \"" << q_trim(parts[i])
                 << "\" (expected Field='value')." << endl;
            return false;
        }
        string key = (table == "doctors") ? doctor_field(c.field) : appt_field(c.field);
        if (key.empty()) {
            cout << "Unknown field in WHERE: \"" << c.field << "\"." << endl;
            return false;
        }
        if (out.count(key) && out[key] != c.value) conflict = true;
        out[key] = c.value;
    }
    return true;
}

static bool read_columns(const string& selPart, const string& table, vector<string>& cols) {
    string s = q_norm(selPart);
    cols.clear();
    if (s == "all" || s == "*") return true;   // empty = all columns

    size_t start = 0;
    while (start <= selPart.size()) {
        size_t comma = selPart.find(',', start);
        string item = (comma == string::npos) ? selPart.substr(start) : selPart.substr(start, comma - start);
        string key = (table == "doctors") ? doctor_field(q_norm(item)) : appt_field(q_norm(item));
        if (key.empty()) {
            cout << "Unknown field in SELECT: \"" << q_norm(item) << "\"." << endl;
            return false;
        }
        cols.push_back(key);
        if (comma == string::npos) break;
        start = comma + 1;
    }
    return true;
}

static bool contains(const vector<string>& v, const string& x) {
    for (size_t i = 0; i < v.size(); i++) if (v[i] == x) return true;
    return false;
}

static bool matches(const map<string, string>& conds, const map<string, string>& rec) {
    for (map<string, string>::const_iterator it = conds.begin(); it != conds.end(); ++it) {
        map<string, string>::const_iterator r = rec.find(it->first);
        if (r == rec.end() || r->second != it->second) return false;
    }
    return true;
}

static void print_row(const vector<string>& cols, const vector<string>& allKeys,
                      const vector<string>& allLabels, const map<string, string>& rec) {
    vector<string> keys = cols.empty() ? allKeys : cols;
    for (size_t i = 0; i < keys.size(); i++) {
        string label = keys[i];
        for (size_t j = 0; j < allKeys.size(); j++) if (allKeys[j] == keys[i]) label = allLabels[j];
        if (i) cout << " | ";
        cout << label << ": " << rec.find(keys[i])->second;
    }
    cout << endl;
}

// ---- Doctors ------------------------------------------------------------
static void query_doctors(fstream& df, const vector<string>& cols, const map<string, string>& conds) {
    vector<string> cand;

    if (conds.count("id")) {                                    // primary index
        string id = conds.find("id")->second;
        if (find_doctor_offset(id) != -1) cand.push_back(id);
    }
    else if (conds.count("name") && conds.count("specialty")) { // two secondary indexes
        vector<string> byName = find_doctors_by_name(conds.find("name")->second);
        vector<string> bySpec = find_doctors_by_specialty(conds.find("specialty")->second);
        for (size_t i = 0; i < byName.size(); i++)
            if (contains(bySpec, byName[i])) cand.push_back(byName[i]);
    }
    else if (conds.count("name"))      cand = find_doctors_by_name(conds.find("name")->second);
    else if (conds.count("specialty")) cand = find_doctors_by_specialty(conds.find("specialty")->second);
    else {
        cout << "Query needs at least one indexed field (Doctor ID, Doctor Name or Doctor Specialty)." << endl;
        return;
    }

    const char* k[] = {"id", "name", "specialty", "address"};
    const char* l[] = {"Doctor ID", "Doctor Name", "Specialty", "Address"};
    vector<string> allKeys(k, k + 4), allLabels(l, l + 4);

    int found = 0;
    for (size_t i = 0; i < cand.size(); i++) {
        int off = find_doctor_offset(cand[i]);
        Doctor d;
        if (off == -1 || !readDoctorAt(df, off, d)) continue;

        map<string, string> rec;
        rec["id"] = q_text(d.id, 15);
        rec["name"] = q_text(d.name, 30);
        rec["specialty"] = q_text(d.specialty, 20);
        rec["address"] = q_text(d.address, 30);

        if (!matches(conds, rec)) continue;
        print_row(cols, allKeys, allLabels, rec);
        found++;
    }
    cout << found << " record(s) found." << endl;
}

// ---- Appointments -------------------------------------------------------
static void query_appointments(fstream& af, const vector<string>& cols, const map<string, string>& conds) {
    vector<string> cand;

    if (conds.count("id")) {                                    // primary index
        string id = conds.find("id")->second;
        if (find_appt_offset(id) != -1) cand.push_back(id);
    }
    else if (conds.count("doctor"))                             // secondary index (linked list file)
        cand = get_appointment_ids_by_doctor(conds.find("doctor")->second);
    else {
        cout << "Query needs at least one indexed field (Appointment ID or Doctor ID)." << endl;
        return;
    }

    const char* k[] = {"id", "date", "doctor"};
    const char* l[] = {"Appointment ID", "Date", "Doctor ID"};
    vector<string> allKeys(k, k + 3), allLabels(l, l + 3);

    int found = 0;
    for (size_t i = 0; i < cand.size(); i++) {
        int off = find_appt_offset(cand[i]);
        Appointment a;
        if (off == -1 || !readAppointmentAt(af, off, a)) continue;

        map<string, string> rec;
        rec["id"] = q_text(a.id, 15);
        rec["date"] = q_text(a.date, 30);
        rec["doctor"] = q_text(a.doctorId, 15);

        if (!matches(conds, rec)) continue;
        print_row(cols, allKeys, allLabels, rec);
        found++;
    }
    cout << found << " record(s) found." << endl;
}

// ---- entry point --------------------------------------------------------
void run_query(fstream& doctorFile, fstream& apptFile, const string& query) {
    string q = q_trim(query);
    while (!q.empty() && q[q.size() - 1] == ';') q = q_trim(q.substr(0, q.size() - 1));

    string lq = q_lower(q);
    if (lq.compare(0, 6, "select") != 0) {
        cout << "Query must start with SELECT." << endl;
        return;
    }

    size_t from = lq.find(" from ");
    if (from == string::npos) { cout << "Missing FROM keyword." << endl; return; }
    size_t where = lq.find(" where ", from);
    if (where == string::npos) { cout << "Missing WHERE keyword." << endl; return; }

    string selPart   = q.substr(6, from - 6);
    string table     = q_norm(q.substr(from + 6, where - (from + 6)));
    string wherePart = q.substr(where + 7);

    if (table != "doctors" && table != "appointments") {
        cout << "Unknown table \"" << table << "\" (use Doctors or Appointments)." << endl;
        return;
    }

    vector<string> cols;
    if (!read_columns(selPart, table, cols)) return;

    map<string, string> conds;
    bool conflict = false;
    if (!read_conditions(wherePart, table, conds, conflict)) return;
    if (conflict) { cout << "0 record(s) found." << endl; return; }

    if (table == "doctors") query_doctors(doctorFile, cols, conds);
    else                    query_appointments(apptFile, cols, conds);
}
