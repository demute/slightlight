const WebSocket = require('ws')

const wss = new WebSocket.Server({ port: 9898 })
var readline = require('readline');
var rl = readline.createInterface({ input: process.stdin, output: process.stdout, terminal: false });

wss.on('connection', ws => {
  ws.on('message', message => {
    console.log(`Received message => ${message}`)
  })

    //  ws.send('ho!')
    rl.on('line', function(line){
        try {
            ws.send(line);
            console.log (line);
        }
        catch (err) {
        }
    })

})
