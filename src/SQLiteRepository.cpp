#include "payroll/Repository.hpp"

#include <sqlite3.h>

#include <stdexcept>
#include <string>
#include <utility>

namespace payroll {

namespace {
// Схема розташована в одному місці; знімки в payroll_lines не змінюються
// після редагування тарифу, табеля, ставок податків або архівування працівника.
constexpr const char* kSchema = R"SQL(
PRAGMA foreign_keys = ON;
CREATE TABLE IF NOT EXISTS employees (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    position TEXT NOT NULL,
    pay_kind TEXT NOT NULL CHECK(pay_kind IN ('HOURLY','SALARIED')),
    rate_kopecks INTEGER NOT NULL CHECK(rate_kopecks > 0),
    active INTEGER NOT NULL DEFAULT 1 CHECK(active IN (0,1))
);
CREATE TABLE IF NOT EXISTS work_records (
    employee_id INTEGER NOT NULL REFERENCES employees(id),
    period TEXT NOT NULL,
    regular_minutes INTEGER NOT NULL CHECK(regular_minutes >= 0),
    overtime_minutes INTEGER NOT NULL CHECK(overtime_minutes >= 0),
    norm_minutes INTEGER NOT NULL CHECK(norm_minutes > 0),
    PRIMARY KEY(employee_id, period)
);
CREATE TABLE IF NOT EXISTS adjustments (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    employee_id INTEGER NOT NULL REFERENCES employees(id),
    period TEXT NOT NULL,
    kind TEXT NOT NULL CHECK(kind IN ('BONUS','DEDUCTION')),
    amount_kopecks INTEGER NOT NULL CHECK(amount_kopecks > 0),
    note TEXT NOT NULL DEFAULT ''
);
CREATE INDEX IF NOT EXISTS idx_adjustments_period ON adjustments(employee_id, period);
CREATE TABLE IF NOT EXISTS tax_settings (
    id INTEGER PRIMARY KEY CHECK(id = 1),
    income_tax_bp INTEGER NOT NULL,
    military_tax_bp INTEGER NOT NULL
);
INSERT OR IGNORE INTO tax_settings(id, income_tax_bp, military_tax_bp) VALUES(1, 1800, 500);
CREATE TABLE IF NOT EXISTS payroll_runs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    period TEXT NOT NULL,
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%d %H:%M:%f', 'now')),
    income_tax_bp INTEGER NOT NULL,
    military_tax_bp INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_runs_period ON payroll_runs(period,id);
CREATE TABLE IF NOT EXISTS payroll_lines (
    run_id INTEGER NOT NULL REFERENCES payroll_runs(id),
    employee_id INTEGER NOT NULL REFERENCES employees(id),
    employee_name TEXT NOT NULL,
    position TEXT NOT NULL,
    pay_kind TEXT NOT NULL,
    rate_kopecks INTEGER NOT NULL,
    regular_minutes INTEGER NOT NULL,
    overtime_minutes INTEGER NOT NULL,
    norm_minutes INTEGER NOT NULL,
    regular_kopecks INTEGER NOT NULL,
    overtime_kopecks INTEGER NOT NULL,
    bonus_kopecks INTEGER NOT NULL,
    gross_kopecks INTEGER NOT NULL,
    income_tax_kopecks INTEGER NOT NULL,
    military_tax_kopecks INTEGER NOT NULL,
    extra_deduction_kopecks INTEGER NOT NULL,
    net_kopecks INTEGER NOT NULL,
    PRIMARY KEY(run_id, employee_id)
);
)SQL";

class Statement {
public:
    Statement(sqlite3* db, const char* sql) : db_(db) {
        if (sqlite3_prepare_v2(db, sql, -1, &stmt_, nullptr) != SQLITE_OK)
            throw std::runtime_error(std::string("Помилка SQL prepare: ") + sqlite3_errmsg(db));
    }
    ~Statement() {
        sqlite3_finalize(stmt_);
    }
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    void set(int index, int val) {
        check(sqlite3_bind_int(stmt_, index, val));
    }
    void set(int index, std::int64_t val) {
        check(sqlite3_bind_int64(stmt_, index, val));
    }
    void set(int index, const std::string& val) {
        check(sqlite3_bind_text(stmt_, index, val.c_str(), -1, SQLITE_TRANSIENT));
    }
    int step() {
        const int result = sqlite3_step(stmt_);
        if (result != SQLITE_ROW && result != SQLITE_DONE)
            throw std::runtime_error(std::string("Помилка SQL execute: ") + sqlite3_errmsg(db_));
        return result;
    }
    void done() {
        if (step() != SQLITE_DONE) {
            throw std::runtime_error("Очікувалося завершення SQL запиту");
        }
    }
    int getInt(int col) const {
        return sqlite3_column_int(stmt_, col);
    }
    std::int64_t getLong(int col) const {
        return sqlite3_column_int64(stmt_, col);
    }
    std::string getString(int col) const {
        const auto* s = sqlite3_column_text(stmt_, col);
        return s ? reinterpret_cast<const char*>(s) : "";
    }
private:
    void check(int code) {
        if (code != SQLITE_OK)
            throw std::runtime_error(std::string("Помилка SQL bind: ") + sqlite3_errmsg(db_));
    }
    sqlite3* db_;
    sqlite3_stmt* stmt_ = nullptr;
};

Employee readEmployee(Statement& s) {
    Employee e;
    e.id = s.getInt(0);
    e.name = s.getString(1);
    e.position = s.getString(2);
    e.kind = payKindFromString(s.getString(3));
    e.rate = Money(s.getLong(4));
    e.active = s.getInt(5) != 0;
    return e;
}

class Transaction {
public:
    explicit Transaction(sqlite3* db) : db_(db) {
        execute("BEGIN IMMEDIATE TRANSACTION;");
    }
    ~Transaction() {
        if (!committed_) sqlite3_exec(db_, "ROLLBACK;", nullptr, nullptr, nullptr);
    }
    void commit() {
        execute("COMMIT;");
        committed_ = true;
    }
private:
    void execute(const char* sql) {
        char* err = nullptr;
        if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
            std::string msg = err ? err : "Транзакція не виконана";
            sqlite3_free(err);
            throw std::runtime_error(msg);
        }
    }
    sqlite3* db_;
    bool committed_ = false;
};
} // namespace

SQLiteRepository::SQLiteRepository(const std::string& filename) {
    if (sqlite3_open_v2(filename.c_str(), &db_,
                        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
                        nullptr) != SQLITE_OK) {
        const std::string message = db_ ? sqlite3_errmsg(db_) : "Не вдалося відкрити БД";
        if (db_) sqlite3_close(db_);
        db_ = nullptr;
        throw std::runtime_error("SQLite: " + message);
    }
    try {
        sqlite3_busy_timeout(db_, 3000);
        execute(kSchema);
    } catch (...) {
        sqlite3_close(db_);
        db_ = nullptr;
        throw;
    }
}

SQLiteRepository::~SQLiteRepository() {
    if (db_) sqlite3_close(db_);
}

void SQLiteRepository::execute(const char* sql) const {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "Помилка SQLite";
        sqlite3_free(err);
        throw std::runtime_error(msg);
    }
}

int SQLiteRepository::addEmployee(const Employee& e) {
    Statement s(db_, "INSERT INTO employees(name, position, pay_kind, rate_kopecks, active) VALUES(?,?,?,?,1)");
    s.set(1, e.name);
    s.set(2, e.position);
    s.set(3, toString(e.kind));
    s.set(4, e.rate.kopecks());
    s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void SQLiteRepository::updateEmployee(const Employee& e) {
    Statement s(db_, "UPDATE employees SET name=?,position=?,pay_kind=?,rate_kopecks=? WHERE id=? AND active=1");
    s.set(1, e.name);
    s.set(2, e.position);
    s.set(3, toString(e.kind));
    s.set(4, e.rate.kopecks());
    s.set(5, e.id);
    s.done();
    if (!sqlite3_changes(db_)) {
        throw std::invalid_argument("Активного працівника не знайдено");
    }
}

void SQLiteRepository::archiveEmployee(int id) {
    Statement s(db_, "UPDATE employees SET active=0 WHERE id=? AND active=1");
    s.set(1, id);
    s.done();
    if (!sqlite3_changes(db_)) {
        throw std::invalid_argument("Активного працівника не знайдено");
    }
}

std::optional<Employee> SQLiteRepository::employee(int id) const {
    Statement s(db_, "SELECT id,name,position,pay_kind,rate_kopecks,active FROM employees WHERE id=?");
    s.set(1, id);
    if (s.step() == SQLITE_DONE) {
        return std::nullopt;
    }
    return readEmployee(s);
}

std::vector<Employee> SQLiteRepository::employees(bool includeArchived) const {
    Statement s(db_, "SELECT id,name,position,pay_kind,rate_kopecks,active FROM employees "
                     "WHERE (?=1 OR active=1) ORDER BY id");
    s.set(1, includeArchived ? 1 : 0);
    std::vector<Employee> result;
    while (s.step() == SQLITE_ROW) {
        result.push_back(readEmployee(s));
    }
    return result;
}

std::vector<Employee> SQLiteRepository::searchEmployees(const std::string& keyword) const {
    Statement s(db_, "SELECT id,name,position,pay_kind,rate_kopecks,active FROM employees "
                     "WHERE instr(name,?)>0 OR instr(position,?)>0 ORDER BY id");
    s.set(1, keyword);
    s.set(2, keyword);
    std::vector<Employee> result;
    while (s.step() == SQLITE_ROW) {
        result.push_back(readEmployee(s));
    }
    return result;
}

void SQLiteRepository::saveWork(const WorkRecord& w) {
    Statement s(db_, "INSERT INTO work_records(employee_id,period,regular_minutes,overtime_minutes,norm_minutes) "
                     "VALUES(?,?,?,?,?) ON CONFLICT(employee_id,period) DO UPDATE SET "
                     "regular_minutes=excluded.regular_minutes,overtime_minutes=excluded.overtime_minutes,"
                     "norm_minutes=excluded.norm_minutes");
    s.set(1, w.employeeId);
    s.set(2, w.period);
    s.set(3, w.regularMinutes);
    s.set(4, w.overtimeMinutes);
    s.set(5, w.normMinutes);
    s.done();
}

std::optional<WorkRecord> SQLiteRepository::work(int employeeId, const std::string& period) const {
    Statement s(db_, "SELECT regular_minutes,overtime_minutes,norm_minutes FROM work_records WHERE employee_id=? AND period=?");
    s.set(1, employeeId);
    s.set(2, period);
    if (s.step() == SQLITE_DONE) {
        return std::nullopt;
    }
    return WorkRecord{employeeId, period, s.getInt(0), s.getInt(1), s.getInt(2)};
}

int SQLiteRepository::addAdjustment(const Adjustment& a) {
    Statement s(db_, "INSERT INTO adjustments(employee_id,period,kind,amount_kopecks,note) VALUES(?,?,?,?,?)");
    s.set(1, a.employeeId);
    s.set(2, a.period);
    s.set(3, toString(a.kind));
    s.set(4, a.amount.kopecks());
    s.set(5, a.note);
    s.done();
    return static_cast<int>(sqlite3_last_insert_rowid(db_));
}

void SQLiteRepository::deleteAdjustment(int id) {
    Statement s(db_, "DELETE FROM adjustments WHERE id=?");
    s.set(1, id);
    s.done();
    if (!sqlite3_changes(db_)) {
        throw std::invalid_argument("Коригування не знайдено");
    }
}

std::vector<Adjustment> SQLiteRepository::adjustments(int employeeId, const std::string& period) const {
    Statement s(db_, "SELECT id,kind,amount_kopecks,note FROM adjustments WHERE employee_id=? AND period=? ORDER BY id");
    s.set(1, employeeId);
    s.set(2, period);
    std::vector<Adjustment> result;
    while (s.step() == SQLITE_ROW) {
        Adjustment a;
        a.id = s.getInt(0);
        a.employeeId = employeeId;
        a.period = period;
        a.kind = adjustmentKindFromString(s.getString(1));
        a.amount = Money(s.getLong(2));
        a.note = s.getString(3);
        result.push_back(a);
    }
    return result;
}

TaxSettings SQLiteRepository::taxSettings() const {
    Statement s(db_, "SELECT income_tax_bp,military_tax_bp FROM tax_settings WHERE id=1");
    if (s.step() != SQLITE_ROW) {
        throw std::runtime_error("Не задані ставки утримань");
    }
    return {s.getInt(0), s.getInt(1)};
}

void SQLiteRepository::setTaxSettings(const TaxSettings& cfg) {
    Statement s(db_, "UPDATE tax_settings SET income_tax_bp=?, military_tax_bp=? WHERE id=1");
    s.set(1, cfg.incomeTaxBp);
    s.set(2, cfg.militaryTaxBp);
    s.done();
}

int SQLiteRepository::saveRun(const PayrollRun& r) {
    if (r.slips.empty()) {
        throw std::invalid_argument("Порожню відомість не можна зберегти");
    }
    Transaction transaction(db_); // Атомарність: заголовок і всі рядки зберігаються разом.
    Statement header(db_, "INSERT INTO payroll_runs(period,income_tax_bp,military_tax_bp) VALUES(?,?,?)");
    header.set(1, r.period);
    header.set(2, r.settings.incomeTaxBp);
    header.set(3, r.settings.militaryTaxBp);
    header.done();
    auto id = static_cast<int>(sqlite3_last_insert_rowid(db_));
    for (const auto& p : r.slips) {
        Statement s(db_, "INSERT INTO payroll_lines(run_id,employee_id,employee_name,position,pay_kind,rate_kopecks,"
                         "regular_minutes,overtime_minutes,norm_minutes,regular_kopecks,overtime_kopecks,"
                         "bonus_kopecks,gross_kopecks,income_tax_kopecks,military_tax_kopecks,"
                         "extra_deduction_kopecks,net_kopecks) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)");
        s.set(1, id);
        s.set(2, p.employeeId);
        s.set(3, p.employeeName);
        s.set(4, p.position);
        s.set(5, toString(p.kind));
        s.set(6, p.rate.kopecks());
        s.set(7, p.work.regularMinutes);
        s.set(8, p.work.overtimeMinutes);
        s.set(9, p.work.normMinutes);
        s.set(10, p.regular.kopecks());
        s.set(11, p.overtime.kopecks());
        s.set(12, p.bonus.kopecks());
        s.set(13, p.gross.kopecks());
        s.set(14, p.incomeTax.kopecks());
        s.set(15, p.militaryTax.kopecks());
        s.set(16, p.extraDeduction.kopecks());
        s.set(17, p.net.kopecks());
        s.done();
    }
    transaction.commit();
    return id;
}

std::optional<PayrollRun> SQLiteRepository::run(int id) const {
    Statement head(db_, "SELECT period,created_at,income_tax_bp,military_tax_bp FROM payroll_runs WHERE id=?");
    head.set(1, id);
    if (head.step() == SQLITE_DONE) {
        return std::nullopt;
    }
    PayrollRun r;
    r.id = id;
    r.period = head.getString(0);
    r.createdAt = head.getString(1);
    r.settings = {head.getInt(2), head.getInt(3)};
    Statement s(db_, "SELECT employee_id,employee_name,position,pay_kind,rate_kopecks,regular_minutes,"
                     "overtime_minutes,norm_minutes,regular_kopecks,overtime_kopecks,bonus_kopecks,gross_kopecks,"
                     "income_tax_kopecks,military_tax_kopecks,extra_deduction_kopecks,net_kopecks "
                     "FROM payroll_lines WHERE run_id=? ORDER BY employee_id");
    s.set(1, id);
    while (s.step() == SQLITE_ROW) {
        Payslip p;
        p.employeeId = s.getInt(0);
        p.employeeName = s.getString(1);
        p.position = s.getString(2);
        p.kind = payKindFromString(s.getString(3));
        p.rate = Money(s.getLong(4));
        p.work = {p.employeeId, r.period, s.getInt(5), s.getInt(6), s.getInt(7)};
        p.regular = Money(s.getLong(8));
        p.overtime = Money(s.getLong(9));
        p.bonus = Money(s.getLong(10));
        p.gross = Money(s.getLong(11));
        p.incomeTax = Money(s.getLong(12));
        p.militaryTax = Money(s.getLong(13));
        p.extraDeduction = Money(s.getLong(14));
        p.net = Money(s.getLong(15));
        r.slips.push_back(p);
    }
    return r;
}

std::optional<PayrollRun> SQLiteRepository::latestRun(const std::string& period) const {
    Statement s(db_, "SELECT id FROM payroll_runs WHERE period=? ORDER BY id DESC LIMIT 1");
    s.set(1, period);
    if (s.step() == SQLITE_DONE) {
        return std::nullopt;
    }
    return run(s.getInt(0));
}

std::vector<RunSummary> SQLiteRepository::history() const {
    Statement s(db_, "SELECT r.id,r.period,r.created_at,COUNT(l.employee_id) FROM payroll_runs r "
                     "LEFT JOIN payroll_lines l ON l.run_id=r.id "
                     "GROUP BY r.id ORDER BY r.id DESC LIMIT 100");
    std::vector<RunSummary> result;
    while (s.step() == SQLITE_ROW)
        result.push_back({s.getInt(0), s.getString(1), s.getString(2), s.getInt(3)});
    return result;
}
} // namespace payroll
