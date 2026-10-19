# HTTPCtrlIntfModule

`HTTPCtrlIntfModule` exposes a simple HTTP transport for commands processed by an application's control core. It inherits from `CtrlIntfModule` and forwards commands using `CtrlIntfModuleMessage` and the configured queue callback. Command parsing and execution belong to the application, not this module.

## Endpoints

| Endpoint | Behavior |
| --- | --- |
| `GET /` | Returns the configured static HTML page (`200 OK`). If no page was provided, returns a default placeholder page. |
| `POST /cmd` | Reads a plain-text command, forwards it to the control core, waits for completion, and returns the resulting text (`200 OK`). |

Invalid or incomplete requests receive `400 Bad Request`. If the command cannot be queued, the module returns `503 Service Unavailable`.

Example using `curl`:

```sh
curl http://<device-ip>/
curl -X POST http://<device-ip>/cmd \
  -H 'Content-Type: text/plain' \
  --data-binary 'YOUR_COMMAND'
```

`YOUR_COMMAND` is a placeholder: available commands and their response formats are defined by the application.

Browser example:

```javascript
const response = await fetch('/cmd', {
  method: 'POST',
  headers: { 'Content-Type': 'text/plain' },
  body: 'YOUR_COMMAND',
});
const result = await response.text();
```

## Integration

Provide an initialized `EthernetInterface`, a listening port, a socket timeout, the command framing parameters, a queue callback, thread configuration, and an optional `web_page` pointer when constructing the module.

The HTML page is supplied by the application and must remain valid while the module uses it. The command handler runs outside this module and is responsible for producing the response in the provided message buffer and releasing the completion semaphore.

`is_listening()` reports whether the server has successfully entered the local listening state. It does **not** verify external connectivity or network access permissions.

The server checks the Ethernet connection and recreates the listening socket after connection loss or socket errors.

