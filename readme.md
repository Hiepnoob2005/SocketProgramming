# 🖥️ Remote Control System (Socket Programming)

Dự án **Remote Control System** là một ứng dụng điều khiển máy tính từ xa lai (Hybrid), kết hợp sức mạnh của **C++ (Socket chuẩn)** để tương tác hệ thống cấp thấp và **Node.js (WebSocket)** để cung cấp giao diện Web hiện đại.

Dự án được xây dựng nhằm mục đích học tập môn Mạng máy tính (Socket Programming), Hệ điều hành và Lập trình hệ thống.

---

## 🚀 Kiến trúc hệ thống

Hệ thống hoạt động theo mô hình 3 lớp (3-Tier Architecture):

1.  **Target Machine (C++ Server):** Chạy trên máy nạn nhân/máy cần điều khiển. Xử lý các lệnh hệ thống (Webcam, Keylog, Process...).
2.  **Gateway (Node.js):** Đóng vai trò cầu nối trung gian.
    * Kết nối TCP Socket với C++ Server.
    * Tạo WebSocket Server để giao tiếp với Web Client.
3.  **Control Machine (Web Client):** Giao diện người dùng trên trình duyệt, gửi lệnh tới Gateway.

---

## ✨ Tính năng chính

### 1. Giám sát hệ thống
* **List Apps:** Liệt kê các ứng dụng đang chạy (có cửa sổ).
* **List Processes:** Liệt kê tất cả các tiến trình (PID, tên, RAM usage).
* **Screenshot:** Chụp ảnh màn hình máy trạm và gửi về Web Client theo thời gian thực.

### 2. Điều khiển & Tương tác
* **Stop/Start App:** Tắt hoặc mở ứng dụng từ xa.
* **Kill Process:** Buộc dừng một tiến trình thông qua PID.
* **Shutdown/Restart:** Tắt hoặc khởi động lại máy trạm.

### 3. Tính năng nâng cao (Surveillance)
* **Webcam Streaming:**
    * Quay video từ Webcam và lưu file `.avi` tại server, sau đó chuyển về client.
    * Chụp ảnh (Snapshot) từ Webcam.
* **Keylogger:** Ghi lại thao tác bàn phím (Keystrokes) và gửi về khi có yêu cầu.

---

## 🛠️ Yêu cầu hệ thống (Prerequisites)

### Đối với C++ Server (Target Machine)
* **Hệ điều hành:** Windows (Khuyến nghị - Full tính năng) hoặc Linux (Hạn chế tính năng Webcam/Keylog).
* **Compiler:** MinGW-w64 (g++) trên Windows hoặc GCC trên Linux.
* **Libraries:** `winsock2`, `gdiplus`, `vfw32` (có sẵn trong Windows SDK/MinGW).

### Đối với Gateway (Control Machine)
* **Runtime:** Node.js (v14.0.0 trở lên).
* **Package Manager:** npm.

---

## ⚡ Cấu hình Mạng (Bắt buộc thực hiện trước)

Để đảm bảo kết nối thông suốt ngay lập tức giữa Gateway và Server mà không bị chặn bởi hệ điều hành, bạn cần **TẮT HOÀN TOÀN TƯỜNG LỬA (FIREWALL)** trên máy chạy Server (nên là máy ảo).

### 1. Trên Windows (Chạy CMD quyền Admin)
Copy và chạy lệnh sau để tắt firewall cho mọi profile mạng:

```cmd
netsh advfirewall set allprofiles state off
```
### 2. Trên Linux
Chạy lệnh sau trên terminal:
```bash
sudo ufw disable
```
**⚠️ Lưu ý:** Sau khi chạy các chức năng xong cần **BẬT TƯỜNG LỬA TRỞ LẠI**.
### 1. Trên Windows (Chạy CMD quyền Admin)
Copy và chạy lệnh sau để tắt firewall cho mọi profile mạng:

```cmd
netsh advfirewall set allprofiles state on
```
### 2. Trên Linux
Chạy lệnh sau trên terminal:
```bash
sudo ufw enable
```
---

## 📦 Hướng dẫn Cài đặt & Build

### 1. Build C++ Server & Console Client

**Trên Windows, tại thư mục SocketProgramming (sử dụng MinGW/Git Bash):**
```bash
# Dùng g++ build file server.exe
g++ -std=c++11 -pthread server.cpp -o server.exe -lws2_32 -luser32 -lpsapi -lgdiplus -lgdi32 -lvfw32 -static-libgcc -static-libstdc++ -static
```

**Trên Linux, tại thư mục SocketProgramming (sử dụng MinGW/Git Bash):**
```bash
# Cài đặt thư viện cần thiết
make install-deps

# Build
make all
```

### 2. Cài đặt Node.js Gateway
Di chuyển vào thư mục chứa package.json:

```bash
# Cài đặt các thư viện (ws, nodemon...)
npm install
```
---

## 🎮 Hướng dẫn sử dụng (Chạy Demo)
Để chạy thử nghiệm trên cùng một máy (`localhost`), hãy mở 3 terminal riêng biệt:

### Bước 1: Khởi động C++ Server

Mở file `server.exe`. Đây là thành phần chạy trên máy cần bị điều khiển.

### Bước 2: Khởi động Gateway

Gateway sẽ kết nối tới C++ Server và mở cổng cho Web.
```bash
# Tại Terminal 2
npm start
# Gateway sẽ kết nối tới Server (8888) và mở WebSocket (8080)
```

Lưu ý: Nếu Server C++ nằm ở máy khác, hãy sửa địa chỉ IP trong file `gateway.js` (`SERVER_HOST`).

### Bước 3: Mở bảng điều khiển

Mở trình duyệt (Chrome/Edge/Firefox/Safari/...) và truy cập:

http://localhost:8080

Lúc này bạn có thể nhấn các nút trên giao diện web để điều khiển C++ Server.

---

## 📂 Cấu trúc thư mục
```bash
.
├── server.cpp          # Mã nguồn Server (Chạy trên máy Target)
├── client.cpp          # Mã nguồn Client Console (Tùy chọn)
├── common.h            # Các định nghĩa chung, struct Packet, xử lý gửi file
├── gateway.js          # Node.js trung gian (TCP <-> WebSocket)
├── index.html          # Giao diện điều khiển Web (Frontend)
├── makefile            # Script build tự động cho C++
├── package.json        # Khai báo dependency cho Node.js
└── videos/             # Thư mục chứa video quay được từ webcam (tự động tạo)
```

## ⚠️ Tuyên bố miễn trừ trách nhiệm (Disclaimer)
Dự án này được tạo ra **DUY NHẤT CHO MỤC ĐÍCH GIÁO DỤC**.

Không sử dụng mã nguồn này để xâm nhập, điều khiển máy tính của người khác khi không có sự cho phép.

Nhóm tác giả không chịu trách nhiệm cho bất kỳ hành vi sử dụng sai mục đích nào của người dùng.