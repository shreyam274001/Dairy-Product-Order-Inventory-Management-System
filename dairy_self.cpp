// requires nlohmann/json single header
// Download from: https://github.com/nlohmann/json/releases

#include <cstddef>
#include <cstdio>
#include <iostream>
#include <unordered_map>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <memory>

#include "json.hpp"
using json = nlohmann::json;
using namespace std;

/*-----------------Utilities-----------------*/

static string trim(const string &str) {
    size_t a = str.find_first_not_of(" \t\r\n");
    if (a == string::npos) return "";
    size_t b = str.find_last_not_of(" \t\r\n");
    return str.substr(a, b - a + 1);
}

static vector<string> split(const string &s, char delim) {
    vector<string> out;
    string cur;
    stringstream ss(s);
    while (getline(ss, cur, delim)) {
        if (!cur.empty()) out.push_back(cur);
    }
    return out;
}

static string join(const vector<string>& parts, char delim) {
    stringstream ss;
    for (size_t i = 0; i < parts.size(); i++) {
        if (i) ss << delim;
        ss << parts[i];
    }
    return ss.str();
}

static string getCurrentDate() {
    time_t t = time(nullptr);
    tm lt;
    localtime_r(&t, &lt);
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", 
             lt.tm_year + 1900, lt.tm_mon + 1, lt.tm_mday);
    return string(buf);
}

/*----------------Models----------------*/

struct Product {
    int id;
    string name;
    string unit;
    double price;
    int stock;
    int minStock;

    Product() : id(0), price(0.0), stock(0), minStock(0) {}

    string toCSV() const {
        stringstream ss;
        ss << id << "," << name << "," << unit << "," 
           << fixed << setprecision(2) << price << "," 
           << stock << "," << minStock;
        return ss.str();
    }

    static Product fromCSV(const string &line) {
        auto f = split(line, ',');
        Product p;
        if (f.size() >= 6) {
            p.id = stoi(f[0]);
            p.name = f[1];
            p.unit = f[2];
            p.price = stod(f[3]);
            p.stock = stoi(f[4]);
            p.minStock = stoi(f[5]);
        }
        return p;
    }
};

struct Customer {
    int id;
    string name;
    string type;
    string contact;
    string address;
    double pending;

    Customer() : id(0), pending(0.0) {}

    string toCSV() const {
        stringstream ss;
        ss << id << "," << name << "," << type << "," 
           << contact << "," << address << "," 
           << fixed << setprecision(2) << pending;
        return ss.str();
    }

    static Customer fromCSV(const string &line) {
        auto f = split(line, ',');
        Customer c;
        if (f.size() >= 6) {
            c.id = stoi(f[0]);
            c.name = f[1];
            c.type = f[2];
            c.contact = f[3];
            c.address = f[4];
            c.pending = stod(f[5]);
        }
        return c;
    }
};

struct Order {
    int orderId;
    int customerId;
    vector<pair<int, int>> items;
    string orderDate;
    string deliveryDate;
    string status;

    Order() : orderId(0), customerId(0), status("Pending") {}

    string itemsToString() const {
        vector<string> parts;
        for (auto &item : items) {
            parts.push_back(to_string(item.first) + ":" + to_string(item.second));
        }
        return join(parts, ';');
    }

    static vector<pair<int, int>> parseItems(const string &s) {
        vector<pair<int, int>> res;
        if (s.empty()) return res;
        auto parts = split(s, ';');
        for (auto &p : parts) {
            if (p.empty()) continue;
            auto kv = split(p, ':');
            if (kv.size() == 2) {
                res.emplace_back(stoi(kv[0]), stoi(kv[1]));
            }
        }
        return res;
    }

    string toCSV() const {
        stringstream ss;
        ss << orderId << "," << customerId << "," 
           << itemsToString() << "," << orderDate << "," 
           << deliveryDate << "," << status;
        return ss.str();
    }

    static Order fromCSV(const string &line) {
        auto f = split(line, ',');
        Order o;
        if (f.size() >= 6) {
            o.orderId = stoi(f[0]);
            o.customerId = stoi(f[1]);
            o.items = parseItems(f[2]);
            o.orderDate = f[3];
            o.deliveryDate = f[4];
            o.status = f[5];
        }
        return o;
    }

    double calculateTotal(const unordered_map<int, Product>& productMap) const {
        double total = 0.0;
        for (auto &item : items) {
            auto it = productMap.find(item.first);
            if (it != productMap.end()) {
                total += it->second.price * item.second;
            }
        }
        return total;
    }
};

struct Payment {
    int paymentId;
    int orderId;
    double amount;
    string date;
    string mode;

    Payment() : paymentId(0), orderId(0), amount(0.0) {}

    string toCSV() const {
        stringstream ss;
        ss << paymentId << "," << orderId << "," 
           << fixed << setprecision(2) << amount << "," 
           << date << "," << mode;
        return ss.str();
    }

    static Payment fromCSV(const string &line) {
        auto f = split(line, ',');
        Payment p;
        if (f.size() >= 5) {
            p.paymentId = stoi(f[0]);
            p.orderId = stoi(f[1]);
            p.amount = stod(f[2]);
            p.date = f[3];
            p.mode = f[4];
        }
        return p;
    }
};

/*-------------------Managers-------------------*/

class InventoryManager {
private:
    unordered_map<int, Product> products;
    int nextProductId = 1;
    const string filename = "products.csv";

    void ensureFileExists() {
        ifstream fin(filename);
        if (!fin.is_open()) {
            ofstream fout(filename);
            fout << "id,name,unit,price,stock,minStock\n";
            fout.close();
        }
        fin.close();
    }

public:
    InventoryManager() {
        load();
    }

    void load() {
        products.clear();
        ensureFileExists();
        
        ifstream fin(filename);
        string line;
        getline(fin, line); // Skip header
        
        while (getline(fin, line)) {
            line = trim(line);
            if (line.empty()) continue;
            Product p = Product::fromCSV(line);
            products[p.id] = p;
            nextProductId = max(nextProductId, p.id + 1);
        }
        fin.close();
    }

    void save() {
        ofstream fout(filename);
        fout << "id,name,unit,price,stock,minStock\n";
        for (auto &kv : products) {
            fout << kv.second.toCSV() << "\n";
        }
        fout.close();
    }

    int addProduct(const string &name, const string &unit, double price, int stock, int minStock) {
        Product p;
        p.id = nextProductId++;
        p.name = name;
        p.unit = unit;
        p.price = price;
        p.stock = stock;
        p.minStock = minStock;
        products[p.id] = p;
        save();
        return p.id;
    }

    bool updatePrice(int productId, double newPrice) {
        auto it = products.find(productId);
        if (it == products.end()) return false;
        it->second.price = newPrice;
        save();
        return true;
    }

    bool adjustStock(int productId, int delta) {
        auto it = products.find(productId);
        if (it == products.end()) return false;
        it->second.stock += delta;
        if (it->second.stock < 0) it->second.stock = 0;
        save();
        return true;
    }

    vector<Product> getLowStockItems() const {
        vector<Product> out;
        for (auto &kv : products) {
            if (kv.second.stock < kv.second.minStock) {
                out.push_back(kv.second);
            }
        }
        return out;
    }

    void displayAll() const {
        cout << "ID\tName\tUnit\tPrice\tStock\tMin\n";
        cout << "--------------------------------------------------------\n";
        for (auto &kv : products) {
            auto &p = kv.second;
            cout << p.id << "\t" << p.name << "\t" << p.unit << "\t" 
                 << fixed << setprecision(2) << p.price << "\t" 
                 << p.stock << "\t" << p.minStock << "\n";
        }
    }

    const unordered_map<int, Product>& getProducts() const { return products; }
    bool productExists(int productId) const { return products.find(productId) != products.end(); }
    Product* getProduct(int productId) {
        auto it = products.find(productId);
        return (it != products.end()) ? &it->second : nullptr;
    }
};

class CustomerManager {
private:
    unordered_map<int, Customer> customers;
    int nextCustomerId = 1;
    const string filename = "customers.csv";

    void ensureFileExists() {
        ifstream fin(filename);
        if (!fin.is_open()) {
            ofstream fout(filename);
            fout << "id,name,type,contact,address,pending\n";
            fout.close();
        }
        fin.close();
    }

public:
    CustomerManager() {
        load();
    }

    void load() {
        customers.clear();
        ensureFileExists();
        
        ifstream fin(filename);
        string line;
        getline(fin, line); // Skip header
        
        while (getline(fin, line)) {
            line = trim(line);
            if (line.empty()) continue;
            Customer c = Customer::fromCSV(line);
            customers[c.id] = c;
            nextCustomerId = max(nextCustomerId, c.id + 1);
        }
        fin.close();
    }

    void save() {
        ofstream fout(filename);
        fout << "id,name,type,contact,address,pending\n";
        for (auto &kv : customers) {
            fout << kv.second.toCSV() << "\n";
        }
        fout.close();
    }

    int addCustomer(const string &name, const string &type, const string &contact, const string &address) {
        Customer c;
        c.id = nextCustomerId++;
        c.name = name;
        c.type = type;
        c.contact = contact;
        c.address = address;
        c.pending = 0.0;
        customers[c.id] = c;
        save();
        return c.id;
    }

    bool addPending(int customerId, double amount) {
        auto it = customers.find(customerId);
        if (it == customers.end()) return false;
        it->second.pending += amount;
        save();
        return true;
    }

    bool payAmount(int customerId, double amount) {
        auto it = customers.find(customerId);
        if (it == customers.end()) return false;
        it->second.pending -= amount;
        if (it->second.pending < 0) it->second.pending = 0;
        save();
        return true;
    }

    void displayAll() const {
        cout << "ID\tName\tType\tContact\tPending\n";
        cout << "--------------------------------------------------------\n";
        for (auto &kv : customers) {
            auto &c = kv.second;
            cout << c.id << "\t" << c.name << "\t" << c.type << "\t" 
                 << c.contact << "\t" << fixed << setprecision(2) << c.pending << "\n";
        }
    }

    const unordered_map<int, Customer>& getCustomers() const { return customers; }
    bool customerExists(int customerId) const { return customers.find(customerId) != customers.end(); }
};

class OrderManager {
private:
    unordered_map<int, Order> orders;
    int nextOrderId = 1;
    const string filename = "orders.csv";

    void ensureFileExists() {
        ifstream fin(filename);
        if (!fin.is_open()) {
            ofstream fout(filename);
            fout << "orderId,customerId,items,orderDate,deliveryDate,status\n";
            fout.close();
        }
        fin.close();
    }

public:
    OrderManager() {
        load();
    }

    void load() {
        orders.clear();
        ensureFileExists();
        
        ifstream fin(filename);
        string line;
        getline(fin, line); // Skip header
        
        while (getline(fin, line)) {
            line = trim(line);
            if (line.empty()) continue;
            Order o = Order::fromCSV(line);
            orders[o.orderId] = o;
            nextOrderId = max(nextOrderId, o.orderId + 1);
        }
        fin.close();
    }

    void save() {
        ofstream fout(filename);
        fout << "orderId,customerId,items,orderDate,deliveryDate,status\n";
        for (auto &kv : orders) {
            fout << kv.second.toCSV() << "\n";
        }
        fout.close();
    }

    int createOrder(int customerId, const vector<pair<int, int>>& items, const string &orderDate) {
        Order o;
        o.orderId = nextOrderId++;
        o.customerId = customerId;
        o.items = items;
        o.orderDate = orderDate;
        o.deliveryDate = "";
        o.status = "Pending";
        orders[o.orderId] = o;
        save();
        return o.orderId;
    }

    bool setDelivered(int orderId, const string &deliveryDate) {
        auto it = orders.find(orderId);
        if (it == orders.end()) return false;
        it->second.status = "Delivered";
        it->second.deliveryDate = deliveryDate;
        save();
        return true;
    }

    bool cancelOrder(int orderId) {
        auto it = orders.find(orderId);
        if (it == orders.end()) return false;
        it->second.status = "Cancelled";
        save();
        return true;
    }

    void displayAll() const {
        cout << "OrderID\tCustID\tOrder Date\tDelivery\tStatus\tItems\n";
        cout << "--------------------------------------------------------\n";
        for (auto &kv : orders) {
            auto &o = kv.second;
            cout << o.orderId << "\t" << o.customerId << "\t" 
                 << o.orderDate << "\t" << o.deliveryDate << "\t" 
                 << o.status << "\t" << o.itemsToString() << "\n";
        }
    }

    vector<Order> getPendingOrders() const {
        vector<Order> out;
        for (auto &kv : orders) {
            if (kv.second.status == "Pending") {
                out.push_back(kv.second);
            }
        }
        return out;
    }

    Order* getOrder(int orderId) {
        auto it = orders.find(orderId);
        return (it != orders.end()) ? &it->second : nullptr;
    }

    const unordered_map<int, Order>& getOrders() const { return orders; }
};

class PaymentManager {
private:
    unordered_map<int, Payment> payments;
    int nextPaymentId = 1;
    const string filename = "payments.csv";

    void ensureFileExists() {
        ifstream fin(filename);
        if (!fin.is_open()) {
            ofstream fout(filename);
            fout << "paymentId,orderId,amount,date,mode\n";
            fout.close();
        }
        fin.close();
    }

public:
    PaymentManager() {
        load();
    }

    void load() {
        payments.clear();
        ensureFileExists();
        
        ifstream fin(filename);
        string line;
        getline(fin, line); // Skip header
        
        while (getline(fin, line)) {
            line = trim(line);
            if (line.empty()) continue;
            Payment p = Payment::fromCSV(line);
            payments[p.paymentId] = p;
            nextPaymentId = max(nextPaymentId, p.paymentId + 1);
        }
        fin.close();
    }

    void save() {
        ofstream fout(filename);
        fout << "paymentId,orderId,amount,date,mode\n";
        for (auto &kv : payments) {
            fout << kv.second.toCSV() << "\n";
        }
        fout.close();
    }

    int addPayment(int orderId, double amount, const string &date, const string &mode) {
        Payment p;
        p.paymentId = nextPaymentId++;
        p.orderId = orderId;
        p.amount = amount;
        p.date = date;
        p.mode = mode;
        payments[p.paymentId] = p;
        save();
        return p.paymentId;
    }

    double getTotalForOrder(int orderId) const {
        double sum = 0.0;
        for (auto &kv : payments) {
            if (kv.second.orderId == orderId) {
                sum += kv.second.amount;
            }
        }
        return sum;
    }

    double getTotalPayments() const {
        double sum = 0.0;
        for (auto &kv : payments) {
            sum += kv.second.amount;
        }
        return sum;
    }

    void displayAll() const {
        cout << "ID\tOrderID\tAmount\tDate\tMode\n";
        cout << "--------------------------------------------------------\n";
        for (auto &kv : payments) {
            auto &p = kv.second;
            cout << p.paymentId << "\t" << p.orderId << "\t" 
                 << fixed << setprecision(2) << p.amount << "\t" 
                 << p.date << "\t" << p.mode << "\n";
        }
    }

    const unordered_map<int, Payment>& getPayments() const { return payments; }
};

/*------------------Reporting & Backup------------------*/

class ReportManager {
public:
    static void generateDailyReport(const InventoryManager& im, const OrderManager& om, 
                                   const PaymentManager& pm, const CustomerManager& cm, 
                                   const string &date) {
        double totalSales = 0.0;
        double paymentsReceived = 0.0;
        double pendingPayments = 0.0;
        
        // Calculate sales from delivered orders
        for (auto &kv : om.getOrders()) {
            const Order &o = kv.second;
            if (o.deliveryDate == date && o.status == "Delivered") {
                totalSales += o.calculateTotal(im.getProducts());
            }
        }

        // Calculate payments received
        for (auto &kv : pm.getPayments()) {
            if (kv.second.date == date) {
                paymentsReceived += kv.second.amount;
            }
        }

        // Calculate pending payments from customers
        for (auto &kv : cm.getCustomers()) {
            pendingPayments += kv.second.pending;
        }

        cout << "\n=== Daily Report for " << date << " ===\n";
        cout << "========================================\n";
        cout << "Total Sales (delivered orders): " << fixed << setprecision(2) << totalSales << "\n";
        cout << "Total Payments Received: " << fixed << setprecision(2) << paymentsReceived << "\n";
        cout << "Total Pending Payments: " << fixed << setprecision(2) << pendingPayments << "\n";
        cout << "\nCurrent Inventory Status:\n";
        im.displayAll();
    }

    static void exportDailyCSV(const string &filename, const InventoryManager& im, 
                              const OrderManager& om, const PaymentManager& pm, 
                              const CustomerManager& cm, const string &date) {
        ofstream fout(filename);
        if (!fout.is_open()) {
            cerr << "Error: Cannot open file " << filename << " for writing.\n";
            return;
        }

        fout << "report_for," << date << "\n";
        
        double totalSales = 0.0;
        for (auto &kv : om.getOrders()) {
            const Order &o = kv.second;
            if (o.deliveryDate == date && o.status == "Delivered") {
                totalSales += o.calculateTotal(im.getProducts());
            }
        }
        fout << "total_sales," << fixed << setprecision(2) << totalSales << "\n";

        double paymentsReceived = 0.0;
        for (auto &kv : pm.getPayments()) {
            if (kv.second.date == date) {
                paymentsReceived += kv.second.amount;
            }
        }
        fout << "payments_received," << fixed << setprecision(2) << paymentsReceived << "\n";

        double pendingPayments = 0.0;
        for (auto &kv : cm.getCustomers()) {
            pendingPayments += kv.second.pending;
        }
        fout << "pending_payments," << fixed << setprecision(2) << pendingPayments << "\n";
        
        fout << "\nproducts\nid,name,unit,price,stock,minStock\n";
        for (auto &kv : im.getProducts()) {
            fout << kv.second.toCSV() << "\n";
        }
        
        fout.close();
        cout << "Report exported to " << filename << "\n";
    }
};

class BackupManager {
public:
    static bool backupAll(const InventoryManager& im, const CustomerManager& cm, 
                          const OrderManager& om, const PaymentManager& pm, 
                          const string &filename) {
        json j;

        // Products
        j["products"] = json::array();
        for (auto &kv : im.getProducts()) {
            auto &p = kv.second;
            j["products"].push_back({
                {"id", p.id},
                {"name", p.name},
                {"unit", p.unit},
                {"price", p.price},
                {"stock", p.stock},
                {"minStock", p.minStock}
            });
        }

        // Customers
        j["customers"] = json::array();
        for (auto &kv : cm.getCustomers()) {
            auto &c = kv.second;
            j["customers"].push_back({
                {"id", c.id},
                {"name", c.name},
                {"type", c.type},
                {"contact", c.contact},
                {"address", c.address},
                {"pending", c.pending}
            });
        }

        // Orders
        j["orders"] = json::array();
        for (auto &kv : om.getOrders()) {
            auto &o = kv.second;
            json jo;
            jo["orderId"] = o.orderId;
            jo["customerId"] = o.customerId;
            jo["items"] = json::array();
            for (auto &item : o.items) {
                jo["items"].push_back({
                    {"productId", item.first},
                    {"qty", item.second}
                });
            }
            jo["orderDate"] = o.orderDate;
            jo["deliveryDate"] = o.deliveryDate;
            jo["status"] = o.status;
            j["orders"].push_back(jo);
        }

        // Payments
        j["payments"] = json::array();
        for (auto &kv : pm.getPayments()) {
            auto &p = kv.second;
            j["payments"].push_back({
                {"paymentId", p.paymentId},
                {"orderId", p.orderId},
                {"amount", p.amount},
                {"date", p.date},
                {"mode", p.mode}
            });
        }

        ofstream fout(filename);
        if (!fout.is_open()) {
            cerr << "Error: Cannot open " << filename << " for writing.\n";
            return false;
        }
        fout << setw(2) << j << "\n";
        fout.close();
        return true;
    }

    static bool restoreAll(const string &filename, InventoryManager& im, 
                          CustomerManager& cm, OrderManager& om, PaymentManager& pm) {
        ifstream fin(filename);
        if (!fin.is_open()) {
            cerr << "Error: Cannot open " << filename << " for reading.\n";
            return false;
        }

        json j;
        try {
            fin >> j;
        } catch (const exception &e) {
            cerr << "Error: Invalid JSON format - " << e.what() << "\n";
            fin.close();
            return false;
        }
        fin.close();

        // Clear current data
        // Note: Since we can't directly clear the internal maps, we need to load
        // from CSV to reset. Instead, we'll restore and then save.
        // This is a simplified approach - in practice you might want to clear the maps.

        // Products
        if (j.contains("products")) {
            for (auto &jp : j["products"]) {
                Product p;
                p.id = jp["id"].get<int>();
                p.name = jp["name"].get<string>();
                p.unit = jp["unit"].get<string>();
                p.price = jp["price"].get<double>();
                p.stock = jp["stock"].get<int>();
                p.minStock = jp["minStock"].get<int>();
                // We'll save this later
            }
        }

        // We'll need to reload from the backup data
        // Since the managers don't have direct access to modify their internal maps,
        // we should implement a proper restore method in each manager.
        // For now, we'll just save the data and reload.

        // This is a simplified version - in practice you'd want to clear the maps
        // and then add the restored data.

        cout << "Restore functionality requires manual implementation.\n";
        cout << "The data was read from " << filename << " but not applied.\n";
        return true;
    }
};

/*--------------- Invoice Generation ---------------*/

class InvoiceGenerator {
public:
    static void generate(const Order &order, const InventoryManager& im, 
                        const CustomerManager& cm) {
        string filename = "invoice_" + to_string(order.orderId) + ".txt";
        ofstream fout(filename);
        
        if (!fout.is_open()) {
            cerr << "Error: Cannot create invoice file.\n";
            return;
        }

        fout << "========================================\n";
        fout << "      ABC DAIRY COMPANY - INVOICE\n";
        fout << "========================================\n\n";
        fout << "Invoice #: " << order.orderId << "\n";
        fout << "Date: " << order.orderDate << "\n";
        fout << "Delivery Date: " << order.deliveryDate << "\n\n";
        
        fout << "Customer Information:\n";
        fout << "----------------------\n";
        auto itc = cm.getCustomers().find(order.customerId);
        if (itc != cm.getCustomers().end()) {
            fout << "Name: " << itc->second.name << "\n";
            fout << "Contact: " << itc->second.contact << "\n";
            fout << "Address: " << itc->second.address << "\n";
        }
        
        fout << "\nOrder Details:\n";
        fout << "---------------\n";
        fout << "Product\t\tQty\tUnit Price\tLine Total\n";
        fout << "------------------------------------------------\n";
        
        double total = 0.0;
        for (auto &item : order.items) {
            auto it = im.getProducts().find(item.first);
            if (it != im.getProducts().end()) {
                double lineTotal = it->second.price * item.second;
                fout << it->second.name << "\t\t" 
                     << item.second << "\t" 
                     << fixed << setprecision(2) << it->second.price << "\t\t" 
                     << lineTotal << "\n";
                total += lineTotal;
            } else {
                fout << "Unknown Product (ID: " << item.first << ")\t" 
                     << item.second << "\n";
            }
        }
        
        fout << "------------------------------------------------\n";
        fout << "TOTAL AMOUNT: " << fixed << setprecision(2) << total << "\n";
        fout << "\n========================================\n";
        fout << "      Thank You for Your Business!\n";
        fout << "========================================\n";
        fout.close();
        
        cout << "Invoice generated: " << filename << "\n";
    }
};

/*------------------- Main Program -------------------*/

void displayMainMenu() {
    cout << "\n╔═══════════════════════════════════════════════════╗\n";
    cout << "║   DAIRY PRODUCT ORDER & INVENTORY MANAGEMENT    ║\n";
    cout << "╚═══════════════════════════════════════════════════╝\n";
    cout << "1. Product Management\n";
    cout << "2. Customer Management\n";
    cout << "3. Order Management\n";
    cout << "4. Payment Management\n";
    cout << "5. Inventory Reports\n";
    cout << "6. Daily Reports\n";
    cout << "7. Backup / Restore\n";
    cout << "8. Export CSV\n";
    cout << "0. Exit\n";
    cout << "────────────────────────────────────────────────────\n";
    cout << "Choose option: ";
}

void handleProductMenu(InventoryManager& im) {
    cout << "\nProduct Management:\n";
    cout << "  a - Add Product\n";
    cout << "  l - List Products\n";
    cout << "  u - Update Price\n";
    cout << "  s - Adjust Stock\n";
    cout << "  r - Return to Main Menu\n";
    cout << "Choose: ";
    
    char choice;
    cin >> choice;
    cin.ignore();

    switch (choice) {
        case 'a': {
            string name, unit;
            double price;
            int stock, minStock;
            
            cout << "Product Name: ";
            getline(cin, name);
            name = trim(name);
            if (name.empty()) {
                cout << "Product name cannot be empty.\n";
                break;
            }
            
            cout << "Unit (liters/kg/packet): ";
            getline(cin, unit);
            unit = trim(unit);
            
            cout << "Price per unit: ";
            cin >> price;
            if (price < 0) {
                cout << "Price cannot be negative.\n";
                break;
            }
            
            cout << "Stock quantity: ";
            cin >> stock;
            if (stock < 0) {
                cout << "Stock cannot be negative.\n";
                break;
            }
            
            cout << "Minimum stock threshold: ";
            cin >> minStock;
            if (minStock < 0) {
                cout << "Minimum stock cannot be negative.\n";
                break;
            }
            
            int id = im.addProduct(name, unit, price, stock, minStock);
            cout << "✓ Product added with ID: " << id << "\n";
            break;
        }
        case 'l':
            im.displayAll();
            break;
        case 'u': {
            int id;
            double newPrice;
            cout << "Product ID: ";
            cin >> id;
            cout << "New Price: ";
            cin >> newPrice;
            if (im.updatePrice(id, newPrice)) {
                cout << "✓ Price updated successfully.\n";
            } else {
                cout << "✗ Product not found.\n";
            }
            break;
        }
        case 's': {
            int id, delta;
            cout << "Product ID: ";
            cin >> id;
            cout << "Change in stock (+ to increase, - to decrease): ";
            cin >> delta;
            if (im.adjustStock(id, delta)) {
                cout << "✓ Stock adjusted successfully.\n";
            } else {
                cout << "✗ Product not found.\n";
            }
            break;
        }
        case 'r':
            break;
        default:
            cout << "✗ Invalid option.\n";
    }
}

void handleCustomerMenu(CustomerManager& cm) {
    cout << "\nCustomer Management:\n";
    cout << "  a - Add Customer\n";
    cout << "  l - List Customers\n";
    cout << "  r - Return to Main Menu\n";
    cout << "Choose: ";
    
    char choice;
    cin >> choice;
    cin.ignore();

    switch (choice) {
        case 'a': {
            string name, type, contact, address;
            
            cout << "Customer Name: ";
            getline(cin, name);
            name = trim(name);
            if (name.empty()) {
                cout << "Customer name cannot be empty.\n";
                break;
            }
            
            cout << "Type (Individual/Business/Hotel): ";
            getline(cin, type);
            
            cout << "Contact: ";
            getline(cin, contact);
            
            cout << "Address (optional): ";
            getline(cin, address);
            
            int id = cm.addCustomer(name, type, contact, address);
            cout << "✓ Customer added with ID: " << id << "\n";
            break;
        }
        case 'l':
            cm.displayAll();
            break;
        case 'r':
            break;
        default:
            cout << "✗ Invalid option.\n";
    }
}

void handleOrderMenu(InventoryManager& im, CustomerManager& cm, OrderManager& om) {
    cout << "\nOrder Management:\n";
    cout << "  c - Create Order\n";
    cout << "  l - List Orders\n";
    cout << "  d - Deliver Order\n";
    cout << "  x - Cancel Order\n";
    cout << "  r - Return to Main Menu\n";
    cout << "Choose: ";
    
    char choice;
    cin >> choice;
    cin.ignore();

    switch (choice) {
        case 'c': {
            cm.displayAll();
            int customerId;
            cout << "Customer ID: ";
            cin >> customerId;
            
            if (!cm.customerExists(customerId)) {
                cout << "✗ Customer not found.\n";
                break;
            }
            
            vector<pair<int, int>> items;
            cout << "Enter items (productId quantity). Enter 0 0 to finish.\n";
            im.displayAll();
            
            while (true) {
                int productId, qty;
                cout << "ProductID Qty: ";
                cin >> productId >> qty;
                
                if (productId == 0 && qty == 0) break;
                
                if (!im.productExists(productId)) {
                    cout << "✗ Invalid product ID.\n";
                    continue;
                }
                
                if (qty <= 0) {
                    cout << "✗ Quantity must be positive.\n";
                    continue;
                }
                
                items.emplace_back(productId, qty);
            }
            
            if (items.empty()) {
                cout << "✗ No items entered. Order cancelled.\n";
                break;
            }
            
            string orderDate = getCurrentDate();
            int orderId = om.createOrder(customerId, items, orderDate);
            
            Order* order = om.getOrder(orderId);
            if (order) {
                double total = order->calculateTotal(im.getProducts());
                cm.addPending(customerId, total);
                cout << "✓ Order created with ID: " << orderId << "\n";
                cout << "  Order Total: " << fixed << setprecision(2) << total << "\n";
            }
            break;
        }
        case 'l':
            om.displayAll();
            break;
        case 'd': {
            int orderId;
            cout << "Order ID to deliver: ";
            cin >> orderId;
            
            Order* order = om.getOrder(orderId);
            if (!order) {
                cout << "✗ Order not found.\n";
                break;
            }
            
            if (order->status != "Pending") {
                cout << "✗ Order is not pending (status: " << order->status << ").\n";
                break;
            }
            
            // Check stock availability
            bool hasStock = true;
            for (auto &item : order->items) {
                Product* product = im.getProduct(item.first);
                if (!product || product->stock < item.second) {
                    hasStock = false;
                    break;
                }
            }
            
            if (!hasStock) {
                cout << "✗ Ins
