#!/usr/bin/env python3
"""HTTP -> HTTPS bridge for the Rec Room LAN patch.

The patched 20230427 client speaks plain HTTP to the LAN server (Unity 2020.3's
Mono TLS cannot verify the public Cloudflare chain on Android 17). workerd runs
with ``--local-protocol https``, so this bridge accepts the client's cleartext
request and forwards it to ``https://127.0.0.1:8805``, keeping the Host header:
the mono worker routes on the leftmost label (``accounts.<anything>`` ->
``accounts``).

    python tools/http_bridge.py            # 0.0.0.0:8806 -> 127.0.0.1:8805
    python tools/http_bridge.py --port 9000

Needs a Windows inbound rule (or a ``netsh interface portproxy`` entry) for the
listen port, because the phone reaches this machine over the LAN.
"""

from __future__ import annotations

import argparse
import http.client
import http.server
import ssl
import sys
import threading

HOP_BY_HOP = {
    "connection",
    "proxy-connection",
    "keep-alive",
    "transfer-encoding",
    "upgrade",
    "proxy-authenticate",
    "proxy-authorization",
    "te",
    "trailers",
}

_context = ssl._create_unverified_context()


class Handler(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    server_version = "recflare-bridge"

    def log_message(self, fmt: str, *args) -> None:  # noqa: A003 - stdlib signature
        sys.stderr.write("[bridge] %s %s\n" % (self.address_string(), fmt % args))

    def _forward(self) -> None:
        upstream_host = self.server.upstream_host
        upstream_port = self.server.upstream_port

        try:
            length = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            length = 0
        body = self.rfile.read(length) if length else None

        headers = {
            key: value
            for key, value in self.headers.items()
            if key.lower() not in HOP_BY_HOP
        }

        conn = http.client.HTTPSConnection(
            upstream_host, upstream_port, context=_context, timeout=120
        )
        try:
            conn.request(self.command, self.path, body=body, headers=headers)
            response = conn.getresponse()
            data = response.read()

            self.send_response(response.status, response.reason)
            for key, value in response.getheaders():
                if key.lower() in HOP_BY_HOP or key.lower() == "content-length":
                    continue
                self.send_header(key, value)
            self.send_header("Content-Length", str(len(data)))
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(data)
        except Exception as exc:  # noqa: BLE001 - report upstream failures to the client
            try:
                self.send_error(502, "bridge error: %s" % exc)
            except Exception:  # noqa: BLE001 - client may already be gone
                pass
        finally:
            conn.close()

    do_GET = _forward
    do_POST = _forward
    do_PUT = _forward
    do_DELETE = _forward
    do_PATCH = _forward
    do_HEAD = _forward
    do_OPTIONS = _forward


class Server(http.server.ThreadingHTTPServer):
    daemon_threads = True
    allow_reuse_address = True


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--listen", default="0.0.0.0")
    parser.add_argument("--port", type=int, default=8806)
    parser.add_argument("--upstream-host", default="127.0.0.1")
    parser.add_argument("--upstream-port", type=int, default=8805)
    args = parser.parse_args()

    server = Server((args.listen, args.port), Handler)
    server.upstream_host = args.upstream_host
    server.upstream_port = args.upstream_port
    print(
        "recflare bridge listening on %s:%d -> https://%s:%d"
        % (args.listen, args.port, args.upstream_host, args.upstream_port),
        flush=True,
    )
    server.serve_forever()


if __name__ == "__main__":
    main()
