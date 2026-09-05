# Dairy Product Order & Inventory Management System

A console-based C++ application for managing dairy product inventory, orders, customers, and payments.

## Features
- Product management (add, list, update price, adjust stock)
- Customer management
- Order processing (create, deliver, cancel)
- Payment tracking
- Low stock alerts
- Daily reports
- JSON backup/restore
- Invoice generation

## Dependencies
- nlohmann/json (included as single header)

## Build
```bash
g++ -std=c++17 dairy_self.cpp -o dairy_management
