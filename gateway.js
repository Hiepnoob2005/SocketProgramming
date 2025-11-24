const WebSocket = require('ws');
const net = require('net');
const http = require('http');
const fs = require('fs');
const path = require('path');

// Configuration
const WEB_PORT = 8080;        // Port cho Web Client kết nối
const SERVER_HOST = '127.0.0.1'; // IP của C++ Server
const SERVER_PORT = 8888;     // Port của C++ Server

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
console.log('  WebSocket Gateway Started');
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
        
        socket.connect(SERVER_PORT, SERVER_HOST, () => {
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
                // Kiểm tra trạng thái kết nối
                ws.send(JSON.stringify({
                    type: 'status',
                    connected: isConnectedToCpp
                }));
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
        
        console.log(`[Gateway] Sent command ${command} to C++ Server`);
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
        }
        
        // Đọc data
        if (headerReceived) {
            if (responseBuffer.length < expectedDataSize) {
                // Chưa đủ dữ liệu
                break;
            }
            
            const dataBuffer = responseBuffer.slice(0, expectedDataSize);
            responseBuffer = responseBuffer.slice(expectedDataSize);
            
            // Xử lý theo loại command
            if (expectedCommand === 6) { // CMD_SCREENSHOT
                // Gửi binary data (ảnh BMP)
                ws.send(JSON.stringify({
                    type: 'response',
                    command: expectedCommand,
                    isImage: true,
                    data: dataBuffer.toString('base64')
                }));
            } else {
                // Gửi text data
                const textData = dataBuffer.toString('utf8');
                ws.send(JSON.stringify({
                    type: 'response',
                    command: expectedCommand,
                    isImage: false,
                    data: textData
                }));
            }
            
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