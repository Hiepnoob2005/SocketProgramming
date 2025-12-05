const WebSocket = require('ws');
const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');

// Configuration
const WEB_PORT = 8080;        // Port cho Web Client kết nối
const SERVER_HOST = '10.211.55.4'; // IP của C++ Server (thay đổi nếu cần)
const SERVER_PORT = 8888;     // Port của C++ Server

// Command codes (phải khớp với common.h)
const CMD = {
    LIST_APPS: 1,
    STOP_APP: 2,
    START_APP: 3,
    LIST_PROCESS: 4,
    KILL_PROCESS: 5,
    SCREENSHOT: 6,
    KEYLOG_START: 7,
    KEYLOG_STOP: 8,
    KEYLOG_GET: 9,
    WEBCAM_ON: 10,
    WEBCAM_OFF: 11,
    SHUTDOWN: 12,
    RESTART: 13,
    WEBCAM_CAPTURE: 14,  // NEW
    EXIT: 99
};

// Tạo HTTP server để serve HTML
const httpServer = http.createServer((req, res) => {
    if (req.url === '/' || req.url === '/index.html') {
        fs.readFile(path.join(__dirname, 'index.html'), (err, data) => {
            if (err) {
                res.writeHead(404);
                res.end('File not found');
                return;
            }
            res.writeHead(200, { 'Content-Type': 'text/html' });
            res.end(data);
        });
    } else {
        res.writeHead(404);
        res.end('Not found');
    }
});

// Tạo WebSocket server
const wss = new WebSocket.Server({ server: httpServer });

console.log('='.repeat(50));
console.log('  WebSocket Gateway v2.0 (with Webcam Support)');
console.log('='.repeat(50));
console.log(`Web Client port: ${WEB_PORT}`);
console.log(`C++ Server: ${SERVER_HOST}:${SERVER_PORT}`);
console.log('Open browser at: http://localhost:' + WEB_PORT);
console.log('='.repeat(50));

// Quản lý kết nối
let cppSocket = null;
let isConnectedToCpp = false;

// Kết nối đến C++ Server
function connectToCppServer() {
    return new Promise((resolve, reject) => {
        const socket = new net.Socket();
        
    const connectOptions = {
        port: SERVER_PORT,
        host: SERVER_HOST,
        family: 4, // Giữ cái này: Ép dùng IPv4 để tránh nó thử IPv6 gây delay/lỗi
    };

    // // Thêm timeout để không bị treo nếu server C++ đơ
    // socket.setTimeout(5000); 
    // socket.on('timeout', () => {
    //     console.log('[Gateway] Connection timed out');
    //     socket.destroy();
    // });
        socket.connect(connectOptions, () => {
            console.log('[Gateway] Connected to C++ Server');
            isConnectedToCpp = true;
            resolve(socket);
        });
        
        socket.on('error', (err) => {
            console.error('[Gateway] C++ Server connection error:', err.message);
            isConnectedToCpp = false;
            reject(err);
        });
        
        socket.on('close', () => {
            console.log('[Gateway] C++ Server connection closed');
            isConnectedToCpp = false;
            cppSocket = null;
        });
    });
}

// Xử lý WebSocket client
wss.on('connection', (ws) => {
    console.log('[Gateway] Web Client connected');
    
    ws.on('message', async (message) => {
        try {
            const data = JSON.parse(message.toString());
            console.log('[Gateway] Received from Web Client:', data);
            
            // Xử lý các loại message
            if (data.type === 'connect') {
                // Kết nối đến C++ Server
                try {
                    if (!cppSocket || !isConnectedToCpp) {
                        cppSocket = await connectToCppServer();
                        
                        // Lắng nghe data từ C++ Server
                        cppSocket.on('data', (buffer) => {
                            handleCppResponse(ws, buffer);
                        });
                    }
                    
                    ws.send(JSON.stringify({
                        type: 'status',
                        connected: true,
                        message: 'Connected to C++ Server'
                    }));
                } catch (err) {
                    ws.send(JSON.stringify({
                        type: 'status',
                        connected: false,
                        message: 'Failed to connect to C++ Server: ' + err.message
                    }));
                }
            } else if (data.type === 'command') {
                // Gửi command đến C++ Server
                if (!cppSocket || !isConnectedToCpp) {
                    ws.send(JSON.stringify({
                        type: 'error',
                        message: 'Not connected to C++ Server'
                    }));
                    return;
                }
                
                sendCommandToCpp(cppSocket, data.cmd, data.data || '');
            } else if (data.type === 'status_check') {
                //  Kiểm tra trạng thái kết nối
                // ws.send(JSON.stringify({
                //     type: 'status',
                //     connected: isConnectedToCpp
                // }));
            }
        } catch (err) {
            console.error('[Gateway] Error processing message:', err);
            ws.send(JSON.stringify({
                type: 'error',
                message: 'Invalid message format'
            }));
        }
    });
    
    ws.on('close', () => {
        console.log('[Gateway] Web Client disconnected');
    });
    
    ws.on('error', (err) => {
        console.error('[Gateway] WebSocket error:', err);
    });
});

// Gửi command đến C++ Server
function sendCommandToCpp(socket, command, data) {
    try {
        // Cấu trúc packet giống với C++:
        // Header: [command (4 bytes), dataSize (4 bytes)]
        // Body: data (dataSize bytes)
        
        const dataBuffer = Buffer.from(data, 'utf8');
        const dataSize = dataBuffer.length;
        
        // Tạo header buffer
        const headerBuffer = Buffer.alloc(8);
        headerBuffer.writeInt32LE(command, 0);  // command
        headerBuffer.writeInt32LE(dataSize, 4); // dataSize
        
        // Gửi header
        socket.write(headerBuffer);
        
        // Gửi data nếu có
        if (dataSize > 0) {
            socket.write(dataBuffer);
        }
        
        console.log(`[Gateway] Sent command ${command} to C++ Server (data: "${data}")`);
    } catch (err) {
        console.error('[Gateway] Error sending command:', err);
    }
}

// Xử lý response từ C++ Server
let responseBuffer = Buffer.alloc(0);
let expectedCommand = 0;
let expectedDataSize = 0;
let headerReceived = false;

function handleCppResponse(ws, buffer) {
    responseBuffer = Buffer.concat([responseBuffer, buffer]);
    
    while (responseBuffer.length > 0) {
        // Đọc header nếu chưa đọc
        if (!headerReceived) {
            if (responseBuffer.length < 8) {
                // Chưa đủ dữ liệu cho header
                break;
            }
            
            expectedCommand = responseBuffer.readInt32LE(0);
            expectedDataSize = responseBuffer.readInt32LE(4);
            responseBuffer = responseBuffer.slice(8);
            headerReceived = true;
            
            console.log(`[Gateway] Receiving command ${expectedCommand}, size: ${expectedDataSize} bytes`);
        }
        
        // Đọc data
        if (headerReceived) {
            if (responseBuffer.length < expectedDataSize) {
                // Chưa đủ dữ liệu
                console.log(`[Gateway] Waiting for more data: ${responseBuffer.length}/${expectedDataSize}`);
                break;
            }
            
            const dataBuffer = responseBuffer.slice(0, expectedDataSize);
            responseBuffer = responseBuffer.slice(expectedDataSize);
            
            // Xử lý theo loại command
            let responseData = {
                type: 'response',
                command: expectedCommand,
                isImage: false,
                isVideo: false,
                data: ''
            };
            
            if (expectedCommand === CMD.SCREENSHOT) {
                // Gửi binary data (ảnh BMP)
                responseData.isImage = true;
                responseData.data = dataBuffer.toString('base64');
                console.log(`[Gateway] Screenshot received: ${expectedDataSize} bytes`);
                
            } else if (expectedCommand === CMD.WEBCAM_CAPTURE) {
                // Kiểm tra xem có phải video data hay error message
                const textCheck = dataBuffer.toString('utf8', 0, Math.min(50, dataBuffer.length));
                
                if (textCheck.startsWith('Failed') || textCheck.startsWith('Invalid') || textCheck.startsWith('VIDEO_TOO_LARGE')) {
                    // Error message
                    responseData.data = dataBuffer.toString('utf8');
                    console.log(`[Gateway] Webcam capture error: ${responseData.data}`);
                } else {
                    // Video data
                    responseData.isVideo = true;
                    responseData.data = dataBuffer.toString('base64');
                    console.log(`[Gateway] Video received: ${expectedDataSize} bytes`);
                }    
            } else if (expectedCommand === CMD.WEBCAM_OFF) { 
                const textMsg = dataBuffer.toString('utf8');
                // Kiểm tra xem message có chứa keyword không
                if (textMsg.startsWith("VIDEO_SAVED_LOCAL")) {
                    console.log('[Gateway] Server finished recording. Processing...');

                    // --- LOGIC MỚI: TÁCH TÊN FILE ---
                    // Message dạng: "VIDEO_SAVED_LOCAL|filename.avi|duration"
                    const parts = textMsg.split('|');
                    let serverFileName = "video_temp.avi"; // Fallback nếu không có tên
                    let duration = "0";
                    if (parts.length > 1) {
                        serverFileName = parts[1].trim(); // Lấy tên file thực tế: rec_xxxx.avi
                    }
                    if (parts.length > 2) {
                        duration = parts[2].trim();
                    }
                    const sourcePath = path.join(__dirname, serverFileName);
                    
                    // Tên file mới để Client tải về (giữ nguyên logic timestamp của Nodejs cũng được)
                    // Hoặc dùng chính tên C++ gửi sang
                    const destFileName = serverFileName; 
                    const destPath = path.join(VIDEO_DIR, destFileName);

                    if (fs.existsSync(sourcePath)) {
                        try {
                            // Di chuyển file vào thư mục videos
                            fs.renameSync(sourcePath, destPath); // rename nhanh hơn copy+unlink

                            console.log(`[Gateway] Moved ${serverFileName} to ${destPath}`);

                            responseData.isVideo = true;
                            responseData.videoUrl = `/videos/${destFileName}`;
                            responseData.duration = duration;
                            responseData.data = "Video ready";
                        } catch (err) {
                            console.error('[Gateway] Error moving file:', err);
                            responseData.data = "Error processing video file.";
                        }
                    } else {
                        console.error(`[Gateway] File not found: ${sourcePath}`);
                        responseData.data = `Error: File ${serverFileName} not found on server.`;
                    }
                } else {
                    responseData.data = textMsg;
                }
            } else {
                // --- BẮT ĐẦU ĐOẠN SỬA ---
                
                // Bước 1: Tạo biến textData từ buffer (QUAN TRỌNG: Phải có dòng này trước)
                const textData = dataBuffer.toString('utf8');

                // Bước 2: In ra Terminal để kiểm tra
                console.log('===================================');
                console.log('--- [DEBUG] DỮ LIỆU TỪ C++ GỬI VỀ ---');
                console.log(textData);
                console.log('===================================');

                // Bước 3: Gán dữ liệu vào gói tin trả về Web
                responseData.data = textData;
                
                // --- KẾT THÚC ĐOẠN SỬA ---            
                }
            
            ws.send(JSON.stringify(responseData));
            console.log(`[Gateway] Sent response for command ${expectedCommand} to Web Client`);
            
            // Reset
            headerReceived = false;
            expectedCommand = 0;
            expectedDataSize = 0;
        }
    }
}

// Bắt đầu HTTP server
httpServer.listen(WEB_PORT, () => {
    console.log(`[Gateway] HTTP/WebSocket server listening on port ${WEB_PORT}`);
});

// Xử lý tắt server
process.on('SIGINT', () => {
    console.log('\n[Gateway] Shutting down...');
    if (cppSocket) {
        cppSocket.end();
    }
    wss.close();
    httpServer.close();
    process.exit(0);
});