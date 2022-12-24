var net = require('net');

var client = new net.Socket();
client.connect(5123, '127.0.0.1', function () {
    console.log('Connected');
    client.setNoDelay(true);
    client.write("'$?#3f'");
});

let sent = 0;
let received = 0;
let readBytes = 0;
let writtenBytes = 0;
let start = performance.now();

client.on('data', function (data) {
    console.log('Received: ' + data);
    client.write("+");
    if (sent == 0) {
        sent++;
        client.write('$?#3f');
    }
    sent++;
    received += ("" + data).split("$").length;
    readBytes += data.length;
    writtenBytes += '$?#3f'.length;
    if (sent % 100 == 0) {
        let passedTime = (performance.now() - start) / 1000;
        console.log("Checkpoint", "received messages: ", received, "written messags: " + sent, "read bytes: ", readBytes, "written bytes: ", writtenBytes, "time: ", passedTime);
    }
});

client.on('close', function () {
    console.log('Connection closed');
});